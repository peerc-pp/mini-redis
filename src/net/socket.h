#pragma once

#include "base/unique_fd.h"

#include <cstdint>
#include <string>

namespace mini_redis {

class Socket final {
 public:
  static Socket create_tcp_ipv4() noexcept;
  // Takes ownership of an existing fd. The caller must not close it afterwards.
  static Socket adopt(int fd) noexcept;

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  Socket(Socket&&) noexcept = default;
  Socket& operator=(Socket&&) noexcept = default;

  [[nodiscard]] int fd() const noexcept;
  [[nodiscard]] bool is_valid() const noexcept;

  [[nodiscard]] bool set_reuse_address() const noexcept;
  [[nodiscard]] bool bind_ipv4(
      const std::string& bind_address,
      std::uint16_t port) const noexcept;
  [[nodiscard]] bool listen(int backlog) const noexcept;
  [[nodiscard]] Socket accept() const noexcept;
  [[nodiscard]] Socket accept_non_blocking() const noexcept;
  [[nodiscard]] bool set_non_blocking() const noexcept;
  [[nodiscard]] bool connect_ipv4(
      const std::string& address,
      std::uint16_t port) const noexcept;
  [[nodiscard]] bool local_port(std::uint16_t& port) const noexcept;
  [[nodiscard]] bool shutdown_write() const noexcept;

 private:
  explicit Socket(int fd) noexcept;

  UniqueFd fd_;
};

}  // namespace mini_redis
