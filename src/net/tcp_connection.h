#pragma once

#include "net/buffer.h"
#include "net/channel.h"
#include "net/socket.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>

namespace mini_redis {

class EventLoop;

struct TcpConnectionOptions {
  std::size_t initial_buffer_size{Buffer::kDefaultInitialSize};
  std::size_t max_input_buffer_size{1024 * 1024};
  std::size_t max_output_buffer_size{1024 * 1024};
};

// Represents one non-blocking TCP byte stream. EventLoop must outlive the
// connection. create() guarantees shared ownership before callbacks start.
class TcpConnection final
    : public std::enable_shared_from_this<TcpConnection> {
 public:
  using Ptr = std::shared_ptr<TcpConnection>;
  using MessageCallback = std::function<void(TcpConnection&, Buffer&)>;
  using CloseCallback = std::function<void(TcpConnection&)>;

  static Ptr create(
      EventLoop& loop,
      Socket socket,
      TcpConnectionOptions options = TcpConnectionOptions{});

  ~TcpConnection();

  TcpConnection(const TcpConnection&) = delete;
  TcpConnection& operator=(const TcpConnection&) = delete;
  TcpConnection(TcpConnection&&) = delete;
  TcpConnection& operator=(TcpConnection&&) = delete;

  [[nodiscard]] bool start();
  [[nodiscard]] bool send(std::string_view data);
  void shutdown() noexcept;
  void force_close() noexcept;

  void set_message_callback(MessageCallback callback);
  void set_close_callback(CloseCallback callback);

  [[nodiscard]] int fd() const noexcept;
  [[nodiscard]] bool connected() const noexcept;
  [[nodiscard]] std::size_t pending_output_bytes() const noexcept;

 private:
  enum class State {
    kConnecting,
    kConnected,
    kDisconnecting,
    kDisconnected,
  };

  TcpConnection(
      EventLoop& loop,
      Socket socket,
      TcpConnectionOptions options);

  void handle_read();
  void handle_write();
  void handle_close() noexcept;
  void shutdown_write() noexcept;

  EventLoop& loop_;
  Socket socket_;
  Channel channel_;
  Buffer input_buffer_;
  Buffer output_buffer_;
  TcpConnectionOptions options_;
  MessageCallback message_callback_;
  CloseCallback close_callback_;
  State state_{State::kConnecting};
  bool registered_{false};
};

}  // namespace mini_redis
