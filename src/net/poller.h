#pragma once

#include "base/unique_fd.h"

#include <sys/epoll.h>

#include <cstdint>
#include <vector>
namespace mini_redis {
    class Poller{
        public:
            static Poller create() noexcept;
            ~Poller()=default;
            Poller(const Poller&)=delete;
            Poller& operator=(const Poller&)=delete;
            Poller(Poller&&) noexcept = default;
            Poller& operator=(Poller&&) noexcept = default;

            [[nodiscard]] int fd() const noexcept;
            [[nodiscard]] bool is_valid() const noexcept;

            [[nodiscard]] bool add(int fd, std::uint32_t events) const noexcept;
            [[nodiscard]] bool modify(int fd, std::uint32_t events) const noexcept;
            [[nodiscard]] bool remove(int fd) const noexcept;

            [[nodiscard]] int wait(
                std::vector<epoll_event>& ready_events,
                int timeout_ms) const noexcept;

        private:
            explicit Poller(int epoll_fd) noexcept;
            UniqueFd epoll_fd_;
    };
}
