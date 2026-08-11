#pragma once

#include <string>

#include "protocol/resp_value.h"

namespace mini_redis {

class RespEncoder {
 public:
  static std::string encode(const RespValue& value);

 private:
  static void encode_into(const RespValue& value, std::string& output);
};

}  // namespace mini_redis
