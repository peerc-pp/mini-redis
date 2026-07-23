#include "net/event_loop.h"

#include "net/channel.h"

#include <cerrno>

namespace mini_redis {

EventLoop::EventLoop()
    : poller_(Poller::create()),
      ready_events_(kMaxEvents) {}

bool EventLoop::is_valid() const noexcept {
  return poller_.is_valid();
}

bool EventLoop::add_channel(Channel& channel) {
  const int fd = channel.fd();

  if (channels_.find(fd) != channels_.end()) {
    return false;
  }

  if (!poller_.add(fd, channel.interest_events())) {
    return false;
  }

  channels_.emplace(fd, &channel);
  return true;
}

bool EventLoop::update_channel(Channel& channel) {
  const auto iterator = channels_.find(channel.fd());

  if (iterator == channels_.end() || iterator->second != &channel) {
    return false;
  }

  return poller_.modify(
      channel.fd(),
      channel.interest_events());
}

bool EventLoop::remove_channel(Channel& channel) {
  const auto iterator = channels_.find(channel.fd());

  if (iterator == channels_.end() || iterator->second != &channel) {
    return false;
  }

  if (!poller_.remove(channel.fd())) {
    return false;
  }

  channels_.erase(iterator);
  return true;
}

int EventLoop::loop_once(int timeout_ms) {
  const int ready_count =
      poller_.wait(ready_events_, timeout_ms);

  if (ready_count <= 0) {
    return ready_count;
  }

  for (int index = 0; index < ready_count; ++index) {
    const epoll_event& event = ready_events_[index];

    const auto iterator = channels_.find(event.data.fd);
    if (iterator == channels_.end()) {
      continue;
    }

    Channel* channel = iterator->second;
    channel->set_ready_events(event.events);
    channel->handle_event();
  }

  return ready_count;
}

void EventLoop::loop() {
  running_ = true;

  while (running_) {
    const int ready_count = loop_once(1000);
    if (ready_count < 0 && errno != EINTR) {
      running_ = false;
    }
  }
}

void EventLoop::stop() noexcept {
  running_ = false;
}

}  // namespace mini_redis
