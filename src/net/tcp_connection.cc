#include "net/tcp_connection.h"

#include "net/event_loop.h"

#include <sys/socket.h>

#include <array>
#include <cerrno>
#include <utility>

namespace mini_redis {
namespace {

constexpr std::size_t kReadChunkSize = 64 * 1024;

}  // namespace

TcpConnection::Ptr TcpConnection::create(
    EventLoop& loop,
    Socket socket,
    TcpConnectionOptions options) {
  return Ptr(new TcpConnection(loop, std::move(socket), options));
}

TcpConnection::TcpConnection(
    EventLoop& loop,
    Socket socket,
    TcpConnectionOptions options)
    : loop_(loop),
      socket_(std::move(socket)),
      channel_(socket_.fd()),
      input_buffer_(options.initial_buffer_size),
      output_buffer_(options.initial_buffer_size),
      options_(options) {
  channel_.set_read_callback([this] { handle_read(); });
  channel_.set_write_callback([this] { handle_write(); });
  channel_.set_close_callback([this] { handle_close(); });
  channel_.set_error_callback([this] { handle_close(); });
}

TcpConnection::~TcpConnection() {
  if (registered_) {
    channel_.disable_all();
    static_cast<void>(loop_.remove_channel(channel_));
  }
}

bool TcpConnection::start() {
  if (state_ != State::kConnecting || !socket_.is_valid()) {
    return false;
  }

  channel_.tie(shared_from_this());
  channel_.enable_reading();
  if (!loop_.add_channel(channel_)) {
    channel_.disable_all();
    return false;
  }

  registered_ = true;
  state_ = State::kConnected;
  return true;
}

bool TcpConnection::send(std::string_view data) {
  if (state_ != State::kConnected || data.empty()) {
    return state_ == State::kConnected;
  }

  const std::size_t pending = output_buffer_.readable_bytes();
  if (pending > options_.max_output_buffer_size ||
      data.size() > options_.max_output_buffer_size - pending) {
    return false;
  }

  std::size_t sent = 0;
  if (pending == 0 && !channel_.is_writing()) {
    while (sent < data.size()) {
      const ssize_t result =
          ::send(fd(), data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
      if (result > 0) {
        sent += static_cast<std::size_t>(result);
        continue;
      }
      if (result < 0 && errno == EINTR) {
        continue;
      }
      if (result < 0 &&
          (errno == EAGAIN || errno == EWOULDBLOCK)) {
        break;
      }

      handle_close();
      return false;
    }
  }

  if (sent == data.size()) {
    return true;
  }

  output_buffer_.append(data.data() + sent, data.size() - sent);
  if (!channel_.is_writing()) {
    channel_.enable_writing();
    if (!loop_.update_channel(channel_)) {
      handle_close();
      return false;
    }
  }
  return true;
}

void TcpConnection::shutdown() noexcept {
  if (state_ != State::kConnected) {
    return;
  }

  state_ = State::kDisconnecting;
  if (output_buffer_.empty()) {
    shutdown_write();
  }
}

void TcpConnection::force_close() noexcept {
  handle_close();
}

void TcpConnection::set_message_callback(MessageCallback callback) {
  message_callback_ = std::move(callback);
}

void TcpConnection::set_close_callback(CloseCallback callback) {
  close_callback_ = std::move(callback);
}

int TcpConnection::fd() const noexcept {
  return socket_.fd();
}

bool TcpConnection::connected() const noexcept {
  return state_ == State::kConnected ||
         state_ == State::kDisconnecting;
}

std::size_t TcpConnection::pending_output_bytes() const noexcept {
  return output_buffer_.readable_bytes();
}

void TcpConnection::handle_read() {
  [[maybe_unused]] const Ptr guard = shared_from_this();
  std::array<char, kReadChunkSize> chunk{};
  bool received_data = false;
  bool peer_closed = false;

  // Non-blocking LT sockets are drained to EAGAIN. This also handles a packet
  // split across many recv() calls without assuming message boundaries.
  while (true) {
    const ssize_t result =
        ::recv(fd(), chunk.data(), chunk.size(), 0);
    if (result > 0) {
      const std::size_t received = static_cast<std::size_t>(result);
      if (input_buffer_.readable_bytes() >
              options_.max_input_buffer_size ||
          received > options_.max_input_buffer_size -
                         input_buffer_.readable_bytes()) {
        handle_close();
        return;
      }
      input_buffer_.append(chunk.data(), received);
      received_data = true;
      continue;
    }

    if (result == 0) {
      peer_closed = true;
      break;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    }

    handle_close();
    return;
  }

  if (received_data && message_callback_ &&
      state_ != State::kDisconnected) {
    message_callback_(*this, input_buffer_);
  }

  if (peer_closed && state_ != State::kDisconnected) {
    handle_close();
  }
}

void TcpConnection::handle_write() {
  [[maybe_unused]] const Ptr guard = shared_from_this();
  if (state_ == State::kDisconnected) {
    return;
  }

  // send() may write only part of the buffer. Keep EPOLLOUT enabled until all
  // pending bytes have been consumed or the socket reports EAGAIN.
  while (!output_buffer_.empty()) {
    const ssize_t result =
        ::send(fd(), output_buffer_.peek(),
               output_buffer_.readable_bytes(), MSG_NOSIGNAL);
    if (result > 0) {
      output_buffer_.retrieve(static_cast<std::size_t>(result));
      continue;
    }
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result < 0 &&
        (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }

    handle_close();
    return;
  }

  channel_.disable_writing();
  if (!loop_.update_channel(channel_)) {
    handle_close();
    return;
  }

  if (state_ == State::kDisconnecting) {
    shutdown_write();
  }
}

void TcpConnection::handle_close() noexcept {
  if (state_ == State::kDisconnected) {
    return;
  }

  [[maybe_unused]] const Ptr guard = shared_from_this();
  state_ = State::kDisconnected;
  channel_.disable_all();
  if (registered_) {
    static_cast<void>(loop_.remove_channel(channel_));
    registered_ = false;
  }

  if (close_callback_) {
    close_callback_(*this);
  }
}

void TcpConnection::shutdown_write() noexcept {
  static_cast<void>(socket_.shutdown_write());
}

}  // namespace mini_redis
