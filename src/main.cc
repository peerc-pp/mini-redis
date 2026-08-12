#include <iostream>
#include <utility>

#include "base/version.h"
#include "net/event_loop.h"
#include "net/tcp_server.h"
#include "server/command_registry.h"
#include "server/session.h"
int main() {
    std::cout << "mini-redis " << mini_redis::version() << '\n';

    // 1. EventLoop 管理所有 epoll 事件。
    mini_redis::EventLoop loop;
    mini_redis::CommandRegistry commands;
    mini_redis::Session session(commands);
    if (!loop.is_valid()) {
        std::cerr << "failed to create event loop\n";
        return 1;
    }

    // 2. 配置监听地址、端口和每个连接的 Buffer 限制。
    mini_redis::TcpServerConfig config{
        mini_redis::AcceptorConfig{
            "127.0.0.1",
            6380,
            128,
        },
        mini_redis::TcpConnectionOptions{},
    };

    // 3. TcpServer 内部管理 Acceptor 和所有 TcpConnection。
    mini_redis::TcpServer server(loop, std::move(config));

    // 4. 每当连接收到数据，TcpConnection 就调用这里。
    server.set_message_callback(
        [&session](mini_redis::TcpConnection& connection,
           mini_redis::Buffer& input) {
            // std::string bytes = input.retrieve_all_as_string();
            session.on_message(connection, input);

        });

    // 5. 创建监听 socket 并注册到 EventLoop。
    if (!server.start()) {
        std::cerr << "failed to listen on 127.0.0.1:6380\n";
        return 1;
    }

    std::cout << "listening on 127.0.0.1:" << server.port() << '\n';

    // 6. 进入 epoll 事件循环。正常运行时这里一直阻塞。
    loop.loop();

    return 0;
}