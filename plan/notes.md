# Notes: Mini-Redis 项目分析与调研

## 仓库现状
- 当前只有一份项目规划文档，尚无源码。
- 当前目录未初始化 Git。
- 后续内容将在阅读与调研过程中持续补充。

## 文档分析

### 已有规划的优点
- 项目目标明确：补齐 TCP/epoll、KV 存储和分布式系统能力。
- 已覆盖网络、协议、数据结构、持久化、过期和性能测试等主要模块。
- 强调量化结果和与 PostgreSQL 内核经验的联系，适合作为求职项目。

### 需要修正的地方
- 原计划偏“横向分层”：容易很久没有可运行产品。应先实现 PING/SET/GET 的端到端 vertical slice。
- RESP 首版只需完整支持 RESP2 请求和常用响应，不应一开始追求 RESP3。
- C++ 中 SDS 的学习价值低于网络 Buffer、对象模型和内存所有权；可作为可选实验，而非主线阻塞项。
- `ziplist` 已不是现代 Redis 的主要紧凑编码方向，应改为了解 `listpack`，主线先完成普通 List/ZSet。
- `webbench` 和 `wrk` 面向 HTTP，不适合作为 RESP 服务主压测工具。应使用 `redis-benchmark`、`memtier_benchmark` 和自定义 pipeline client。
- AOF、AOF rewrite、fork+COW snapshot 在 3-5 天内一起完成不现实。应先做 AOF 和恢复，再做自定义 snapshot；不追求 Redis RDB 文件格式兼容。
- Raft 必须等单机状态机接口、持久化和测试稳定后再接入；首版不要同时自研协程库和 RPC 框架。
- 单线程 Reactor 应先保证正确性。线程池只处理明确的阻塞任务，不应过早让多个线程并发修改数据库状态。

### 推荐的产品边界
- Core Release：Linux + C++17、单进程、单 EventLoop、RESP2、redis-cli 兼容、String/List/Hash/ZSet、TTL、AOF、简单 snapshot、可观测性、完整测试与 benchmark。
- 不纳入 Core：RESP3、Redis Cluster、Lua、事务、Pub/Sub、ACL、模块系统、复制、完整 RDB 格式兼容。
- Extension：Raft 复制单独作为第二个里程碑；游戏后台改造应作为另一个项目分支，而非与 Raft 同时推进。

## 开源项目与资料

### 权威资料
- Redis RESP specification: https://redis.io/docs/latest/develop/reference/protocol-spec/
- Redis persistence: https://redis.io/docs/latest/operate/oss_and_stack/management/persistence/
- Redis benchmark guide: https://redis.io/docs/latest/operate/oss_and_stack/management/optimization/benchmarks/
- Redis 7.2 source: https://github.com/redis/redis/tree/7.2
- Raft paper: https://raft.github.io/raft.pdf

### 教学与对照项目
- `build-your-own.org/redis`：适合按低层步骤练习 socket、协议和数据结构。
- `lhfeiie/mini-redis`：路线与本项目高度一致，但当前公开 README 显示只完成最简 TCP，不能作为完整答案。
- `ascendho/mini-redis`：基于 Build Your Own Redis，适合对照注释和目录拆分，测试与 benchmark 仍是计划项。
- `qinguoyi/TinyWebServer`：适合观察 epoll、LT/ET、定时器和 Reactor/Proactor；不要照搬 HTTP/MySQL 业务层。
- `markparticle/WebServer`：适合参考现代 C++ Buffer、Timer、RAII 和模块拆分。
- `chenshuo/muduo`：网络层架构标杆，重点读 EventLoop、Channel、Poller、TcpConnection、Buffer、TimerQueue。
- `redis/redis`：行为和算法的最终权威，固定阅读 7.2 分支，避免直接陷入 unstable 的复杂度。
- `wfz050207/KVstorageBaseRaft-cpp`：可看模块边界和 Protobuf + muduo 组合，但仓库提交历史很短，不应用作 Raft 正确性的唯一依据。

### Redis 源码阅读映射
- Event loop: `src/ae.c`, `src/ae_epoll.c`
- Networking: `src/anet.c`, `src/networking.c`, `src/server.c`
- Buffer/string: `src/sds.c`
- Dict/rehash: `src/dict.c`
- ZSet/skiplist: `src/t_zset.c`
- Compact encoding: `src/listpack.c`
- Expiration: `src/db.c`, `src/expire.c`（按 7.2 实际目录核对）
- Persistence: `src/aof.c`, `src/rdb.c`
- Tests: `tests/unit/`

## 综合判断

- 最佳路线是“两遍实现”：第一遍用标准库快速打通端到端，第二遍替换关键数据结构并量化差异。
- 项目的核心价值不是支持最多命令，而是能解释完整请求路径、边界条件、崩溃恢复和性能数据。
- 每个阶段必须有测试门禁和学习产物，否则容易变成源码拼装项目。
- 合理工期约 10-12 周（每周 15-20 小时）；若全职投入可压缩到 6-8 周。
