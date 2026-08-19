#include <iostream>
#include <utility>

#include "base/version.h"
#include "net/event_loop.h"
#include "net/tcp_server.h"
#include "server/command_registry.h"
#include "server/session.h"
#include "storage/database.h"

int main() {
    std::cout << "mini-redis " << mini_redis::version() << '\n';

    mini_redis::EventLoop loop;
    if (!loop.is_valid()) {
        std::cerr << "failed to create event loop\n";
        return 1;
    }

    mini_redis::Database database;
    mini_redis::CommandRegistry commands(database);
    mini_redis::Session session(commands);

    mini_redis::TcpServerConfig config{
        mini_redis::AcceptorConfig{
            "127.0.0.1",
            6380,
            128,
        },
        mini_redis::TcpConnectionOptions{},
    };

    mini_redis::TcpServer server(loop, std::move(config));
    server.set_message_callback(
        [&session](mini_redis::TcpConnection& connection,
                   mini_redis::Buffer& input) {
            session.on_message(connection, input);
        });

    if (!server.start()) {
        std::cerr << "failed to listen on 127.0.0.1:6380\n";
        return 1;
    }

    std::cout << "listening on 127.0.0.1:" << server.port() << '\n';
    loop.loop();
    return 0;
}
