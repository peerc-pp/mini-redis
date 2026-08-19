# Differential Tests

`redis_oracle_test.py` starts Mini-Redis on port 6380 and a reference Redis
server on port 6390. It sends the same RESP2 command sequence to both servers
and compares typed replies.

The comparison covers all Week 3 commands, including missing keys, wrong
types, collection deletion, and binary-safe strings. HGETALL replies are
compared as unordered field/value pairs.

The test uses only the Python standard library. CMake registers it when both
Python 3 and `redis-server` are available.
