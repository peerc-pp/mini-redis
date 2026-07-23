#pragma once

#include "net/poller.h"

#include <sys/epoll.h>

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace mini_redis {

class Channel;

class EventLoop final {
 public:
  EventLoop();

  EventLoop(const EventLoop&) = delete;
  EventLoop& operator=(const EventLoop&) = delete;
  EventLoop(EventLoop&&) = delete;
  EventLoop& operator=(EventLoop&&) = delete;

  [[nodiscard]] bool is_valid() const noexcept;

  [[nodiscard]] bool add_channel(Channel& channel);
  [[nodiscard]] bool update_channel(Channel& channel);
  [[nodiscard]] bool remove_channel(Channel& channel);

  [[nodiscard]] int loop_once(int timeout_ms);
  void loop();
  void stop() noexcept;

 private:
  static constexpr std::size_t kMaxEvents = 64;

  Poller poller_;
  std::vector<epoll_event> ready_events_;
  std::unordered_map<int, Channel*> channels_;
  bool running_{false};
};

}  // namespace mini_redis
