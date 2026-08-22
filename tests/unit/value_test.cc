#include "storage/value.h"

#include <iostream>
#include <string>

namespace {

using mini_redis::Value;

bool test_string_value() {
  Value value = Value::string("alice");
  const Value& const_value = value;

  return value.is_string() &&
         !value.is_list() &&
         !value.is_hash() &&
         value.as_string() != nullptr &&
         *value.as_string() == "alice" &&
         const_value.as_string() != nullptr &&
         *const_value.as_string() == "alice" &&
         value.as_list() == nullptr &&
         value.as_hash() == nullptr;
}

bool test_list_value() {
  Value value = Value::list(Value::List{"first", "second"});

  Value::List* list = value.as_list();
  if (list == nullptr) {
    return false;
  }

  list->push_front("zero");

  const Value& const_value = value;
  const Value::List* stored = const_value.as_list();

  return value.is_list() &&
         !value.is_string() &&
         !value.is_hash() &&
         stored != nullptr &&
         stored->size() == 3 &&
         stored->front() == "zero" &&
         stored->back() == "second" &&
         value.as_string() == nullptr &&
         value.as_hash() == nullptr;
}

bool test_hash_value() {
  Value value = Value::hash(Value::Hash{{"name", "alice"}});

  Value::Hash* hash = value.as_hash();
  if (hash == nullptr) {
    return false;
  }

  (*hash)["city"] = "Shanghai";

  const Value& const_value = value;
  const Value::Hash* stored = const_value.as_hash();

  return value.is_hash() &&
         !value.is_string() &&
         !value.is_list() &&
         stored != nullptr &&
         stored->at("name") == "alice" &&
         stored->at("city") == "Shanghai" &&
         value.as_string() == nullptr &&
         value.as_list() == nullptr;
}

bool test_zset_value() {
  Value value = Value::zset();
  Value::SortedSet* zset = value.as_zset();
  if (zset == nullptr ||
      zset->add(100.0, "alice") !=
          Value::SortedSet::AddResult::kAdded) {
    return false;
  }

  const Value& const_value = value;
  const Value::SortedSet* stored = const_value.as_zset();
  return value.is_zset() &&
         !value.is_string() &&
         !value.is_list() &&
         !value.is_hash() &&
         stored != nullptr &&
         stored->score_of("alice") == 100.0 &&
         value.as_string() == nullptr &&
         value.as_list() == nullptr &&
         value.as_hash() == nullptr;
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
  if (!run_test("string value", test_string_value) ||
      !run_test("list value", test_list_value) ||
      !run_test("hash value", test_hash_value) ||
      !run_test("zset value", test_zset_value)) {
    return 1;
  }

  return 0;
}
