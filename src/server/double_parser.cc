#include "server/double_parser.h"

#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace mini_redis {

std::optional<double> parse_double(
    std::string_view text) noexcept {
  if (text.empty()) {
    return std::nullopt;
  }
  if (text == "+inf" || text == "inf") {
    return std::numeric_limits<double>::infinity();
  }
  if (text == "-inf") {
    return -std::numeric_limits<double>::infinity();
  }

  double value = 0.0;
  const auto result = std::from_chars(
      text.data(), text.data() + text.size(), value,
      std::chars_format::general);
  if (result.ec != std::errc{} ||
      result.ptr != text.data() + text.size() ||
      std::isnan(value)) {
    return std::nullopt;
  }
  return value;
}

std::string format_double(double value) {
  std::array<char, 128> buffer{};
  const auto result = std::to_chars(
      buffer.data(), buffer.data() + buffer.size(), value,
      std::chars_format::general);
  if (result.ec != std::errc{}) {
    throw std::runtime_error("failed to format double");
  }
  return std::string(buffer.data(), result.ptr);
}

}  // namespace mini_redis
