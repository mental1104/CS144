add_test(NAME http_parser COMMAND http_parser_test)
add_test(NAME http_static_file_handler COMMAND http_static_file_handler_test)
add_test(NAME http_tcp_connection_e2e COMMAND http_tcp_connection_e2e_test)

add_custom_target (check_http
                   COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 10 -R '^http_'
                   COMMENT "Testing HTTP/1.1 over TCPConnection...")
