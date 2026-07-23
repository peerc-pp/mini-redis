#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mini_redis {

// A growable byte buffer. Bytes in [peek(), peek() + readable_bytes())
// are valid until the next non-const operation.
class Buffer final {
 public:
  static constexpr std::size_t kDefaultInitialSize = 4096;

  explicit Buffer(std::size_t initial_size = kDefaultInitialSize);

  [[nodiscard]] std::size_t readable_bytes() const noexcept;
  [[nodiscard]] std::size_t writable_bytes() const noexcept;
  [[nodiscard]] bool empty() const noexcept;
  [[nodiscard]] const char* peek() const noexcept;

  void retrieve(std::size_t length) noexcept;
  void retrieve_all() noexcept;
  [[nodiscard]] std::string retrieve_as_string(std::size_t length);
  [[nodiscard]] std::string retrieve_all_as_string();

  void append(const char* data, std::size_t length);
  void append(std::string_view data);

 private:
  void ensure_writable_bytes(std::size_t length);

  std::vector<char> storage_;
  std::size_t read_index_{0};
  std::size_t write_index_{0};
};

}  // namespace mini_redis
