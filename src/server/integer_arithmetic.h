#pragma once

#include <cstdint>
#include <optional>

namespace mini_redis {

[[nodiscard]] std::optional<std::int64_t> checked_add(
    std::int64_t value, std::int64_t delta) noexcept;

}  // namespace mini_redis
