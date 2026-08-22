#pragma once

#include "storage/zset.h"

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>

namespace mini_redis {

class Value final {
 public:
  using String = std::string;
  using List = std::deque<std::string>;
  using Hash = std::unordered_map<std::string, std::string>;
  using SortedSet = ZSet;

  static Value string(String value);
  static Value list(List value = {});
  static Value hash(Hash value = {});
  static Value zset();

  [[nodiscard]] bool is_string() const noexcept;
  [[nodiscard]] bool is_list() const noexcept;
  [[nodiscard]] bool is_hash() const noexcept;
  [[nodiscard]] bool is_zset() const noexcept;

  [[nodiscard]] String* as_string() noexcept;
  [[nodiscard]] const String* as_string() const noexcept;

  [[nodiscard]] List* as_list() noexcept;
  [[nodiscard]] const List* as_list() const noexcept;

  [[nodiscard]] Hash* as_hash() noexcept;
  [[nodiscard]] const Hash* as_hash() const noexcept;

  [[nodiscard]] SortedSet* as_zset() noexcept;
  [[nodiscard]] const SortedSet* as_zset() const noexcept;

 private:
  using ZSetStorage = std::unique_ptr<SortedSet>;
  using Storage = std::variant<String, List, Hash, ZSetStorage>;

  explicit Value(Storage storage);

  Storage storage_;
};

}  // namespace mini_redis