# Protocol Scope

The Core Release targets RESP2 compatibility for the selected command subset.

## Required Behavior

- Incremental parsing across arbitrary TCP packet boundaries.
- Multiple pipelined requests in one input buffer.
- Binary-safe bulk strings.
- Explicit limits for bulk length, array length, and nesting depth.
- Structured parser errors without process termination.

RESP3 is outside the Core Release scope.
