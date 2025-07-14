# Sockets: Theory, Types, and Platform APIs

<!--!
\defgroup sockets_theory Sockets: Theory, Types, and Platform APIs
\ingroup docs
\hidegroupgraph
[TOC]
-->

[![CC0](https://licensebuttons.net/p/zero/1.0/88x31.png)](https://creativecommons.org/publicdomain/zero/1.0/)

*Disclaimer: ChatGPT generated document*.

## Introduction

Sockets are fundamental building blocks for network communication in modern computing. They provide a standardized
interface for processes to exchange data over networks, whether locally (inter-process communication) or across the
globe (Internet communication). This document provides a comprehensive overview of socket theory, types, internal
mechanisms, and platform-specific APIs.

---

## 1. What is a Socket?

A **socket** is an abstraction representing one endpoint of a two-way communication link between two programs running on
a network. Sockets allow applications to send and receive data, regardless of the underlying network technology.

- **Analogy:** Think of a socket as a telephone jack: it provides a standard way to connect and communicate, regardless
  of the phone or the network.

---

## 2. Types of Sockets (In-Depth)

### 2.1. Stream Sockets (TCP)

- **Type:** SOCK_STREAM
- **Protocol:** TCP (Transmission Control Protocol)
- **How it works:**
    - Establishes a connection using a three-way handshake (SYN, SYN-ACK, ACK).
    - Data is sent as a continuous stream of bytes; the protocol ensures all data arrives in order and without
      duplication.
    - TCP handles retransmission, congestion control, and flow control.
    - The OS kernel maintains a buffer for each socket; the application reads/writes to this buffer, and the kernel
      manages network transmission.
    - Connection is full-duplex (data can flow both ways simultaneously).
- **Use cases:** Web servers, file transfers, email, SSH.

### 2.2. Datagram Sockets (UDP)

- **Type:** SOCK_DGRAM
- **Protocol:** UDP (User Datagram Protocol)
- **How it works:**
    - No connection is established; each message (datagram) is sent independently.
    - No guarantee of delivery, order, or duplication protection.
    - Each `sendto()` call transmits a single datagram; each `recvfrom()` call receives one datagram.
    - Lower overhead and latency compared to TCP, but the application must handle lost or out-of-order packets if
      needed.
    - The OS kernel maintains a buffer for incoming datagrams; if the buffer is full, new datagrams are dropped.
- **Use cases:** Real-time applications (VoIP, games), DNS, streaming.

### 2.3. Raw Sockets

- **Type:** SOCK_RAW
- **Protocol:** Custom or direct access to IP
- **How it works:**
    - Allows direct construction and parsing of network packets, bypassing most of the OS protocol stack.
    - Used for implementing custom protocols, network diagnostics, or security tools.
    - Requires elevated privileges (root/admin) on most systems.
    - The application is responsible for building packet headers and handling protocol details.
- **Use cases:** Ping, traceroute, protocol analyzers, custom networking tools.

### 2.4. Unix Domain Sockets (Local IPC)

- **Type:** AF_UNIX or AF_LOCAL
- **Protocol:** Local (not networked)
- **How it works:**
    - Used for communication between processes on the same host.
    - Can be stream (SOCK_STREAM) or datagram (SOCK_DGRAM).
    - Data never leaves the host; the kernel transfers data directly between processes, often via memory copies.
    - Addressed by filesystem path (e.g., `/tmp/mysocket`).
    - Faster and more secure than TCP/UDP for local IPC.
- **Use cases:** Local services, daemons, desktop applications, X11, Docker.

---

## 2.5. Comparison Table: Socket Types

| Type        | Protocol | Connection | Ordered | Reliable | Message Boundaries | Use Case               |
|-------------|----------|------------|---------|----------|--------------------|------------------------|
| Stream      | TCP      | Yes        | Yes     | Yes      | No                 | Web, file transfer     |
| Datagram    | UDP      | No         | No      | No       | Yes                | Games, VoIP, DNS       |
| Raw         | Custom   | N/A        | N/A     | N/A      | N/A                | Diagnostics, protocols |
| Unix Domain | Local    | Yes/No     | Yes/No  | Yes/No   | Yes/No             | Local IPC              |

---

## 2.6. Under the Hood: How Socket Types Work

### Stream Sockets (TCP)

- The kernel maintains a state machine for each connection (e.g., LISTEN, SYN_SENT, ESTABLISHED, FIN_WAIT).
- Data is split into segments, each with sequence numbers and checksums.
- Retransmission and acknowledgment are handled automatically.
- Flow control (window size) and congestion control (e.g., slow start) are built in.
- Closing a connection involves a four-way handshake (FIN/ACK exchange).

### Datagram Sockets (UDP)

- Each datagram is sent as a single packet; the kernel does not guarantee delivery or order.
- No connection state is maintained; the socket is stateless.
- Applications may implement their own reliability mechanisms if needed.
- Broadcast and multicast are supported (send to multiple recipients).

### Raw Sockets

- The application is responsible for constructing the entire packet, including headers.
- The kernel may provide some checks (e.g., IP header checksum), but most protocol logic is up to the application.
- Useful for low-level network programming and diagnostics.

### Unix Domain Sockets

- The kernel uses internal data structures (not network stack) to transfer data between processes.
- Supports passing file descriptors and credentials between processes (ancillary data).
- Can be used for both stream and datagram communication.

---

## 2.7. Security and Performance Considerations

- **TCP:** Reliable and secure (with TLS), but higher overhead and latency.
- **UDP:** Fast and lightweight, but no built-in security or reliability.
- **Raw:** Powerful but dangerous; can be used for attacks or diagnostics.
- **Unix Domain:** Very fast and secure for local IPC; not routable over networks.

---

## 3. Socket Address Families

- **AF_INET:** IPv4 Internet protocols
- **AF_INET6:** IPv6 Internet protocols
- **AF_UNIX / AF_LOCAL:** Unix domain (local) sockets

---

## 4. How Sockets Work: The Lifecycle

### 4.1. Server Side

1. **Create:** `socket()`
2. **Bind:** `bind()` to a local address/port
3. **Listen:** `listen()` (for stream sockets)
4. **Accept:** `accept()` incoming connections
5. **Communicate:** `send()`, `recv()` (or `read()`, `write()`)
6. **Close:** `close()` (POSIX) or `closesocket()` (Windows)

### 4.2. Client Side

1. **Create:** `socket()`
2. **Connect:** `connect()` to server address/port
3. **Communicate:** `send()`, `recv()`
4. **Close:** `close()` / `closesocket()`

---

## 5. Under the Hood: How Sockets Operate

- **File Descriptor:** On POSIX, a socket is a file descriptor; on Windows, it's a SOCKET handle.
- **Buffers:** Each socket has send/receive buffers managed by the OS.
- **Blocking vs Non-blocking:**
    - Blocking: Calls wait until operation completes.
    - Non-blocking: Calls return immediately; use polling or event notification.
- **Timeouts:** Sockets can be configured to time out on operations.
- **Select/Poll/Epoll:** Mechanisms to monitor multiple sockets for readiness.

---

## 6. Platform-Specific APIs

### 6.1. POSIX (Linux, macOS, Unix)

- **Header:** `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, `<unistd.h>`
- **Functions:** `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`, `close()`
- **Unix Domain Sockets:** Use `AF_UNIX` address family; pathnames as addresses.
- **Non-blocking:** `fcntl()` to set `O_NONBLOCK`.
- **Timeouts:** `setsockopt()` with `SO_RCVTIMEO`/`SO_SNDTIMEO`.

### 6.2. Windows (Winsock)

- **Header:** `<winsock2.h>`, `<ws2tcpip.h>`
- **Initialization:** `WSAStartup()` before using sockets; `WSACleanup()` after.
- **Functions:** `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`, `closesocket()`
- **Non-blocking:** `ioctlsocket()`
- **Timeouts:** `setsockopt()`
- **Differences:**
    - File descriptors are not integers (use SOCKET type)
    - Error codes differ (use `WSAGetLastError()`)
    - No native Unix domain sockets (Windows 10+ supports AF_UNIX)

---

## 7. Advanced Topics

- **Socket Options:** Control behavior with `setsockopt()`/`getsockopt()` (e.g., reuse address, buffer sizes).
- **Multicast/Broadcast:** UDP sockets can send to multiple recipients.
- **SSL/TLS:** Secure sockets layer for encrypted communication (e.g., OpenSSL, SChannel).
- **Asynchronous I/O:** Overlapped I/O (Windows), `epoll` (Linux), `kqueue` (BSD), `select`/`poll` (portable).
- **Zero-Copy:** Advanced APIs for high-performance networking.

---

## 8. Summary Table

| Type        | Protocol | Connection | Ordered | Reliable | Use Case               |
|-------------|----------|------------|---------|----------|------------------------|
| Stream      | TCP      | Yes        | Yes     | Yes      | Web, file transfer     |
| Datagram    | UDP      | No         | No      | No       | Games, VoIP, DNS       |
| Raw         | Custom   | N/A        | N/A     | N/A      | Diagnostics, protocols |
| Unix Domain | Local    | Yes/No     | Yes/No  | Yes/No   | Local IPC              |

---

## 9. References

- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [UNIX Network Programming, W. Richard Stevens]
- [Microsoft Winsock Documentation](https://docs.microsoft.com/en-us/windows/win32/winsock/)
- [man 7 socket](https://man7.org/linux/man-pages/man7/socket.7.html)

---

*This document is part of the jsocketpp project. For usage examples and API documentation, see the main README and
source code.*
