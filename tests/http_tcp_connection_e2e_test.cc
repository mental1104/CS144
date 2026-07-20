#include "http_connection_session.hh"
#include "http_test_support.hh"

#include "tcp_config.hh"
#include "wrapping_integers.hh"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using http1_demo::HttpConnectionSession;
using http1_demo::StaticFileHandler;

namespace {

std::size_t transfer(TCPConnection &from, TCPConnection &to) {
    std::size_t count = 0;
    auto &segments = from.segments_out();
    while (!segments.empty()) {
        TCPSegment segment = std::move(segments.front());
        segments.pop();
        to.segment_received(std::move(segment));
        ++count;
    }
    return count;
}

std::string run_request(const std::vector<std::string> &application_chunks) {
    const std::string root = "/tmp/cs144-http-e2e-static";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    {
        std::ofstream output{root + "/hello.txt", std::ios::binary};
        output << "Hello through TCPConnection!\n";
    }

    TCPConfig client_config;
    client_config.fixed_isn = WrappingInt32{1000};
    client_config.rt_timeout = 20;

    TCPConfig server_config;
    server_config.fixed_isn = WrappingInt32{2000};
    server_config.rt_timeout = 20;

    TCPConnection client{client_config};
    TCPConnection server{server_config};
    HttpConnectionSession server_http{server, StaticFileHandler{root}};

    client.connect();

    std::size_t next_chunk = 0;
    std::string response;
    bool client_closed_output = false;

    for (std::size_t step = 0; step < 20000; ++step) {
        std::size_t activity = 0;
        activity += transfer(client, server);
        server_http.poll();
        activity += transfer(server, client);

        if (next_chunk < application_chunks.size() && client.remaining_outbound_capacity() > 0) {
            const std::size_t written = client.write(application_chunks[next_chunk]);
            require_http(written == application_chunks[next_chunk].size(), "request chunk should fit sender capacity");
            ++next_chunk;
            ++activity;
        }

        server_http.poll();
        activity += transfer(client, server);
        server_http.poll();
        activity += transfer(server, client);

        ByteStream &client_inbound = client.inbound_stream();
        if (!client_inbound.buffer_empty()) {
            response += client_inbound.read(client_inbound.buffer_size());
            ++activity;
        }

        if (client_inbound.eof() && !client_closed_output) {
            client.end_input_stream();
            client_closed_output = true;
            ++activity;
        }

        client.tick(1);
        server.tick(1);
        server_http.poll();

        if (client_closed_output && !client.active() && !server.active()) {
            std::filesystem::remove_all(root);
            return response;
        }

        if (activity == 0 && step > 1000 && client_closed_output) {
            break;
        }
    }

    std::filesystem::remove_all(root);
    require_http(false, "TCPConnection pair did not reach clean shutdown");
    return {};
}

}  // namespace

int main() {
    const std::string whole_request =
        "GET /hello.txt HTTP/1.1\r\n"
        "Host: example.test\r\n"
        "Connection: close\r\n"
        "\r\n";

    const std::string whole_response = run_request({whole_request});

    std::vector<std::string> one_byte_chunks;
    for (const char ch : whole_request) {
        one_byte_chunks.emplace_back(1, ch);
    }
    const std::string bytewise_response = run_request(one_byte_chunks);

    require_http(whole_response == bytewise_response,
                 "HTTP response must not depend on application write or TCP segment boundaries");
    require_http(whole_response.find("HTTP/1.1 200 OK\r\n") == 0, "status line");
    require_http(whole_response.find("Content-Length: 29\r\n") != std::string::npos, "content length");
    require_http(whole_response.find("\r\n\r\nHello through TCPConnection!\n") != std::string::npos, "body");
    return 0;
}
