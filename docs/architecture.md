# Architecture

## Initial Boundaries

- `base`: shared low-level utilities with no dependency on higher layers.
- `net`: Linux socket, epoll, event loop, connection, and buffer abstractions.
- `protocol`: RESP values, incremental parsing, and response encoding.
- `server`: sessions, command registration, dispatch, and process lifecycle.
- `storage`: database objects, dictionaries, lists, sorted sets, and expiration.
- `persistence`: AOF, rewrite, snapshot, and recovery.

## Dependency Direction

```text
server -> protocol
server -> storage
server -> net

persistence -> storage
base <- all modules
```

The storage layer must not depend on sockets or RESP so it can later serve as a deterministic
Raft state machine.

Week-specific decisions are recorded in
[`week-03-design-decisions.md`](week-03-design-decisions.md).
