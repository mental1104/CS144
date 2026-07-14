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
    , _stream(capacity)
    , tcptimer(retx_timeout, retx_timeout) {}

size_t TCPSender::bytes_in_flight() const {
    return _bytes_in_flight;
}

void TCPSender::fill_window() {
    // syn-sender
    if (next_seqno_absolute() == 0)
        send_noempty_segment(0);

    size_t window = _window_size == 0 ? 1 : _window_size;
    size_t in_flight = bytes_in_flight();

    if (in_flight >= window)
        return;

    // connection出bug的第一个地方，stream里面有值的时候才可以写入,否则会一直写入空
    while (!stream_in().buffer_empty() && in_flight < window) {
        send_noempty_segment(window - in_flight);
        in_flight = bytes_in_flight();
    }

    // fin
    if (!_flag_fin && stream_in().eof() && in_flight < window &&
        stream_in().buffer_empty())
        send_noempty_segment(window - in_flight);
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno,
                             const uint16_t window_size) {
    // 当前收到的 ACK 转成 absolute seqno
    const uint64_t received_ack_absolute = unwrap(ackno, _isn, _next_seqno);

    // ACK 不能确认 sender 还没发送过的数据
    if (received_ack_absolute > _next_seqno) {
        return;
    }

    // 如果已有旧 ACK，把旧 ACK 也转成 absolute seqno
    const uint64_t previous_ack_absolute =
        _ackno.has_value() ? unwrap(_ackno.value(), _isn, _next_seqno) : 0;

    // ACK 不能倒退；相等允许，因为窗口可能更新
    if (_ackno.has_value() && received_ack_absolute < previous_ack_absolute) {
        return;
    }

    // 记录最新 ACK 和最新接收方窗口
    _ackno = ackno;
    _window_size = window_size;

    // 记录本次 ACK 是否真的清掉了至少一个 outstanding segment
    bool removed_any_outstanding_segment = false;

    // TCP ACK 是累计确认，所以只能从最早未确认的 segment 开始清账
    while (!_outstanding_segment.empty()) {
        // 队首就是最早发送、最早需要被确认的 segment
        const TCPSegment &oldest_unacked_segment = _outstanding_segment.front();

        // 队首 segment 的起始 absolute seqno
        const uint64_t segment_start_absolute =
            unwrap(oldest_unacked_segment.header().seqno, _isn, _next_seqno);

        // 队首 segment 的结束 absolute seqno
        const uint64_t segment_end_absolute =
            segment_start_absolute + oldest_unacked_segment.length_in_sequence_space();

        // 如果当前 ACK 还没有完整覆盖队首 segment，后面的 segment 也不可能被确认
        if (segment_end_absolute > received_ack_absolute) {
            break;
        }

        // 队首 segment 已经被完整确认，从在途字节数中扣掉它占用的 sequence space
        _bytes_in_flight -= oldest_unacked_segment.length_in_sequence_space();

        // 从 outstanding 账本中移除这个已经确认的 segment
        _outstanding_segment.pop_front();

        // 标记本次 ACK 确实推进了确认进度
        removed_any_outstanding_segment = true;
    }

    // 如果本次 ACK 清掉了至少一个 segment，说明传输有进展，重置 RTO
    if (removed_any_outstanding_segment) {
        tcptimer.start();
    }

    // 如果没有任何未确认 segment 了，就关闭重传定时器
    if (_outstanding_segment.empty()) {
        tcptimer.stop();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {

    if (tcptimer.is_running() == false || tcptimer.advance_and_expired(ms_since_last_tick) == false)
        return;

    _segments_out.push(_outstanding_segment.front());

    // _window_size的初始值要为1，不然开始的时候，出错没办法double，例如syn出错
    tcptimer.restart_after_timeout(_window_size);

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
    // This function only handles fresh sends. Retransmission reuses
    // _outstanding_segment.front() in tick() and never reads from ByteStream.
    //
    // `remain` is remaining TCP sequence-space in the current window, not just
    // payload bytes. SYN and FIN each consume one sequence number.

    const bool is_initial_syn = next_seqno_absolute() == 0;

    // 1. Decide whether a segment is allowed.
    // The initial SYN is the only fresh send allowed when remain == 0. FIN is
    // not allowed in that case because FIN also consumes one sequence number.
    if (remain == 0 && !is_initial_syn) {
        return;
    }

    TCPSegment segment;

    // 2. Set seqno and SYN.
    segment.header().seqno = wrap(_next_seqno, _isn);

    if (is_initial_syn) {
        segment.header().syn = true;
    }

    // 3. Read payload, bounded by both remaining sequence-space and max size.
    const size_t payload_size = std::min(remain, TCPConfig::MAX_PAYLOAD_SIZE);
    Buffer payload(stream_in().read(payload_size));
    segment.payload() = payload;

    remain -= payload.size();

    // 4. Attach FIN if EOF is reached and one sequence number remains.
    const bool can_send_fin =
        !_flag_fin &&
        remain > 0 &&
        stream_in().eof() &&
        stream_in().buffer_empty();

    if (can_send_fin) {
        segment.header().fin = true;
        _flag_fin = true;
    }

    // 5. Publish to the outbound queue and the unacknowledged ledger.
    // _segments_out is the outbound queue; _outstanding_segment is the ledger
    // used for ACK accounting and retransmission.
    _segments_out.push(segment);
    _outstanding_segment.push_back(segment);

    // 6. Update sequence high-water mark and bytes in flight.
    // length_in_sequence_space() counts TCP sequence-space: SYN, payload bytes,
    // and FIN.
    const size_t sequence_length = segment.length_in_sequence_space();

    _next_seqno += sequence_length;
    _bytes_in_flight += sequence_length;

    // 7. Start the retransmission timer for the first outstanding segment.
    if (!tcptimer.is_running()) {
        tcptimer.start();
    }
}
