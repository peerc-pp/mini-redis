# Week 5: ZSet and SkipList

## Outcome

Week 5 added a sorted-set value to the existing storage and command path:

```text
RESP request -> CommandRegistry -> Value::SortedSet -> ZSet
                                                |-> HashTable<member, score>
                                                `-> SkipList<(score, member)>
```

The server now supports `ZADD`, `ZREM`, `ZSCORE`, `ZRANK`, and `ZRANGE`. The implementation keeps
member lookup and score order as two permanent logical indexes rather than trying to make one data
structure serve both access patterns.

## Implementation Map

| File | Responsibility |
|---|---|
| `src/storage/skip_list.h/.cc` | Score/member ordering, random levels, spans, rank, and rank ranges |
| `src/storage/zset.h/.cc` | Dictionary plus SkipList ownership and dual-index mutation rules |
| `src/storage/value.h/.cc` | ZSet type integration and exclusive ownership inside `Value` |
| `src/server/double_parser.h/.cc` | Strict score parsing and RESP score formatting |
| `src/server/command_registry.h/.cc` | ZSet command arity, dispatch, Redis errors, and RESP results |
| `tests/unit/*zset*`, `tests/unit/skip_list_test.cc`, `tests/differential/redis_oracle_test.py` | Structural, command, sanitizer, random-model, and Redis-oracle verification |

## SkipList Shape

`SkipList` is deliberately ZSet-specific. Nodes are ordered by `(score, member)`: score is the
primary key, and raw member string order breaks equal-score ties. NaN is rejected because it does
not define a stable total order.

The level-0 forward pointers form the complete ordered sequence. Higher levels are probabilistic
shortcuts, with a promotion probability of approximately 1/4 and a maximum height of 32. Every
forward pointer also stores a span: the number of level-0 nodes crossed by that jump. Accumulating
spans makes zero-based rank lookup expected `O(log N)` instead of requiring a linear scan.

The expected costs are:

| Operation | Expected cost |
|---|---:|
| Insert or erase | `O(log N)` |
| Rank lookup | `O(log N)` |
| Range by rank returning M members | `O(log N + M)` |

Production construction uses a random seed. Tests can inject a fixed seed so the generated level
layout and failures are reproducible.

## Dual-Index Invariant

For every live member, both indexes must contain exactly the same score:

```text
scores_[member] == score
        if and only if
ordered_ contains (score, member)
```

Adding a new member first inserts the ordered node, then inserts the dictionary entry. If dictionary
insertion throws or reports an impossible conflict, the ordered node is removed before the error is
propagated.

Updating an existing member uses prepare-then-commit ordering:

```text
insert new (score, member)
-> erase old (score, member)
-> update scores_[member]
```

The only allocation-prone step happens before the old state is destroyed. Mini-Redis executes
commands on one EventLoop thread, so no other command can observe the brief internal state where
both ordered nodes exist. An impossible mismatch between indexes raises `std::logic_error` rather
than silently returning corrupt results.

## Command Scope

| Command | Implemented behavior |
|---|---|
| `ZADD key score member` | Adds one member or updates its score; returns 1 for a new member and 0 otherwise |
| `ZREM key member [member ...]` | Removes multiple members and deletes the database key when the ZSet becomes empty |
| `ZSCORE key member` | Returns the formatted score or a null bulk string |
| `ZRANK key member` | Returns a zero-based rank or a null bulk string |
| `ZRANGE key start stop` | Returns members only; supports negative indexes and an inclusive stop |

Scores use strict floating-point parsing. Normal finite values and positive or negative infinity are
accepted; NaN, overflow, trailing characters, and malformed text are rejected without mutation.
Wrong-type keys return the normal Redis `WRONGTYPE` error.

## Verification

The validation layers cover different failure classes:

- `skip_list_test.cc` checks ordering, equal-score ties, spans, rank, erase, range boundaries, NaN,
  and 10,000 fixed-seed random operations against `std::set`.
- `zset_test.cc` checks add/update/remove semantics and 100,000 fixed-seed operations against an
  `std::unordered_map` plus ordered `std::set` model. Every checkpoint compares size, full order,
  score lookup, and rank lookup across both indexes.
- Command tests cover response types, negative ranges, missing members, invalid scores, wrong types,
  multi-member removal, and automatic deletion of empty ZSets.
- The complete Debug and ASan/UBSan presets each passed 16/16 tests. The 100,000-operation ZSet test
  also passed as a focused executable in both configurations.
- A later documentation audit found that the new ZSet oracle expressions were not appended to the
  command sequence. After fixing the wiring, the targeted Debug oracle passed and now executes 12
  ZSet operations inside a 49-command typed RESP2 comparison with a real Redis server.

## Known Boundaries

- `ZADD` accepts one score/member pair and does not implement `NX`, `XX`, `GT`, `LT`, `CH`, or `INCR`.
- `ZRANGE` does not implement `WITHSCORES`, score ranges, lexicographical ranges, or reverse order.
- The SkipList is not a generic ordered-container API and is intentionally non-copyable.
- Compact listpack encoding, persistence, TTL interaction, and concurrent mutation are outside this
  version.

## Week 6 Handoff

Week 5 established a permanent multi-index invariant. Week 6 adds time as another source of state:
keys must disappear consistently from the main dictionary and expiration index through lazy access
checks and budgeted active cleanup, without introducing a full-database latency spike.
