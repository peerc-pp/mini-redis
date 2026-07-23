#include "base/unique_fd.h"
#include "net/buffer.h"
#include "net/event_loop.h"
#include "net/socket.h"
#include "net/tcp_connection.h"
#include "net/tcp_server.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr int kEventTimeoutMs = 20;
constexpr int kMaxPumpAttempts = 1000;

bool pump_event_loop(mini_redis::EventLoop& loop,
                     int timeout_ms = kEventTimeoutMs) {
  const int result = loop.loop_once(timeout_ms);
  return result >= 0 || errno == EINTR;
}

bool send_all_with_loop(int fd, std::string_view data,
                        mini_redis::EventLoop& loop) {
  std::size_t sent = 0;
  for (int attempt = 0;
       attempt < kMaxPumpAttempts && sent < data.size();
       ++attempt) {
    const ssize_t result =
        ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
    if (result > 0) {
      sent += static_cast<std::size_t>(result);
    } else if (result < 0 && errno == EINTR) {
      continue;
    } else if (result < 0 &&
               (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // The peer needs EventLoop time to drain its receive buffer.
    } else {
      return false;
    }

    if (!pump_event_loop(loop, 0)) {
      return false;
    }
  }
  return sent == data.size();
}

bool receive_exact_with_loop(int fd, std::size_t expected_size,
                             mini_redis::EventLoop& loop,
                             std::string& received) {
  received.clear();
  received.reserve(expected_size);

  for (int attempt = 0;
       attempt < kMaxPumpAttempts && received.size() < expected_size;
       ++attempt) {
    if (!pump_event_loop(loop)) {
      return false;
    }

    char chunk[8192];
    const std::size_t remaining = expected_size - received.size();
    const std::size_t request_size =
        remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    const ssize_t result = ::recv(fd, chunk, request_size, 0);
    if (result > 0) {
      received.append(chunk, static_cast<std::size_t>(result));
      continue;
    }
    if (result < 0 &&
        (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
      continue;
    }
    return false;
  }

  return received.size() == expected_size;
}

bool wait_for_connection_count(mini_redis::TcpServer& server,
                               mini_redis::EventLoop& loop,
                               std::size_t expected_count) {
  for (int attempt = 0; attempt < kMaxPumpAttempts; ++attempt) {
    if (server.connection_count() == expected_count) {
      return true;
    }
    if (!pump_event_loop(loop)) {
      return false;
    }
  }
  return false;
}

bool test_buffer_compacts_and_grows() {
  mini_redis::Buffer buffer(4);
  buffer.append("abcd");
  if (buffer.readable_bytes() != 4 ||
      buffer.retrieve_as_string(2) != "ab") {
    return false;
  }

  buffer.append("efgh");
  if (buffer.retrieve_all_as_string() != "cdefgh" ||
      !buffer.empty()) {
    return false;
  }

  const std::string large_payload(32 * 1024, 'b');
  buffer.append(large_payload);
  buffer.retrieve(large_payload.size() + 1);
  return buffer.empty() && buffer.readable_bytes() == 0;
}

bool test_tcp_connection_dispatch_and_limits() {
  int socket_fds[2]{};
  if (::socketpair(AF_UNIX,
                   SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                   0, socket_fds) != 0) {
    return false;
  }

  mini_redis::EventLoop loop;
  mini_redis::UniqueFd peer(socket_fds[1]);
  mini_redis::TcpConnectionOptions options;
  options.max_input_buffer_size = 64;
  options.max_output_buffer_size = 64;

  mini_redis::TcpConnection::Ptr connection =
      mini_redis::TcpConnection::create(
          loop, mini_redis::Socket::adopt(socket_fds[0]), options);

  bool send_succeeded = true;
  bool closed = false;
  connection->set_message_callback(
      [&send_succeeded](mini_redis::TcpConnection& active_connection,
                        mini_redis::Buffer& input) {
        const std::string message = input.retrieve_all_as_string();
        send_succeeded = active_connection.send(message);
      });
  connection->set_close_callback(
      [&closed](mini_redis::TcpConnection&) { closed = true; });

  if (!loop.is_valid() || !connection->start()) {
    return false;
  }

  const std::string message = "socketpair-echo";
  if (::send(peer.get(), message.data(), message.size(), MSG_NOSIGNAL) !=
          static_cast<ssize_t>(message.size()) ||
      !pump_event_loop(loop)) {
    return false;
  }

  std::string echoed;
  if (!receive_exact_with_loop(peer.get(), message.size(), loop, echoed) ||
      echoed != message || !send_succeeded) {
    return false;
  }

  const std::string oversized_output(
      options.max_output_buffer_size + 1, 'o');
  if (connection->send(oversized_output) ||
      connection->pending_output_bytes() != 0) {
    return false;
  }

  const std::string oversized_input(
      options.max_input_buffer_size + 1, 'i');
  if (::send(peer.get(), oversized_input.data(), oversized_input.size(),
             MSG_NOSIGNAL) !=
          static_cast<ssize_t>(oversized_input.size()) ||
      !pump_event_loop(loop)) {
    return false;
  }

  return closed && !connection->connected();
}

bool test_tcp_connection_partial_write() {
  int socket_fds[2]{};
  if (::socketpair(AF_UNIX,
                   SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                   0, socket_fds) != 0) {
    return false;
  }

  const int small_send_buffer = 4096;
  if (::setsockopt(socket_fds[0], SOL_SOCKET, SO_SNDBUF,
                   &small_send_buffer,
                   sizeof(small_send_buffer)) != 0) {
    ::close(socket_fds[0]);
    ::close(socket_fds[1]);
    return false;
  }

  mini_redis::EventLoop loop;
  mini_redis::UniqueFd peer(socket_fds[1]);
  mini_redis::TcpConnectionOptions options;
  options.max_output_buffer_size = 512 * 1024;
  mini_redis::TcpConnection::Ptr connection =
      mini_redis::TcpConnection::create(
          loop, mini_redis::Socket::adopt(socket_fds[0]), options);
  if (!connection->start()) {
    return false;
  }

  const std::string payload(256 * 1024, 'p');
  if (!connection->send(payload) ||
      connection->pending_output_bytes() == 0) {
    return false;
  }

  std::string received;
  if (!receive_exact_with_loop(
          peer.get(), payload.size(), loop, received) ||
      received != payload ||
      connection->pending_output_bytes() != 0) {
    return false;
  }

  connection->force_close();
  return !connection->connected();
}

bool test_tcp_server_echo_workloads() {
  constexpr std::size_t kClientCount = 100;

  mini_redis::EventLoop loop;
  mini_redis::TcpServerConfig config{
      mini_redis::AcceptorConfig{"127.0.0.1", 0, 128},
      mini_redis::TcpConnectionOptions{
          4096, 2 * 1024 * 1024, 2 * 1024 * 1024}};
  mini_redis::TcpServer server(loop, std::move(config));
  server.set_message_callback(
      [](mini_redis::TcpConnection& connection,
         mini_redis::Buffer& input) {
        const std::string bytes = input.retrieve_all_as_string();
        if (!connection.send(bytes)) {
          connection.force_close();
        }
      });

  if (!server.start() || server.port() == 0) {
    return false;
  }

  std::vector<mini_redis::Socket> clients;
  clients.reserve(kClientCount);
  for (std::size_t index = 0; index < kClientCount; ++index) {
    mini_redis::Socket client =
        mini_redis::Socket::create_tcp_ipv4();
    if (!client.is_valid() ||
        !client.connect_ipv4("127.0.0.1", server.port()) ||
        !client.set_non_blocking()) {
      return false;
    }
    clients.push_back(std::move(client));
  }

  if (!wait_for_connection_count(server, loop, kClientCount)) {
    return false;
  }

  std::vector<std::string> messages;
  messages.reserve(kClientCount);
  for (std::size_t index = 0; index < kClientCount; ++index) {
    messages.push_back("client-" + std::to_string(index));
    const std::string& message = messages.back();
    if (::send(clients[index].fd(), message.data(), message.size(),
               MSG_NOSIGNAL) !=
        static_cast<ssize_t>(message.size())) {
      return false;
    }
  }

  for (std::size_t index = 0; index < kClientCount; ++index) {
    std::string echoed;
    if (!receive_exact_with_loop(
            clients[index].fd(), messages[index].size(), loop, echoed) ||
        echoed != messages[index]) {
      return false;
    }
  }

  const std::string bytewise_message = "bytewise";
  for (const char byte : bytewise_message) {
    if (::send(clients[0].fd(), &byte, 1, MSG_NOSIGNAL) != 1) {
      return false;
    }
    if (!pump_event_loop(loop)) {
      return false;
    }
  }
  std::string bytewise_echo;
  if (!receive_exact_with_loop(clients[0].fd(),
                               bytewise_message.size(), loop,
                               bytewise_echo) ||
      bytewise_echo != bytewise_message) {
    return false;
  }

  const std::string large_message(256 * 1024, 'L');
  std::string large_echo;
  if (!send_all_with_loop(clients[1].fd(), large_message, loop) ||
      !receive_exact_with_loop(clients[1].fd(),
                               large_message.size(), loop, large_echo) ||
      large_echo != large_message) {
    return false;
  }

  const std::string final_message = "half-close";
  if (!send_all_with_loop(clients[2].fd(), final_message, loop) ||
      ::shutdown(clients[2].fd(), SHUT_WR) != 0) {
    return false;
  }
  std::string final_echo;
  if (!receive_exact_with_loop(clients[2].fd(),
                               final_message.size(), loop, final_echo) ||
      final_echo != final_message ||
      !wait_for_connection_count(server, loop, kClientCount - 1)) {
    return false;
  }

  server.stop();
  return !server.is_running() && server.connection_count() == 0;
}

bool run_test(const char* name, bool (*test)()) {
  if (test()) {
    return true;
  }
  std::cerr << name << " failed\n";
  return false;
}

}  // namespace

int main() {
  if (!run_test("Buffer compaction/growth",
                test_buffer_compacts_and_grows) ||
      !run_test("TcpConnection dispatch/limits",
                test_tcp_connection_dispatch_and_limits) ||
      !run_test("TcpConnection partial write",
                test_tcp_connection_partial_write) ||
      !run_test("TcpServer echo workloads",
                test_tcp_server_echo_workloads)) {
    return 1;
  }
  return 0;
}
