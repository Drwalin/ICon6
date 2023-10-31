
# ICon6

Provides secure communication between two endpoints with UPD socket. Any client
can connect to any other client (calling them servers is purely semantical).
Networking and encryption is done with ValveSoftware/GameNetworkingSockets.
User can pass OnReceive callback for all binary messages per-peer basis or user
can use RPC or RMI semantics (set per Host).

Each message is limited by GameNetworkingSockets to 512KiB.

## Requirements

- cmake

## Vulnerability

At least the same as ValveSoftware/GameNetworkingSockets.

## ICon6 RPC/RMI protocol description

### RPC description message
```
+----------+-----------+
| function | arguments |
+----------+-----------+
```
- function - NULL-terminated function name string
- arguments - arguemnts packed with bitscpp ByteWriter

### RMI description message
```
+---------+-------------+--------+-----------+
+ flag 1B | objectID 8B | method | arguments |
+---------+-------------+--------+-----------+
```
- flag - for now must have value 0
- objectID - uint64\_t object identificator
- method - NULL-terminated method name string to be called on object with id
objectID
- arguments - arguemnts packed with bitscpp ByteWriter

