add_executable(
  mini_redis_unit_tests
  unit/version_test.cc
)

target_link_libraries(mini_redis_unit_tests PRIVATE mini_redis_core)

mini_redis_set_project_warnings(mini_redis_unit_tests)
mini_redis_enable_sanitizers(mini_redis_unit_tests)

add_test(NAME mini_redis_unit_tests COMMAND mini_redis_unit_tests)

add_executable(
  mini_redis_network_tests
  unit/network_test.cc
)

target_link_libraries(mini_redis_network_tests PRIVATE mini_redis_core)

mini_redis_set_project_warnings(mini_redis_network_tests)
mini_redis_enable_sanitizers(mini_redis_network_tests)

add_test(NAME mini_redis_network_tests COMMAND mini_redis_network_tests)

add_executable(
  mini_redis_phase_one_network_tests
  integration/network_phase_one_test.cc
)

target_link_libraries(
  mini_redis_phase_one_network_tests
  PRIVATE
    mini_redis_core
)

mini_redis_set_project_warnings(mini_redis_phase_one_network_tests)
mini_redis_enable_sanitizers(mini_redis_phase_one_network_tests)

add_test(
  NAME mini_redis_phase_one_network_tests
  COMMAND mini_redis_phase_one_network_tests
)
