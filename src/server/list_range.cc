#include "server/list_range.h"

#include <algorithm>
#include <cstdint>

namespace mini_redis {
namespace {

std::uint64_t negative_magnitude(std::int64_t value) noexcept {
  return static_cast<std::uint64_t>(-(value + 1)) + 1;
}

}  // namespace

std::optional<ListRange> normalize_list_range(
    std::size_t length,
    std::int64_t start,
    std::int64_t stop) noexcept {
  if (length == 0) {
    return std::nullopt;
  }

  const auto unsigned_length = static_cast<std::uint64_t>(length);

  std::uint64_t begin = 0;
  if (start >= 0) {
    begin = static_cast<std::uint64_t>(start);
    if (begin >= unsigned_length) {
      return std::nullopt;
    }
  } else {
    const auto distance = negative_magnitude(start);
    begin = distance > unsigned_length ? 0 : unsigned_length - distance;
  }

  std::uint64_t last = 0;
  if (stop >= 0) {
    last = std::min(
        static_cast<std::uint64_t>(stop), unsigned_length - 1);
  } else {
    const auto distance = negative_magnitude(stop);
    if (distance > unsigned_length) {
      return std::nullopt;
    }
    last = unsigned_length - distance;
  }

  if (begin > last) {
    return std::nullopt;
  }

  return ListRange{
      static_cast<std::size_t>(begin),
      static_cast<std::size_t>(last + 1),
  };
}

}  // namespace mini_redis
