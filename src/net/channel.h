#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace mini_redis {

class Channel final {
 public:
  using EventCallback = std::function<void()>;

  explicit Channel(int fd) noexcept;

  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;
  Channel(Channel&&) = delete;
  Channel& operator=(Channel&&) = delete;

  [[nodiscard]] int fd() const noexcept;
  [[nodiscard]] std::uint32_t interest_events() const noexcept;
  [[nodiscard]] bool is_reading() const noexcept;
  [[nodiscard]] bool is_writing() const noexcept;

  void set_ready_events(std::uint32_t events) noexcept;

  // Keeps owner alive for the complete callback dispatch. This prevents a
  // close callback from destroying the object that contains this Channel.
  void tie(const std::shared_ptr<void>& owner) noexcept;

  void enable_reading() noexcept;
  void disable_reading() noexcept;
  void enable_writing() noexcept;
  void disable_writing() noexcept;
  void disable_all() noexcept;

  void set_read_callback(EventCallback callback);
  void set_write_callback(EventCallback callback);
  void set_close_callback(EventCallback callback);
  void set_error_callback(EventCallback callback);

  void handle_event();

 private:
  int fd_;
  std::uint32_t interest_events_{0};
  std::uint32_t ready_events_{0};

  EventCallback read_callback_;
  EventCallback write_callback_;
  EventCallback close_callback_;
  EventCallback error_callback_;
  std::weak_ptr<void> owner_;
  bool tied_{false};
};

}  // namespace mini_redis
