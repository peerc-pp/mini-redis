# Mini-Redis

Mini-Redis is a learning-oriented, Redis-compatible in-memory key-value server written in
C++17. The project will grow from a blocking TCP echo server into an epoll-based Reactor,
RESP2 command server, storage engine, and persistence layer.

## Current Status

The repository contains the project skeleton, build configuration, documentation structure,
and a smoke test. Network and Redis functionality are intentionally not implemented yet.

## Requirements

- CMake 3.22 or newer
- A C++17 compiler
- Ninja
- Linux or WSL2 for epoll development

The scaffold also builds on Windows, but the network layer will target Linux APIs.

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

On Windows:

```powershell
.\build\debug\src\mini_redis_server.exe
```

## Development

- Format C++ files with `clang-format`.
- Run `clang-tidy` through the generated `compile_commands.json`.
- Use Conventional Commits.
- Develop features on `feature/*` branches created from `develop`.

See [the implementation roadmap](plan/Mini-Redis项目完成路线.md) for the full learning and
delivery path. See [the Git workflow](docs/git-workflow.md) for repository configuration,
branching, and commit conventions.
