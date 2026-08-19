#include "storage/database.h"

#include <utility>

namespace mini_redis {

void Database::set(std::string key, Value value) {
  values_.insert_or_assign(std::move(key), std::move(value));
}

Value* Database::find(std::string_view key) {
  const auto entry = values_.find(std::string(key));
  if (entry == values_.end()) {
    return nullptr;
  }

  return &entry->second;
}

const Value* Database::find(std::string_view key) const {
  const auto entry = values_.find(std::string(key));
  if (entry == values_.end()) {
    return nullptr;
  }

  return &entry->second;
}

bool Database::erase(std::string_view key) {
  return values_.erase(std::string(key)) != 0;
}

bool Database::exists(std::string_view key) const {
  return values_.find(std::string(key)) != values_.end();
}

}  // namespace mini_redis