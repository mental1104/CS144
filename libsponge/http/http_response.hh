#pragma once

#include <map>
#include <string>

namespace http1_demo {

struct HttpResponse {
    int status{200};
    std::string reason{"OK"};
    std::map<std::string, std::string> headers{};
    std::string body{};

    std::string serialize() const;
};

HttpResponse make_text_response(int status,
                                const std::string &reason,
                                const std::string &body,
                                const std::string &content_type = "text/plain; charset=utf-8");

}  // namespace http1_demo
