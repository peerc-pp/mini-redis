# Mini-Redis：C++ 高性能内存键值数据库 — 项目方向与重点规划

> 本文档基于胡明月现有简历背景（PostgreSQL 内核开发经验）与腾讯后台开发岗位 JD 的 Gap 分析，规划新项目的学习方向、技术重点与落地节奏。

---

## 一、为什么做这个项目

### 1.1 简历 Gap 分析

| Gap 维度 | 严重程度 | 说明 |
|---|---|---|
| **网络编程**（TCP/epoll/IPC） | 🔴 最高 | 腾讯后台开发 JD 硬性要求，简历完全空白 |
| **KV 存储原理**（NoSQL/Redis） | 🟡 中等 | JD 明确列出"了解 NoSQL、Key-value 存储原理"，缺乏实践 |
| **分布式基础**（CAP/Raft/高可用） | 🟡 中等 | JD 加分项，仅有单机数据库经验，缺少分布式视角 |
| **游戏后台专项** | 🟢 可选 | 如目标锁定游戏方向，需额外补充 |

### 1.2 项目与现有简历的协同

```
EdgeGC（MVCC 垃圾回收）    → 证明：系统级 C 编程 + 数据库内核深度 + 量化实验能力
对称哈希连接（优化器/执行引擎） → 证明：代码架构改造 + 算法实现 + 引擎集成能力
Mini-Redis（本项目）        → 补上：网络编程 + KV 存储 + 分布式共识
                              → 证明：从零搭建服务器系统的能力
───────────────────────────────────────────────────────
三者组合 = 后台服务器开发的完整能力拼图
```

---

## 二、项目整体架构

```
┌─────────────────────────────────────────────┐
│              客户端（redis-cli 兼容）          │
├─────────────────────────────────────────────┤
│              RESP 协议解析层                   │
├─────────────┬───────────────┬───────────────┤
│   命令分发    │   过期 Key 管理  │   持久化引擎    │
├─────────────┴───────────────┴───────────────┤
│              存储引擎层                        │
│     Hash（渐进式 rehash）│ SkipList │ List    │
├─────────────────────────────────────────────┤
│              事件驱动网络层                     │
│     EventLoop │ Channel │ Socket │ epoll     │
├─────────────────────────────────────────────┤
│              Linux 系统层                      │
│         TCP │ 非阻塞 IO │ 线程池               │
└─────────────────────────────────────────────┘
```

### 进阶架构（加 Raft 分布式）

```
┌─────────────────────────────────────────────┐
│              Raft 客户端                      │
├─────────────────────────────────────────────┤
│              Raft 共识层                       │
│   领导者选举 │ 日志复制 │ 快照 │ 持久化         │
├─────────────────────────────────────────────┤
│              RPC 通信层（Protobuf）            │
├─────────────────────────────────────────────┤
│          KV 状态机（复用单机存储引擎）           │
└─────────────────────────────────────────────┘
```

---

## 三、分阶段技术重点

### 阶段 1：TCP 网络编程基础（最高优先级）

**目标：** 从零理解 socket 编程全流程，补上简历最大 Gap

**必须掌握的知识点：**

| 知识点 | 具体内容 | 面试高频度 |
|---|---|---|
| Socket API | `socket/bind/listen/accept/read/write/close` | ⭐⭐⭐ |
| TCP 三次握手/四次挥手 | 状态变迁、TIME_WAIT 产生与处理 | ⭐⭐⭐ |
| 非阻塞 IO | `fcntl` 设置 O_NONBLOCK，为什么需要非阻塞 | ⭐⭐⭐ |
| epoll 机制 | `epoll_create/epoll_ctl/epoll_wait`，LT vs ET 区别 | ⭐⭐⭐ |
| Reactor 模式 | 事件循环 + 回调，为什么是后台服务主流模型 | ⭐⭐⭐ |
| TCP 粘包处理 | 应用层协议如何定义消息边界 | ⭐⭐ |
| C10K 问题 | 为什么需要 IO 多路复用，select/poll/epoll 演进 | ⭐⭐ |

**代码产出：**

- [ ] 最简 TCP Echo Server（单线程阻塞 → 理解基本流程）
- [ ] 多进程版（fork + SIGCHLD 处理僵尸进程）
- [ ] 多线程版（pthread_create + detach）
- [ ] epoll 版（理解 IO 多路复用的必要性）
- [ ] 封装 EventLoop / Channel / Socket 三层抽象

**参考资源：**
- 书籍：《Linux 高性能服务器编程》（游双）第 5-8 章
- 书籍：《UNIX 网络编程》Vol.1 第 4-9 章
- 开源参考：[qinguoyi/TinyWebServer](https://github.com/qinguoyi/TinyWebServer) — 19.6k ⭐，epoll + Reactor/Proactor 双模式实现，重点看网络层

---

### 阶段 2：RESP 协议解析

**目标：** 理解应用层协议设计，解决 TCP 粘包问题

**必须掌握的知识点：**

| 知识点 | 具体内容 |
|---|---|
| RESP 协议格式 | Simple String / Error / Integer / Bulk String / Array 五种类型 |
| Inline 命令 | 空格分隔的命令解析（如 `PING`） |
| Multi Bulk 请求 | `*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n` |
| 状态机解析 | 逐字节解析，处理不完整报文（TCP 可能半包/粘包） |
| 缓冲区管理 | 输入缓冲区自动扩容，处理大 Value 场景 |

**代码产出：**

- [ ] RESP 解析器类（支持所有 5 种类型）
- [ ] 状态机驱动的请求解析（处理半包情况）
- [ ] 响应序列化（将执行结果编码为 RESP 格式返回客户端）
- [ ] 兼容 redis-cli 连接测试

**参考资源：**
- [Redis 官方协议文档](https://redis.io/docs/latest/develop/reference/protocol-spec/)
- 开源参考：[lhfeiie/mini-redis](https://github.com/lhfeiie/mini-redis) — 阶段三 RESP 解析部分

---

### 阶段 3：存储引擎实现

**目标：** 实现核心数据结构，结合已有 PG 内核经验做差异化

**必须掌握的知识点：**

| 数据结构 | 实现重点 | 和 PG 的关联（面试加分） |
|---|---|---|
| 渐进式 rehash 哈希表 | 避免全量 rehash 延迟抖动，分摊到每次操作 | 对比 PG 的 Hash 表扩容策略 |
| 跳表（SkipList） | 概率平衡、O(logN) 查找/插入/删除、范围查询 | 对比 PG 的 B-Tree 索引结构 |
| 双向链表（List） | 压缩列表优化（小数据量用 ziplist） | 对比 PG 的堆表存储 |
| SDS（Simple Dynamic String） | 二进制安全、O(1) 长度获取、预分配减少 realloc | 对比 PG 的 varlena 类型 |

**代码产出：**

- [ ] Hash 表（渐进式 rehash）
- [ ] SkipList（支持 ZRANGE/ZRANK 等范围操作）
- [ ] List（双向链表，支持 LPUSH/RPUSH/LPOP/RPOP）
- [ ] SDS 字符串实现
- [ ] 统一命令分发框架（命令注册 + 查找 + 执行）

**面试亮点准备：**
> "我在实现渐进式 rehash 时，借鉴了 PG 中 walwriter 的分摊思想——将一次大操作拆成多次小操作，避免长耗时阻塞主循环。"

---

### 阶段 4：持久化与过期机制

**目标：** 补全数据库核心功能，体现工程完整性

**必须掌握的知识点：**

| 模块 | 实现重点 | 面试高频问题 |
|---|---|---|
| AOF 持久化 | 命令追加写日志，always/everysec/no 三种刷盘策略 | AOF 重写（bgrewriteaof）原理 |
| RDB 快照 | fork 子进程 + copy-on-write 生成快照 | RDB 与 AOF 的取舍 |
| 惰性删除 | 访问时检查过期，过期则删除 | 和 PG 的 vacuum 有什么异同 |
| 定期扫描 | 周期性随机抽样删除过期 Key | 扫描频率与时长的权衡 |

**代码产出：**

- [ ] AOF 写入（三种刷盘策略可配置）
- [ ] RDB 快照生成（fork + COW）
- [ ] 过期 Key 管理（惰性删除 + 定期扫描）
- [ ] AOF 重写（去冗余命令，压缩文件）

**面试亮点准备：**
> "Redis 的过期删除策略（惰性+定期）和 PG 的 vacuum 机制本质上解决的是同一个问题——如何高效回收不再需要的存储空间。区别在于 Redis 基于时间戳判定，PG 基于 MVCC 快照可见性判定。"

---

### 阶段 5：性能测试与量化（必须做）

**目标：** 和 EdgeGC 一样，用数据说话

**测试方案：**

| 测试维度 | 工具 | 指标 |
|---|---|---|
| 单命令吞吐 | `redis-benchmark` | GET/SET QPS |
| 并发连接 | `webbench` / `wrk` | 并发连接数、响应延迟 |
| AOF 影响对比 | `redis-benchmark` | 开/关 AOF 的 QPS 差异 |
| 内存占用 | `top` / 自带 info 命令 | 不同数据量下的内存表现 |
| 多线程对比（如实现） | `redis-benchmark` | 单线程 vs 多线程 QPS 对比 |

**必须产出的量化数据（简历用）：**
- [ ] 单机 GET QPS
- [ ] 单机 SET QPS
- [ ] AOF 开启后 QPS 降幅
- [ ] 并发连接数上限
- [ ] （如实现多线程）多线程提升比例

---

### 阶段 6（进阶 A）：Raft 分布式共识

**目标：** 补上分布式视角，覆盖 JD 加分项

**必须掌握的知识点：**

| 模块 | 核心概念 | 实现重点 |
|---|---|---|
| 领导者选举 | 随机超时 + RequestVote RPC + 任期比较 | 选举超时为什么随机化 |
| 日志复制 | AppendEntries RPC + 一致性检查 + 快速回滚 | 如何保证日志一致性 |
| 快照与日志压缩 | 快照替代旧日志，缩小日志体积 | 快照的触发时机与传输 |
| 持久化 | currentTerm / votedFor / log 必须落盘 | 为什么这三项必须持久化 |
| 线性一致性读 | ReadIndex / Lease Read | 读请求如何保证强一致 |

**代码产出：**

- [ ] Raft 核心状态机（Follower/Candidate/Leader 状态转换）
- [ ] RequestVote / AppendEntries RPC
- [ ] 日志复制与一致性检查
- [ ] 快照机制
- [ ] KV 状态机接入 Raft
- [ ] 简易 RPC 框架（基于 Protobuf + muduo 或自研）

**参考资源：**
- 论文：In Search of an Understandable Consensus Algorithm（Raft 原始论文）
- 可视化：[The Secret Lives of Data](http://thesecretlivesofdata.com/raft/) — Raft 动画演示
- 开源参考：[wfz050207/KVstorageBaseRaft-cpp](https://github.com/wfz050207/KVstorageBaseRaft-cpp) — C++20 Raft + 协程 + RPC
- 课程：MIT 6.824 Lab 2/3（如追求更高难度）

**容错测试：**
- [ ] 3 节点集群，1 节点 kill 后集群仍可读写
- [ ] 5 节点集群，2 节点 kill 后集群仍可读写
- [ ] 故障节点恢复后自动追上日志
- [ ] 网络分区场景下的行为验证

---

### 阶段 7（进阶 B）：游戏后台改造

**目标：** 如目标锁定游戏后台方向，将 Mini-Redis 网络层改造为游戏服务器

**改造要点：**

| 改造方向 | 具体内容 |
|---|---|
| 房间/玩家状态管理 | 将 KV 存储替换为游戏实体管理（房间、玩家、道具） |
| 状态同步 | 实现简化版状态同步（位置/属性广播），理解状态同步 vs 帧同步区别 |
| 心跳包机制 | 客户端定期发心跳，服务端超时检测断线 |
| 断线重连 | 会话状态保存与恢复，重连后同步增量状态 |
| 定时器系统 | 基于 epoll 定时器或时间轮实现游戏逻辑定时任务 |

**参考资源：**
- [cloudwu/skynet](https://github.com/cloudwu/skynet) — C 语言经典游戏服务器框架，重点看 actor 模型设计
- [yedf/handy](https://github.com/yedf/handy) — C++11 网络库，简洁的事件驱动封装

---

## 四、开发环境与工具链

| 工具 | 用途 | 版本建议 |
|---|---|---|
| GCC / G++ | 编译 | 支持 C++17（GCC 8+） |
| CMake | 构建 | 3.22+ |
| GDB | 调试 | 配合你的 GDB 经验 |
| Valgrind | 内存泄漏检测 | 面试加分 |
| redis-cli | 兼容性测试 | 5.x+ |
| redis-benchmark | 性能测试 | 5.x+ |
| Wireshark / tcpdump | 网络抓包调试 | 理解 TCP 行为 |

---

## 五、参考开源项目汇总

| 项目 | 地址 | 星数 | 参考价值 |
|---|---|---|---|
| TinyWebServer | [qinguoyi/TinyWebServer](https://github.com/qinguoyi/TinyWebServer) | 19.6k ⭐ | 网络层设计（epoll + Reactor/Proactor），**重点参考** |
| mini-redis | [lhfeiie/mini-redis](https://github.com/lhfeiie/mini-redis) | — | 8 阶段渐进式教学，**最佳跟学路线** |
| mini-redis | [ascendho/mini-redis](https://github.com/ascendho/mini-redis) | — | Socket + 数据结构，注释友好 |
| KVstorageBaseRaft-cpp | [wfz050207/KVstorageBaseRaft-cpp](https://github.com/wfz050207/KVstorageBaseRaft-cpp) | — | Raft + 协程 + RPC，分布式阶段参考 |
| muduo | [chenshuo/muduo](https://github.com/chenshuo/muduo) | 8k+ ⭐ | C++ 网络库标杆，Reactor 模式经典实现 |
| WebServer (C++11) | [markparticle/WebServer](https://github.com/markparticle/WebServer) | — | TinyWebServer 的 C++11 重构版，代码更现代 |

---

## 六、简历写法模板

### 基础版（完成阶段 1-5）

```
Mini-Redis：基于 epoll 的高性能内存键值数据库
03/2026 - 06/2026

- 事件驱动网络层：基于 C++17 实现 epoll LT 事件驱动的 TCP 服务器，
  采用 Reactor 模式管理客户端连接与非阻塞 IO；封装 Socket/Channel/
  EventLoop 三层抽象，实现连接全生命周期管理；支持多客户端并发连接，
  处理 C10K 级并发场景。

- RESP 协议解析：实现 Redis 序列化协议的完整解析器，支持 Inline 与
  Multi Bulk 两种请求格式，覆盖五种数据结构的 20+ 核心命令；采用
  状态机驱动解析流程，处理 TCP 粘包与不完整报文的边界情况。

- 存储引擎实现：实现渐进式 rehash 哈希表避免全量 rehash 延迟抖动，
  实现跳表支持有序集合的 O(logN) 范围查询；设计统一命令分发框架，
  支持命令的水平扩展。

- 持久化与过期机制：实现 AOF 与 RDB 双持久化方案，AOF 支持
  always/everysec/no 三种刷盘策略；实现惰性删除 + 定期扫描的
  过期 Key 清理机制。

- 性能验证：redis-benchmark 压测单机 GET QPS 达 XX,XXX，SET QPS
  达 XX,XXX；AOF（everysec）开启后 QPS 降幅控制在 XX% 以内。

技术栈：C++17、epoll、TCP、RESP、CMake、Linux、GDB
```

### 分布式版（完成阶段 1-6）

```
Mini-Redis：基于 Raft 共识的分布式内存键值数据库
03/2026 - 06/2026

- 事件驱动网络层：（同上）

- Raft 共识算法实现：实现完整的 Raft 共识协议，包括领导者选举
  （随机超时 + 任期比较）、日志复制（AppendEntries 一致性检查 +
  快速回滚）、快照与日志压缩；基于 Protobuf 定义 RPC 协议，
  实现节点间选举请求、心跳与日志条目的可靠传输。

- 分布式 KV 状态机：将单机 KV 存储作为 Raft 状态机，客户端写入
  请求经 Leader 转化为日志条目，多数节点确认后提交执行，保证
  线性一致性读；实现 ReadIndex 机制避免读请求走日志复制路径。

- 容错与恢复：实现 Raft 节点持久化（currentTerm/votedFor/log），
  节点宕机重启后从持久化状态恢复并重新加入集群；3 节点集群中
  1 节点故障时集群仍可正常提供读写服务。

- 性能验证：3 节点 Raft 集群写入 QPS 达 XX,XXX，领导者选举
  完成时间 < XXms；kill -9 模拟节点故障，验证故障转移与数据一致性。

技术栈：C++17/20、epoll、Raft、Protobuf、RPC、CMake、Linux、GDB
```

---

## 七、面试高频问题准备

### 网络编程方向

1. **epoll 的 LT 和 ET 有什么区别？各自适用场景？**
2. **Reactor 和 Proactor 模式的区别？你选了哪个？为什么？**
3. **TCP 粘包是怎么产生的？你怎么处理的？**
4. **为什么非阻塞 IO 要配合 epoll 使用？**
5. **C10K 问题的本质是什么？怎么解决？**
6. **TIME_WAIT 是什么？大量 TIME_WAIT 怎么处理？**

### 存储引擎方向

7. **渐进式 rehash 是怎么实现的？和一次性 rehash 比优势在哪？**
8. **跳表和红黑树对比，各有什么优缺点？Redis 为什么选跳表？**
9. **AOF 和 RDB 的区别？生产环境怎么选？**
10. **过期 Key 的删除策略？和 PG 的 vacuum 有什么异同？**

### 分布式方向（进阶）

11. **Raft 的领导者选举流程？为什么超时时间要随机化？**
12. **日志复制的流程？如何保证一致性？**
13. **什么是线性一致性读？ReadIndex 怎么实现？**
14. **脑裂场景怎么处理？**
15. **Raft 和 Paxos 的对比？Raft 的可理解性体现在哪？**

### 和 PG 内核的关联（差异化亮点）

16. **Redis 的过期删除和 PG 的 vacuum 本质上解决什么相同问题？**
17. **Redis 的渐进式 rehash 和 PG 的 hash 扩容有什么异同？**
18. **从 PG 内核经验出发，你觉得 Redis 还有什么可以优化的地方？**

---

## 八、建议开发节奏

| 阶段 | 内容 | 建议时间 | 优先级 |
|---|---|---|---|
| 1 | TCP 网络编程基础 | 1-2 周 | ★★★ |
| 2 | RESP 协议解析 | 3-5 天 | ★★★ |
| 3 | 存储引擎实现 | 1 周 | ★★☆ |
| 4 | 持久化与过期 | 3-5 天 | ★★☆ |
| 5 | 性能测试与量化 | 2-3 天 | ★★★ |
| 6 | Raft 分布式（可选） | 1-2 周 | ★☆☆ |
| 7 | 游戏后台改造（可选） | 1-2 周 | ★☆☆ |

**关键原则：阶段 1-5 是必须完成的底线，阶段 6-7 根据时间和目标方向选择。**
