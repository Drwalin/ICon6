
# ICon6

!!! Superseded by https://github.com/Drwalin/ICon7

Provides secure communication between two endpoints with UPD socket. Any client
can connect to any other client (calling them servers is purely semantical).
Networking and encryption is done with ValveSoftware/GameNetworkingSockets.
User can pass OnReceive callback for all binary messages per-peer basis or user
can use RPC or RMI semantics (set per Host).

Each message is limited by GameNetworkingSockets to 512KiB.

## Requirements

- cmake

## How to compile

```bash
mkdir build
cmake ..
make
```

## Usage

### Examples

Examples are provided in `examples/` directory. Examples called `example1.cpp`
and `example6.cpp` work only on linux. It is recommended to learn on
chat\_server/chat\_client or echo\_server/echo\_client.

### Basic functionality

Befor starting using ay of the functions user needs to call:
```c++
icon6::Initialize();
```

And to exit gracefully user needs needs to call:
```c++
icon6::Deinitialize();
```

There are 2 basic classes:
    - icon6::Host
    - icon6::Peer

Host represents socket or listening socket while Peer represents connection.

Data receiving is implemented using callbacks which can be set per Host (all new
Peers will use this new callback) or per Peer basis.

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

