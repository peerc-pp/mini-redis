#pragma once

#include "net/acceptor.h"
#include "net/tcp_connection.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace mini_redis {

class EventLoop;

struct TcpServerConfig {
  AcceptorConfig acceptor;
  TcpConnectionOptions connection;
};

// Composes Acceptor and TcpConnection. It owns all active connections while
// EventLoop owns only non-owning Channel pointers.
class TcpServer final {
 public:
  using MessageCallback = TcpConnection::MessageCallback;

  TcpServer(EventLoop& loop, TcpServerConfig config);
  ~TcpServer();

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;
  TcpServer(TcpServer&&) = delete;
  TcpServer& operator=(TcpServer&&) = delete;

  [[nodiscard]] bool start();
  void stop() noexcept;

  void set_message_callback(MessageCallback callback);

  [[nodiscard]] bool is_running() const noexcept;
  [[nodiscard]] std::uint16_t port() const noexcept;
  [[nodiscard]] std::size_t connection_count() const noexcept;

 private:
  void handle_new_connection(Socket socket);
  void handle_connection_close(TcpConnection& connection) noexcept;

  EventLoop& loop_;
  TcpServerConfig config_;
  Acceptor acceptor_;
  MessageCallback message_callback_;
  std::unordered_map<int, TcpConnection::Ptr> connections_;
  bool running_{false};
};

}  // namespace mini_redis
