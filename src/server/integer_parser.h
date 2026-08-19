#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace mini_redis {

[[nodiscard]] std::optional<std::int64_t> parse_integer(
    std::string_view text) noexcept;

}  // namespace mini_redis
