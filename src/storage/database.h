#pragma once

#include "storage/hash_table.h"
#include "storage/value.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace mini_redis {

class Database final {
 public:
  void set(std::string key, Value value);

  [[nodiscard]] Value* find(std::string_view key);

  [[nodiscard]] const Value* find(std::string_view key) const;

  [[nodiscard]] bool erase(std::string_view key);

  [[nodiscard]] bool exists(std::string_view key) const;

 private:
  HashTable<std::string, Value> values_;
};

}  // namespace mini_redis
