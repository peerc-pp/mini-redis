#pragma once

#include "net/buffer.h"
#include "protocol/resp_parser.h"
#include "server/command_registry.h"

namespace mini_redis {

class TcpConnection;

class Session final {
 public:
    explicit Session(const CommandRegistry& commands);

    void on_message(TcpConnection& connection, Buffer& input);

 private:
    RespParser parser_;
    const CommandRegistry& commands_;
};

}  // namespace mini_redis