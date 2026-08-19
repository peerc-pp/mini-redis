#include "server/integer_parser.h"

#include <charconv>
#include <system_error>

namespace mini_redis {
namespace {

bool has_canonical_integer_format(std::string_view text) noexcept {
  if (text.empty()) {
    return false;
  }

  std::size_t digit_begin = 0;
  if (text.front() == '-') {
    digit_begin = 1;
  }

  if (digit_begin == text.size()) {
    return false;
  }

  if (text[digit_begin] == '0') {
    return digit_begin == 0 && text.size() == 1;
  }

  if (text[digit_begin] < '1' || text[digit_begin] > '9') {
    return false;
  }

  for (std::size_t index = digit_begin + 1;
       index < text.size(); ++index) {
    if (text[index] < '0' || text[index] > '9') {
      return false;
    }
  }

  return true;
}

}  // namespace

std::optional<std::int64_t> parse_integer(
    std::string_view text) noexcept {
  if (!has_canonical_integer_format(text)) {
    return std::nullopt;
  }

  std::int64_t value = 0;
  const char* const begin = text.data();
  const char* const end = begin + text.size();
  const auto result = std::from_chars(begin, end, value, 10);

  if (result.ec != std::errc{} || result.ptr != end) {
    return std::nullopt;
  }

  return value;
}

}  // namespace mini_redis
