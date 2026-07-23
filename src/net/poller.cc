#include "net/poller.h"

#include <cerrno>
#include <sys/epoll.h>
namespace mini_redis{
    Poller::Poller(int epoll_fd) noexcept : epoll_fd_(epoll_fd) {}
    Poller Poller::create() noexcept{
        return Poller(::epoll_create1(EPOLL_CLOEXEC));
    }
    int Poller::fd() const noexcept{
        return epoll_fd_.get();
    }
    bool Poller::is_valid() const noexcept{
        return epoll_fd_.is_valid();
    }
    bool Poller::add(int fd, std::uint32_t events) const noexcept{
        epoll_event event{};
        event.events=events;
        event.data.fd=fd;
        return ::epoll_ctl(epoll_fd_.get(),EPOLL_CTL_ADD,fd,&event)==0;
    }
    bool Poller::modify(int fd, std::uint32_t events) const noexcept{
         epoll_event event{};
        event.events=events;
        event.data.fd=fd;
        return ::epoll_ctl(epoll_fd_.get(),EPOLL_CTL_MOD,fd,&event)==0;
    }
    bool Poller::remove(int fd) const noexcept{
        return ::epoll_ctl(epoll_fd_.get(),EPOLL_CTL_DEL,fd,nullptr)==0;
    }
    int Poller::wait(
        std::vector<epoll_event>& ready_events,
        int timeout_ms) const noexcept{
         if (ready_events.empty()) {
            errno = EINVAL;
            return -1;
        }
        return ::epoll_wait(
            epoll_fd_.get(),
            ready_events.data(),
            static_cast<int>(ready_events.size()),
            timeout_ms);
    }
}
