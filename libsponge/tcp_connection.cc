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
        TCPSegment seg = move(_sender.segments_out().front());
        _sender.segments_out().pop();

        fill_ack_and_window(seg);

        _segments_out.push(move(seg));
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
    if (!_active) {
        return;
    }

    const TCPHeader &header = seg.header();

    if (header.rst) {
        if (!in_listen_state()) {
            mark_streams_error();
        }
        return;
    }

    if (in_listen_state() && !header.syn) {
        return;
    }

    _time_since_last_segment_received = 0;

    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
    }

    _receiver.segment_received(seg);

    disable_linger_after_passive_close();

    _sender.fill_window();

    send_ack_if_needed(seg);

    send_segments();
    update_active();
}

void TCPConnection::segment_received(TCPSegment &&seg) {
    if (!_active) {
        return;
    }

    const TCPHeader header = seg.header();

    if (header.rst) {
        if (!in_listen_state()) {
            mark_streams_error();
        }
        return;
    }

    if (in_listen_state() && !header.syn) {
        return;
    }

    const bool segment_consumes_sequence_space = seg.length_in_sequence_space() > 0;
    _time_since_last_segment_received = 0;

    if (header.ack) {
        _sender.ack_received(header.ackno, header.win);
    }

    _receiver.segment_received(move(seg));

    disable_linger_after_passive_close();

    _sender.fill_window();

    if (segment_consumes_sequence_space && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }

    send_segments();
    update_active();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    const size_t bytes_written = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    update_active();
    return bytes_written;
}

size_t TCPConnection::write(string &&data) {
    const size_t bytes_written = _sender.stream_in().write(move(data));
    _sender.fill_window();
    send_segments();
    update_active();
    return bytes_written;
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
