#include "server/integer_arithmetic.h"

#include <cstdint>
#include <iostream>
#include <limits>

namespace {

bool adds_as(std::int64_t value,
             std::int64_t delta,
             std::int64_t expected) {
  const auto result = mini_redis::checked_add(value, delta);
  return result.has_value() && *result == expected;
}

bool overflows(std::int64_t value, std::int64_t delta) {
  return !mini_redis::checked_add(value, delta).has_value();
}

bool test_normal_addition() {
  return adds_as(10, 1, 11) &&
         adds_as(10, -1, 9) &&
         adds_as(-10, 5, -5) &&
         adds_as(42, 0, 42);
}

bool test_boundary_results() {
  constexpr std::int64_t kMinimum =
      std::numeric_limits<std::int64_t>::min();
  constexpr std::int64_t kMaximum =
      std::numeric_limits<std::int64_t>::max();

  return adds_as(kMaximum - 1, 1, kMaximum) &&
         adds_as(kMinimum + 1, -1, kMinimum) &&
         adds_as(0, kMinimum, kMinimum) &&
         adds_as(kMaximum, kMinimum, -1);
}

bool test_overflow() {
  constexpr std::int64_t kMinimum =
      std::numeric_limits<std::int64_t>::min();
  constexpr std::int64_t kMaximum =
      std::numeric_limits<std::int64_t>::max();

  return overflows(kMaximum, 1) &&
         overflows(kMinimum, -1) &&
         overflows(-1, kMinimum) &&
         overflows(kMaximum, kMaximum);
}

bool run_test(const char* name, bool (*test)()) {
  if (test()) {
    return true;
  }

  std::cerr << name << " failed\n";
  return false;
}

}  // namespace

int main() {
  if (!run_test("normal addition", test_normal_addition) ||
      !run_test("boundary results", test_boundary_results) ||
      !run_test("overflow", test_overflow)) {
    return 1;
  }

  return 0;
}
