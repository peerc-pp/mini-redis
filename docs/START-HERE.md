# 从这里开始：Mini-Redis 第一课

先不要通读项目路线，也不要试图一次学完 TCP、RAII、epoll 和 Redis。

你现在唯一的目标是：

> 看懂一个 TCP Server 如何创建端口，并等待客户端连接。

今天不实现 Redis，不处理多个客户端，也不追求优雅架构。

## 一、先记住这张图

```text
你的程序                         Linux 内核

socket()   ------------------>  创建一个 socket
   |                             返回编号 3
   v
bind()     ------------------>  把编号 3 绑定到 127.0.0.1:6380
   |
   v
listen()   ------------------>  让编号 3 开始监听连接
   |
   v
accept()   ------------------>  等待客户端连接
                                 客户端到来后返回新编号 4
```

这里最重要的只有两件事：

1. `socket()` 返回一个整数，例如 `3`。这个整数叫 file descriptor，简称 fd。
2. `accept()` 会返回另一个 fd。监听端口和与客户端通信使用的不是同一个 fd。

暂时不需要研究 fd table、open file description 或 kernel object 的详细结构。

## 二、只学三个概念

### 1. 什么是 Process

源代码编译后得到 executable。运行 executable 后，操作系统创建一个 process。

你现在只需要这样理解：

```text
源代码 -> 编译 -> 可执行文件 -> 运行 -> Process
```

Process 拥有自己的内存和一张“已打开资源编号表”。

### 2. 什么是 Kernel

普通程序不能直接操作网卡。它要请求 Linux kernel 帮忙：

```text
你的 C++ 程序 -> socket() -> Linux kernel -> 创建网络资源
```

`socket()`、`bind()`、`listen()` 和 `accept()` 都是 system call。

当前只需知道：system call 是程序向 kernel 请求服务的接口。

### 3. 什么是 File Descriptor

File descriptor 是 process 用来引用一个已打开资源的整数编号。

例如：

```text
0 -> standard input
1 -> standard output
2 -> standard error
3 -> listening socket
4 -> client connection
```

因此，fd 不是网络数据，也不是 IP 地址。它只是程序用来找到内核资源的编号。

## 三、认识第一个函数：`socket()`

函数形式：

```cpp
int socket(int domain, int type, int protocol);
```

我们将使用：

```cpp
const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
```

逐个理解参数：

| 参数 | 当前含义 |
|---|---|
| `AF_INET` | 使用 IPv4 |
| `SOCK_STREAM` | 使用 TCP byte stream |
| `0` | 让系统选择默认的 TCP protocol |

返回值：

```text
>= 0  创建成功，返回 fd
-1    创建失败
```

第一天只需阅读 [socket(2)](https://man7.org/linux/man-pages/man2/socket.2.html) 的这三部分：

1. SYNOPSIS
2. 第一段 DESCRIPTION
3. RETURN VALUE

看到暂时不懂的 flag 和 error 可以跳过。

## 四、Server 的五步流程

### Step 1: `socket()`

向 kernel 申请 TCP socket，得到 listening fd 的候选者。

### Step 2: `bind()`

告诉 kernel：

> 这个 socket 要使用 `127.0.0.1:6380`。

可以把 IP 理解为地址，把 port 理解为这个地址上的房间号。

### Step 3: `listen()`

告诉 kernel：

> 从现在开始，这个 socket 用来等待客户端连接。

### Step 4: `accept()`

程序停在这里等待。当客户端连接后，`accept()` 返回一个新的 client fd。

```text
server_fd: 一直负责接待新客户端
client_fd: 负责和某一个客户端收发数据
```

### Step 5: `close()`

不再使用 fd 时通知 kernel 释放资源。

第一版程序先做到这里，不急着写 `recv()` 和 `send()`。

## 五、今天的动手实验

### 实验目标

完成一个只能监听端口、接受一个连接、然后退出的程序。

程序运行时：

```text
Listening on 127.0.0.1:6380
Waiting for a client...
Client connected
```

### 你需要写的伪代码

先不要复制完整答案，尝试将以下步骤翻译成 C++：

```text
创建 server socket
如果失败，输出错误并退出

设置服务器地址为 127.0.0.1:6380
绑定地址
开始监听

调用 accept 等待客户端
客户端连接后输出提示

关闭 client fd
关闭 server fd
```

### 如何测试

Server 运行后，在另一个 WSL terminal 执行：

```bash
nc 127.0.0.1 6380
```

如果 Server 输出 `Client connected`，今天的目标就完成了。

## 六、今天暂时不要学

以下内容都重要，但今天主动跳过：

- 三次握手的每一个 TCP state
- `EINTR`、`EPIPE`
- partial read/write
- non-blocking
- epoll
- Reactor
- RESP
- RAII 和 move semantics 的完整细节
- 动态 Buffer

不是不学，而是在代码第一次需要它们时再学。

## 七、三个检查题

如果你能回答下面三题，就可以开始写代码：

1. `socket()` 成功后返回的整数表示什么？
2. `bind()` 在 Server 启动过程中负责什么？
3. listening fd 和 `accept()` 返回的 client fd 有什么区别？

参考答案：

1. 当前 process 用来引用内核 socket 资源的 file descriptor。
2. 把 socket 绑定到本地 IP 和 port。
3. listening fd 用来接收新连接；client fd 用来与一个具体客户端通信。

## 八、后面的学习顺序

不要一次学习全部内容。按代码需求逐步推进：

```text
Lesson 1: socket/bind/listen/accept
    ↓
Lesson 2: recv/send，实现 Echo
    ↓
Lesson 3: 错误处理和 partial write
    ↓
Lesson 4: RAII，使用 UniqueFd 管理 fd
    ↓
Lesson 5: blocking Server 的局限
    ↓
Lesson 6: non-blocking + epoll
```

详细的工程要求仍保留在
[Blocking Echo Server Milestone](milestone-01-blocking-echo-server.md)，但它现在只作为
查阅手册，不要求你从头读到尾。
