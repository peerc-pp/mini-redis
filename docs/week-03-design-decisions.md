# Week 3 Design Decisions

## Structured command results

`CommandRegistry` returns `RespValue` as the first structured command-result
type. A `RespValue` retains its logical type and payload; it is not an encoded
wire response. `Session` passes the result to `RespEncoder`, which is the layer
that produces RESP bytes.

This keeps the storage layer independent of RESP. `Database` and `Value` expose
only storage-domain types and never construct protocol responses.

An additional `CommandResult` wrapper is intentionally deferred. It would
currently duplicate `RespValue` without adding another consumer. Revisit this
decision if commands need to serve a non-RESP API or another result consumer.

## Value types

The Week 3 `Value` variant contains String, List, and Hash. ZSet is deliberately
deferred to Week 5 so its dictionary and skip-list indexes can be designed and
tested together instead of adding a temporary placeholder representation.

Wrong accessors return nullable borrowed pointers. A null `Database::find`
result means that the key is missing; a null `Value::as_*` result means that the
key exists with the wrong type.

## Collection and numeric invariants

- Empty Lists and Hashes produced by removal commands are deleted from the
  top-level database.
- Redis integer text is parsed with a strict signed 64-bit grammar.
- Arithmetic overflow is checked before addition, so failed INCR/DECR commands
  do not mutate stored values.
- LRANGE indices are normalized from Redis' inclusive interval to an internal
  half-open interval.

## Command metadata

`CommandSpec` currently stores arity and the handler. Read/write classification
and key-position metadata are understood as future inputs to persistence,
replication, ACL, and clustering. Adding unused fields is deferred until one of
those consumers is implemented.

## Validation

Week 3 provides 20 commands, storage tests that run without networking, debug
and sanitizer test presets, and a typed black-box differential test against
Redis 7. HGETALL responses are compared as unordered field/value pairs.
