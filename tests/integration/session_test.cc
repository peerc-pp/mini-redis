#include "net/event_loop.h"
#include "net/socket.h"
#include "net/tcp_server.h"
#include "server/command_registry.h"
#include "server/session.h"
#include "storage/database.h"

#include <sys/socket.h>

#include <cerrno>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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
    char chunk[4096];
    const std::size_t remaining = expected_size - received.size();
    const std::size_t request_size =
        remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    const ssize_t result = ::recv(fd, chunk, request_size, 0);
    if (result > 0) {
      received.append(chunk, static_cast<std::size_t>(result));
    } else if (result < 0 &&
               (errno == EINTR || errno == EAGAIN ||
                errno == EWOULDBLOCK)) {
      continue;
    } else {
      return false;
    }
  }
  return received.size() == expected_size;
}

bool has_no_response(int fd, mini_redis::EventLoop& loop) {
  if (!pump_event_loop(loop)) {
    return false;
  }
  char byte = 0;
  const ssize_t result = ::recv(fd, &byte, 1, 0);
  return result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
}

bool wait_for_eof(int fd, mini_redis::EventLoop& loop) {
  for (int attempt = 0; attempt < kMaxPumpAttempts; ++attempt) {
    if (!pump_event_loop(loop)) {
      return false;
    }
    char byte = 0;
    const ssize_t result = ::recv(fd, &byte, 1, 0);
    if (result == 0) {
      return true;
    }
    if (result < 0 &&
        (errno == EINTR || errno == EAGAIN ||
         errno == EWOULDBLOCK)) {
      continue;
    }
    return false;
  }
  return false;
}

class TestServer {
 public:
  TestServer()
      : commands_(database_),
        session_(commands_),
        server_(
            loop_,
            mini_redis::TcpServerConfig{
                mini_redis::AcceptorConfig{"127.0.0.1", 0, 16},
                mini_redis::TcpConnectionOptions{}}) {
    server_.set_message_callback(
        [this](mini_redis::TcpConnection& connection,
               mini_redis::Buffer& input) {
          session_.on_message(connection, input);
        });
  }

  bool start() {
    return loop_.is_valid() && server_.start() &&
           server_.port() != 0;
  }

  std::optional<mini_redis::Socket> connect_client() {
    mini_redis::Socket client =
        mini_redis::Socket::create_tcp_ipv4();
    if (!client.is_valid() ||
        !client.connect_ipv4("127.0.0.1", server_.port()) ||
        !client.set_non_blocking()) {
      return std::nullopt;
    }
    return std::optional<mini_redis::Socket>(std::move(client));
  }

  mini_redis::EventLoop& loop() {
    return loop_;
  }

 private:
  mini_redis::EventLoop loop_;
  mini_redis::Database database_;
  mini_redis::CommandRegistry commands_;
  mini_redis::Session session_;
  mini_redis::TcpServer server_;
};

bool test_pipeline_and_command_responses() {
  TestServer fixture;
  if (!fixture.start()) {
    return false;
  }
  std::optional<mini_redis::Socket> client =
      fixture.connect_client();
  if (!client.has_value()) {
    return false;
  }

  const std::string request =
      "*1\r\n$4\r\nPING\r\n"
      "*2\r\n$4\r\nECHO\r\n$5\r\nhello\r\n"
      "*2\r\n$4\r\nping\r\n$2\r\nhi\r\n"
      "*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$5\r\nalice\r\n"
      "*2\r\n$3\r\nGET\r\n$4\r\nname\r\n"
      "*2\r\n$6\r\nEXISTS\r\n$4\r\nname\r\n"
      "*2\r\n$3\r\nDEL\r\n$4\r\nname\r\n"
      "*2\r\n$3\r\nGET\r\n$4\r\nname\r\n"
      "*2\r\n$6\r\nEXISTS\r\n$4\r\nname\r\n"
      "*2\r\n$3\r\nDEL\r\n$4\r\nname\r\n"
      "*2\r\n$3\r\nGET\r\n$7\r\nmissing\r\n"
      "*3\r\n$3\r\nSET\r\n$5\r\nfirst\r\n$3\r\none\r\n"
      "*3\r\n$3\r\nSET\r\n$6\r\nsecond\r\n$3\r\ntwo\r\n"
      "*4\r\n$6\r\nEXISTS\r\n$5\r\nfirst\r\n$6\r\nsecond\r\n$7\r\nmissing\r\n"
      "*4\r\n$3\r\nDEL\r\n$5\r\nfirst\r\n$6\r\nsecond\r\n$7\r\nmissing\r\n"
      "*3\r\n$6\r\nEXISTS\r\n$5\r\nfirst\r\n$6\r\nsecond\r\n"
      "*1\r\n$4\r\nWHAT\r\n";
  const std::string expected =
      "+PONG\r\n"
      "$5\r\nhello\r\n"
      "$2\r\nhi\r\n"
      "+OK\r\n"
      "$5\r\nalice\r\n"
      ":1\r\n"
      ":1\r\n"
      "$-1\r\n"
      ":0\r\n"
      ":0\r\n"
      "$-1\r\n"
      "+OK\r\n"
      "+OK\r\n"
      ":2\r\n"
      ":2\r\n"
      ":0\r\n"
      "-ERR unknown command 'WHAT'\r\n";

  std::string response;
  return send_all_with_loop(client->fd(), request, fixture.loop()) &&
         receive_exact_with_loop(
             client->fd(), expected.size(), fixture.loop(), response) &&
         response == expected;
}

bool test_clients_share_database() {
  TestServer fixture;
  if (!fixture.start()) {
    return false;
  }
  std::optional<mini_redis::Socket> writer =
      fixture.connect_client();
  std::optional<mini_redis::Socket> reader =
      fixture.connect_client();
  if (!writer.has_value() || !reader.has_value()) {
    return false;
  }

  const std::string set_request =
      "*3\r\n$3\r\nSET\r\n$6\r\nshared\r\n$5\r\nvalue\r\n";
  const std::string set_response = "+OK\r\n";
  std::string response;
  if (!send_all_with_loop(
          writer->fd(), set_request, fixture.loop()) ||
      !receive_exact_with_loop(
          writer->fd(), set_response.size(), fixture.loop(), response) ||
      response != set_response) {
    return false;
  }

  const std::string get_request =
      "*2\r\n$3\r\nGET\r\n$6\r\nshared\r\n";
  const std::string get_response = "$5\r\nvalue\r\n";
  return send_all_with_loop(
             reader->fd(), get_request, fixture.loop()) &&
         receive_exact_with_loop(
             reader->fd(), get_response.size(), fixture.loop(), response) &&
         response == get_response;
}

bool test_partial_request_waits_for_more_data() {
  TestServer fixture;
  if (!fixture.start()) {
    return false;
  }
  std::optional<mini_redis::Socket> client =
      fixture.connect_client();
  if (!client.has_value()) {
    return false;
  }

  const std::string first_half =
      "*2\r\n$4\r\nECHO\r\n$5\r\nhel";
  if (!send_all_with_loop(
          client->fd(), first_half, fixture.loop()) ||
      !has_no_response(client->fd(), fixture.loop())) {
    return false;
  }

  const std::string expected = "$5\r\nhello\r\n";
  std::string response;
  return send_all_with_loop(
             client->fd(), "lo\r\n", fixture.loop()) &&
         receive_exact_with_loop(
             client->fd(), expected.size(), fixture.loop(), response) &&
         response == expected;
}

bool test_protocol_error_returns_error_and_eof() {
  TestServer fixture;
  if (!fixture.start()) {
    return false;
  }
  std::optional<mini_redis::Socket> client =
      fixture.connect_client();
  if (!client.has_value()) {
    return false;
  }

  const std::string expected =
      "-ERR Protocol error: invalid bulk string length\r\n";
  std::string response;
  return send_all_with_loop(client->fd(), "$x\r\n", fixture.loop()) &&
         receive_exact_with_loop(
             client->fd(), expected.size(), fixture.loop(), response) &&
         response == expected &&
         wait_for_eof(client->fd(), fixture.loop());
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
  if (!run_test("pipeline and responses",
                test_pipeline_and_command_responses) ||
      !run_test("clients share database",
                test_clients_share_database) ||
      !run_test("partial request",
                test_partial_request_waits_for_more_data) ||
      !run_test("protocol error",
                test_protocol_error_returns_error_and_eof)) {
    return 1;
  }
  return 0;
}
