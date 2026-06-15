# Milestone 01: Blocking TCP Echo Server

> 这是一份工程验收手册，不适合第一次从头通读。如果你刚开始学习网络编程，请先完成
> [Mini-Redis 第一课](START-HERE.md)，遇到具体问题时再回到本文查阅。

## 1. 这一阶段的目的

Blocking Echo Server 不是为了实现一个有用的产品，而是为了完整理解 TCP Server 的
最小生命周期。后续 non-blocking、epoll、Reactor、RESP 都建立在这一条路径上：

```text
socket
  -> setsockopt
  -> bind
  -> listen
  -> accept
  -> recv
  -> send
  -> close client
  -> accept next client
```

完成后，你应能解释每个系统调用操作了什么内核对象、返回什么、何时阻塞，以及失败后
哪些 file descriptor 必须关闭。

## 2. 开发环境

项目最终依赖 Linux `epoll`，因此推荐：

- Windows 负责编辑、Git 和 GitHub。
- WSL2 Ubuntu 负责编译、运行、调试和系统调用观察。
- 源码最好 clone 到 WSL 的 `~/mini-redis`，而不是长期放在 `/mnt/d`。

本阶段的 blocking socket 也可以用 Winsock 实现，但那会引入 `WSAStartup()`、
`closesocket()` 和 `WSAGetLastError()` 等 Windows 专属概念，不利于后续自然过渡到
Linux `epoll`。

## 3. 开始编码前必须学习的知识

### 3.1 Process、Kernel 与 File Descriptor

需要回答：

1. user space 和 kernel space 分别是什么？
2. system call 为什么比普通函数调用更重？
3. file descriptor 为什么只是当前进程中的一个整数索引？
4. `socket()` 返回的 fd 指向什么内核对象？
5. 忘记 `close()` 为什么会形成 fd leak？
6. 进程退出后内核为什么能回收 fd，但程序仍然不能依赖这一点？

学习重点：

- process file descriptor table
- open file description
- RAII ownership
- resource leak 与异常/提前返回路径

### 3.2 TCP 基础

需要掌握：

- TCP 是可靠、有序、全双工的 byte stream。
- TCP 不保留应用层 message boundary。
- Server 是被动打开，Client 是主动打开。
- 三次握手建立连接，四次挥手关闭连接。
- listening socket 和 connected socket 是两个不同的 fd。
- `accept()` 不会把 listening fd 变成 connected fd，而是返回一个新 fd。

当前阶段只需理解状态：

```text
Server: CLOSED -> LISTEN -> ESTABLISHED
Client: CLOSED -> SYN_SENT -> ESTABLISHED
```

TIME_WAIT、半关闭和粘包先形成概念，后续阶段再深入。

### 3.3 IPv4 地址与字节序

需要掌握：

- `sockaddr_in`
- `AF_INET`
- `INADDR_ANY` 与 `127.0.0.1`
- port 范围 0-65535
- host byte order 与 network byte order
- `htons()`、`htonl()`、`ntohs()`、`ntohl()`
- 为什么 API 接受通用的 `sockaddr*`

建议首版只 bind `127.0.0.1:6380`，避免无意中对局域网开放未完成的服务。

### 3.4 六个核心系统调用

#### `socket()`

```cpp
int socket(int domain, int type, int protocol);
```

首版参数：

```cpp
socket(AF_INET, SOCK_STREAM, 0);
```

成功返回 fd，失败返回 `-1` 并设置 `errno`。

#### `bind()`

把本地 IP 和 port 绑定到 socket。重点错误：

- `EADDRINUSE`: 端口已被占用。
- `EACCES`: 没有权限使用地址或低端口。

#### `listen()`

把主动 socket 转换成 listening socket。`backlog` 与“最多客户端数”不是同一概念；
它主要影响等待 `accept()` 的已连接队列。

#### `accept()`

从连接队列中取出一个连接并返回新的 connected fd。blocking 模式下没有连接时会阻塞。

#### `recv()`

必须区分三种结果：

```text
> 0  收到的字节数
= 0  peer 已有序关闭发送方向
< 0  出错，检查 errno
```

一次 `recv()` 返回少于 buffer 大小是正常行为，不能把它当作一条完整消息的边界。

#### `send()`

成功返回本次实际写入 socket send buffer 的字节数。即便是 blocking socket，也不能把
“请求发送 N bytes”理解为“一定返回 N”；必须用循环处理 partial write。

发送到已关闭连接时需要考虑 `SIGPIPE`。Linux 下可使用 `MSG_NOSIGNAL`，或者统一忽略
`SIGPIPE` 并检查错误。

### 3.5 `errno` 与可恢复错误

系统调用失败后立即读取 `errno`。本阶段至少认识：

- `EINTR`: 被 signal 中断，可以按调用语义重试。
- `EADDRINUSE`: 端口冲突。
- `ECONNRESET`: peer 异常重置连接。
- `EPIPE`: 向已关闭连接写入。

不要只输出“socket failed”；错误信息至少包含 operation 和 `strerror(errno)`。

## 4. C++ 方面需要学习的知识

### 4.1 RAII

创建一个只拥有一个 fd 的 move-only wrapper：

```text
UniqueFd
  - constructor(fd)
  - destructor -> close(fd)
  - deleted copy constructor/assignment
  - move constructor/assignment
  - get()
  - valid()
  - release()/reset()
```

这一练习比在每个错误分支手写 `close()` 更有价值，也是后续 `Socket` 类的基础。

### 4.2 Ownership

需要明确：

- listening fd 由 Server 长期拥有。
- accepted fd 只在一次 client session 中拥有。
- helper function 接收 fd 时，是借用还是接管 ownership？
- 不允许两个对象同时认为自己拥有同一个 fd。

### 4.3 Buffer 与 binary-safe

首版使用固定大小的：

```cpp
std::array<std::byte, 4096>
```

或：

```cpp
std::array<char, 4096>
```

网络数据不是以 `'\0'` 结尾的 C string。必须使用 `recv()` 返回的长度，不能调用
`strlen()` 决定发送长度。

## 5. 推荐实现拆分

本阶段不要直接创建完整的 `EventLoop`、`Channel` 和 `TcpServer`。建议只实现：

```text
src/base/unique_fd.h
src/base/unique_fd.cc
src/net/blocking_echo_server.h
src/net/blocking_echo_server.cc
src/main.cc
tests/unit/unique_fd_test.cc
tests/integration/blocking_echo_test.*
docs/tcp-notes.md
```

### `BlockingEchoServer` 的最小接口

```cpp
struct BlockingEchoServerConfig {
    std::string bind_address;
    std::uint16_t port;
    int backlog;
};

class BlockingEchoServer {
public:
    explicit BlockingEchoServer(BlockingEchoServerConfig config);
    int run();
};
```

配置对象应保持 immutable。网络错误可以返回结构化状态或抛出带上下文的异常，但整个
项目应选择一种一致策略。

## 6. 推荐编码顺序

### Step 1: `UniqueFd`

先用普通文件或 `pipe()` 测试 destructor 和 move semantics，不接触网络。

验收：

- 离开 scope 后 fd 被关闭。
- move 后原对象不再拥有 fd。
- `reset()` 不会发生 double close。

### Step 2: 创建 listening socket

实现：

1. `socket(AF_INET, SOCK_STREAM, 0)`
2. `setsockopt(SO_REUSEADDR)`
3. 构造 `sockaddr_in`
4. `bind()`
5. `listen()`

验收：

```bash
ss -ltnp | grep 6380
```

可以看到进程监听 `127.0.0.1:6380`。

### Step 3: 接受一个客户端

调用 `accept()`，打印 peer IP、port 和 accepted fd。

验收：

```bash
nc 127.0.0.1 6380
```

连接建立后 Server 从 `accept()` 返回。

### Step 4: Echo loop

对一个 connected fd 重复：

1. `recv()`
2. 如果返回 0，结束 session。
3. 如果返回 `-1` 且 `errno == EINTR`，重试。
4. 对收到的 bytes 执行 `sendAll()`。

### Step 5: 返回 accept loop

一个客户端退出后关闭 connected fd，Server 回到 `accept()` 等待下一个客户端。

注意：这仍然是串行 Server。一个客户端长时间不发送数据时，其他客户端无法得到服务。
这是 blocking 模型的预期限制，也是下一阶段引出 non-blocking + epoll 的实验依据。

## 7. 必须编写的测试

### Unit Test

- `UniqueFd` closes on destruction。
- `UniqueFd` move transfer。
- `UniqueFd::reset()`。

### Integration Test

- 连接到 `127.0.0.1:6380`。
- 发送 `"hello"`，收到相同 5 bytes。
- 发送包含 `'\0'` 的 binary payload，响应完全一致。
- 发送大于单次 buffer 的 payload。
- Client 关闭后，第二个 Client 仍可连接。

### 手工观察

```bash
strace -f -e trace=network,read,write,close ./build/debug/src/mini_redis_server
```

观察：

- listening fd 和 accepted fd 是否不同。
- `accept()` 和 `recv()` 分别在哪里阻塞。
- client 退出后 `recv()` 返回什么。

## 8. 参考资料与阅读顺序

### 第一层：系统调用契约

先读 Linux man-pages：

- [socket(2)](https://man7.org/linux/man-pages/man2/socket.2.html)
- [bind(2)](https://man7.org/linux/man-pages/man2/bind.2.html)
- [listen(2)](https://man7.org/linux/man-pages/man2/listen.2.html)
- [accept(2)](https://man7.org/linux/man-pages/man2/accept.2.html)
- [recv(2)](https://man7.org/linux/man-pages/man2/recv.2.html)
- [send(2)](https://man7.org/linux/man-pages/man2/send.2.html)
- [tcp(7)](https://man7.org/linux/man-pages/man7/tcp.7.html)

每页只需先读 SYNOPSIS、DESCRIPTION、RETURN VALUE 和常见 ERRORS。

### 第二层：Redis 的 socket helper

阅读 [Redis 7.2 `src/anet.c`](https://github.com/redis/redis/blob/7.2/src/anet.c)：

- `anetCreateSocket`
- `anetSetReuseAddr`
- `anetListen`
- `_anetTcpServer`
- `anetGenericAccept`
- `anetTcpAccept`

阅读目标不是复制代码，而是回答：

1. Redis 在哪些失败路径关闭 fd？
2. 为什么 `SO_REUSEADDR` 在创建 socket 后立即设置？
3. 为什么 `accept()` 遇到 `EINTR` 会重试？
4. Redis 后续为什么使用 `accept4(SOCK_NONBLOCK | SOCK_CLOEXEC)`？

`anetNonBlock` 留到下一阶段精读。

### 第三层：muduo 的最终抽象

阅读：

- [muduo Echo example](https://github.com/chenshuo/muduo/blob/master/examples/simple/echo/echo.cc)
- muduo `TcpServer`
- muduo `TcpConnection`
- muduo `Buffer`

当前只观察最终 API：

```text
connection callback
message callback
conn->send(message)
```

等你亲自处理过 accept、recv、partial write 后，才会理解这些抽象替你隐藏了什么。

### 后续再读

- TinyWebServer `webserver.cpp`
- markparticle/WebServer `webserver.cpp`

这两个项目同时包含 epoll、timer、thread pool 和 HTTP。应在 blocking Server 完成后，
进入 Week 1 时再读它们的 listen fd、accept 和 event loop 部分。

## 9. 不应该照抄的内容

- 不要把 Redis `anet.c` 整个翻译成 C++。
- 不要在第一版加入 epoll、thread pool、timer。
- 不要使用全局 listening fd。
- 不要用 `assert()` 处理端口占用等运行时错误。
- 不要假设一次 `send()` 会发送全部数据。
- 不要把 TCP 数据当作以 `'\0'` 结尾的字符串。
- 不要为了“跨平台”同时实现 Winsock 和 Linux socket。

## 10. 完成标准

只有满足以下条件才进入 non-blocking 阶段：

- Debug 和 Release 构建通过。
- unit 和 integration tests 通过。
- ASan/UBSan 无错误。
- Valgrind 无确定性 fd/memory leak。
- 可使用 `nc` 完成多次串行连接。
- binary payload 和大 payload 可被完整 echo。
- 能画出 listening fd、connected fd 与 kernel socket 的关系。
- 能解释 blocking Server 为什么一次只能服务一个活跃客户端。
- `docs/tcp-notes.md` 记录系统调用、返回值、错误和 `strace` 观察。

建议提交：

```text
feat(net): add blocking echo server
```

完成该 milestone 后，再进入 non-blocking socket、`fcntl(O_NONBLOCK)`、EAGAIN 和 epoll。
