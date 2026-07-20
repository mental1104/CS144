#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

inline void require_http(const bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
