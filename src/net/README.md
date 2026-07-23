# Network Module

Implemented components:

- `Socket` and `UniqueFd`: socket creation and fd ownership.
- `Poller`, `Channel`, and `EventLoop`: epoll LT event registration and dispatch.
- `Acceptor`: non-blocking accept loop.
- `TcpConnection`: connection lifecycle, input/output buffers, partial writes, and backpressure
  limits.
- `TcpServer`: listening socket and active connection ownership.
- `Buffer`: growable TCP byte-stream storage.

The module targets Linux non-blocking sockets and epoll. `EventLoop` must outlive every
`Acceptor`, `TcpConnection`, and `TcpServer` registered with it.

Run the complete network verification:

```bash
./scripts/test-network.sh
```

The earlier learning step remains documented in the
[blocking Echo Server milestone](../../docs/milestone-01-blocking-echo-server.md).
