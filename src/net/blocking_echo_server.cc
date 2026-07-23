#include "net/blocking_echo_server.h"

#include "net/socket.h"

#include <cerrno>
#include <cstdio>
#include <sys/socket.h>

#include <array>
#include <cstddef>
#include <utility>

namespace mini_redis {
namespace {

constexpr std::size_t kBufferSize = 4096;

bool send_all(int fd, const char* data, std::size_t size) {
  std::size_t sent = 0;

  while (sent < size) {
    const ssize_t result =
        ::send(fd, data + sent, size - sent, MSG_NOSIGNAL);

    if (result > 0) {
      sent += static_cast<std::size_t>(result);
      continue;
    }

    if (result < 0 && errno == EINTR) {
      continue;
    }

    if (result < 0) {
      std::perror("send");
    }
    return false;
  }

  return true;
}

bool echo_client(const Socket& client) {
  std::array<char, kBufferSize> buffer{};

  while (true) {
    const ssize_t bytes_received =
        ::recv(client.fd(), buffer.data(), buffer.size(), 0);

    if (bytes_received > 0) {
      if (!send_all(client.fd(), buffer.data(),
                    static_cast<std::size_t>(bytes_received))) {
        return false;
      }
      continue;
    }

    if (bytes_received == 0) {
      return true;
    }

    if (errno == EINTR) {
      continue;
    }

    std::perror("recv");
    return false;
  }
}

}  // namespace

BlockingEchoServer::BlockingEchoServer(BlockingEchoServerConfig config)
    : config_(std::move(config)) {}

int BlockingEchoServer::run() {
  Socket server = Socket::create_tcp_ipv4();
  if (!server.is_valid()) {
    std::perror("socket");
    return 1;
  }

  if (!server.set_reuse_address()) {
    std::perror("setsockopt");
    return 1;
  }

  if (!server.bind_ipv4(config_.bind_address, config_.port)) {
    std::perror("bind");
    return 1;
  }

  if (!server.listen(config_.backlog)) {
    std::perror("listen");
    return 1;
  }

  while (true) {
    Socket client = server.accept();
    if (!client.is_valid()) {
      if (errno == EINTR) {
        continue;
      }

      std::perror("accept");
      return 1;
    }

    echo_client(client);
  }
}

}  // namespace mini_redis
