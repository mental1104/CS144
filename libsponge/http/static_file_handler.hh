#pragma once

#include "http_request.hh"
#include "http_response.hh"

#include <string>
#include <utility>

namespace http1_demo {

class StaticFileHandler {
  public:
    explicit StaticFileHandler(std::string document_root) : _document_root(std::move(document_root)) {}

    HttpResponse handle(const HttpRequest &request) const;

  private:
    static std::string content_type_for_path(const std::string &path);
    static bool safe_target(const std::string &target);

    std::string _document_root;
};

}  // namespace http1_demo
