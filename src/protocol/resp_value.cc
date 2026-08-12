#include "protocol/resp_value.h"

#include <utility>

namespace mini_redis {

RespValue RespValue::simple_string(std::string value) {
    return RespValue(Type::kSimpleString, std::move(value));
}

RespValue RespValue::error(std::string value) { return RespValue(Type::kError, std::move(value)); }

RespValue RespValue::integer(std::int64_t value) { return RespValue(Type::kInteger, value); }

RespValue RespValue::bulk_string(std::string value) {
    return RespValue(Type::kBulkString, std::move(value));
}

RespValue RespValue::null_bulk_string() {
    return RespValue(Type::kNullBulkString, std::monostate{});
}

RespValue RespValue::array(std::vector<RespValue> values) {
    return RespValue(Type::kArray, std::move(values));
}

RespValue::Type RespValue::type() const noexcept { return type_; }

const std::string& RespValue::string_value() const { return std::get<std::string>(value_); }

std::int64_t RespValue::integer_value() const { return std::get<std::int64_t>(value_); }

const std::vector<RespValue>& RespValue::array_value() const {
    return std::get<std::vector<RespValue>>(value_);
}

RespValue::RespValue(Type type, Value value) : type_(type), value_(std::move(value)) {}

}  // namespace mini_redis
