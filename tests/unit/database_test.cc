#include "storage/database.h"

#include <iostream>
#include <string>

namespace {

bool test_set_and_get() {
  mini_redis::Database database;
  database.set("name", mini_redis::Value::string("alice"));

  const mini_redis::Value* value = database.find("name");
  return value != nullptr && value->as_string() != nullptr &&
         *value->as_string() == "alice" &&
         database.exists("name");
}

bool test_set_overwrites_existing_value() {
  mini_redis::Database database;
  database.set("name", mini_redis::Value::string("alice"));
  database.set("name", mini_redis::Value::string("bob"));

  const mini_redis::Value* value = database.find("name");
  return value != nullptr && value->as_string() != nullptr &&
         *value->as_string() == "bob";
}

bool test_missing_key() {
  const mini_redis::Database database;
  return database.find("missing") == nullptr &&
         !database.exists("missing");
}

bool test_erase() {
  mini_redis::Database database;
  database.set("name", mini_redis::Value::string("alice"));

  return database.erase("name") && !database.exists("name") &&
         database.find("name") == nullptr &&
         !database.erase("name");
}

bool test_keys_are_independent() {
  mini_redis::Database database;
  database.set("first", mini_redis::Value::string("one"));
  database.set("second", mini_redis::Value::string("two"));
  if (!database.erase("first")) {
    return false;
  }

  const mini_redis::Value* second = database.find("second");
  return !database.exists("first") && second != nullptr &&
         second->as_string() != nullptr &&
         *second->as_string() == "two";
}

bool test_empty_and_binary_strings() {
  mini_redis::Database database;
  const std::string binary_key("key\0part", 8);
  const std::string binary_value("\0x\r\n", 4);

  database.set("", mini_redis::Value::string(""));
  database.set(binary_key, mini_redis::Value::string(binary_value));

  const mini_redis::Value* empty = database.find("");
  const mini_redis::Value* binary = database.find(binary_key);
  return empty != nullptr && empty->as_string() != nullptr &&
         empty->as_string()->empty() && binary != nullptr &&
         binary->as_string() != nullptr &&
         *binary->as_string() == binary_value;
}

bool test_typed_values() {
  mini_redis::Database database;
  database.set(
      "items", mini_redis::Value::list({"first"}));
  database.set(
      "profile",
      mini_redis::Value::hash({{"name", "alice"}}));

  mini_redis::Value* items = database.find("items");
  if (items == nullptr || items->as_list() == nullptr) {
    return false;
  }
  items->as_list()->push_back("second");

  const mini_redis::Database& const_database = database;
  const mini_redis::Value* stored_items =
      const_database.find("items");
  const mini_redis::Value* profile =
      const_database.find("profile");

  return stored_items != nullptr &&
         stored_items->as_list() != nullptr &&
         stored_items->as_list()->back() == "second" &&
         profile != nullptr && profile->as_hash() != nullptr &&
         profile->as_hash()->at("name") == "alice";
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
  if (!run_test("set and get", test_set_and_get) ||
      !run_test("overwrite", test_set_overwrites_existing_value) ||
      !run_test("missing key", test_missing_key) ||
      !run_test("erase", test_erase) ||
      !run_test("independent keys", test_keys_are_independent) ||
      !run_test("empty and binary strings",
                test_empty_and_binary_strings) ||
      !run_test("typed values", test_typed_values)) {
    return 1;
  }

  return 0;
}
