cmake_minimum_required(VERSION 3.22)

project(
  mini_redis
  VERSION 0.1.0
  DESCRIPTION "A learning-oriented Redis-compatible in-memory key-value server"
  LANGUAGES CXX
)

option(MINI_REDIS_BUILD_TESTS "Build Mini-Redis tests" ON)
option(MINI_REDIS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

add_subdirectory(src)

if(MINI_REDIS_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
