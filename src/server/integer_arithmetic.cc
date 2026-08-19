#include "server/integer_arithmetic.h"

#include <limits>

namespace mini_redis {

std::optional<std::int64_t> checked_add(
    std::int64_t value, std::int64_t delta) noexcept {
  constexpr std::int64_t kMinimum =
      std::numeric_limits<std::int64_t>::min();
  constexpr std::int64_t kMaximum =
      std::numeric_limits<std::int64_t>::max();

  if (delta > 0 && value > kMaximum - delta) {
    return std::nullopt;
  }

  if (delta < 0 && value < kMinimum - delta) {
    return std::nullopt;
  }

  return value + delta;
}

}  // namespace mini_redis
