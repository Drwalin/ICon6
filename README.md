
# ICon6

Provides secure communication between two endpoints with UPD socket. Any client
can connect to any other client (calling them servers is purely semantical).
Networking is done with enet library and encryption is done with libsodium.

## Requirements

- cmake
- installed libsodium on in version 1.0.18

## Vulnerability

All application data is securely encrypted. Networking protocol of enet is not
encrypted, thus there is insecurity for find out if sent message is send by
reliable or sequenced channel. In case of connections not secured by
certificates (which are not implemented yet), attacker can perform Man in the
Middle attack. Attacker can also block/modify internal enet protocol messages to
perform a DoS attack.

Message size is limited by enet to 32 MiB, but encryption limits this size
further. It is recommended to not use bigger messages than 1 MiB in size.

There is no practical limit to number of sent messages (up to 2^64 messages sent
with encryption). Messages sent in different directions are encrypted using
different secret keys established during handshake. There is no rekeying
implemented.

