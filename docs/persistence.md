# Persistence

Persistence will be implemented after the in-memory command path is stable.

## Planned Order

1. AOF append and replay.
2. Configurable `always`, `everysec`, and `no` fsync policies.
3. AOF rewrite with atomic replacement.
4. A versioned custom snapshot format with checksum.
5. Snapshot plus AOF-tail recovery.

Redis RDB binary compatibility is not a project goal.
