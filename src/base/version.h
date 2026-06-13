#pragma once

#include <string_view>

namespace mini_redis {

[[nodiscard]] std::string_view version() noexcept;

}  // namespace mini_redis
