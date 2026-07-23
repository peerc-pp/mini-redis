#pragma once

#include "net/channel.h"
#include "net/socket.h"

#include <cstdint>
#include <functional>
#include <string>

namespace mini_redis {

class EventLoop;

struct AcceptorConfig {
  std::string bind_address;
  std::uint16_t port;
  int backlog;
};

// Owns the listening socket and turns readable events into accepted sockets.
// EventLoop must outlive Acceptor.
class Acceptor final {
 public:
  using NewConnectionCallback = std::function<void(Socket)>;

  Acceptor(EventLoop& loop, AcceptorConfig config);
  ~Acceptor();

  Acceptor(const Acceptor&) = delete;
  Acceptor& operator=(const Acceptor&) = delete;
  Acceptor(Acceptor&&) = delete;
  Acceptor& operator=(Acceptor&&) = delete;

  [[nodiscard]] bool listen();
  void stop() noexcept;

  void set_new_connection_callback(NewConnectionCallback callback);

  [[nodiscard]] bool is_listening() const noexcept;
  [[nodiscard]] int fd() const noexcept;
  [[nodiscard]] std::uint16_t port() const noexcept;

 private:
  void handle_read();

  EventLoop& loop_;
  AcceptorConfig config_;
  Socket listen_socket_;
  Channel channel_;
  NewConnectionCallback new_connection_callback_;
  bool listening_{false};
};

}  // namespace mini_redis
