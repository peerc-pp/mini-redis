#!/usr/bin/env python3

import argparse
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import BinaryIO, Iterable


Part = str | bytes
Reply = tuple[str, object]


class RespConnection:
    def __init__(self, host: str, port: int) -> None:
        self._socket = socket.create_connection((host, port), timeout=2.0)
        self._stream = self._socket.makefile("rwb", buffering=0)

    def close(self) -> None:
        self._stream.close()
        self._socket.close()

    def execute(self, parts: Iterable[Part]) -> Reply:
        encoded = [part.encode() if isinstance(part, str) else part
                   for part in parts]
        request = [f"*{len(encoded)}\r\n".encode()]
        for part in encoded:
            request.extend(
                [f"${len(part)}\r\n".encode(), part, b"\r\n"])
        self._stream.write(b"".join(request))
        return read_reply(self._stream)


def read_line(stream: BinaryIO) -> bytes:
    line = stream.readline()
    if not line.endswith(b"\r\n"):
        raise RuntimeError(f"invalid or incomplete RESP line: {line!r}")
    return line[:-2]


def read_exact(stream: BinaryIO, size: int) -> bytes:
    data = stream.read(size)
    if data is None or len(data) != size:
        raise RuntimeError(
            f"incomplete RESP payload: expected {size}, got {len(data or b'')}")
    return data


def read_reply(stream: BinaryIO) -> Reply:
    prefix = read_exact(stream, 1)
    if prefix == b"+":
        return ("simple", read_line(stream))
    if prefix == b"-":
        return ("error", read_line(stream))
    if prefix == b":":
        return ("integer", int(read_line(stream)))
    if prefix == b"$":
        length = int(read_line(stream))
        if length == -1:
            return ("null_bulk", None)
        payload = read_exact(stream, length)
        if read_exact(stream, 2) != b"\r\n":
            raise RuntimeError("bulk string is missing its trailing CRLF")
        return ("bulk", payload)
    if prefix == b"*":
        count = int(read_line(stream))
        if count == -1:
            return ("null_array", None)
        return ("array", [read_reply(stream) for _ in range(count)])
    raise RuntimeError(f"unsupported RESP prefix: {prefix!r}")


def normalize_reply(command: list[Part], reply: Reply) -> Reply:
    command_name = command[0]
    if isinstance(command_name, bytes):
        command_name = command_name.decode("ascii")
    if command_name.upper() != "HGETALL" or reply[0] != "array":
        return reply

    elements = reply[1]
    if not isinstance(elements, list) or len(elements) % 2 != 0:
        raise RuntimeError(f"invalid HGETALL response: {reply!r}")

    pairs: list[tuple[bytes, bytes]] = []
    for index in range(0, len(elements), 2):
        field = elements[index]
        value = elements[index + 1]
        if field[0] != "bulk" or value[0] != "bulk":
            raise RuntimeError(f"non-bulk HGETALL pair: {field!r}, {value!r}")
        pairs.append((field[1], value[1]))
    return ("hash", sorted(pairs))


def wait_until_ready(process: subprocess.Popen[str], port: int) -> None:
    deadline = time.monotonic() + 10.0
    while time.monotonic() < deadline:
        if process.poll() is not None:
            output = process.stdout.read() if process.stdout else ""
            raise RuntimeError(
                f"server exited during startup with {process.returncode}:\n{output}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return
        except OSError:
            time.sleep(0.05)
    raise RuntimeError(f"server did not listen on port {port} within 10 seconds")


def ensure_port_unused(port: int) -> None:
    try:
        with socket.create_connection(("127.0.0.1", port), timeout=0.2):
            raise RuntimeError(f"port {port} is already in use")
    except ConnectionRefusedError:
        return
    except TimeoutError:
        return


def stop_process(process: subprocess.Popen[str]) -> str:
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=3.0)
    return process.stdout.read() if process.stdout else ""


def command_sequence() -> list[list[Part]]:
    binary_value = b"A\x00B\r\nC"
    return [
        ["PING"],
        ["ECHO", binary_value],
        ["SET", "string:key", binary_value],
        ["GET", "string:key"],
        ["GET", "missing:key"],
        ["EXISTS", "string:key", "missing:key", "string:key"],
        ["DEL", "missing:key"],
        ["INCR", "counter"],
        ["INCR", "counter"],
        ["DECR", "counter"],
        ["GET", "counter"],
        ["LPUSH", "list:key", "A", "B", "C"],
        ["RPUSH", "list:key", "D", "E"],
        ["LLEN", "list:key"],
        ["LRANGE", "list:key", "0", "-1"],
        ["LRANGE", "list:key", "-100", "2"],
        ["LPOP", "list:key"],
        ["RPOP", "list:key"],
        ["LRANGE", "list:key", "0", "-1"],
        ["HSET", "hash:key", "name", "Alice", "age", "20"],
        ["HSET", "hash:key", "name", "Bob", "city", "Shanghai"],
        ["HGET", "hash:key", "name"],
        ["HGET", "hash:key", "missing"],
        ["HEXISTS", "hash:key", "city"],
        ["HEXISTS", "hash:key", "missing"],
        ["HLEN", "hash:key"],
        ["HGETALL", "hash:key"],
        ["HDEL", "hash:key", "age", "missing"],
        ["HGETALL", "hash:key"],
        ["SET", "wrongtype:key", "value"],
        ["LPUSH", "wrongtype:key", "item"],
        ["HGET", "wrongtype:key", "field"],
        ["GET", "hash:key"],
        ["HDEL", "hash:key", "name", "city"],
        ["EXISTS", "hash:key"],
        ["DEL", "string:key", "counter", "list:key", "wrongtype:key"],
        ["EXISTS", "string:key", "counter", "list:key", "wrongtype:key"],
    ]


def compare_servers(mini_port: int, redis_port: int) -> None:
    mini = RespConnection("127.0.0.1", mini_port)
    reference = RespConnection("127.0.0.1", redis_port)
    try:
        commands = command_sequence()
        for number, command in enumerate(commands, start=1):
            mini_reply = normalize_reply(command, mini.execute(command))
            redis_reply = normalize_reply(command, reference.execute(command))
            if mini_reply != redis_reply:
                raise AssertionError(
                    f"command {number} differs: {command!r}\n"
                    f"Mini-Redis: {mini_reply!r}\n"
                    f"Redis:      {redis_reply!r}")
        print(f"{len(commands)} differential commands matched Redis")
    finally:
        mini.close()
        reference.close()


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mini-server", required=True)
    parser.add_argument("--redis-server", default="redis-server")
    parser.add_argument("--mini-port", type=int, default=6380)
    parser.add_argument("--redis-port", type=int, default=6390)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    mini_server = Path(arguments.mini_server).resolve()
    if not mini_server.is_file():
        raise RuntimeError(f"Mini-Redis server not found: {mini_server}")

    ensure_port_unused(arguments.mini_port)
    ensure_port_unused(arguments.redis_port)

    processes: list[tuple[str, subprocess.Popen[str]]] = []
    failure = False
    with tempfile.TemporaryDirectory(prefix="mini-redis-oracle-") as redis_dir:
        try:
            mini_process = subprocess.Popen(
                [str(mini_server)],
                cwd=mini_server.parent,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            processes.append(("Mini-Redis", mini_process))

            redis_process = subprocess.Popen(
                [
                    arguments.redis_server,
                    "--bind", "127.0.0.1",
                    "--port", str(arguments.redis_port),
                    "--save", "",
                    "--appendonly", "no",
                    "--daemonize", "no",
                    "--dir", redis_dir,
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            processes.append(("Redis", redis_process))

            wait_until_ready(mini_process, arguments.mini_port)
            wait_until_ready(redis_process, arguments.redis_port)
            compare_servers(arguments.mini_port, arguments.redis_port)
        except BaseException:
            failure = True
            raise
        finally:
            outputs = [(name, stop_process(process))
                       for name, process in reversed(processes)]
            if failure:
                for name, output in outputs:
                    if output:
                        print(f"--- {name} output ---\n{output}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, RuntimeError) as error:
        print(f"differential test failed: {error}", file=sys.stderr)
        raise SystemExit(1)
