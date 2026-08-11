#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace mini_redis {

// RespValue is the in-memory representation of one RESP value.
//
// Examples:
//   +OK\r\n        -> Type::kSimpleString, string value "OK"
//   -ERR bad\r\n   -> Type::kError, string value "ERR bad"
//   :1\r\n         -> Type::kInteger, integer value 1
//   $3\r\nfoo\r\n  -> Type::kBulkString, string value "foo"
//   $-1\r\n        -> Type::kNullBulkString, no payload
//   *2\r\n...\r\n  -> Type::kArray, vector of RespValue
class RespValue {
 public:
  enum class Type {
    kSimpleString,
    kError,
    kInteger,
    kBulkString,
    kNullBulkString,
    kArray,
  };

  // Factory functions.
  //
  // These make call sites clearer than exposing constructors directly.
  // For example:
  //   RespValue::simple_string("OK")
  // is more readable than:
  //   RespValue(Type::kSimpleString, "OK")
  static RespValue simple_string(std::string value);
  static RespValue error(std::string value);
  static RespValue integer(std::int64_t value);
  static RespValue bulk_string(std::string value);
  static RespValue null_bulk_string();
  static RespValue array(std::vector<RespValue> values);

  // Returns which RESP type this value currently represents.
  Type type() const noexcept;

  // Accessors.
  //
  // string_value() should be used for:
  //   kSimpleString
  //   kError
  //   kBulkString
  //
  // integer_value() should be used for:
  //   kInteger
  //
  // array_value() should be used for:
  //   kArray
  //
  // For now, it is acceptable to let std::get throw if the caller uses the
  // wrong accessor. Later, if you want more explicit errors, you can add
  // type checks here.
  const std::string& string_value() const;
  std::int64_t integer_value() const;
  const std::vector<RespValue>& array_value() const;

 private:
  // The actual payload storage.
  //
  // std::monostate is used for kNullBulkString because null bulk string has
  // no payload. It is different from an empty bulk string:
  //
  //   $0\r\n\r\n   -> empty string
  //   $-1\r\n      -> null
  using Value =
      std::variant<std::monostate, std::string, std::int64_t,
                   std::vector<RespValue>>;

  // Private constructor forces users to create values through factory
  // functions above. This keeps type/payload pairs consistent.
  RespValue(Type type, Value value);

  Type type_;
  Value value_;
};

}  // namespace mini_redis