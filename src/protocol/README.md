# Protocol Module

Implemented components:

- `RespValue`: in-memory values for simple strings, errors, integers, bulk strings, null bulk
  strings, and arrays.
- `RespEncoder`: RESP2 encoding for every currently represented value type.
- `RespParser`: incremental, binary-safe parsing for request-side bulk strings and arrays.

The parser consumes bytes only after one complete value has been parsed. Incomplete and malformed
input remains in the input `Buffer`, allowing the connection layer to wait for more data or apply
its protocol-error policy. Limits are enforced for bulk length, array length, and nesting depth.

Protocol unit tests cover empty and null values, binary payloads, nested arrays, pipelined values,
partial input, malformed lengths, resource limits, and nesting limits.

The next step is to connect the parser and encoder to a server-side session and command dispatcher.
