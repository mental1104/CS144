#include "static_file_handler.hh"

#include <fstream>
#include <sstream>

namespace http1_demo {

HttpResponse StaticFileHandler::handle(const HttpRequest &request) const {
    if (request.method != "GET") {
        HttpResponse response = make_text_response(405, "Method Not Allowed", "405 Method Not Allowed\n");
        response.headers["Allow"] = "GET";
        return response;
    }

    if (!safe_target(request.target)) {
        return make_text_response(400, "Bad Request", "400 Bad Request\n");
    }

    std::string target = request.target;
    const std::size_t query = target.find('?');
    if (query != std::string::npos) {
        target.erase(query);
    }
    if (target == "/") {
        target = "/index.html";
    }

    const std::string path = _document_root + target;
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return make_text_response(404, "Not Found", "404 Not Found\n");
    }

    std::ostringstream body;
    body << input.rdbuf();

    HttpResponse response;
    response.status = 200;
    response.reason = "OK";
    response.body = body.str();
    response.headers["Connection"] = "close";
    response.headers["Content-Length"] = std::to_string(response.body.size());
    response.headers["Content-Type"] = content_type_for_path(path);
    return response;
}

bool StaticFileHandler::safe_target(const std::string &target) {
    return !target.empty() && target.front() == '/' && target.find("..") == std::string::npos &&
           target.find('\\') == std::string::npos;
}

std::string StaticFileHandler::content_type_for_path(const std::string &path) {
    const std::size_t dot = path.rfind('.');
    const std::string extension = dot == std::string::npos ? std::string{} : path.substr(dot);

    if (extension == ".html" || extension == ".htm") {
        return "text/html; charset=utf-8";
    }
    if (extension == ".txt") {
        return "text/plain; charset=utf-8";
    }
    if (extension == ".css") {
        return "text/css; charset=utf-8";
    }
    return "application/octet-stream";
}

}  // namespace http1_demo
