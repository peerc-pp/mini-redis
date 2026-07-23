#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/temp/network-tests"
CXX="${CXX:-g++}"

NORMAL_UNIT="${BUILD_DIR}/network-unit-tests"
NORMAL_PHASE_ONE="${BUILD_DIR}/network-phase-one-tests"
SANITIZED_UNIT="${BUILD_DIR}/network-unit-tests-sanitized"
SANITIZED_PHASE_ONE="${BUILD_DIR}/network-phase-one-tests-sanitized"

cleanup() {
  rm -f \
    "${NORMAL_UNIT}" \
    "${NORMAL_PHASE_ONE}" \
    "${SANITIZED_UNIT}" \
    "${SANITIZED_PHASE_ONE}"
  rmdir "${BUILD_DIR}" 2>/dev/null || true
}
trap cleanup EXIT

mkdir -p "${BUILD_DIR}"

COMMON_FLAGS=(
  -std=c++17
  -Wall
  -Wextra
  -Wpedantic
  -Werror
  -I"${ROOT_DIR}/src"
)

SOURCES=(
  "${ROOT_DIR}/src/base/unique_fd.cc"
  "${ROOT_DIR}/src/net/socket.cc"
  "${ROOT_DIR}/src/net/poller.cc"
  "${ROOT_DIR}/src/net/channel.cc"
  "${ROOT_DIR}/src/net/event_loop.cc"
  "${ROOT_DIR}/src/net/buffer.cc"
  "${ROOT_DIR}/src/net/acceptor.cc"
  "${ROOT_DIR}/src/net/tcp_connection.cc"
  "${ROOT_DIR}/src/net/tcp_server.cc"
)

compile_and_run() {
  local output="$1"
  local test_source="$2"
  shift 2

  "${CXX}" "${COMMON_FLAGS[@]}" "$@" \
    "${SOURCES[@]}" "${test_source}" -o "${output}"
  timeout 30s "${output}"
}

compile_and_run \
  "${NORMAL_UNIT}" \
  "${ROOT_DIR}/tests/unit/network_test.cc"
compile_and_run \
  "${NORMAL_PHASE_ONE}" \
  "${ROOT_DIR}/tests/integration/network_phase_one_test.cc"

SANITIZER_FLAGS=(
  -fsanitize=address,undefined
  -fno-omit-frame-pointer
)

compile_and_run \
  "${SANITIZED_UNIT}" \
  "${ROOT_DIR}/tests/unit/network_test.cc" \
  "${SANITIZER_FLAGS[@]}"
compile_and_run \
  "${SANITIZED_PHASE_ONE}" \
  "${ROOT_DIR}/tests/integration/network_phase_one_test.cc" \
  "${SANITIZER_FLAGS[@]}"

printf '%s\n' "All network tests passed."
