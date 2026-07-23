#include "net/tcp_server.h"

#include "net/event_loop.h"

#include <utility>
#include <vector>

namespace mini_redis {

TcpServer::TcpServer(EventLoop& loop, TcpServerConfig config)
    : loop_(loop),
      config_(std::move(config)),
      acceptor_(loop_, config_.acceptor) {
  acceptor_.set_new_connection_callback(
      [this](Socket socket) {
        handle_new_connection(std::move(socket));
      });
}

TcpServer::~TcpServer() {
  stop();
}

bool TcpServer::start() {
  if (running_) {
    return true;
  }
  if (!acceptor_.listen()) {
    return false;
  }

  running_ = true;
  return true;
}

void TcpServer::stop() noexcept {
  if (!running_ && connections_.empty()) {
    return;
  }

  acceptor_.stop();
  running_ = false;

  std::vector<TcpConnection::Ptr> active_connections;
  active_connections.reserve(connections_.size());
  for (const auto& entry : connections_) {
    active_connections.push_back(entry.second);
  }
  connections_.clear();

  for (const TcpConnection::Ptr& connection : active_connections) {
    connection->force_close();
  }
}

void TcpServer::set_message_callback(MessageCallback callback) {
  message_callback_ = std::move(callback);
  for (const auto& entry : connections_) {
    entry.second->set_message_callback(message_callback_);
  }
}

bool TcpServer::is_running() const noexcept {
  return running_;
}

std::uint16_t TcpServer::port() const noexcept {
  return acceptor_.port();
}

std::size_t TcpServer::connection_count() const noexcept {
  return connections_.size();
}

void TcpServer::handle_new_connection(Socket socket) {
  TcpConnection::Ptr connection =
      TcpConnection::create(loop_, std::move(socket), config_.connection);
  const int connection_fd = connection->fd();

  connection->set_message_callback(message_callback_);
  connection->set_close_callback(
      [this](TcpConnection& closed_connection) {
        handle_connection_close(closed_connection);
      });

  const auto inserted =
      connections_.emplace(connection_fd, connection);
  if (!inserted.second) {
    return;
  }
  if (!connection->start()) {
    connections_.erase(connection_fd);
  }
}

void TcpServer::handle_connection_close(
    TcpConnection& connection) noexcept {
  connections_.erase(connection.fd());
}

}  // namespace mini_redis
