#pragma once

namespace mini_redis {
    class UniqueFd {
        public:
             explicit UniqueFd(int fd) noexcept;
            [[nodiscard]] int get() const noexcept;
            [[nodiscard]] bool is_valid() const noexcept;
            UniqueFd(const UniqueFd&) = delete;
            UniqueFd& operator=(const UniqueFd&) = delete;
            UniqueFd(UniqueFd&& other) noexcept;
            UniqueFd& operator=(UniqueFd&& other) noexcept;
            ~UniqueFd();
        private:
            int fd_;   
            static constexpr int kInvalidFd = -1;
            

    };
}