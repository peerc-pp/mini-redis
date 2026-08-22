#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace mini_redis {

[[nodiscard]] std::optional<double> parse_double(
    std::string_view text) noexcept;

[[nodiscard]] std::string format_double(double value);

}  // namespace mini_redis
