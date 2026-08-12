#pragma once

#include "protocol/resp_value.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mini_redis {

class CommandRegistry final {
 public:
    CommandRegistry();

    [[nodiscard]] RespValue execute(const RespValue& request) const;

 private:
    using RequestElements = std::vector<RespValue>;
    using Handler = RespValue (*)(const RequestElements&);

    struct CommandSpec {
        std::size_t min_argument_count;
        std::size_t max_argument_count;
        Handler handler;
    };

    static std::string normalize_command_name(
        std::string_view command_name);

    static RespValue execute_ping(
        const RequestElements& request_elements);

    static RespValue execute_echo(
        const RequestElements& request_elements);

    std::unordered_map<std::string, CommandSpec> commands_;
};

}  // namespace mini_redis