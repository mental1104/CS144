add_test(NAME t_udp_connection COMMAND udp_connection_test)

add_custom_target (check_udp
                   COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 10 -R '^t_udp_connection$'
                   COMMENT "Testing the minimal connected UDP adapter...")
