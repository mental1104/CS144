#include "tcp_connection.hh"

#include <algorithm>
#include <iostream>
#include <limits>
#include <utility>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

bool TCPConnection::in_listen_state() const {
    return !_receiver.ackno().has_value() && _sender.next_seqno_absolute() == 0;
}

bool TCPConnection::inbound_stream_finished() const { return _receiver.stream_out().input_ended(); }

bool TCPConnection::outbound_stream_finished() const { return _sender.stream_in().eof(); }

bool TCPConnection::has_unacknowledged_segments() const { return _sender.bytes_in_flight() != 0; }

bool TCPConnection::clean_shutdown_complete() const {
    return inbound_stream_finished() && outbound_stream_finished() && !has_unacknowledged_segments();
}

bool TCPConnection::linger_timer_expired() const {
    return _time_since_last_segment_received >= 10 * _cfg.rt_timeout;
}

void TCPConnection::fill_ack_and_window(TCPSegment &seg) const {
    if (!_receiver.ackno().has_value()) {
        return;
    }

    seg.header().ack = true;
    seg.header().ackno = _receiver.ackno().value();
    seg.header().win = min(_receiver.window_size(), size_t{numeric_limits<uint16_t>::max()});
}

void TCPConnection::mark_streams_error() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::disable_linger_after_passive_close() {
    if (inbound_stream_finished() && !outbound_stream_finished()) {
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::send_ack_if_needed(const TCPSegment &seg) {
    if (seg.length_in_sequence_space() > 0 && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
}

void TCPConnection::send_segments() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        fill_ack_and_window(seg);

        _segments_out.push(seg);
    }
}

void TCPConnection::reset_connection() {
    TCPSegment rst_seg;
    rst_seg.header().rst = true;
    rst_seg.header().seqno = _sender.next_seqno();

    fill_ack_and_window(rst_seg);

    _segments_out.push(rst_seg);
    mark_streams_error();
}

void TCPConnection::update_active() {
    if (_sender.stream_in().error() || _receiver.stream_out().error()) {
        _active = false;
        return;
    }

    if (!clean_shutdown_complete()) {
        _active = true;
        return;
    }

    if (!_linger_after_streams_finish || linger_timer_expired()) {
        _active = false;
    } else {
        _active = true;
    }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // 连接已经关闭或被重置后，后续收到的任何报文都不再参与状态机处理。
    if (!_active) {
        return;
    }

    const TCPHeader &header = seg.header();

    // RST 会立即终止已建立或已同步的连接；但 LISTEN 状态下还没有具体连接，
    // 此时收到的 RST 不应该把本地输入/输出流标成错误，直接忽略即可。
    if (header.rst) {
        if (!in_listen_state()) {
            mark_streams_error();
        }
        return;
    }

    // LISTEN 状态的主线是等待 SYN 来创建连接状态。非 SYN 报文既不能推进握手，
    // 也没有可接受的序列号空间，因此短路丢弃，不更新时间、ACK 或发送侧状态。
    if (in_listen_state() && !header.syn) {
        return;
    }

    // 走到这里说明报文和当前连接相关：要么是握手 SYN，要么属于已有连接。
    // 因此它应当刷新“上次收到报文”的计时。
    _time_since_last_segment_received = 0;

    // ACK 分支只更新发送侧：释放已确认的数据，并记录对端通告窗口。
    // 这个分支不会提前退出，因为同一个报文还可能携带 SYN/FIN 或 payload。
    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
    }

    // 主线：先把占用序列号空间的内容交给接收侧重组，再根据被动关闭情况调整
    // linger；随后让发送侧利用新窗口继续发数据。如果对端发来了数据/控制位但
    // 当前没有待发响应，则补一个纯 ACK，最后给所有待发报文填上 ACK 和窗口。
    _receiver.segment_received(seg);

    disable_linger_after_passive_close();

    _sender.fill_window();

    send_ack_if_needed(seg);

    send_segments();
    update_active();
}

bool TCPConnection::active() const { return _active; }

template <typename Data>
size_t TCPConnection::write_impl(Data &&data) {
    const size_t bytes_written = _sender.stream_in().write(std::forward<Data>(data));
    _sender.fill_window();
    send_segments();
    update_active();
    return bytes_written;
}

size_t TCPConnection::write(const string &data) { return write_impl(data); }

size_t TCPConnection::write(string &&data) {
    return write_impl(std::move(data));
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active) {
        return;
    }

    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        reset_connection();
        return;
    }

    send_segments();
    update_active();
}

void TCPConnection::send_empty_segment() {
    if (!_active || !_receiver.ackno().has_value()) {
        return;
    }

    _sender.send_empty_segment();
    send_segments();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
    update_active();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            reset_connection();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
