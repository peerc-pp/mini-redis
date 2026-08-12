# Mini-Redis

Mini-Redis is a learning-oriented, Redis-compatible in-memory key-value server written in
C++17. The project will grow from a blocking TCP echo server into an epoll-based Reactor,
RESP2 command server, storage engine, and persistence layer.

## Current Status

The Linux network layer is implemented with non-blocking sockets, epoll LT, and a single-threaded
Reactor. It includes connection ownership, growable input/output buffers, partial writes, and
backpressure limits. The RESP2 module currently provides values, response encoding, and an
incremental request parser for arrays and bulk strings.

The executable still runs the earlier blocking echo server. Command dispatch, Redis commands,
storage, expiration, and persistence are not implemented yet. The next milestone is an end-to-end
`redis-cli -> TcpServer -> RESP -> PING/ECHO -> RESP` path.

## Requirements

- CMake 3.22 or newer
- A C++17 compiler
- Ninja
- Linux or WSL2; the network layer depends on Linux socket and epoll APIs.

## Build

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Run the current executable:

```bash
./build/debug/src/mini_redis_server
```

Run the focused network verification, including ASan and UBSan builds:

```bash
./scripts/test-network.sh
```

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
