#include <iostream>
#include <utility>

#include "base/version.h"
#include "net/blocking_echo_server.h"

int main() {
    std::cout << "mini-redis " << mini_redis::version() << '\n';

    mini_redis::BlockingEchoServerConfig config{
        "127.0.0.1",
        6380,
        128,
    };
    mini_redis::BlockingEchoServer server(std::move(config));
    return server.run();
}
