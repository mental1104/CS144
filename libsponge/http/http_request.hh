#pragma once

#include <map>
#include <string>

namespace http1_demo {

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;

    std::string header(const std::string &name) const;
};

}  // namespace http1_demo
