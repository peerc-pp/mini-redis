#include "base/unique_fd.h"
#include "net/channel.h"
#include "net/event_loop.h"
#include "net/poller.h"
#include "net/socket.h"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <iostream>
#include <utility>
#include <vector>

namespace {

bool test_unique_fd_closes_on_destruction() {
  int pipe_fds[2]{};
  if (::pipe(pipe_fds) != 0) {
    std::perror("pipe");
    return false;
  }

  const int observed_fd = pipe_fds[0];
  {
    mini_redis::UniqueFd read_end(pipe_fds[0]);
    mini_redis::UniqueFd write_end(pipe_fds[1]);
  }

  errno = 0;
  return ::fcntl(observed_fd, F_GETFD) == -1 && errno == EBADF;
}

bool test_unique_fd_move_transfers_ownership() {
  int pipe_fds[2]{};
  if (::pipe(pipe_fds) != 0) {
    std::perror("pipe");
    return false;
  }

  mini_redis::UniqueFd original(pipe_fds[0]);
  mini_redis::UniqueFd write_end(pipe_fds[1]);
  const int observed_fd = original.get();
  mini_redis::UniqueFd moved(std::move(original));

  return !original.is_valid() && moved.is_valid() &&
         moved.get() == observed_fd;
}

bool test_socket_creation_and_options() {
  mini_redis::Socket socket = mini_redis::Socket::create_tcp_ipv4();
  if (!socket.is_valid() || !socket.set_reuse_address() ||
      !socket.set_non_blocking()) {
    return false;
  }

  const int flags = ::fcntl(socket.fd(), F_GETFL, 0);
  return flags != -1 && (flags & O_NONBLOCK) != 0;
}

bool test_socket_bind_and_listen() {
  mini_redis::Socket socket = mini_redis::Socket::create_tcp_ipv4();
  return socket.is_valid() && socket.bind_ipv4("127.0.0.1", 0) &&
         socket.listen(8);
}

bool test_poller_lifecycle() {
  int pipe_fds[2]{};
  if (::pipe(pipe_fds) != 0) {
    std::perror("pipe");
    return false;
  }

  mini_redis::UniqueFd read_end(pipe_fds[0]);
  mini_redis::UniqueFd write_end(pipe_fds[1]);
  mini_redis::Poller poller = mini_redis::Poller::create();

  if (!poller.is_valid() ||
      !poller.add(read_end.get(), EPOLLIN) ||
      !poller.modify(read_end.get(), EPOLLIN)) {
    return false;
  }

  const char byte = 'x';
  if (::write(write_end.get(), &byte, 1) != 1) {
    return false;
  }

  std::vector<epoll_event> ready_events(4);
  const int ready_count = poller.wait(ready_events, 100);

  bool found_readable_fd = false;
  for (int index = 0; index < ready_count; ++index) {
    const epoll_event& event =
        ready_events[static_cast<std::size_t>(index)];
    if (event.data.fd == read_end.get() &&
        (event.events & EPOLLIN) != 0) {
      found_readable_fd = true;
      break;
    }
  }

  if (ready_count <= 0 || !found_readable_fd ||
      !poller.remove(read_end.get())) {
    return false;
  }

  if (poller.wait(ready_events, 0) != 0) {
    return false;
  }

  std::vector<epoll_event> empty_events;
  errno = 0;
  return poller.wait(empty_events, 0) == -1 && errno == EINVAL;
}

bool test_channel_event_dispatch() {
  mini_redis::Channel channel(42);

  int read_count = 0;
  int write_count = 0;
  int close_count = 0;
  int error_count = 0;

  channel.set_read_callback([&read_count] { ++read_count; });
  channel.set_write_callback([&write_count] { ++write_count; });
  channel.set_close_callback([&close_count] { ++close_count; });
  channel.set_error_callback([&error_count] { ++error_count; });

  channel.enable_reading();
  channel.enable_writing();
  const std::uint32_t expected_events =
      EPOLLIN | EPOLLRDHUP | EPOLLOUT;
  if (channel.fd() != 42 || !channel.is_reading() ||
      !channel.is_writing() ||
      channel.interest_events() != expected_events) {
    return false;
  }

  channel.set_ready_events(EPOLLIN | EPOLLOUT);
  channel.handle_event();
  if (read_count != 1 || write_count != 1 ||
      close_count != 0 || error_count != 0) {
    return false;
  }

  channel.set_ready_events(EPOLLRDHUP);
  channel.handle_event();
  if (read_count != 2) {
    return false;
  }

  channel.set_ready_events(EPOLLERR);
  channel.handle_event();
  if (error_count != 1) {
    return false;
  }

  channel.set_ready_events(EPOLLHUP);
  channel.handle_event();
  if (close_count != 1) {
    return false;
  }

  channel.disable_writing();
  if (channel.is_writing() || !channel.is_reading()) {
    return false;
  }

  channel.disable_reading();
  if (channel.is_reading()) {
    return false;
  }

  channel.enable_reading();
  channel.enable_writing();
  channel.disable_all();
  return channel.interest_events() == 0;
}

bool test_event_loop_channel_lifecycle_and_dispatch() {
  int pipe_fds[2]{};
  if (::pipe(pipe_fds) != 0) {
    std::perror("pipe");
    return false;
  }

  mini_redis::UniqueFd read_end(pipe_fds[0]);
  mini_redis::UniqueFd write_end(pipe_fds[1]);
  mini_redis::EventLoop event_loop;
  mini_redis::Channel channel(read_end.get());

  int read_count = 0;
  char received_byte = '\0';
  channel.set_read_callback(
      [&event_loop, &read_end, &read_count, &received_byte] {
        char byte = '\0';
        if (::read(read_end.get(), &byte, 1) == 1) {
          ++read_count;
          received_byte = byte;
        }
        event_loop.stop();
      });
  channel.enable_reading();

  if (!event_loop.is_valid() ||
      !event_loop.add_channel(channel) ||
      event_loop.add_channel(channel)) {
    return false;
  }

  mini_redis::Channel channel_alias(read_end.get());
  channel_alias.enable_reading();
  if (event_loop.update_channel(channel_alias) ||
      event_loop.remove_channel(channel_alias)) {
    return false;
  }

  const char sent_byte = 'e';
  if (::write(write_end.get(), &sent_byte, 1) != 1 ||
      event_loop.loop_once(100) != 1 ||
      read_count != 1 ||
      received_byte != sent_byte) {
    return false;
  }

  const char loop_byte = 'l';
  if (::write(write_end.get(), &loop_byte, 1) != 1) {
    return false;
  }
  event_loop.loop();
  if (read_count != 2 || received_byte != loop_byte) {
    return false;
  }

  channel.disable_reading();
  if (!event_loop.update_channel(channel) ||
      channel.interest_events() != 0 ||
      event_loop.loop_once(0) != 0) {
    return false;
  }

  channel.enable_reading();
  if (!event_loop.update_channel(channel) ||
      !event_loop.remove_channel(channel) ||
      event_loop.remove_channel(channel)) {
    return false;
  }

  const char ignored_byte = 'x';
  return ::write(write_end.get(), &ignored_byte, 1) == 1 &&
         event_loop.loop_once(10) == 0 &&
         read_count == 2;
}

}  // namespace

int main() {
  if (!test_unique_fd_closes_on_destruction()) {
    std::cerr << "UniqueFd destruction test failed\n";
    return 1;
  }
  if (!test_unique_fd_move_transfers_ownership()) {
    std::cerr << "UniqueFd move test failed\n";
    return 1;
  }
  if (!test_socket_creation_and_options()) {
    std::cerr << "Socket option test failed\n";
    return 1;
  }
  if (!test_socket_bind_and_listen()) {
    std::cerr << "Socket bind/listen test failed\n";
    return 1;
  }
  if (!test_poller_lifecycle()) {
    std::cerr << "Poller lifecycle test failed\n";
    return 1;
  }
  if (!test_channel_event_dispatch()) {
    std::cerr << "Channel event dispatch test failed\n";
    return 1;
  }
  if (!test_event_loop_channel_lifecycle_and_dispatch()) {
    std::cerr << "EventLoop lifecycle/dispatch test failed\n";
    return 1;
  }

  return 0;
}
