#include "storage/hash_table.h"

#include <cstddef>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {

struct ConstantHash final {
  [[nodiscard]] std::size_t operator()(
      const std::string&) const noexcept {
    return 0;
  }
};

bool test_basic_operations() {
  mini_redis::HashTable<std::string, int> table;
  if (!table.insert_or_assign("answer", 41) ||
      table.insert_or_assign("answer", 42)) {
    return false;
  }
  const int* answer = table.find("answer");
  return answer != nullptr && *answer == 42 && table.size() == 1 &&
         table.erase("answer") && table.empty() &&
         !table.erase("answer");
}

bool test_collisions_and_binary_keys() {
  mini_redis::HashTable<std::string, int, ConstantHash> table(1);
  const std::string binary_key("key\0part", 8);
  table.insert_or_assign("first", 1);
  table.insert_or_assign("second", 2);
  table.insert_or_assign(binary_key, 3);
  if (!table.erase("second")) {
    return false;
  }
  const int* first = table.find("first");
  const int* binary = table.find(binary_key);
  return first != nullptr && *first == 1 &&
         !table.contains("second") && binary != nullptr &&
         *binary == 3;
}

bool test_rehash_preserves_entries() {
  mini_redis::HashTable<int, std::string> table(2);
  for (int key = 0; key < 200; ++key) {
    table.insert_or_assign(key, std::to_string(key));
  }
  table.reserve(500);
  const std::size_t reserved = table.bucket_count();
  table.rehash(1);
  if (reserved < 500 ||
      table.load_factor() > table.max_load_factor()) {
    return false;
  }
  for (int key = 0; key < 200; ++key) {
    const std::string* value = table.find(key);
    if (value == nullptr || *value != std::to_string(key)) {
      return false;
    }
  }
  return true;
}

bool tables_match(
    const mini_redis::HashTable<int, int>& actual,
    const std::unordered_map<int, int>& expected) {
  if (actual.size() != expected.size()) {
    return false;
  }
  for (int key = -100; key <= 100; ++key) {
    const int* actual_value = actual.find(key);
    const auto expected_value = expected.find(key);
    if ((actual_value == nullptr) !=
        (expected_value == expected.end())) {
      return false;
    }
    if (actual_value != nullptr &&
        *actual_value != expected_value->second) {
      return false;
    }
  }
  return true;
}

bool test_random_operations() {
  mini_redis::HashTable<int, int> actual(1);
  std::unordered_map<int, int> expected;
  std::mt19937 generator(0x5EEDU);
  std::uniform_int_distribution<int> operation(0, 2);
  std::uniform_int_distribution<int> keys(-100, 100);
  std::uniform_int_distribution<int> values(-10000, 10000);

  for (int step = 0; step < 10000; ++step) {
    const int key = keys(generator);
    switch (operation(generator)) {
      case 0: {
        const int value = values(generator);
        if (actual.insert_or_assign(key, value) !=
            expected.insert_or_assign(key, value).second) {
          return false;
        }
        break;
      }
      case 1:
        if (actual.erase(key) != (expected.erase(key) != 0)) {
          return false;
        }
        break;
      case 2:
        if ((actual.find(key) == nullptr) !=
            (expected.find(key) == expected.end())) {
          return false;
        }
        break;
      default:
        return false;
    }
    if (step % 100 == 0 && !tables_match(actual, expected)) {
      return false;
    }
  }
  return tables_match(actual, expected);
}


bool test_reads_do_not_advance_rehash() {
  mini_redis::HashTable<int, int> table(8, 10.0F);
  for (int key = 0; key < 8; ++key) {
    table.insert_or_assign(key, key * 10);
  }

  table.rehash(16);
  if (!table.is_rehashing() || table.rehash_progress() != 0.0F) {
    return false;
  }

  const mini_redis::HashTable<int, int>& const_table = table;
  for (int key = 0; key < 8; ++key) {
    const int* value = const_table.find(key);
    if (value == nullptr || *value != key * 10) {
      return false;
    }
  }

  return table.is_rehashing() &&
         table.rehash_progress() == 0.0F;
}

bool test_writes_advance_one_bucket() {
  mini_redis::HashTable<int, int> table(8, 10.0F);
  for (int key = 0; key < 8; ++key) {
    table.insert_or_assign(key, key);
  }
  table.rehash(16);

  table.insert_or_assign(100, 100);
  if (!table.is_rehashing() ||
      table.rehash_progress() != 0.125F) {
    return false;
  }

  table.erase(999);
  return table.is_rehashing() &&
         table.rehash_progress() == 0.25F;
}

bool test_operations_during_rehash() {
  mini_redis::HashTable<int, int> table(8, 10.0F);
  for (int key = 0; key < 8; ++key) {
    table.insert_or_assign(key, key);
  }
  table.rehash(16);

  if (table.insert_or_assign(7, 70) || !table.erase(6) ||
      !table.insert_or_assign(100, 100)) {
    return false;
  }

  const int* updated = table.find(7);
  const int* inserted = table.find(100);
  if (updated == nullptr || *updated != 70 ||
      table.contains(6) || inserted == nullptr ||
      *inserted != 100 || table.size() != 8) {
    return false;
  }

  while (table.is_rehashing()) {
    table.erase(1000);
  }

  updated = table.find(7);
  inserted = table.find(100);
  return !table.is_rehashing() &&
         table.rehash_progress() == 1.0F &&
         updated != nullptr && *updated == 70 &&
         !table.contains(6) && inserted != nullptr &&
         *inserted == 100 && table.size() == 8;
}

bool test_invalid_load_factor() {
  try {
    const mini_redis::HashTable<int, int> table(8, 0.0F);
    static_cast<void>(table);
  } catch (const std::invalid_argument&) {
    return true;
  }
  return false;
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
  return run_test("basic operations", test_basic_operations) &&
                 run_test("collisions/binary keys",
                          test_collisions_and_binary_keys) &&
                 run_test("rehash preserves entries",
                          test_rehash_preserves_entries) &&
                 run_test("random operations",
                          test_random_operations) &&
                 run_test("reads do not advance rehash",
                          test_reads_do_not_advance_rehash) &&
                 run_test("writes advance one bucket",
                          test_writes_advance_one_bucket) &&
                 run_test("operations during rehash",
                          test_operations_during_rehash) &&
                 run_test("invalid load factor",
                          test_invalid_load_factor)
             ? 0
             : 1;
}
