#pragma once

#include "http_parser.hh"
#include "static_file_handler.hh"
#include "tcp_connection.hh"

#include <string>
#include <utility>

namespace http1_demo {

// Teaching adapter: HTTP reads only the reassembled ByteStream exposed by TCPConnection.
// It never observes TCPSegment boundaries, sequence numbers, ACKs, or retransmissions.
class HttpConnectionSession {
  public:
    HttpConnectionSession(TCPConnection &connection, StaticFileHandler handler)
        : _connection(connection), _handler(std::move(handler)) {}

    // Drain currently available inbound bytes, parse at most one request,
    // and flush the response into TCPConnection's outbound ByteStream.
    void poll();

    bool response_started() const { return _response_started; }
    bool response_fully_queued() const { return _response_fully_queued; }

  private:
    void prepare_response();
    void flush_response();

    TCPConnection &_connection;
    StaticFileHandler _handler;
    HttpRequestParser _parser{};
    std::string _pending_response{};
    std::size_t _response_offset{0};
    bool _response_started{false};
    bool _response_fully_queued{false};
};

}  // namespace http1_demo
