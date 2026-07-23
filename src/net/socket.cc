#include "net/socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace mini_redis {

Socket::Socket(int fd) noexcept : fd_(fd) {}

Socket Socket::create_tcp_ipv4() noexcept {
  return Socket(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
}

Socket Socket::adopt(int fd) noexcept {
  return Socket(fd);
}

int Socket::fd() const noexcept { return fd_.get(); }

bool Socket::is_valid() const noexcept { return fd_.is_valid(); }

bool Socket::set_reuse_address() const noexcept {
  const int enabled = 1;
  return ::setsockopt(fd(), SOL_SOCKET, SO_REUSEADDR, &enabled,
                      sizeof(enabled)) == 0;
}

bool Socket::bind_ipv4(const std::string& bind_address,
                       std::uint16_t port) const noexcept {
  sockaddr_in server_address{};
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);

  if (::inet_pton(AF_INET, bind_address.c_str(),
                  &server_address.sin_addr) != 1) {
    return false;
  }

  return ::bind(fd(), reinterpret_cast<const sockaddr*>(&server_address),
                sizeof(server_address)) == 0;
}

Socket Socket::accept() const noexcept {
  return Socket(::accept4(fd(), nullptr, nullptr, SOCK_CLOEXEC));
}

Socket Socket::accept_non_blocking() const noexcept {
  return Socket(
      ::accept4(fd(), nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC));
}

bool Socket::listen(int backlog) const noexcept {
  return ::listen(fd(), backlog) == 0;
}

bool Socket::set_non_blocking() const noexcept {
  const int flags = ::fcntl(fd(), F_GETFL, 0);
  if (flags == -1) {
    return false;
  }

  return ::fcntl(fd(), F_SETFL, flags | O_NONBLOCK) != -1;
}

bool Socket::connect_ipv4(const std::string& address,
                          std::uint16_t port) const noexcept {
  sockaddr_in peer_address{};
  peer_address.sin_family = AF_INET;
  peer_address.sin_port = htons(port);

  if (::inet_pton(AF_INET, address.c_str(), &peer_address.sin_addr) != 1) {
    return false;
  }

  return ::connect(fd(), reinterpret_cast<const sockaddr*>(&peer_address),
                   sizeof(peer_address)) == 0;
}

bool Socket::local_port(std::uint16_t& port) const noexcept {
  sockaddr_in local_address{};
  socklen_t address_length = sizeof(local_address);
  if (::getsockname(fd(), reinterpret_cast<sockaddr*>(&local_address),
                    &address_length) != 0) {
    return false;
  }

  port = ntohs(local_address.sin_port);
  return true;
}

bool Socket::shutdown_write() const noexcept {
  return ::shutdown(fd(), SHUT_WR) == 0;
}

}  // namespace mini_redis
