# Design Sources

Record every significant external design reference here before adopting it.

| Date | Module | Source | Adopted Idea | Differences |
|---|---|---|---|---|
| 2026-08-20 | HashTable / rehash | Redis 7.2 `src/dict.c` | Keep old and new tables during incremental migration; search both while migration is active | Mini-Redis moves one whole bucket before each mutation, does not advance rehash on reads, uses `std::list` buckets, and omits Redis iterators and pause/safe-iterator machinery |
| 2026-08-22 | ZSet / SkipList | Redis 7.2 `src/t_zset.c` | Combine a `member -> score` dictionary with a score-ordered SkipList; use spans for rank queries and member bytes as the equal-score tie-break | Mini-Redis uses a dedicated C++ `ZSet`, injectable fixed seeds for tests, and only the five Week 5 commands; it omits compact encodings and advanced ZADD/ZRANGE options |

Large code fragments should not be copied. Prefer documenting invariants and reimplementing
the behavior from tests and primary sources.
