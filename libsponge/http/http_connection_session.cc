#include "http_connection_session.hh"

#include "http_response.hh"

#include <algorithm>

namespace http1_demo {

void HttpConnectionSession::poll() {
    ByteStream &inbound = _connection.inbound_stream();
    while (!inbound.buffer_empty() && !_response_started) {
        _parser.push(inbound.read(inbound.buffer_size()));
        if (_parser.complete() || _parser.error()) {
            prepare_response();
        }
    }

    if (!_response_started && inbound.input_ended()) {
        prepare_response();
    }

    flush_response();
}

void HttpConnectionSession::prepare_response() {
    if (_response_started) {
        return;
    }

    const HttpResponse response = !_parser.complete()
                                      ? make_text_response(400, "Bad Request", "400 Bad Request\n")
                                      : _handler.handle(_parser.request());
    _pending_response = response.serialize();
    _response_started = true;
}

void HttpConnectionSession::flush_response() {
    if (!_response_started || _response_fully_queued) {
        return;
    }

    while (_response_offset < _pending_response.size() && _connection.remaining_outbound_capacity() > 0) {
        const std::size_t room = _connection.remaining_outbound_capacity();
        const std::size_t remaining = _pending_response.size() - _response_offset;
        const std::size_t chunk_size = std::min(room, remaining);
        const std::size_t written =
            _connection.write(_pending_response.substr(_response_offset, chunk_size));
        if (written == 0) {
            return;
        }
        _response_offset += written;
    }

    if (_response_offset == _pending_response.size()) {
        _connection.end_input_stream();
        _response_fully_queued = true;
    }
}

}  // namespace http1_demo
