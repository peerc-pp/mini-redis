#include "net/channel.h"

#include <sys/epoll.h>

#include <utility>

namespace mini_redis {

Channel::Channel(int fd) noexcept : fd_(fd) {}

int Channel::fd() const noexcept {
  return fd_;
}

std::uint32_t Channel::interest_events() const noexcept {
  return interest_events_;
}

bool Channel::is_reading() const noexcept {
  return (interest_events_ & EPOLLIN) != 0;
}

bool Channel::is_writing() const noexcept {
  return (interest_events_ & EPOLLOUT) != 0;
}

void Channel::set_ready_events(std::uint32_t events) noexcept {
  ready_events_ = events;
}

void Channel::tie(const std::shared_ptr<void>& owner) noexcept {
  owner_ = owner;
  tied_ = true;
}

void Channel::enable_reading() noexcept {
  interest_events_ |= EPOLLIN | EPOLLRDHUP;
}

void Channel::disable_reading() noexcept {
  interest_events_ &= ~(EPOLLIN | EPOLLRDHUP);
}

void Channel::enable_writing() noexcept {
  interest_events_ |= EPOLLOUT;
}

void Channel::disable_writing() noexcept {
  interest_events_ &= ~EPOLLOUT;
}

void Channel::disable_all() noexcept {
  interest_events_ = 0;
}

void Channel::set_read_callback(EventCallback callback) {
  read_callback_ = std::move(callback);
}

void Channel::set_write_callback(EventCallback callback) {
  write_callback_ = std::move(callback);
}

void Channel::set_close_callback(EventCallback callback) {
  close_callback_ = std::move(callback);
}

void Channel::set_error_callback(EventCallback callback) {
  error_callback_ = std::move(callback);
}

void Channel::handle_event() {
  [[maybe_unused]] std::shared_ptr<void> owner_guard;
  if (tied_) {
    owner_guard = owner_.lock();
    if (!owner_guard) {
      return;
    }
  }

  if ((ready_events_ & EPOLLHUP) != 0 &&
      (ready_events_ & EPOLLIN) == 0) {
    if (close_callback_) {
      close_callback_();
    }
    return;
  }

  if ((ready_events_ & EPOLLERR) != 0 && error_callback_) {
    error_callback_();
  }

  if ((ready_events_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) != 0 &&
      read_callback_) {
    read_callback_();
  }

  if ((ready_events_ & EPOLLOUT) != 0 && write_callback_) {
    write_callback_();
  }
}

}  // namespace mini_redis
