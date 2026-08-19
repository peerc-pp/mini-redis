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

bool Value::is_string() const noexcept {
  return std::holds_alternative<String>(storage_);
}

bool Value::is_list() const noexcept {
  return std::holds_alternative<List>(storage_);
}

bool Value::is_hash() const noexcept {
  return std::holds_alternative<Hash>(storage_);
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

}  // namespace mini_redis