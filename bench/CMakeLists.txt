add_executable(
  mini_redis_hash_table_benchmark
  hash_table_benchmark.cc
)

target_include_directories(
  mini_redis_hash_table_benchmark
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
)

mini_redis_set_project_warnings(mini_redis_hash_table_benchmark)
