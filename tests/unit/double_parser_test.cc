#include "server/double_parser.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

bool parses_as(std::string_view text, double expected) {
  const auto parsed = mini_redis::parse_double(text);
  return parsed.has_value() && *parsed == expected;
}

bool test_valid_values() {
  return parses_as("0", 0.0) &&
         parses_as("-12.5", -12.5) &&
         parses_as("1e3", 1000.0) &&
         parses_as("+inf",
                   std::numeric_limits<double>::infinity()) &&
         parses_as("-inf",
                   -std::numeric_limits<double>::infinity());
}

bool test_invalid_values() {
  return !mini_redis::parse_double("").has_value() &&
         !mini_redis::parse_double("12x").has_value() &&
         !mini_redis::parse_double(" 12").has_value() &&
         !mini_redis::parse_double("nan").has_value() &&
         !mini_redis::parse_double("1e9999").has_value();
}

bool test_formatting() {
  return mini_redis::format_double(100.0) == "100" &&
         mini_redis::format_double(-12.5) == "-12.5" &&
         mini_redis::format_double(
             std::numeric_limits<double>::infinity()) == "inf";
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
  return run_test("valid values", test_valid_values) &&
                 run_test("invalid values", test_invalid_values) &&
                 run_test("formatting", test_formatting)
             ? 0
             : 1;
}
