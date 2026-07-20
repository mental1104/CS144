file (GLOB LIB_SOURCES "*.cc" "util/*.cc" "tcp_helpers/*.cc" "http/*.cc")
add_library (sponge STATIC ${LIB_SOURCES})
