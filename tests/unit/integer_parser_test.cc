#include "server/integer_parser.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

bool parses_as(std::string_view text, std::int64_t expected) {
  const auto value = mini_redis::parse_integer(text);
  return value.has_value() && *value == expected;
}

bool rejects(std::string_view text) {
  return !mini_redis::parse_integer(text).has_value();
}

bool test_valid_integers() {
  return parses_as("0", 0) &&
         parses_as("1", 1) &&
         parses_as("-1", -1) &&
         parses_as("9223372036854775807",
                   std::numeric_limits<std::int64_t>::max()) &&
         parses_as("-9223372036854775808",
                   std::numeric_limits<std::int64_t>::min());
}

bool test_invalid_formats() {
  return rejects("") &&
         rejects("+1") &&
         rejects("01") &&
         rejects("-0") &&
         rejects(" 1") &&
         rejects("1 ") &&
         rejects("1.0") &&
         rejects("12x") &&
         rejects(std::string_view("1\0", 2));
}

bool test_out_of_range() {
  return rejects("9223372036854775808") &&
         rejects("-9223372036854775809");
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
  if (!run_test("valid integers", test_valid_integers) ||
      !run_test("invalid formats", test_invalid_formats) ||
      !run_test("out of range", test_out_of_range)) {
    return 1;
  }

  return 0;
}
