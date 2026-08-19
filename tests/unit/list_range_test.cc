#include "server/list_range.h"

#include <cstdint>
#include <iostream>
#include <limits>

namespace {

bool normalizes_as(std::size_t length,
                   std::int64_t start,
                   std::int64_t stop,
                   std::size_t expected_begin,
                   std::size_t expected_end) {
  const auto range =
      mini_redis::normalize_list_range(length, start, stop);
  return range.has_value() &&
         range->begin == expected_begin &&
         range->end == expected_end;
}

bool is_empty(std::size_t length,
              std::int64_t start,
              std::int64_t stop) {
  return !mini_redis::normalize_list_range(length, start, stop)
              .has_value();
}

bool test_non_negative_indices() {
  return normalizes_as(5, 0, 2, 0, 3) &&
         normalizes_as(5, 2, 4, 2, 5) &&
         normalizes_as(5, 0, 4, 0, 5);
}

bool test_negative_indices() {
  return normalizes_as(5, -2, -1, 3, 5) &&
         normalizes_as(5, 0, -1, 0, 5) &&
         normalizes_as(5, -5, -5, 0, 1);
}

bool test_out_of_range_indices_are_clipped() {
  return normalizes_as(5, -100, 1, 0, 2) &&
         normalizes_as(5, 2, 100, 2, 5) &&
         normalizes_as(
             5,
             std::numeric_limits<std::int64_t>::min(),
             std::numeric_limits<std::int64_t>::max(),
             0,
             5);
}

bool test_empty_ranges() {
  return is_empty(0, 0, -1) &&
         is_empty(5, 5, 10) &&
         is_empty(5, 4, 2) &&
         is_empty(5, 0, -6) &&
         is_empty(5, -1, -2);
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
  if (!run_test("non-negative indices", test_non_negative_indices) ||
      !run_test("negative indices", test_negative_indices) ||
      !run_test("out-of-range clipping",
                test_out_of_range_indices_are_clipped) ||
      !run_test("empty ranges", test_empty_ranges)) {
    return 1;
  }

  return 0;
}
