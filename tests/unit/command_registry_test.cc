#include "protocol/resp_value.h"
#include "server/command_registry.h"

#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using mini_redis::CommandRegistry;
using mini_redis::RespValue;

RespValue make_command(std::initializer_list<std::string_view> elements) {
  std::vector<RespValue> values;
  values.reserve(elements.size());
  for (const std::string_view element : elements) {
    values.push_back(RespValue::bulk_string(std::string(element)));
  }
  return RespValue::array(std::move(values));
}

bool is_string_response(const RespValue& response,
                        RespValue::Type expected_type,
                        std::string_view expected_value) {
  return response.type() == expected_type &&
         response.string_value() == expected_value;
}

bool test_ping() {
  const CommandRegistry commands;
  const RespValue plain = commands.execute(make_command({"PING"}));
  const RespValue with_message =
      commands.execute(make_command({"PING", "hello"}));
  return is_string_response(
             plain, RespValue::Type::kSimpleString, "PONG") &&
         is_string_response(
             with_message, RespValue::Type::kBulkString, "hello");
}

bool test_echo_and_case_insensitive_names() {
  const CommandRegistry commands;
  const RespValue echo =
      commands.execute(make_command({"eChO", "hello"}));
  const RespValue ping = commands.execute(make_command({"ping"}));
  return is_string_response(
             echo, RespValue::Type::kBulkString, "hello") &&
         is_string_response(
             ping, RespValue::Type::kSimpleString, "PONG");
}

bool test_argument_count_errors() {
  const CommandRegistry commands;
  const RespValue missing =
      commands.execute(make_command({"ECHO"}));
  const RespValue extra =
      commands.execute(make_command({"PING", "one", "two"}));
  return is_string_response(
             missing, RespValue::Type::kError,
             "ERR wrong number of arguments for 'ECHO' command") &&
         is_string_response(
             extra, RespValue::Type::kError,
             "ERR wrong number of arguments for 'PING' command");
}

bool test_invalid_requests() {
  const CommandRegistry commands;
  const RespValue not_array =
      commands.execute(RespValue::bulk_string("PING"));
  const RespValue empty_array =
      commands.execute(RespValue::array({}));
  const RespValue empty_name =
      commands.execute(make_command({""}));
  const RespValue non_bulk = commands.execute(
      RespValue::array(
          {RespValue::bulk_string("ECHO"), RespValue::integer(1)}));
  const RespValue unknown =
      commands.execute(make_command({"WHAT"}));

  return is_string_response(
             not_array, RespValue::Type::kError,
             "ERR command request must be an array") &&
         is_string_response(
             empty_array, RespValue::Type::kError,
             "ERR command array must not be empty") &&
         is_string_response(
             empty_name, RespValue::Type::kError,
             "ERR command name must not be empty") &&
         is_string_response(
             non_bulk, RespValue::Type::kError,
             "ERR command and arguments must be bulk strings") &&
         is_string_response(
             unknown, RespValue::Type::kError,
             "ERR unknown command 'WHAT'");
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
  if (!run_test("PING", test_ping) ||
      !run_test("ECHO and command case",
                test_echo_and_case_insensitive_names) ||
      !run_test("argument count", test_argument_count_errors) ||
      !run_test("invalid requests", test_invalid_requests)) {
    return 1;
  }
  return 0;
}
