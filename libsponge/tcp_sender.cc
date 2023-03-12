#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , tcptimer(retx_timeout, retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    if (_ackno.has_value()) 
        return _next_seqno - unwrap(_ackno.value(), _isn, _next_seqno);
    else 
        return next_seqno_absolute();
}

void TCPSender::fill_window() {
    // syn-sender
    if (next_seqno_absolute() == 0)
        send_noempty_segment(0);

    // send but not received acked
    if (next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight())
        return;

    size_t window = _window_size == 0 ? 1 : _window_size;

    // connection出bug的第一个地方，stream里面有值的时候才可以写入,否则会一直写入空
    while (!stream_in().buffer_empty() && remain() != window) {
        send_noempty_segment(window - remain());
    }

    // fin
    if (!_flag_fin && stream_in().eof() && remain() != window &&
        stream_in().buffer_empty())

    send_noempty_segment(window - remain());
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 超范围，返回
    uint64_t ackno_absolute = unwrap(ackno, _isn, _next_seqno);
    if (ackno_absolute > _next_seqno)
        return;

    if (_ackno.has_value() &&
        ackno_absolute < (unwrap(_ackno.value(), _isn, _next_seqno)))
        return;

    _ackno = ackno;
    _window_size = window_size;

    while (!_outstanding_segment.empty()) {
        auto it = _outstanding_segment.front();
        size_t ack_absolute_seqno = unwrap(_ackno.value(), _isn, _next_seqno);
        size_t end_seqno = unwrap(it.header().seqno, _isn, _next_seqno) +
                                  it.length_in_sequence_space();

        if (end_seqno <= ack_absolute_seqno) {
            _outstanding_segment.pop_front();
            tcptimer.start();
        } else
            break;
    }

    if (_outstanding_segment.empty())
        tcptimer.close();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 

    if (tcptimer.timer() == false || tcptimer.rto(ms_since_last_tick) == false)
        return;

    _segments_out.push(_outstanding_segment.front());

    // _window_size的初始值要为1，不然开始的时候，出错没办法double，例如syn出错
    tcptimer.double_rto_restart(_window_size);

 }

unsigned int TCPSender::consecutive_retransmissions() const { 
    return tcptimer.retransmission();
 }

void TCPSender::send_empty_segment() {
    TCPSegment tcpSegment;
    tcpSegment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(tcpSegment);
}

void TCPSender::send_noempty_segment(size_t remain) {
    // tcp-not-acked
    if (next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight())
        return;

    TCPSegment tcpSegment;

    // syn
    if (next_seqno_absolute() == 0)
        tcpSegment.header().syn = true;

    tcpSegment.header().seqno = wrap(_next_seqno, _isn);
    Buffer _payload(stream_in().read(min(remain, TCPConfig::MAX_PAYLOAD_SIZE)));
    tcpSegment.payload() = _payload;
    remain -= _payload.size();

    if (remain > 0 && stream_in().eof() == true &&
        stream_in().buffer_empty() == true) {

        _flag_fin = true;
        tcpSegment.header().fin = true;
    }

    _segments_out.push(tcpSegment);
    _outstanding_segment.push_back(tcpSegment);
    _next_seqno += tcpSegment.length_in_sequence_space();

    if (!tcptimer.timer())
        tcptimer.start();
}
