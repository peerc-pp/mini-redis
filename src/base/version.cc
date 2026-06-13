#include "base/version.h"

#ifndef MINI_REDIS_VERSION
#define MINI_REDIS_VERSION "0.0.0-unknown"
#endif

namespace mini_redis {

std::string_view version() noexcept { return MINI_REDIS_VERSION; }

}  // namespace mini_redis
