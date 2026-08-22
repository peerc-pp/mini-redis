#include "storage/value.h"

#include <utility>

namespace mini_redis {

Value::Value(Storage storage)
    : storage_(std::move(storage)) {}

Value Value::string(String value) {
  return Value(Storage{std::move(value)});
}

Value Value::list(List value) {
  return Value(Storage{std::move(value)});
}

Value Value::hash(Hash value) {
  return Value(Storage{std::move(value)});
}

Value Value::zset() {
  return Value(Storage{std::make_unique<SortedSet>()});
}

bool Value::is_string() const noexcept {
  return std::holds_alternative<String>(storage_);
}

bool Value::is_list() const noexcept {
  return std::holds_alternative<List>(storage_);
}

bool Value::is_hash() const noexcept {
  return std::holds_alternative<Hash>(storage_);
}

bool Value::is_zset() const noexcept {
  return std::holds_alternative<ZSetStorage>(storage_);
}

Value::String* Value::as_string() noexcept {
  return std::get_if<String>(&storage_);
}

const Value::String* Value::as_string() const noexcept {
  return std::get_if<String>(&storage_);
}

Value::List* Value::as_list() noexcept {
  return std::get_if<List>(&storage_);
}

const Value::List* Value::as_list() const noexcept {
  return std::get_if<List>(&storage_);
}

Value::Hash* Value::as_hash() noexcept {
  return std::get_if<Hash>(&storage_);
}

const Value::Hash* Value::as_hash() const noexcept {
  return std::get_if<Hash>(&storage_);
}

Value::SortedSet* Value::as_zset() noexcept {
  ZSetStorage* value = std::get_if<ZSetStorage>(&storage_);
  return value == nullptr ? nullptr : value->get();
}

const Value::SortedSet* Value::as_zset() const noexcept {
  const ZSetStorage* value =
      std::get_if<ZSetStorage>(&storage_);
  return value == nullptr ? nullptr : value->get();
}

}  // namespace mini_redis
