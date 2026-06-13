add_library(
  mini_redis_core
  base/version.cc
)

target_include_directories(
  mini_redis_core
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(
  mini_redis_core
  PRIVATE
    MINI_REDIS_VERSION="${PROJECT_VERSION}"
)

mini_redis_set_project_warnings(mini_redis_core)
mini_redis_enable_sanitizers(mini_redis_core)

add_executable(mini_redis_server main.cc)
target_link_libraries(mini_redis_server PRIVATE mini_redis_core)

mini_redis_set_project_warnings(mini_redis_server)
mini_redis_enable_sanitizers(mini_redis_server)
