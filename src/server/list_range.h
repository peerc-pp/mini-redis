#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace mini_redis {

struct ListRange {
  std::size_t begin;
  std::size_t end;
};

[[nodiscard]] std::optional<ListRange> normalize_list_range(
    std::size_t length, std::int64_t start, std::int64_t stop) noexcept;

}  // namespace mini_redis
