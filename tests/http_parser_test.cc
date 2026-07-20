#include "http_parser.hh"
#include "http_test_support.hh"

#include <string>
#include <vector>

using http1_demo::HttpRequestParser;

namespace {

const std::string request =
    "GET /hello.txt HTTP/1.1\r\n"
    "Host: example.test\r\n"
    "Connection: close\r\n"
    "\r\n";

void assert_request(const HttpRequestParser &parser) {
    require_http(parser.complete(), "request should be complete");
    require_http(parser.request().method == "GET", "method");
    require_http(parser.request().target == "/hello.txt", "target");
    require_http(parser.request().version == "HTTP/1.1", "version");
    require_http(parser.request().header("HOST") == "example.test", "case-insensitive Host lookup");
}

void complete_input() {
    HttpRequestParser parser;
    parser.push(request);
    assert_request(parser);
}

void arbitrary_chunks() {
    HttpRequestParser parser;
    const std::vector<std::string> chunks = {
        "GET /hello.txt HT", "TP/1.1\r", "\nHost: ex", "ample.test\r\nCon",
        "nection: close\r\n", "\r", "\n"};
    for (const auto &chunk : chunks) {
        parser.push(chunk);
    }
    assert_request(parser);
}

void one_byte_at_a_time() {
    HttpRequestParser parser;
    for (const char ch : request) {
        parser.push(std::string(1, ch));
    }
    assert_request(parser);
}

void malformed_request_line() {
    HttpRequestParser parser;
    parser.push("GET /missing-version\r\nHost: example.test\r\n\r\n");
    require_http(parser.error(), "malformed request line should fail");
}

void missing_host() {
    HttpRequestParser parser;
    parser.push("GET / HTTP/1.1\r\nConnection: close\r\n\r\n");
    require_http(parser.error(), "HTTP/1.1 request without Host should fail");
}

void malformed_header() {
    HttpRequestParser parser;
    parser.push("GET / HTTP/1.1\r\nHost example.test\r\n\r\n");
    require_http(parser.error(), "header without colon should fail");
}

void oversized_header_across_chunks() {
    HttpRequestParser parser;
    parser.push("GET / HTTP/1.1\r\nHost: example.test\r\nX-Fill: ");
    for (std::size_t i = 0; i < 17; ++i) {
        parser.push(std::string(1024, 'x'));
    }
    require_http(parser.error(), "header limit should count all received chunks");
}

}  // namespace

int main() {
    complete_input();
    arbitrary_chunks();
    one_byte_at_a_time();
    malformed_request_line();
    missing_host();
    malformed_header();
    oversized_header_across_chunks();
    return 0;
}
