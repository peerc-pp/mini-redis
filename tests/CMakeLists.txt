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
  mini_redis_protocol_tests
  unit/protocol_test.cc
)

target_link_libraries(mini_redis_protocol_tests PRIVATE mini_redis_core)

mini_redis_set_project_warnings(mini_redis_protocol_tests)
mini_redis_enable_sanitizers(mini_redis_protocol_tests)

add_test(NAME mini_redis_protocol_tests COMMAND mini_redis_protocol_tests)

add_executable(
  mini_redis_command_registry_tests
  unit/command_registry_test.cc
)

target_link_libraries(
  mini_redis_command_registry_tests
  PRIVATE
    mini_redis_core
)

mini_redis_set_project_warnings(mini_redis_command_registry_tests)
mini_redis_enable_sanitizers(mini_redis_command_registry_tests)

add_test(
  NAME mini_redis_command_registry_tests
  COMMAND mini_redis_command_registry_tests
)

add_executable(
  mini_redis_database_tests
  unit/database_test.cc
)

target_link_libraries(
  mini_redis_database_tests
  PRIVATE
    mini_redis_core
)

mini_redis_set_project_warnings(mini_redis_database_tests)
mini_redis_enable_sanitizers(mini_redis_database_tests)

add_test(
  NAME mini_redis_database_tests
  COMMAND mini_redis_database_tests
)

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

add_executable(
  mini_redis_session_tests
  integration/session_test.cc
)

target_link_libraries(
  mini_redis_session_tests
  PRIVATE
    mini_redis_core
)

mini_redis_set_project_warnings(mini_redis_session_tests)
mini_redis_enable_sanitizers(mini_redis_session_tests)

add_test(
  NAME mini_redis_session_tests
  COMMAND mini_redis_session_tests
)

add_executable(
  mini_redis_value_tests
  unit/value_test.cc
)

target_link_libraries(
  mini_redis_value_tests
  PRIVATE
    mini_redis_core
)

mini_redis_set_project_warnings(mini_redis_value_tests)
mini_redis_enable_sanitizers(mini_redis_value_tests)

add_test(
  NAME mini_redis_value_tests
  COMMAND mini_redis_value_tests
)

add_executable(
  mini_redis_integer_parser_tests
  unit/integer_parser_test.cc
)

target_link_libraries(
  mini_redis_integer_parser_tests
  PRIVATE
    mini_redis_core
)

mini_redis_set_project_warnings(mini_redis_integer_parser_tests)
mini_redis_enable_sanitizers(mini_redis_integer_parser_tests)

add_test(
  NAME mini_redis_integer_parser_tests
  COMMAND mini_redis_integer_parser_tests
)

add_executable(
  mini_redis_integer_arithmetic_tests
  unit/integer_arithmetic_test.cc
)

target_link_libraries(
  mini_redis_integer_arithmetic_tests
  PRIVATE
    mini_redis_core
)

mini_redis_set_project_warnings(mini_redis_integer_arithmetic_tests)
mini_redis_enable_sanitizers(mini_redis_integer_arithmetic_tests)

add_test(
  NAME mini_redis_integer_arithmetic_tests
  COMMAND mini_redis_integer_arithmetic_tests
)

add_executable(
  mini_redis_list_range_tests
  unit/list_range_test.cc
)

target_link_libraries(
  mini_redis_list_range_tests
  PRIVATE
    mini_redis_core
)

mini_redis_set_project_warnings(mini_redis_list_range_tests)
mini_redis_enable_sanitizers(mini_redis_list_range_tests)

add_test(
  NAME mini_redis_list_range_tests
  COMMAND mini_redis_list_range_tests
)

add_executable(
  mini_redis_hash_table_tests
  unit/hash_table_test.cc
)

target_link_libraries(
  mini_redis_hash_table_tests
  PRIVATE
    mini_redis_core
)

mini_redis_set_project_warnings(mini_redis_hash_table_tests)
mini_redis_enable_sanitizers(mini_redis_hash_table_tests)

add_test(
  NAME mini_redis_hash_table_tests
  COMMAND mini_redis_hash_table_tests
)
find_package(Python3 COMPONENTS Interpreter QUIET)
find_program(MINI_REDIS_REDIS_SERVER_EXECUTABLE redis-server)

if(Python3_Interpreter_FOUND AND MINI_REDIS_REDIS_SERVER_EXECUTABLE)
  add_test(
    NAME mini_redis_redis_oracle_tests
    COMMAND
      "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/differential/redis_oracle_test.py"
      --mini-server "$<TARGET_FILE:mini_redis_server>"
      --redis-server "${MINI_REDIS_REDIS_SERVER_EXECUTABLE}"
  )

  set_tests_properties(
    mini_redis_redis_oracle_tests
    PROPERTIES
      RUN_SERIAL TRUE
      TIMEOUT 30
  )
endif()
