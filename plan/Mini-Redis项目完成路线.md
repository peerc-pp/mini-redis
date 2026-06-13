# Mini-Redis 项目完成路线

## 1. 项目最终目标

实现一个运行在 Linux 上、使用 C++17 编写的单机内存 KV Server：

- 基于 non-blocking socket、epoll 和单线程 Reactor。
- 支持 RESP2，能够被 `redis-cli` 直接连接。
- 支持 String、List、Hash、ZSet 的核心命令。
- 支持 TTL、AOF 恢复和自定义 snapshot。
- 有 unit、integration、differential、fuzz、sanitizer 和 benchmark。
- 能用设计文档、实验数据和故障测试解释系统行为。

项目完成的标志不是“命令数量足够多”，而是从 TCP 字节流到数据落盘的完整链路可运行、可测试、可解释、可复现。

## 2. 先修正原规划中的几个关键点

1. 先做 vertical slice：尽快让 `redis-cli -> epoll -> RESP -> PING/SET/GET -> RESP` 跑通。
2. 第一遍使用标准容器，第二遍替换为自研渐进式 rehash 和 skiplist。
3. 主线只实现 RESP2；RESP3 不进入 Core Release。
4. C++ 主线优先实现 binary-safe `Buffer`，SDS 作为可选学习实验。
5. 不再把 `ziplist` 当主目标；现代 Redis 应阅读 `listpack`。
6. RESP 压测使用 `redis-benchmark`、`memtier_benchmark` 和自定义 pipeline client，不使用 HTTP 专用的 `wrk/webbench`。
7. 先做 AOF，再做 snapshot；不追求兼容 Redis RDB 文件格式。
8. Raft 必须等单机版达到 Definition of Done 后再启动。

## 3. Core Release 范围

### 必做功能

| 模块 | 首版范围 |
|---|---|
| Network | IPv4 TCP、non-blocking、epoll LT、连接关闭、读写缓冲、backpressure |
| Protocol | RESP2 Array/Bulk String 请求；全部 RESP2 响应类型；半包、粘包、pipeline |
| String | PING、ECHO、SET、GET、DEL、EXISTS、INCR、DECR |
| TTL | EXPIRE、PEXPIRE、TTL、PTTL、PERSIST |
| List | LPUSH、RPUSH、LPOP、RPOP、LLEN、LRANGE |
| Hash | HSET、HGET、HDEL、HEXISTS、HLEN、HGETALL |
| ZSet | ZADD、ZREM、ZSCORE、ZRANK、ZRANGE |
| Persistence | AOF append/replay、fsync policy、AOF rewrite；自定义 snapshot |
| Operations | CONFIG file、INFO、日志、graceful shutdown、统计指标 |

### 暂不实现

- RESP3、TLS、ACL、Lua、事务、Pub/Sub。
- 主从复制、Sentinel、Redis Cluster。
- Redis RDB 文件格式兼容。
- 多线程并发执行命令。
- 完整 Redis 命令语义和所有边界选项。

这些内容写入 `docs/non-goals.md`，防止开发过程中无限扩张。

## 4. 推荐架构

```text
redis-cli
   |
TcpServer -> TcpConnection -> InputBuffer
   |                              |
EventLoop <- Channel <- epoll   RespParser
                                  |
                              CommandRegistry
                                  |
                     Database -> Value/Object
                        |            |
                  ExpireIndex   Dict/List/SkipList
                        |
                 AOF / Snapshot
```

### 关键设计决定

- 首版使用一个 EventLoop、一个线程，数据库状态只由该线程修改。
- epoll 先使用 LT；完成正确性和 benchmark 后，再用 ET 做对照实验。
- `TcpConnection` 同时拥有 input/output buffer，不能假设一次 `write` 会发完。
- output buffer 设置 high-water mark，防止慢客户端无限占用内存。
- `RespParser` 是增量状态机：输入不足时返回 `NeedMoreData`，不能丢失状态。
- `Database` 只接受已解析命令，不依赖 socket；以后可直接复用为 Raft state machine。
- TTL 使用可注入的 clock，测试时不依赖真实 sleep。
- mutation 成功后生成规范化 AOF record；恢复过程复用 RESP parser 和 command executor。

## 5. 推荐目录结构

```text
mini-redis/
├── CMakeLists.txt
├── cmake/
├── config/
│   └── mini-redis.conf
├── docs/
│   ├── architecture.md
│   ├── protocol.md
│   ├── persistence.md
│   ├── benchmark.md
│   └── non-goals.md
├── src/
│   ├── base/          # logging, status, clock, non-copyable helpers
│   ├── net/           # Socket, Channel, Poller, EventLoop, Buffer...
│   ├── protocol/      # RespValue, RespParser, RespEncoder
│   ├── server/        # Session, Command, CommandRegistry, RedisServer
│   ├── storage/       # Database, Value, Dict, List, SkipList, ExpireIndex
│   └── persistence/   # AofWriter, AofRewriter, Snapshot
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── differential/
│   └── fuzz/
├── bench/
└── scripts/
```

依照 small-file principle，每个 `.h/.cc` 尽量保持在 200-400 行；超过 400 行时按职责拆分。

## 6. 12 周学习与实现路线

以下按每周 15-20 小时设计。全职投入可压缩为 6-8 周，但不要删除测试门禁。

### Week 0：工程初始化与知识基线

**学习**
- Linux process、file descriptor、socket 生命周期。
- CMake target、RAII、move semantics、smart pointer、`std::variant`。
- TCP 三次握手、四次挥手、TIME_WAIT、半关闭。

**实现**
- 初始化 Git，建立 `main/develop`。
- 建立 CMake、GoogleTest、clang-format、clang-tidy。
- 配置 Debug、Release、ASan/UBSan 四种构建。
- 写 blocking echo server，随后写 matching client。

**验收门禁**
- Echo server 可处理正常断开和 Ctrl+C。
- `ctest` 可运行；ASan/UBSan 构建可运行。
- 产出 `docs/tcp-notes.md`，画出一次连接的系统调用顺序。

### Week 1：non-blocking + epoll Reactor

**学习**
- `fcntl(O_NONBLOCK)`、EAGAIN/EWOULDBLOCK、EINTR。
- epoll LT/ET、one-shot、fd readiness。
- Reactor 的 EventLoop、Channel、Poller 职责。

**实现顺序**
1. `Socket`：fd ownership、bind/listen/accept、socket options。
2. `Poller`：封装 epoll create/ctl/wait。
3. `Channel`：fd、interest events、callbacks。
4. `EventLoop`：poll -> dispatch 的循环。
5. `Acceptor`、`TcpConnection`、`TcpServer`。
6. input/output `Buffer` 和 partial write。

**重点测试**
- 100 个客户端同时 echo。
- 客户端逐字节发送、一次发送大包、读到一半断开。
- 慢客户端不读取响应时，服务端不能无限增长或 busy loop。

**验收门禁**
- `nc` 和自定义 client 均可连接。
- Valgrind 或 ASan 无 leak/use-after-free。
- 能解释 LT/ET、Reactor/Proactor、为什么非阻塞 fd 必须循环读到 EAGAIN。

### Week 2：RESP2 与第一个端到端版本

**学习**
- RESP2 的 Simple String、Error、Integer、Bulk String、Array、Null。
- TCP 是 byte stream，协议需要 framing。
- incremental parser、finite-state machine、parser limits。

**实现**
- `RespValue` 使用 tagged type 或 `std::variant`。
- `RespParser::parse(Buffer&)` 返回 Complete/NeedMoreData/Error。
- `RespEncoder` 生成合法响应。
- `CommandRegistry` 支持注册、参数校验和 dispatch。
- 完成 PING、ECHO、SET、GET、DEL、EXISTS。

**重点测试**
- 将同一请求在每个 byte boundary 拆成两段。
- 多条命令粘在一个 packet 中。
- pipeline 1000 条命令。
- malformed length、负长度、超长 bulk string、嵌套深度限制。

**验收门禁**
- `redis-cli -p <port> PING` 返回 `PONG`。
- redis-cli 可执行 SET/GET/DEL。
- parser unit tests 覆盖所有边界类型。

### Week 3：数据库对象模型和命令语义

**学习**
- Redis object/type model。
- integer overflow、binary-safe string、错误语义。
- command metadata：arity、read/write、key positions。

**实现**
- 首版 `Database` 使用 `std::unordered_map<std::string, Value>`。
- `Value` 支持 String/List/Hash/ZSet 的 variant。
- 完成 String、List、Hash 的核心命令。
- 命令执行返回结构化 `CommandResult`，不要在 storage 中拼 RESP。

**测试**
- wrong type、missing key、empty collection 自动删除。
- INCR 的符号、边界和 overflow。
- 以真实 Redis 作为 oracle 做 subset differential test。

**验收门禁**
- 至少 20 个核心命令可用。
- storage unit tests 不启动网络即可运行。
- 相同命令序列在 Mini-Redis 与 Redis 上得到相同响应。

### Week 4：渐进式 rehash

**学习**
- load factor、bucket、collision、rehash latency。
- Redis `dict.c` 中双表和 `rehashidx`。
- 渐进迁移、查找双表、迭代期间 rehash 的约束。

**实现**
- `HashTable<K,V>` 拥有 old/new 两张 table。
- 每次读写搬迁固定数量 bucket。
- 支持扩容和缩容；提供 rehash progress 统计。
- 用自研 HashTable 替换顶层 `std::unordered_map`。

**测试与实验**
- 随机操作与 `std::unordered_map` 做 property test。
- rehash 过程中插入、删除、更新、查找。
- 对比一次性 rehash 与渐进式 rehash 的 p50/p99 latency。

**验收门禁**
- 功能 differential test 全部继续通过。
- 报告中展示 latency spike 的量化差异。

### Week 5：ZSet 与 skiplist

**学习**
- skiplist 概率层高、期望复杂度、range query。
- 为什么 ZSet 通常需要 member->score dict + score-ordered skiplist。
- NaN、重复 score、lexicographical tie-break。

**实现**
- SkipList insert/find/delete/rank/range。
- ZSet 同时维护 dict 和 skiplist。
- 完成 ZADD、ZREM、ZSCORE、ZRANK、ZRANGE。

**测试**
- 固定 random seed，验证可复现。
- 与 `std::map`/排序 vector 做随机 differential test。
- 两个索引在任何失败路径后仍保持一致。

**验收门禁**
- 10 万次随机 ZSet 操作无 invariant failure。
- 能解释 skiplist 与 B-Tree、red-black tree 的取舍。

### Week 6：TTL 与时间驱动任务

**学习**
- lazy expiration、active expiration、timer heap/time wheel。
- monotonic clock 与 wall clock 的区别。
- Redis 过期语义和访问时删除。

**实现**
- 主 dict + expire dict，deadline 使用毫秒。
- lookup 前 lazy expiration。
- EventLoop 周期任务执行有时间预算的抽样清理。
- EXPIRE、PEXPIRE、TTL、PTTL、PERSIST。

**测试**
- 使用 fake clock，覆盖边界时刻，不使用 `sleep`。
- 大量过期 key 时单次清理不能长时间阻塞 loop。
- TTL 更新、删除、覆盖 SET 后的语义。

**验收门禁**
- 可量化 active expiration 每轮预算和遗留 key 数。
- p99 latency 不因一次全量过期扫描出现明显尖峰。

### Week 7：AOF 与崩溃恢复

**学习**
- append-only log、fsync、page cache、write ordering。
- always/everysec/no 的 durability/performance trade-off。
- torn write、partial tail、idempotence。

**实现**
- mutation 成功后追加规范化 RESP2 command。
- 三种 fsync policy。
- 启动时 replay AOF；允许忽略或截断不完整尾部。
- 记录 replay 时间和 command count。

**故障测试**
- 连续写入时 `kill -9`，重启后检查已持久化数据。
- 人工截断 AOF 最后若干 bytes。
- 磁盘写失败时停止接受 mutation 或进入明确错误状态。

**验收门禁**
- 形成 durability matrix：每种策略最多可能丢失多少数据。
- AOF on/off benchmark 可复现。

### Week 8：AOF rewrite 与 snapshot

**学习**
- log compaction、fork+COW、atomic rename、checksum、format version。
- snapshot 与 WAL/AOF 的恢复顺序。

**实现**
- AOF rewrite：从当前状态生成最小恢复命令，写临时文件后 atomic rename。
- 首版 snapshot 使用自定义 binary format：magic、version、records、checksum。
- 先实现阻塞 snapshot，再把 fork+COW 作为优化实验。

**测试**
- rewrite 期间持续 mutation，不能丢失增量。
- snapshot corruption、未知版本、checksum mismatch。
- snapshot + AOF tail 组合恢复。

**验收门禁**
- rewrite 前后逻辑状态完全相同。
- 记录 fork 前后 RSS 和 COW 开销，不虚构“零拷贝”结论。

### Week 9：可观测性与工程化

**实现**
- 配置：port、bind address、max clients、max request size、fsync policy、data dir。
- INFO：uptime、connections、commands processed、key count、expired keys、memory estimate、AOF status。
- 日志分级和 graceful shutdown。
- fd limit、连接上限和错误响应。

**验收门禁**
- 错误配置启动失败并给出清晰原因。
- SIGTERM 后停止 accept、flush 持久化、关闭连接。
- 源码无 `print()` debug、bare catch、hardcoded path。

### Week 10：系统验证

**测试矩阵**
- Unit：parser、encoder、buffer、dict、skiplist、TTL、AOF format。
- Integration：真实 socket + redis-cli。
- Differential：同一随机命令序列对比 Redis。
- Fuzz：RESP parser 和 snapshot decoder。
- Sanitizer：ASan、UBSan；可选 TSan。
- Static analysis：clang-tidy。
- Fault injection：kill、truncated file、slow client、connection reset。

**验收门禁**
- Debug/Release/Sanitizer 构建全部通过。
- 测试命令和环境写入 README，能在新机器复现。
- 修复 flaky tests；不通过增加重试掩盖。

### Week 11：性能实验与优化

**基线工具**
- `redis-benchmark`：固定 payload、clients、requests、pipeline。
- `memtier_benchmark`：读写比例、key pattern、pipeline、latency percentile。
- 自定义 benchmark：rehash、skiplist、parser、AOF。

**实验原则**
- Release build、固定 CPU governor/环境、预热、重复至少 5 次。
- 同时报告 throughput、p50、p95、p99，而不只报告 QPS。
- 对比项每次只改变一个变量。
- 记录 CPU、RSS、context switch、系统调用和数据集大小。

**建议实验**
1. epoll LT vs ET。
2. pipeline 1/8/32。
3. AOF off/everysec/always。
4. 标准容器 vs 自研 dict。
5. active expiration 不同时间预算。
6. Mini-Redis vs Redis：只用于理解差距，不做不公平宣传。

### Week 12：交付与面试材料

**交付物**
- README：目标、架构、构建、运行、命令、测试、benchmark。
- `docs/architecture.md`：一次 GET/SET 的完整调用链。
- `docs/persistence.md`：crash model 和 durability matrix。
- `docs/benchmark.md`：环境、命令、原始结果、图表、结论与局限。
- Demo script：启动、redis-cli、TTL、重启恢复、benchmark。
- 一页项目复盘：最难问题、失败方案、性能瓶颈、下一步。

**最终门禁**
- 新环境 15 分钟内可完成 build -> test -> run -> redis-cli。
- 所有简历数字均能由脚本和原始数据复现。
- 能在 5 分钟内讲清 request path，在 15 分钟内讲清两个技术难点。

## 7. 开源项目如何使用

| 项目 | 重点阅读 | 不要照搬 |
|---|---|---|
| [Redis 7.2](https://github.com/redis/redis/tree/7.2) | `ae*`、`networking.c`、`dict.c`、`t_zset.c`、`listpack.c`、`aof.c`、`rdb.c` | 全部命令、兼容包袱、宏和全局状态 |
| [muduo](https://github.com/chenshuo/muduo) | EventLoop、Channel、Poller、TcpConnection、Buffer、TimerQueue | 多线程 EventLoop pool 和完整库规模 |
| [TinyWebServer](https://github.com/qinguoyi/TinyWebServer) | epoll、LT/ET、timer、连接生命周期 | HTTP parser、MySQL、业务层和项目特有全局设计 |
| [WebServer](https://github.com/markparticle/WebServer) | RAII、Buffer、timer、现代 C++ 目录组织 | HTTP 和数据库连接池 |
| [Build Your Own Redis](https://build-your-own.org/redis/) | 从 byte stream 到 KV server 的增量练习 | 逐行复制教程答案 |
| [lhfeiie/mini-redis](https://github.com/lhfeiie/mini-redis) | 学习阶段划分 | 将其当成完整实现；公开进度并不完整 |
| [ascendho/mini-redis](https://github.com/ascendho/mini-redis) | 注释、socket 与数据结构练习 | 未完成的测试和 benchmark 设计 |
| [KVstorageBaseRaft-cpp](https://github.com/wfz050207/KVstorageBaseRaft-cpp) | Raft/KV/RPC 的模块边界 | 将其作为 Raft 正确性的唯一依据 |

### 推荐阅读纪律

1. 先写自己的接口、invariant 和测试。
2. 再阅读一个权威实现和一个教学实现。
3. 只记录设计差异，不复制大段代码。
4. 每次借鉴在 `docs/design-sources.md` 记录来源、采用内容和未采用原因。
5. Redis 源码固定在 7.2 branch/tag 阅读，避免版本变化造成路径和语义漂移。

## 8. 每周固定工作节奏

- Day 1：学习本周概念，写 1-2 页 notes 和接口草图。
- Day 2-3：先写测试，再实现最小功能。
- Day 4：读权威源码，对比并修正设计。
- Day 5：integration + sanitizer + code review。
- Day 6：benchmark/故障实验，记录原始结果。
- Day 7：整理文档、关闭本周 milestone、确定下周风险。

每周只允许一个主里程碑。未通过本周验收门禁时，不提前进入下一个复杂模块。

## 9. 前 7 天可以直接执行的任务

### Day 1
- 初始化 Git、CMake、GoogleTest、clang-format。
- 编译一个 `mini_redis_server` 和一个 unit test。
- 写 `docs/non-goals.md`。

### Day 2
- 完成 blocking echo server。
- 用 `strace` 观察 socket/bind/listen/accept/read/write/close。

### Day 3
- 改为 non-blocking socket。
- 正确处理 EINTR、EAGAIN、partial read/write。

### Day 4
- 封装 Poller 和 Channel，完成 epoll LT echo。

### Day 5
- 完成 EventLoop、Acceptor、TcpConnection 的最小版本。

### Day 6
- 添加 Buffer，测试大报文、逐字节发送和慢客户端。

### Day 7
- 跑 ASan/UBSan、整理网络层图、做第一次 milestone review。

首周不要实现 Redis 命令。首要目标是把连接生命周期和 byte stream 处理做扎实。

## 10. Raft 进阶路线

只有 Core Release 全部通过后才进入 Raft，预计再增加 4-6 周。

### 前置重构
- 将写命令转换为 deterministic `Command`。
- `Database::apply(Command)` 不读取系统时间、不做网络 IO。
- log index/term 和 state machine apply index 分离。

### 实现顺序
1. 单节点 Raft state machine。
2. election timer、RequestVote。
3. AppendEntries heartbeat。
4. log replication、majority commit、apply。
5. conflict optimization。
6. persistent term/vote/log。
7. snapshot/install snapshot。
8. linearizable read：先实现 read-through-log，再研究 ReadIndex。

### 正确性门禁
- 3 节点 kill 1 个后可继续提交。
- 5 节点 kill 2 个后可继续提交。
- minority partition 不能提交。
- old leader 回归后不能覆盖已提交日志。
- 随机 kill/restart/partition 测试至少运行 30 分钟。

Raft 的权威来源应是 [Raft paper](https://raft.github.io/raft.pdf) 和经过广泛测试的实现；C++ 教学仓库只用于观察工程组织。

## 11. Definition of Done

满足以下全部条件，才算真正完成 Core Release：

- 所有必做命令通过 unit、integration 和 differential tests。
- RESP parser 可处理任意 packet boundary、pipeline 和 malformed input。
- ASan/UBSan 无错误，Valgrind 无确定性 leak。
- AOF 在 kill/truncate 场景下有定义明确的恢复行为。
- benchmark 有脚本、环境记录、原始数据和 percentile。
- README 能让另一台 Linux 机器完成构建和演示。
- 架构、持久化和性能选择均有文档，可解释 trade-off。
- 简历描述只使用可复现的数据，不写未经验证的 C10K、QPS 或延迟数字。

## 12. 学完后你应真正掌握的知识

### Linux 网络
- fd、non-blocking IO、epoll、Reactor、连接生命周期、backpressure。

### 协议与服务端工程
- framing、incremental parsing、pipeline、资源限制、错误隔离。

### 数据结构
- hash table、渐进式 rehash、skiplist、复合索引 invariant。

### 数据库内核
- object model、TTL、AOF/WAL、snapshot、crash recovery、durability。

### 性能工程
- benchmark 设计、tail latency、profiling、控制变量和结果复现。

### 分布式扩展
- deterministic state machine、Raft election/log replication/snapshot、故障模型。

这条路线完成后，项目价值会从“仿写 Redis 命令”提升为“独立设计并验证一个小型数据库服务器”。
