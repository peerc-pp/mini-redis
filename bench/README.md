# Benchmarks

This directory contains comparison implementations and benchmark executables. They are not linked
into the production server.

The Week 4 `mini_redis_hash_table_benchmark` compares:

- `src/storage/hash_table.h`: the production two-table incremental rehash implementation;
- `bench/synchronous_hash_table.h`: a single-table baseline that migrates every entry when growth
  begins.

Both strategies receive the same key/value types, load factor, bucket counts, and insertion order.
Setup is excluded from the timed growth-trigger operation.

The exact build command, workloads, environment, representative output, interpretation, and
limitations have one canonical location: [`docs/benchmark.md`](../docs/benchmark.md).
