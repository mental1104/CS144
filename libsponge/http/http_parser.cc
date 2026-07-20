#include "http_parser.hh"

#include <cctype>
#include <sstream>

namespace http1_demo {

namespace {
constexpr std::size_t MAX_HEADER_BYTES = 16 * 1024;
}

std::string HttpRequest::header(const std::string &name) const {
    std::string key;
    key.reserve(name.size());
    for (const unsigned char ch : name) {
        key.push_back(static_cast<char>(std::tolower(ch)));
    }

    const auto it = headers.find(key);
    return it == headers.end() ? std::string{} : it->second;
}

void HttpRequestParser::push(const std::string &bytes) {
    if (_state == ParseState::Complete || _state == ParseState::Error || bytes.empty()) {
        return;
    }

    _received_bytes += bytes.size();
    if (_received_bytes > MAX_HEADER_BYTES) {
        fail("request header exceeds teaching limit");
        return;
    }

    _buffer.append(bytes);
    process();
}

void HttpRequestParser::process() {
    while (_state == ParseState::RequestLine || _state == ParseState::Headers) {
        const std::size_t line_end = _buffer.find("\r\n");
        if (line_end == std::string::npos) {
            return;
        }

        const std::string line = _buffer.substr(0, line_end);
        _buffer.erase(0, line_end + 2);

        if (_state == ParseState::RequestLine) {
            parse_request_line(line);
            continue;
        }

        if (line.empty()) {
            if (_request.header("host").empty()) {
                fail("HTTP/1.1 requires Host header");
            } else {
                _state = ParseState::Complete;
            }
            return;
        }

        parse_header_line(line);
    }
}

void HttpRequestParser::parse_request_line(const std::string &line) {
    std::istringstream input{line};
    std::string extra;

    if (!(input >> _request.method >> _request.target >> _request.version) || (input >> extra)) {
        fail("malformed request line");
        return;
    }

    if (_request.method.empty() || _request.target.empty() || _request.version != "HTTP/1.1") {
        fail("unsupported or malformed request line");
        return;
    }

    if (_request.target.front() != '/') {
        fail("origin-form request target must start with '/'");
        return;
    }

    _state = ParseState::Headers;
}

void HttpRequestParser::parse_header_line(const std::string &line) {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0) {
        fail("malformed header line");
        return;
    }

    const std::string raw_name = line.substr(0, colon);
    if (!valid_header_name(raw_name)) {
        fail("invalid header name");
        return;
    }

    const std::string name = lowercase_ascii(raw_name);
    const std::string value = trim_ows(line.substr(colon + 1));

    if (_request.headers.count(name) != 0) {
        fail("duplicate headers are outside this teaching subset");
        return;
    }

    _request.headers.emplace(name, value);
}

std::string HttpRequestParser::trim_ows(const std::string &value) {
    std::size_t begin = 0;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t')) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t')) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string HttpRequestParser::lowercase_ascii(const std::string &value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char ch : value) {
        result.push_back(static_cast<char>(std::tolower(ch)));
    }
    return result;
}

bool HttpRequestParser::valid_header_name(const std::string &name) {
    for (const unsigned char ch : name) {
        if (!(std::isalnum(ch) || ch == '-' || ch == '_')) {
            return false;
        }
    }
    return !name.empty();
}

void HttpRequestParser::fail(const std::string &message) {
    _state = ParseState::Error;
    _error_message = message;
}

}  // namespace http1_demo
