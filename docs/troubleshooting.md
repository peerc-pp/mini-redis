# Mini-Redis Troubleshooting Log

本文档记录项目开发过程中实际遇到的问题、根因、解决办法和验证方式。遇到新问题时，
优先在这里追加记录，避免重复排查。

## 记录格式

每条记录尽量包含：

1. 现象和错误信息。
2. 根因。
3. 最终采用的解决办法。
4. 验证方法。
5. 可以复用的经验。

## 问题索引

| ID | 日期 | 模块 | 问题 | 状态 |
| --- | --- | --- | --- | --- |
| NET-001 | 2026-07-23 | Channel | `channel` 和 `Channel` 命名不一致 | 已解决 |
| NET-002 | 2026-07-23 | EventLoop | 忽略 `[[nodiscard]]` 返回值导致严格编译失败 | 已解决 |
| NET-003 | 2026-07-24 | Reactor lifecycle | callback 删除连接导致 use-after-free | 已解决 |
| NET-004 | 2026-07-24 | TcpConnection | non-blocking partial write 和 `EAGAIN` | 已解决 |
| NET-005 | 2026-07-24 | TcpConnection | 慢客户端可能造成 buffer 无界增长 | 已解决 |
| TOOL-001 | 2026-07-23 | PowerShell/WSL | sanitizer 参数中的逗号被 PowerShell 解析 | 已解决 |
| TOOL-002 | 2026-07-24 | PowerShell/WSL | Bash 变量被 PowerShell 提前展开 | 已解决 |
| TOOL-003 | 2026-07-24 | Build | WSL 中缺少 CMake 和可执行的 `rg` | 已绕过 |

## NET-001：`channel` 和 `Channel` 命名不一致

### 现象

头文件声明的是小写类名：

```cpp
class channel;
```

实现文件却定义：

```cpp
channel::Channel(int fd);
```

编译器无法把构造函数定义与类声明对应起来。

### 原因

C++ 区分大小写。类名、构造函数名和作用域限定符必须完全一致。项目约定类名使用
PascalCase，因此应统一使用 `Channel`。

### 解决办法

统一声明和实现：

```cpp
class Channel final {
 public:
  explicit Channel(int fd) noexcept;
};

Channel::Channel(int fd) noexcept : fd_(fd) {}
```

### 验证

使用严格警告编译：

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror ...
```

### 经验

重命名 C++ 类型时需要同时检查：

- class 声明。
- constructor 和 destructor。
- 成员函数的作用域限定符。
- include 和使用方。

## NET-002：忽略 `[[nodiscard]]` 返回值

### 现象

`EventLoop::loop()` 中直接调用：

```cpp
loop_once(1000);
```

严格编译报错：

```text
error: ignoring return value of 'EventLoop::loop_once(int)',
declared with attribute 'nodiscard'
```

### 原因

`loop_once()` 的返回值表示：

- 大于 `0`：本轮就绪事件数量。
- 等于 `0`：timeout。
- 小于 `0`：`epoll_wait()` 失败。

忽略返回值会遗漏错误处理，而 `-Werror` 会把该 warning 提升为 error。

### 解决办法

读取返回值，并区分 `EINTR` 和其他错误：

```cpp
const int ready_count = loop_once(1000);
if (ready_count < 0 && errno != EINTR) {
  running_ = false;
}
```

`EINTR` 表示 syscall 被 signal 中断，可以继续下一轮；其他错误则结束循环。

### 经验

`[[nodiscard]]` 不是装饰。它通常表示调用方必须处理错误、状态或 ownership 结果。

## NET-003：callback 删除连接导致 use-after-free

### 现象

普通测试可以通过，但 ASan 在 half-close 场景报告：

```text
AddressSanitizer: heap-use-after-free
READ in mini_redis::Channel::handle_event()
```

调用链大致为：

```text
Channel::handle_event()
  -> read callback
  -> TcpConnection::handle_read()
  -> close callback
  -> TcpServer 从 connections_ 删除 shared_ptr
  -> TcpConnection 和内部 Channel 被销毁
  -> Channel::handle_event() 继续访问自身
```

### 原因

`EventLoop` 只保存非 owning 的 `Channel*`。`TcpServer` 删除最后一个
`shared_ptr<TcpConnection>` 后，包含在连接对象中的 `Channel` 会立即析构，但当前
event dispatch 尚未结束。

仅仅在 `TcpConnection::handle_read()` 内保存一个 `shared_ptr` 不够，因为该 guard 会在
read callback 返回时释放，而 `Channel::handle_event()` 仍可能继续处理 write event。

### 解决办法

采用 Reactor 常见的 `Channel::tie()` 设计：

```cpp
void Channel::tie(const std::shared_ptr<void>& owner) noexcept {
  owner_ = owner;
  tied_ = true;
}
```

dispatch 开始时锁定 `weak_ptr`：

```cpp
std::shared_ptr<void> owner_guard;
if (tied_) {
  owner_guard = owner_.lock();
  if (!owner_guard) {
    return;
  }
}
```

`TcpConnection::start()` 注册 Channel 前执行：

```cpp
channel_.tie(shared_from_this());
```

这样 owner 至少存活到整次 `Channel::handle_event()` 返回。

### 验证

- ASan/UBSan 测试通过。
- 100-client echo 测试通过。
- client 发送数据后执行 `shutdown(SHUT_WR)` 的 half-close 测试通过。

### 经验

callback 可能修改甚至销毁 callback 所属对象。Reactor 中需要明确区分：

- `EventLoop` 对 `Channel` 的非 owning 引用。
- `TcpServer` 对 `TcpConnection` 的 owning 引用。
- event dispatch 期间的临时 lifetime guard。

## NET-004：non-blocking partial write

### 现象

`send()` 即使没有报错，也可能只写入部分数据。慢客户端不读取数据时，socket send
buffer 会写满，随后返回：

```text
-1, errno = EAGAIN
```

把一次 `send()` 成功误认为整个 payload 已发送，会导致响应被截断。

### 原因

TCP 是 byte stream，non-blocking syscall 只保证报告本次实际处理的字节数，不保证一次
处理完整 buffer。

### 解决办法

1. 立即发送能够写入的部分。
2. 将剩余部分追加到 output `Buffer`。
3. 启用 `EPOLLOUT`。
4. write callback 中继续发送，直到 buffer 为空或再次遇到 `EAGAIN`。
5. buffer 为空后关闭 `EPOLLOUT`，避免 busy loop。

核心判断：

```cpp
if (result > 0) {
  output_buffer_.retrieve(static_cast<std::size_t>(result));
} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
  return;
}
```

### 验证

测试通过 `setsockopt(SO_SNDBUF)` 强制缩小 send buffer，再发送 256 KiB payload：

- `send()` 后存在 pending output。
- `EPOLLOUT` 触发后 output buffer 被完整排空。
- client 收到的内容与原始 payload 完全相同。

## NET-005：慢客户端造成 buffer 无界增长

### 现象

如果 client 持续发送但应用不消费 input buffer，或者 server 持续发送但 client 不读取，
进程内存可能持续增长。

### 原因

non-blocking 避免了线程阻塞，但不会自动提供 backpressure。应用层仍需要定义可接受的
pending data 上限。

### 解决办法

为每个连接配置：

```cpp
struct TcpConnectionOptions {
  std::size_t initial_buffer_size;
  std::size_t max_input_buffer_size;
  std::size_t max_output_buffer_size;
};
```

- input 超过上限：关闭异常连接。
- output 请求超过上限：`send()` 返回 `false`，由上层决定关闭连接或停止生产数据。
- Echo server 测试 callback 在 `send()` 失败时调用 `force_close()`。

### 经验

non-blocking、backpressure 和 memory limit 是三个不同问题，不能只实现
`O_NONBLOCK` 就认为慢客户端问题已经解决。

## TOOL-001：sanitizer 参数被 PowerShell 解析

### 现象

从 PowerShell 调用 WSL 编译时：

```powershell
wsl ... g++ -fsanitize=address,undefined ...
```

PowerShell 报 `ParserError`，命令并未进入 `g++`。

### 原因

PowerShell 将未引用参数中的逗号当作语法分隔符。

### 解决办法

给整个参数加引号：

```powershell
wsl ... g++ '-fsanitize=address,undefined' ...
```

在 Bash script 的 array 中则可以直接写：

```bash
SANITIZER_FLAGS=(-fsanitize=address,undefined)
```

### 经验

区分错误来自 host shell、WSL 还是 compiler。看到 compiler 输出前，不能判断为 C++ 编译
错误。

## TOOL-002：Bash 变量被 PowerShell 提前展开

### 现象

通过 PowerShell 执行包含 Bash 循环的命令：

```bash
for file in ...; do
  nl -ba "$file"
done
```

传入 WSL 后 `$file` 变成空字符串。

### 原因

命令字符串首先经过 PowerShell。PowerShell 在 Bash 执行前展开了 `$file`。

### 解决办法

- 简单场景使用明确文件路径。
- 或正确转义 `$`。
- 复杂脚本写入 `.sh` 文件后直接由 Bash 执行，减少多层 quoting。

### 经验

跨 shell 命令需要考虑两次解析。临时命令越复杂，越适合放进独立 test script。

## TOOL-003：WSL 缺少 CMake，`rg` 指向不可执行路径

### 现象

- `cmake`、`ninja` 和 `clang-format` 在当前 WSL 环境中不可用。
- `rg` 被解析到 Windows Codex App 目录，WSL 执行时报 `Permission denied`。

### 解决办法

- 文件查找暂时使用 `find` 和 `grep`。
- 使用严格的直接编译命令验证 C++：

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror ...
```

- 新增统一测试入口：

```bash
./scripts/test-network.sh
```

该脚本会运行普通测试以及 ASan/UBSan 测试，并自动清理临时 binary。

### 后续建议

安装项目要求的 CMake、Ninja、clang-format 和 ripgrep 后，还应执行：

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

直接 `g++` 测试可以验证源码，但不能永久替代 CMake target 和 CTest 配置验证。

## 新问题模板

```markdown
## ID：问题标题

### 现象

### 原因

### 解决办法

### 验证

### 经验
```
