#include "http_response.hh"

#include <sstream>

namespace http1_demo {

std::string HttpResponse::serialize() const {
    std::ostringstream output;
    output << "HTTP/1.1 " << status << ' ' << reason << "\r\n";
    for (const auto &header : headers) {
        output << header.first << ": " << header.second << "\r\n";
    }
    output << "\r\n" << body;
    return output.str();
}

HttpResponse make_text_response(const int status,
                                const std::string &reason,
                                const std::string &body,
                                const std::string &content_type) {
    HttpResponse response;
    response.status = status;
    response.reason = reason;
    response.body = body;
    response.headers["Connection"] = "close";
    response.headers["Content-Length"] = std::to_string(body.size());
    response.headers["Content-Type"] = content_type;
    return response;
}

}  // namespace http1_demo
