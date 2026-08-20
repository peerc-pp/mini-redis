# Benchmark Method

Benchmark results must include the environment, exact command, dataset size, warm-up policy,
repetition count, throughput, and latency percentiles.

## Planned Tools

- `redis-benchmark`
- `memtier_benchmark`
- Module-level C++ microbenchmarks
- Linux `perf`, `strace`, and resource statistics

No performance claim belongs in the README or resume unless the raw result is reproducible.

## HashTable Rehash Microbenchmark

The benchmark compares the production incremental HashTable with the
single-table synchronous baseline kept in `bench/`. Both use the same key/value
types, initial bucket counts, maximum load factor, and input sequence.

Build and run:

```bash
cmake -S . -B build/benchmark -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DMINI_REDIS_BUILD_TESTS=OFF \
  -DMINI_REDIS_BUILD_BENCHMARKS=ON
cmake --build build/benchmark --target mini_redis_hash_table_benchmark
./build/benchmark/bench/mini_redis_hash_table_benchmark
```

Workloads:

- `continuous_insert`: 300,000 individually timed inserts.
- `growth_trigger`: 84 timed growth-triggering inserts across bucket counts
  1,024 through 65,536, with 12 repetitions each.
- Setup work is outside the timed region.
- There is no separate warm-up. Repeat on an idle machine before making an
  external performance claim.

Representative result from WSL2, GCC 13.3.0, Release, 2026-08-20:

| Workload | Strategy | p50 (ns) | p99 (ns) | max (ns) |
|---|---|---:|---:|---:|
| continuous insert | synchronous | 24 | 635 | 9,584,906 |
| continuous insert | incremental | 32 | 76 | 7,884,578 |
| growth trigger | synchronous | 51,617 | 1,668,936 | 1,754,955 |
| growth trigger | incremental | 18,092 | 248,114 | 645,399 |

This run shows the trade-off: incremental migration adds a small amount to
typical insertion latency while reducing tail latency. The remaining
multi-millisecond continuous-workload maximum shows that allocating the new
bucket array is still synchronous; node migration is incremental, but allocation
is not.
