#pragma once

#include "http_request.hh"

#include <cstddef>
#include <string>

namespace http1_demo {

enum class ParseState {
    RequestLine,
    Headers,
    Complete,
    Error,
};

class HttpRequestParser {
  public:
    void push(const std::string &bytes);

    ParseState state() const { return _state; }
    bool complete() const { return _state == ParseState::Complete; }
    bool error() const { return _state == ParseState::Error; }

    const HttpRequest &request() const { return _request; }
    const std::string &error_message() const { return _error_message; }
    std::size_t buffered_bytes() const { return _buffer.size(); }

  private:
    static std::string trim_ows(const std::string &value);
    static std::string lowercase_ascii(const std::string &value);
    static bool valid_header_name(const std::string &name);

    void process();
    void parse_request_line(const std::string &line);
    void parse_header_line(const std::string &line);
    void fail(const std::string &message);

    ParseState _state{ParseState::RequestLine};
    std::string _buffer{};
    HttpRequest _request{};
    std::string _error_message{};
    std::size_t _received_bytes{0};
};

}  // namespace http1_demo
