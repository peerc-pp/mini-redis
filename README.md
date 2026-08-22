# Mini-Redis

Mini-Redis is a learning-oriented, Redis-compatible in-memory key-value server written in
C++17. The project will grow from a blocking TCP echo server into an epoll-based Reactor,
RESP2 command server, storage engine, and persistence layer.

## Current Status

Weeks 0-5 are complete. The executable runs an end-to-end RESP2 server on
`127.0.0.1:6380` through the following path:

```text
redis-cli -> TcpServer -> Session -> RespParser -> CommandRegistry -> Database
```

The current implementation includes:

- non-blocking sockets, epoll LT, a single-threaded Reactor, partial writes, and backpressure;
- an incremental RESP2 parser and encoder with packet-boundary and pipeline coverage;
- String, List, Hash, and ZSet values with 25 core commands;
- Redis-oracle differential tests for the supported command subset;
- a production `HashTable` with two-table incremental rehash, random differential tests, and a
  synchronous-rehash comparison benchmark.

Week 6 is the next milestone: add TTL with lazy expiration, budgeted active expiration, an
injectable clock, and the `EXPIRE`, `PEXPIRE`, `TTL`, `PTTL`, and `PERSIST` commands.

## Supported Commands

- Core/String: `PING`, `ECHO`, `SET`, `GET`, `DEL`, `EXISTS`, `INCR`, `DECR`
- List: `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LLEN`, `LRANGE`
- Hash: `HSET`, `HGET`, `HDEL`, `HEXISTS`, `HLEN`, `HGETALL`
- ZSet: `ZADD`, `ZREM`, `ZSCORE`, `ZRANK`, `ZRANGE`

The command surface intentionally implements a Redis-compatible subset. In particular, the first
`ZRANGE` form is `ZRANGE key start stop`: it returns members, supports negative indexes, and does
not yet implement `WITHSCORES`.

## Requirements

- CMake 3.22 or newer
- A C++17 compiler
- Ninja
- Linux or WSL2; the network layer depends on Linux socket and epoll APIs.

## Build

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Run the current executable:

```bash
./build/debug/src/mini_redis_server
redis-cli -p 6380 PING
```

Run the Debug and sanitizer test suites:

```bash
ctest --preset debug --output-on-failure
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers --output-on-failure
```

Python 3 and `redis-server` must be available on `PATH` for the Redis-oracle differential test to
be registered.

Version design and verification records:

- [Week 4: incremental HashTable and rehash](docs/week-04-hash-table.md)
- [Week 5: ZSet and SkipList](docs/week-05-zset.md)
- [HashTable benchmark reproduction and results](docs/benchmark.md)

## Development

- Format C++ files with `clang-format`.
- Run `clang-tidy` through the generated `compile_commands.json`.
- Use Conventional Commits.
- Develop features on `feature/*` branches created from `develop`.

New to network programming? Start with
[Mini-Redis 第一课](docs/START-HERE.md). It intentionally teaches only the first small step.

See [the implementation roadmap](plan/Mini-Redis项目完成路线.md) for the full learning and
delivery path. See [the Git workflow](docs/git-workflow.md) for repository configuration,
branching, and commit conventions.
