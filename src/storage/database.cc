#include "storage/database.h"

#include <utility>

namespace mini_redis {

void Database::set(std::string key, Value value) {
  values_.insert_or_assign(std::move(key), std::move(value));
}

Value* Database::find(std::string_view key) {
  return values_.find(std::string(key));
}

const Value* Database::find(std::string_view key) const {
  return values_.find(std::string(key));
}

bool Database::erase(std::string_view key) {
  return values_.erase(std::string(key));
}

bool Database::exists(std::string_view key) const {
  return values_.contains(std::string(key));
}

}  // namespace mini_redis
