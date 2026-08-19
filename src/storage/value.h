#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <variant>

namespace mini_redis {

class Value final {
 public:
  using String = std::string;
  using List = std::deque<std::string>;
  using Hash = std::unordered_map<std::string, std::string>;

  static Value string(String value);
  static Value list(List value = {});
  static Value hash(Hash value = {});

  [[nodiscard]] bool is_string() const noexcept;
  [[nodiscard]] bool is_list() const noexcept;
  [[nodiscard]] bool is_hash() const noexcept;

  [[nodiscard]] String* as_string() noexcept;
  [[nodiscard]] const String* as_string() const noexcept;

  [[nodiscard]] List* as_list() noexcept;
  [[nodiscard]] const List* as_list() const noexcept;

  [[nodiscard]] Hash* as_hash() noexcept;
  [[nodiscard]] const Hash* as_hash() const noexcept;

 private:
  using Storage = std::variant<String, List, Hash>;

  explicit Value(Storage storage);

  Storage storage_;
};

}  // namespace mini_redis