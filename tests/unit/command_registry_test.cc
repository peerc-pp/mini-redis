#include "protocol/resp_value.h"
#include "server/command_registry.h"
#include "storage/database.h"

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using mini_redis::CommandRegistry;
using mini_redis::Database;
using mini_redis::RespValue;

struct CommandFixture {
  Database database;
  CommandRegistry commands{database};
};

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

bool is_integer_response(const RespValue& response,
                         std::int64_t expected_value) {
  return response.type() == RespValue::Type::kInteger &&
         response.integer_value() == expected_value;
}

bool is_bulk_string_array(
    const RespValue& response,
    std::initializer_list<std::string_view> expected_values) {
  if (response.type() != RespValue::Type::kArray ||
      response.array_value().size() != expected_values.size()) {
    return false;
  }

  const auto& actual_values = response.array_value();
  std::size_t index = 0;
  for (const std::string_view expected : expected_values) {
    if (!is_string_response(
            actual_values[index], RespValue::Type::kBulkString,
            expected)) {
      return false;
    }
    ++index;
  }

  return true;
}

bool is_hash_array(const RespValue& response,
                   const mini_redis::Value::Hash& expected) {
  if (response.type() != RespValue::Type::kArray) {
    return false;
  }

  const auto& elements = response.array_value();
  if (elements.size() != expected.size() * 2) {
    return false;
  }

  for (std::size_t index = 0; index < elements.size(); index += 2) {
    if (elements[index].type() != RespValue::Type::kBulkString ||
        elements[index + 1].type() != RespValue::Type::kBulkString) {
      return false;
    }

    const auto field = expected.find(elements[index].string_value());
    if (field == expected.end() ||
        field->second != elements[index + 1].string_value()) {
      return false;
    }
  }

  return true;
}

bool test_ping() {
  CommandFixture fixture;
  const RespValue plain =
      fixture.commands.execute(make_command({"PING"}));
  const RespValue with_message =
      fixture.commands.execute(make_command({"PING", "hello"}));
  return is_string_response(
             plain, RespValue::Type::kSimpleString, "PONG") &&
         is_string_response(
             with_message, RespValue::Type::kBulkString, "hello");
}

bool test_echo_and_case_insensitive_names() {
  CommandFixture fixture;
  const RespValue echo =
      fixture.commands.execute(make_command({"eChO", "hello"}));
  const RespValue ping =
      fixture.commands.execute(make_command({"ping"}));
  return is_string_response(
             echo, RespValue::Type::kBulkString, "hello") &&
         is_string_response(
             ping, RespValue::Type::kSimpleString, "PONG");
}

bool test_argument_count_errors() {
  CommandFixture fixture;
  const RespValue echo_missing =
      fixture.commands.execute(make_command({"ECHO"}));
  const RespValue ping_extra =
      fixture.commands.execute(make_command({"PING", "one", "two"}));
  const RespValue set_missing =
      fixture.commands.execute(make_command({"SET", "key"}));
  const RespValue get_extra =
      fixture.commands.execute(make_command({"GET", "key", "extra"}));
  const RespValue del_missing =
      fixture.commands.execute(make_command({"DEL"}));
  const RespValue exists_missing =
      fixture.commands.execute(make_command({"EXISTS"}));

  return is_string_response(
             echo_missing, RespValue::Type::kError,
             "ERR wrong number of arguments for 'ECHO' command") &&
         is_string_response(
             ping_extra, RespValue::Type::kError,
             "ERR wrong number of arguments for 'PING' command") &&
         is_string_response(
             set_missing, RespValue::Type::kError,
             "ERR wrong number of arguments for 'SET' command") &&
         is_string_response(
             get_extra, RespValue::Type::kError,
             "ERR wrong number of arguments for 'GET' command") &&
         is_string_response(
             del_missing, RespValue::Type::kError,
             "ERR wrong number of arguments for 'DEL' command") &&
         is_string_response(
             exists_missing, RespValue::Type::kError,
             "ERR wrong number of arguments for 'EXISTS' command");
}

bool test_invalid_requests() {
  CommandFixture fixture;
  const RespValue not_array =
      fixture.commands.execute(RespValue::bulk_string("PING"));
  const RespValue empty_array =
      fixture.commands.execute(RespValue::array({}));
  const RespValue empty_name =
      fixture.commands.execute(make_command({""}));
  const RespValue non_bulk = fixture.commands.execute(
      RespValue::array(
          {RespValue::bulk_string("ECHO"), RespValue::integer(1)}));
  const RespValue unknown =
      fixture.commands.execute(make_command({"WHAT"}));

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

bool test_set_and_get() {
  CommandFixture fixture;
  const RespValue set =
      fixture.commands.execute(
          make_command({"SET", "name", "alice"}));
  const RespValue get =
      fixture.commands.execute(make_command({"GET", "name"}));
  const RespValue missing =
      fixture.commands.execute(make_command({"GET", "missing"}));

  return is_string_response(
             set, RespValue::Type::kSimpleString, "OK") &&
         is_string_response(
             get, RespValue::Type::kBulkString, "alice") &&
         missing.type() == RespValue::Type::kNullBulkString;
}

bool test_set_overwrites_and_uses_injected_database() {
  CommandFixture fixture;
  fixture.database.set("direct", mini_redis::Value::string("first"));

  const RespValue before =
      fixture.commands.execute(make_command({"GET", "direct"}));
  const RespValue set =
      fixture.commands.execute(
          make_command({"set", "direct", "second"}));
  const mini_redis::Value* stored = fixture.database.find("direct");

  return is_string_response(
             before, RespValue::Type::kBulkString, "first") &&
         is_string_response(
             set, RespValue::Type::kSimpleString, "OK") &&
         stored != nullptr && stored->as_string() != nullptr &&
         *stored->as_string() == "second";
}

bool test_del_and_exists() {
  CommandFixture fixture;

  const RespValue missing_exists =
      fixture.commands.execute(make_command({"EXISTS", "name"}));
  const RespValue missing_del =
      fixture.commands.execute(make_command({"DEL", "name"}));
  fixture.database.set("name", mini_redis::Value::string("alice"));
  const RespValue present_exists =
      fixture.commands.execute(make_command({"exists", "name"}));
  const RespValue deleted =
      fixture.commands.execute(make_command({"del", "name"}));
  const RespValue exists_after =
      fixture.commands.execute(make_command({"EXISTS", "name"}));
  const RespValue deleted_again =
      fixture.commands.execute(make_command({"DEL", "name"}));

  return is_integer_response(missing_exists, 0) &&
         is_integer_response(missing_del, 0) &&
         is_integer_response(present_exists, 1) &&
         is_integer_response(deleted, 1) &&
         is_integer_response(exists_after, 0) &&
         is_integer_response(deleted_again, 0) &&
         fixture.database.find("name") == nullptr;
}

bool test_multi_key_del_and_exists() {
  CommandFixture fixture;
  fixture.database.set("first", mini_redis::Value::string("one"));
  fixture.database.set("second", mini_redis::Value::string("two"));

  const RespValue exists = fixture.commands.execute(
      make_command({"EXISTS", "first", "second", "missing"}));
  const RespValue deleted = fixture.commands.execute(
      make_command({"DEL", "first", "second", "missing"}));
  const RespValue exists_after = fixture.commands.execute(
      make_command({"EXISTS", "first", "second"}));

  return is_integer_response(exists, 2) &&
         is_integer_response(deleted, 2) &&
         is_integer_response(exists_after, 0);
}

bool test_duplicate_keys() {
  CommandFixture fixture;
  fixture.database.set("name", mini_redis::Value::string("alice"));

  const RespValue exists = fixture.commands.execute(
      make_command({"EXISTS", "name", "name"}));
  const RespValue deleted = fixture.commands.execute(
      make_command({"DEL", "name", "name"}));

  return is_integer_response(exists, 2) &&
         is_integer_response(deleted, 1) &&
         !fixture.database.exists("name");
}

bool test_get_wrong_type() {
  CommandFixture fixture;
  fixture.database.set(
      "items", mini_redis::Value::list({"first"}));

  const RespValue response =
      fixture.commands.execute(make_command({"GET", "items"}));
  return is_string_response(
      response, RespValue::Type::kError,
      "WRONGTYPE Operation against a key holding the wrong kind of value");
}

bool test_incr_and_decr() {
  CommandFixture fixture;

  const RespValue first =
      fixture.commands.execute(make_command({"INCR", "counter"}));
  const RespValue second =
      fixture.commands.execute(make_command({"incr", "counter"}));
  const RespValue decreased =
      fixture.commands.execute(make_command({"DECR", "counter"}));
  const RespValue missing_decreased =
      fixture.commands.execute(make_command({"DECR", "down"}));
  const RespValue stored =
      fixture.commands.execute(make_command({"GET", "counter"}));

  return is_integer_response(first, 1) &&
         is_integer_response(second, 2) &&
         is_integer_response(decreased, 1) &&
         is_integer_response(missing_decreased, -1) &&
         is_string_response(
             stored, RespValue::Type::kBulkString, "1");
}

bool test_integer_change_errors_do_not_mutate() {
  CommandFixture fixture;
  fixture.database.set(
      "invalid", mini_redis::Value::string("12x"));
  fixture.database.set(
      "maximum",
      mini_redis::Value::string("9223372036854775807"));
  fixture.database.set(
      "minimum",
      mini_redis::Value::string("-9223372036854775808"));
  fixture.database.set(
      "items", mini_redis::Value::list({"first"}));

  const RespValue invalid =
      fixture.commands.execute(make_command({"INCR", "invalid"}));
  const RespValue overflow =
      fixture.commands.execute(make_command({"INCR", "maximum"}));
  const RespValue underflow =
      fixture.commands.execute(make_command({"DECR", "minimum"}));
  const RespValue wrong_type =
      fixture.commands.execute(make_command({"INCR", "items"}));

  const mini_redis::Value* invalid_value =
      fixture.database.find("invalid");
  const mini_redis::Value* maximum_value =
      fixture.database.find("maximum");
  const mini_redis::Value* minimum_value =
      fixture.database.find("minimum");
  const mini_redis::Value* items_value =
      fixture.database.find("items");

  constexpr std::string_view kIntegerError =
      "ERR value is not an integer or out of range";
  return is_string_response(
             invalid, RespValue::Type::kError, kIntegerError) &&
         is_string_response(
             overflow, RespValue::Type::kError, kIntegerError) &&
         is_string_response(
             underflow, RespValue::Type::kError, kIntegerError) &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         invalid_value != nullptr &&
         invalid_value->as_string() != nullptr &&
         *invalid_value->as_string() == "12x" &&
         maximum_value != nullptr &&
         maximum_value->as_string() != nullptr &&
         *maximum_value->as_string() == "9223372036854775807" &&
         minimum_value != nullptr &&
         minimum_value->as_string() != nullptr &&
         *minimum_value->as_string() == "-9223372036854775808" &&
         items_value != nullptr &&
         items_value->as_list() != nullptr &&
         items_value->as_list()->front() == "first";
}

bool test_lpush_and_rpush() {
  CommandFixture fixture;

  const RespValue left = fixture.commands.execute(
      make_command({"LPUSH", "items", "A", "B", "C"}));
  const RespValue right = fixture.commands.execute(
      make_command({"rpush", "items", "D", "E"}));

  const mini_redis::Value* value = fixture.database.find("items");
  const mini_redis::Value::List expected{
      "C", "B", "A", "D", "E"};

  return is_integer_response(left, 3) &&
         is_integer_response(right, 5) &&
         value != nullptr && value->as_list() != nullptr &&
         *value->as_list() == expected;
}

bool test_list_push_wrong_type_does_not_mutate() {
  CommandFixture fixture;
  fixture.database.set(
      "name", mini_redis::Value::string("alice"));

  const RespValue response = fixture.commands.execute(
      make_command({"LPUSH", "name", "bob"}));
  const mini_redis::Value* value = fixture.database.find("name");

  return is_string_response(
             response, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         value != nullptr && value->as_string() != nullptr &&
         *value->as_string() == "alice";
}

bool test_lpop_rpop_and_empty_key_deletion() {
  CommandFixture fixture;
  const RespValue pushed = fixture.commands.execute(
      make_command({"RPUSH", "items", "A", "B", "C"}));

  const RespValue left =
      fixture.commands.execute(make_command({"LPOP", "items"}));
  const RespValue right =
      fixture.commands.execute(make_command({"RPOP", "items"}));
  const RespValue last =
      fixture.commands.execute(make_command({"LPOP", "items"}));
  const RespValue missing =
      fixture.commands.execute(make_command({"RPOP", "items"}));

  return is_integer_response(pushed, 3) &&
         is_string_response(
             left, RespValue::Type::kBulkString, "A") &&
         is_string_response(
             right, RespValue::Type::kBulkString, "C") &&
         is_string_response(
             last, RespValue::Type::kBulkString, "B") &&
         missing.type() == RespValue::Type::kNullBulkString &&
         !fixture.database.exists("items");
}

bool test_list_pop_boundaries_do_not_mutate_wrong_type() {
  CommandFixture fixture;
  fixture.database.set(
      "name", mini_redis::Value::string("alice"));
  fixture.database.set("empty", mini_redis::Value::list());

  const RespValue wrong_type =
      fixture.commands.execute(make_command({"LPOP", "name"}));
  const RespValue empty =
      fixture.commands.execute(make_command({"LPOP", "empty"}));
  const mini_redis::Value* name = fixture.database.find("name");

  return is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         empty.type() == RespValue::Type::kNullBulkString &&
         name != nullptr && name->as_string() != nullptr &&
         *name->as_string() == "alice" &&
         !fixture.database.exists("empty");
}

bool test_llen() {
  CommandFixture fixture;
  fixture.database.set(
      "items", mini_redis::Value::list({"A", "B"}));
  fixture.database.set(
      "name", mini_redis::Value::string("alice"));

  const RespValue length =
      fixture.commands.execute(make_command({"LLEN", "items"}));
  const RespValue missing =
      fixture.commands.execute(make_command({"LLEN", "missing"}));
  const RespValue wrong_type =
      fixture.commands.execute(make_command({"LLEN", "name"}));
  const mini_redis::Value* items = fixture.database.find("items");

  return is_integer_response(length, 2) &&
         is_integer_response(missing, 0) &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         items != nullptr && items->as_list() != nullptr &&
         *items->as_list() == mini_redis::Value::List({"A", "B"});
}

bool test_lrange_indices_and_clipping() {
  CommandFixture fixture;
  fixture.database.set(
      "items",
      mini_redis::Value::list({"A", "B", "C", "D", "E"}));

  const RespValue positive = fixture.commands.execute(
      make_command({"LRANGE", "items", "0", "2"}));
  const RespValue negative = fixture.commands.execute(
      make_command({"lrange", "items", "-2", "-1"}));
  const RespValue clipped_left = fixture.commands.execute(
      make_command({"LRANGE", "items", "-100", "1"}));
  const RespValue clipped_right = fixture.commands.execute(
      make_command({"LRANGE", "items", "2", "100"}));
  const RespValue reversed = fixture.commands.execute(
      make_command({"LRANGE", "items", "4", "2"}));

  const mini_redis::Value* items = fixture.database.find("items");
  return is_bulk_string_array(positive, {"A", "B", "C"}) &&
         is_bulk_string_array(negative, {"D", "E"}) &&
         is_bulk_string_array(clipped_left, {"A", "B"}) &&
         is_bulk_string_array(clipped_right, {"C", "D", "E"}) &&
         is_bulk_string_array(reversed, {}) &&
         items != nullptr && items->as_list() != nullptr &&
         *items->as_list() ==
             mini_redis::Value::List({"A", "B", "C", "D", "E"});
}

bool test_lrange_boundaries_and_errors() {
  CommandFixture fixture;
  fixture.database.set(
      "items", mini_redis::Value::list({"A", "B"}));
  fixture.database.set(
      "name", mini_redis::Value::string("alice"));

  const RespValue missing = fixture.commands.execute(
      make_command({"LRANGE", "missing", "0", "-1"}));
  const RespValue wrong_type = fixture.commands.execute(
      make_command({"LRANGE", "name", "0", "-1"}));
  const RespValue invalid_start = fixture.commands.execute(
      make_command({"LRANGE", "items", "+1", "2"}));
  const RespValue invalid_stop = fixture.commands.execute(
      make_command({"LRANGE", "items", "0", "2x"}));

  constexpr std::string_view kIntegerError =
      "ERR value is not an integer or out of range";
  return is_bulk_string_array(missing, {}) &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         is_string_response(
             invalid_start, RespValue::Type::kError, kIntegerError) &&
         is_string_response(
             invalid_stop, RespValue::Type::kError, kIntegerError);
}

bool test_hset_inserts_updates_and_counts_new_fields() {
  CommandFixture fixture;

  const RespValue created = fixture.commands.execute(
      make_command(
          {"HSET", "user:1", "name", "Alice", "age", "20"}));
  const RespValue updated = fixture.commands.execute(
      make_command(
          {"hset", "user:1", "name", "Bob", "city", "Shanghai"}));
  const RespValue duplicate = fixture.commands.execute(
      make_command(
          {"HSET", "other", "field", "first", "field", "last"}));

  const mini_redis::Value* user = fixture.database.find("user:1");
  const mini_redis::Value* other = fixture.database.find("other");
  return is_integer_response(created, 2) &&
         is_integer_response(updated, 1) &&
         is_integer_response(duplicate, 1) &&
         user != nullptr && user->as_hash() != nullptr &&
         user->as_hash()->size() == 3 &&
         user->as_hash()->at("name") == "Bob" &&
         user->as_hash()->at("age") == "20" &&
         user->as_hash()->at("city") == "Shanghai" &&
         other != nullptr && other->as_hash() != nullptr &&
         other->as_hash()->size() == 1 &&
         other->as_hash()->at("field") == "last";
}

bool test_hset_errors_do_not_mutate() {
  CommandFixture fixture;
  fixture.database.set(
      "name", mini_redis::Value::string("Alice"));

  const RespValue unpaired = fixture.commands.execute(
      make_command({"HSET", "user:1", "name", "Alice", "age"}));
  const RespValue wrong_type = fixture.commands.execute(
      make_command({"HSET", "name", "field", "value"}));
  const mini_redis::Value* name = fixture.database.find("name");

  return is_string_response(
             unpaired, RespValue::Type::kError,
             "ERR wrong number of arguments for 'HSET' command") &&
         !fixture.database.exists("user:1") &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         name != nullptr && name->as_string() != nullptr &&
         *name->as_string() == "Alice";
}

bool test_hget() {
  CommandFixture fixture;
  fixture.database.set(
      "user:1",
      mini_redis::Value::hash({{"name", "Alice"}, {"age", "20"}}));
  fixture.database.set(
      "plain", mini_redis::Value::string("value"));

  const RespValue existing = fixture.commands.execute(
      make_command({"HGET", "user:1", "name"}));
  const RespValue missing_field = fixture.commands.execute(
      make_command({"hget", "user:1", "city"}));
  const RespValue missing_key = fixture.commands.execute(
      make_command({"HGET", "missing", "name"}));
  const RespValue wrong_type = fixture.commands.execute(
      make_command({"HGET", "plain", "field"}));

  const mini_redis::Value* user = fixture.database.find("user:1");
  return is_string_response(
             existing, RespValue::Type::kBulkString, "Alice") &&
         missing_field.type() == RespValue::Type::kNullBulkString &&
         missing_key.type() == RespValue::Type::kNullBulkString &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         user != nullptr && user->as_hash() != nullptr &&
         user->as_hash()->size() == 2 &&
         user->as_hash()->at("name") == "Alice" &&
         user->as_hash()->at("age") == "20";
}

bool test_hdel_counts_deletions_and_removes_empty_hash() {
  CommandFixture fixture;
  fixture.database.set(
      "user:1",
      mini_redis::Value::hash(
          {{"name", "Alice"}, {"age", "20"}, {"city", "Shanghai"}}));

  const RespValue partial = fixture.commands.execute(
      make_command({"HDEL", "user:1", "name", "missing", "age"}));
  const mini_redis::Value* remaining = fixture.database.find("user:1");
  const bool partial_result_ok =
      is_integer_response(partial, 2) &&
      remaining != nullptr && remaining->as_hash() != nullptr &&
      remaining->as_hash()->size() == 1 &&
      remaining->as_hash()->at("city") == "Shanghai";

  const RespValue emptied = fixture.commands.execute(
      make_command({"hdel", "user:1", "city", "city"}));

  return partial_result_ok &&
         is_integer_response(emptied, 1) &&
         !fixture.database.exists("user:1");
}

bool test_hdel_missing_and_wrong_type() {
  CommandFixture fixture;
  fixture.database.set(
      "plain", mini_redis::Value::string("value"));

  const RespValue missing = fixture.commands.execute(
      make_command({"HDEL", "missing", "field"}));
  const RespValue wrong_type = fixture.commands.execute(
      make_command({"HDEL", "plain", "field"}));
  const mini_redis::Value* plain = fixture.database.find("plain");

  return is_integer_response(missing, 0) &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         plain != nullptr && plain->as_string() != nullptr &&
         *plain->as_string() == "value";
}

bool test_hexists() {
  CommandFixture fixture;
  fixture.database.set(
      "user:1", mini_redis::Value::hash({{"name", "Alice"}}));
  fixture.database.set(
      "plain", mini_redis::Value::string("value"));

  const RespValue existing = fixture.commands.execute(
      make_command({"HEXISTS", "user:1", "name"}));
  const RespValue missing_field = fixture.commands.execute(
      make_command({"hexists", "user:1", "age"}));
  const RespValue missing_key = fixture.commands.execute(
      make_command({"HEXISTS", "missing", "name"}));
  const RespValue wrong_type = fixture.commands.execute(
      make_command({"HEXISTS", "plain", "field"}));

  const mini_redis::Value* user = fixture.database.find("user:1");
  return is_integer_response(existing, 1) &&
         is_integer_response(missing_field, 0) &&
         is_integer_response(missing_key, 0) &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         user != nullptr && user->as_hash() != nullptr &&
         user->as_hash()->size() == 1 &&
         user->as_hash()->at("name") == "Alice";
}

bool test_hlen() {
  CommandFixture fixture;
  fixture.database.set(
      "user:1",
      mini_redis::Value::hash({{"name", "Alice"}, {"age", "20"}}));
  fixture.database.set(
      "plain", mini_redis::Value::string("value"));

  const RespValue length = fixture.commands.execute(
      make_command({"HLEN", "user:1"}));
  const RespValue missing = fixture.commands.execute(
      make_command({"hlen", "missing"}));
  const RespValue wrong_type = fixture.commands.execute(
      make_command({"HLEN", "plain"}));

  const mini_redis::Value* user = fixture.database.find("user:1");
  return is_integer_response(length, 2) &&
         is_integer_response(missing, 0) &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         user != nullptr && user->as_hash() != nullptr &&
         user->as_hash()->size() == 2;
}

bool test_hgetall() {
  CommandFixture fixture;
  const mini_redis::Value::Hash expected{
      {"name", "Alice"}, {"age", "20"}, {"city", "Shanghai"}};
  fixture.database.set("user:1", mini_redis::Value::hash(expected));
  fixture.database.set(
      "plain", mini_redis::Value::string("value"));

  const RespValue all = fixture.commands.execute(
      make_command({"HGETALL", "user:1"}));
  const RespValue missing = fixture.commands.execute(
      make_command({"hgetall", "missing"}));
  const RespValue wrong_type = fixture.commands.execute(
      make_command({"HGETALL", "plain"}));

  const mini_redis::Value* user = fixture.database.find("user:1");
  return is_hash_array(all, expected) &&
         is_hash_array(missing, {}) &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value") &&
         user != nullptr && user->as_hash() != nullptr &&
         *user->as_hash() == expected;
}



bool test_zset_commands() {
  CommandFixture fixture;

  const RespValue carol = fixture.commands.execute(
      make_command({"ZADD", "board", "100", "carol"}));
  const RespValue bob = fixture.commands.execute(
      make_command({"ZADD", "board", "80", "bob"}));
  const RespValue alice = fixture.commands.execute(
      make_command({"ZADD", "board", "100", "alice"}));
  const RespValue updated = fixture.commands.execute(
      make_command({"ZADD", "board", "130", "alice"}));
  const RespValue score = fixture.commands.execute(
      make_command({"ZSCORE", "board", "alice"}));
  const RespValue rank = fixture.commands.execute(
      make_command({"ZRANK", "board", "carol"}));
  const RespValue all = fixture.commands.execute(
      make_command({"ZRANGE", "board", "0", "-1"}));
  const RespValue tail = fixture.commands.execute(
      make_command({"ZRANGE", "board", "-2", "-1"}));
  const RespValue removed = fixture.commands.execute(
      make_command({"ZREM", "board", "missing", "alice"}));
  const RespValue remaining = fixture.commands.execute(
      make_command({"ZRANGE", "board", "0", "-1"}));
  const RespValue emptied = fixture.commands.execute(
      make_command({"ZREM", "board", "bob", "carol"}));

  return is_integer_response(carol, 1) &&
         is_integer_response(bob, 1) &&
         is_integer_response(alice, 1) &&
         is_integer_response(updated, 0) &&
         is_string_response(
             score, RespValue::Type::kBulkString, "130") &&
         is_integer_response(rank, 1) &&
         is_bulk_string_array(all, {"bob", "carol", "alice"}) &&
         is_bulk_string_array(tail, {"carol", "alice"}) &&
         is_integer_response(removed, 1) &&
         is_bulk_string_array(remaining, {"bob", "carol"}) &&
         is_integer_response(emptied, 2) &&
         !fixture.database.exists("board");
}

bool test_zset_boundaries_and_errors() {
  CommandFixture fixture;
  fixture.database.set(
      "plain", mini_redis::Value::string("value"));

  const RespValue invalid_score = fixture.commands.execute(
      make_command({"ZADD", "invalid", "nan", "alice"}));
  const RespValue missing_score = fixture.commands.execute(
      make_command({"ZSCORE", "missing", "alice"}));
  const RespValue missing_rank = fixture.commands.execute(
      make_command({"ZRANK", "missing", "alice"}));
  const RespValue missing_range = fixture.commands.execute(
      make_command({"ZRANGE", "missing", "0", "-1"}));
  const RespValue invalid_index = fixture.commands.execute(
      make_command({"ZRANGE", "missing", "x", "-1"}));
  const RespValue wrong_type = fixture.commands.execute(
      make_command({"ZADD", "plain", "1", "alice"}));

  return is_string_response(
             invalid_score, RespValue::Type::kError,
             "ERR value is not a valid float") &&
         !fixture.database.exists("invalid") &&
         missing_score.type() == RespValue::Type::kNullBulkString &&
         missing_rank.type() == RespValue::Type::kNullBulkString &&
         is_bulk_string_array(missing_range, {}) &&
         is_string_response(
             invalid_index, RespValue::Type::kError,
             "ERR value is not an integer or out of range") &&
         is_string_response(
             wrong_type, RespValue::Type::kError,
             "WRONGTYPE Operation against a key holding the wrong kind of value");
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
      !run_test("invalid requests", test_invalid_requests) ||
      !run_test("SET and GET", test_set_and_get) ||
      !run_test("overwrite and injected database",
                test_set_overwrites_and_uses_injected_database) ||
      !run_test("DEL and EXISTS", test_del_and_exists) ||
      !run_test("multi-key DEL and EXISTS",
                test_multi_key_del_and_exists) ||
      !run_test("duplicate keys", test_duplicate_keys) ||
      !run_test("GET wrong type", test_get_wrong_type) ||
      !run_test("INCR and DECR", test_incr_and_decr) ||
      !run_test("integer errors do not mutate",
                test_integer_change_errors_do_not_mutate) ||
      !run_test("LPUSH and RPUSH", test_lpush_and_rpush) ||
      !run_test("list push wrong type",
                test_list_push_wrong_type_does_not_mutate) ||
      !run_test("LPOP and RPOP",
                test_lpop_rpop_and_empty_key_deletion) ||
      !run_test("list pop boundaries",
                test_list_pop_boundaries_do_not_mutate_wrong_type) ||
      !run_test("LLEN", test_llen) ||
      !run_test("LRANGE indices", test_lrange_indices_and_clipping) ||
      !run_test("LRANGE boundaries", test_lrange_boundaries_and_errors) ||
      !run_test("HSET behavior",
                test_hset_inserts_updates_and_counts_new_fields) ||
      !run_test("HSET errors", test_hset_errors_do_not_mutate) ||
      !run_test("HGET", test_hget) ||
      !run_test("HDEL behavior",
                test_hdel_counts_deletions_and_removes_empty_hash) ||
      !run_test("HDEL errors", test_hdel_missing_and_wrong_type) ||
      !run_test("HEXISTS", test_hexists) ||
      !run_test("HLEN", test_hlen) ||
      !run_test("HGETALL", test_hgetall) ||
      !run_test("ZSet commands", test_zset_commands) ||
      !run_test("ZSet errors", test_zset_boundaries_and_errors)) {
    return 1;
  }
  return 0;
}
