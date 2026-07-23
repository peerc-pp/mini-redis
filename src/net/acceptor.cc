#include "net/acceptor.h"

#include "net/event_loop.h"

#include <cerrno>
#include <utility>

namespace mini_redis {

Acceptor::Acceptor(EventLoop& loop, AcceptorConfig config)
    : loop_(loop),
      config_(std::move(config)),
      listen_socket_(Socket::create_tcp_ipv4()),
      channel_(listen_socket_.fd()) {
  channel_.set_read_callback([this] { handle_read(); });
}

Acceptor::~Acceptor() {
  stop();
}

bool Acceptor::listen() {
  if (listening_) {
    return true;
  }

  if (!listen_socket_.is_valid() ||
      !listen_socket_.set_reuse_address() ||
      !listen_socket_.set_non_blocking() ||
      !listen_socket_.bind_ipv4(config_.bind_address, config_.port) ||
      !listen_socket_.listen(config_.backlog)) {
    return false;
  }

  channel_.enable_reading();
  if (!loop_.add_channel(channel_)) {
    channel_.disable_all();
    return false;
  }

  listening_ = true;
  return true;
}

void Acceptor::stop() noexcept {
  if (!listening_) {
    return;
  }

  channel_.disable_all();
  static_cast<void>(loop_.remove_channel(channel_));
  listening_ = false;
}

void Acceptor::set_new_connection_callback(
    NewConnectionCallback callback) {
  new_connection_callback_ = std::move(callback);
}

bool Acceptor::is_listening() const noexcept {
  return listening_;
}

int Acceptor::fd() const noexcept {
  return listen_socket_.fd();
}

std::uint16_t Acceptor::port() const noexcept {
  std::uint16_t bound_port = 0;
  if (!listen_socket_.local_port(bound_port)) {
    return 0;
  }
  return bound_port;
}

void Acceptor::handle_read() {
  // accept4() is drained to EAGAIN so one LT notification handles the
  // complete queue and every accepted fd is non-blocking and close-on-exec.
  while (true) {
    Socket client = listen_socket_.accept_non_blocking();
    if (client.is_valid()) {
      if (new_connection_callback_) {
        new_connection_callback_(std::move(client));
      }
      continue;
    }

    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    return;
  }
}

}  // namespace mini_redis
