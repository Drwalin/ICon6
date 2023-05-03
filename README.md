
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

## ICon6 certificates

### Structure of saved certificate key (CertKey)

| bytes | description 
| ----- | ----------- 
| 4     | opslimit argument for `crypto_pwhash`
| 4     | memlimit argument for `crypto_pwhash`
| 16    | salt for key derivation (`crypto_pwhash`)
| 24    | nonce
| 32    | private key
| 64    | public key
| 16    | mac

(private key | public key) are encrypted with chacha20-poly1305-ietf with
key derived from password with `crypto_pwhash` and given arguments

### Structure of certificate (Cert)

| bytes | value name | description 
| ----- | ---------- | ----------- 
| 4     |            | version of certificate, unsigned integer saved in LE order
| 4     | B          | **certificate size** in bytes, unsigned integer saved in LE order
| X     |            | certificate data
| Y     |            | **owner signature** of `data[0 : B-Y-Z]`
| Z     |            | **parent signature** of `data[0 : B-Z]` (created with **owner public key** for self signed)

#### Certificate version #1

| bytes | value name | description 
| ----- | ---------- | ----------- 
| 4     |            | version of certificate, must be 1
| 4     | B          | **certificate size** in bytes, unsigned integer saved in LE order
| 32    |            | **owner public key** 
| 32    |            | **parent public key** (same as **owner public key** for self signed)
| 4     |            | **depth** of this certificate in certificate chain
| 20    |            | **issue date** in string format `YYYY-MM-DD_HH:mm:ss` in UTC-0 time `NULL`-terminated
| 20    |            | **expire date** in string format `YYYY-MM-DD_HH:mm:ss` in UTC-0 time `NULL`-terminated
| 4     |            | **ROLE**
| 4     | M          | number of bytes in **owner name**
| 4     | N          | number of bytes in **owner url**
| M     |            | **onwer name** `NULL`-terminated
| N     |            | **owner url** `NULL`-terminated
| 64    |            | **owner signature** of `data[0 : B-Y-Z]`
| 64    |            | **parent signature** of `data[0 : B-Z]` (created with **owner public key** for self signed)

##### Possible values for ROLE in certificate version #1

| value LE | description
| -------- | -----------
| 0        | Root CA - only signing
| 1        | Not root CA, but only signing
| 2        | Server certificate - for accepting communication in ICon6
| 3        | Client one-time self signed certificate 

Only signing curve used is `ed25519`.

### Structure of certificate request (CertReq)

Everything except: certificate signature by parent



