add_test(NAME congestion_control_model COMMAND congestion_control_model_test)

add_custom_target (check_congestion_control
                   COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 10 -R '^congestion_control_'
                   COMMENT "Testing congestion-control teaching models...")
