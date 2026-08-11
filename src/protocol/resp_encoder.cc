#include "protocol/resp_encoder.h"

#include <string>

namespace mini_redis {
  namespace {

  constexpr const char* kCrlf = "\r\n";

  }  // namespace

std::string RespEncoder::encode(const RespValue& value) {
  std::string output;
  encode_into(value, output);
  return output;
}

void RespEncoder::encode_into(const RespValue& value,
                              std::string& output) {
  switch (value.type()) {
    case RespValue::Type::kSimpleString:
      output += "+";
      output += value.string_value();
      output += kCrlf;
      break;

    case RespValue::Type::kError:
      output += "-";
      output += value.string_value();
      output += kCrlf;
      break;

    case RespValue::Type::kInteger:
      output += ":";
      output += std::to_string(value.integer_value());
      output += kCrlf;
      break;

    case RespValue::Type::kBulkString:
      output += "$";
      output += std::to_string(value.string_value().size());
      output += kCrlf;
      output += value.string_value();
      output += kCrlf;
      break;

    case RespValue::Type::kNullBulkString:
      output += "$-1";
      output += kCrlf;
      break;

    case RespValue::Type::kArray:
      output += "*";
      output += std::to_string(value.array_value().size());
      output += kCrlf;
      for (const RespValue& element : value.array_value()) {
        encode_into(element, output);
      }
      break;
  }
}

}  // namespace mini_redis
