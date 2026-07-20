#include "static_file_handler.hh"
#include "http_test_support.hh"

#include <filesystem>
#include <fstream>
#include <string>

using http1_demo::HttpRequest;
using http1_demo::StaticFileHandler;

namespace {

HttpRequest request_for(const std::string &method, const std::string &target) {
    HttpRequest request;
    request.method = method;
    request.target = target;
    request.version = "HTTP/1.1";
    request.headers["host"] = "example.test";
    return request;
}

}  // namespace

int main() {
    const std::string root = "/tmp/cs144-http-static-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    {
        std::ofstream output{root + "/index.html", std::ios::binary};
        output << "<h1>Hello</h1>\n";
    }

    const StaticFileHandler handler{root};

    const auto ok = handler.handle(request_for("GET", "/"));
    require_http(ok.status == 200, "existing file should return 200");
    require_http(ok.body == "<h1>Hello</h1>\n", "body should match file");
    require_http(ok.headers.at("Content-Length") == std::to_string(ok.body.size()), "content length");

    const auto missing = handler.handle(request_for("GET", "/missing.txt"));
    require_http(missing.status == 404, "missing file should return 404");

    const auto method = handler.handle(request_for("POST", "/"));
    require_http(method.status == 405, "unsupported method should return 405");
    require_http(method.headers.at("Allow") == "GET", "405 should advertise GET");

    const auto traversal = handler.handle(request_for("GET", "/../secret"));
    require_http(traversal.status == 400, "path traversal should return 400");

    std::filesystem::remove_all(root);
    return 0;
}
