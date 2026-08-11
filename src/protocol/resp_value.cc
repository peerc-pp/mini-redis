#include "protocol/resp_value.h"

#include <utility>

namespace mini_redis {

RespValue RespValue::simple_string(std::string value) {
  // TODO:
  // Return a RespValue whose type is Type::kSimpleString.
  // The payload should be the given string.
  //
  // Hint:
    return RespValue(Type::kSimpleString, std::move(value));
}

RespValue RespValue::error(std::string value) {
  // TODO:
  // Return a RespValue whose type is Type::kError.
  // The payload should be the given error string.
  return RespValue(Type::kError, std::move(value));
}

RespValue RespValue::integer(std::int64_t value) {
  // TODO:
  // Return a RespValue whose type is Type::kInteger.
  // The payload should be the integer.
  return RespValue(Type::kInteger, std::move(value));
}

RespValue RespValue::bulk_string(std::string value) {
  // TODO:
  // Return a RespValue whose type is Type::kBulkString.
  // The payload should be the given string.
  //
  // Remember:
  //   empty string is still a valid bulk string.
  //   It is not the same as null bulk string.
  return RespValue(Type::kBulkString, std::move(value));
}

RespValue RespValue::null_bulk_string() {
  // TODO:
  // Return a RespValue whose type is Type::kNullBulkString.
  // The payload should be std::monostate{}.
  return RespValue(Type::kNullBulkString, std::monostate{});
}

RespValue RespValue::array(std::vector<RespValue> values) {
  // TODO:
  // Return a RespValue whose type is Type::kArray.
  // The payload should be the vector.
  return RespValue(Type::kArray, std::move(values));
}

RespValue::Type RespValue::type() const noexcept {
  // TODO:
  // Return the stored type_.
return type_;
}

const std::string& RespValue::string_value() const {
  // TODO:
  // Return the std::string stored inside value_.
  //
  // Hint:
    return std::get<std::string>(value_);
}

std::int64_t RespValue::integer_value() const {
  // TODO:
  // Return the std::int64_t stored inside value_.
  return std::get<std::int64_t>(value_);
}

const std::vector<RespValue>& RespValue::array_value() const {
  // TODO:
  // Return the std::vector<RespValue> stored inside value_.
  return std::get<std::vector<RespValue>>(value_);
}

RespValue::RespValue(Type type, Value value)
    : type_(type), value_(std::move(value)) {
  // TODO:
  // Store both arguments into the two member variables.
  //
  // Hint:
  
}

}  // namespace mini_redis