#include "protocol/resp_encoder.h"
#include "protocol/resp_value.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using mini_redis::RespEncoder;
using mini_redis::RespValue;

bool test_string_values() {
  const RespValue simple = RespValue::simple_string("OK");
  if (simple.type() != RespValue::Type::kSimpleString ||
      simple.string_value() != "OK") {
    return false;
  }

  const RespValue error = RespValue::error("ERR bad");
  if (error.type() != RespValue::Type::kError ||
      error.string_value() != "ERR bad") {
    return false;
  }

  const RespValue bulk = RespValue::bulk_string("foo");
  if (bulk.type() != RespValue::Type::kBulkString ||
      bulk.string_value() != "foo") {
    return false;
  }

  const RespValue empty_bulk = RespValue::bulk_string("");
  return empty_bulk.type() == RespValue::Type::kBulkString &&
         empty_bulk.string_value().empty();
}

bool test_integer_value() {
  const RespValue value = RespValue::integer(123);
  const RespValue negative = RespValue::integer(-42);

  return value.type() == RespValue::Type::kInteger &&
         value.integer_value() == 123 &&
         negative.type() == RespValue::Type::kInteger &&
         negative.integer_value() == -42;
}

bool test_null_bulk_string() {
  const RespValue value = RespValue::null_bulk_string();

  return value.type() == RespValue::Type::kNullBulkString;
}

bool test_array_value() {
  std::vector<RespValue> elements;
  elements.push_back(RespValue::bulk_string("GET"));
  elements.push_back(RespValue::bulk_string("foo"));

  const RespValue value = RespValue::array(std::move(elements));
  if (value.type() != RespValue::Type::kArray ||
      value.array_value().size() != 2) {
    return false;
  }

  const RespValue& command = value.array_value()[0];
  const RespValue& key = value.array_value()[1];
  return command.type() == RespValue::Type::kBulkString &&
         command.string_value() == "GET" &&
         key.type() == RespValue::Type::kBulkString &&
         key.string_value() == "foo";
}

bool test_resp_encoder_scalars() {
  return RespEncoder::encode(RespValue::simple_string("OK")) ==
             "+OK\r\n" &&
         RespEncoder::encode(RespValue::error("ERR bad")) ==
             "-ERR bad\r\n" &&
         RespEncoder::encode(RespValue::integer(123)) == ":123\r\n" &&
         RespEncoder::encode(RespValue::integer(-42)) == ":-42\r\n" &&
         RespEncoder::encode(RespValue::bulk_string("foo")) ==
             "$3\r\nfoo\r\n" &&
         RespEncoder::encode(RespValue::bulk_string("")) ==
             "$0\r\n\r\n" &&
         RespEncoder::encode(RespValue::null_bulk_string()) == "$-1\r\n";
}

bool test_resp_encoder_array() {
  std::vector<RespValue> elements;
  elements.push_back(RespValue::bulk_string("GET"));
  elements.push_back(RespValue::bulk_string("foo"));

  if (RespEncoder::encode(RespValue::array(std::move(elements))) !=
      "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n") {
    return false;
  }

  std::vector<RespValue> nested;
  nested.push_back(RespValue::array(
      std::vector<RespValue>{RespValue::integer(1)}));
  nested.push_back(RespValue::null_bulk_string());

  return RespEncoder::encode(RespValue::array(std::move(nested))) ==
         "*2\r\n*1\r\n:1\r\n$-1\r\n";
}

bool run_test(const char* name, bool (*test)()) {
  if (test()) {
    return true;
  }

  std::cerr << name << " failed\n";
  return false;
}

}  // namespace

int main() {
  if (!run_test("string values", test_string_values) ||
      !run_test("integer value", test_integer_value) ||
      !run_test("null bulk string", test_null_bulk_string) ||
      !run_test("array value", test_array_value) ||
      !run_test("resp encoder scalars", test_resp_encoder_scalars) ||
      !run_test("resp encoder array", test_resp_encoder_array)) {
    return 1;
  }

  return 0;
}
