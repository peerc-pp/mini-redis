#pragma once

#include "protocol/resp_value.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mini_redis {

class Database;

class CommandRegistry final {
 public:
    explicit CommandRegistry(Database& database);

    [[nodiscard]] RespValue execute(const RespValue& request);

 private:
    using RequestElements = std::vector<RespValue>;

    enum class ListEnd {
        kLeft,
        kRight,
    };

    using Handler = RespValue (CommandRegistry::*)(
        const RequestElements&);

    struct CommandSpec {
        std::size_t min_argument_count;
        std::size_t max_argument_count;
        Handler handler;
    };

    static std::string normalize_command_name(
        std::string_view command_name);

    RespValue execute_ping(
        const RequestElements& request_elements);

    RespValue execute_echo(
        const RequestElements& request_elements);

    RespValue execute_set(
        const RequestElements& request_elements);

    RespValue execute_get(
        const RequestElements& request_elements);

    RespValue execute_incr(
        const RequestElements& request_elements);

    RespValue execute_decr(
        const RequestElements& request_elements);

    RespValue execute_integer_change(
        const RequestElements& request_elements,
        std::int64_t delta);

    RespValue execute_lpush(
        const RequestElements& request_elements);

    RespValue execute_rpush(
        const RequestElements& request_elements);

    RespValue execute_list_push(
        const RequestElements& request_elements,
        ListEnd end);

    RespValue execute_lpop(
        const RequestElements& request_elements);

    RespValue execute_rpop(
        const RequestElements& request_elements);

    RespValue execute_list_pop(
        const RequestElements& request_elements,
        ListEnd end);

    RespValue execute_llen(
        const RequestElements& request_elements);

    RespValue execute_lrange(
        const RequestElements& request_elements);

    RespValue execute_hset(
        const RequestElements& request_elements);

    RespValue execute_hget(
        const RequestElements& request_elements);

    RespValue execute_hdel(
        const RequestElements& request_elements);

    RespValue execute_hexists(
        const RequestElements& request_elements);

    RespValue execute_hlen(
        const RequestElements& request_elements);

    RespValue execute_hgetall(
        const RequestElements& request_elements);

    RespValue execute_zadd(
        const RequestElements& request_elements);

    RespValue execute_zrem(
        const RequestElements& request_elements);

    RespValue execute_zscore(
        const RequestElements& request_elements);

    RespValue execute_zrank(
        const RequestElements& request_elements);

    RespValue execute_zrange(
        const RequestElements& request_elements);

    RespValue execute_del(
        const RequestElements& request_elements);

    RespValue execute_exists(
        const RequestElements& request_elements);

    Database& database_;
    std::unordered_map<std::string, CommandSpec> commands_;
};

}  // namespace mini_redis
