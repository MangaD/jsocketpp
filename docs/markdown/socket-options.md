# Socket Options in jsocketpp

<!--!
\defgroup socket-options Socket Options in jsocketpp
\ingroup docs
\hidegroupgraph
[TOC]
-->

This document describes the various **socket options** supported by `jsocketpp`, how they relate to each socket type,
and best practices for using them in your applications. It is intended as a practical reference for users and
contributors.

## Table of Contents

* [What Are Socket Options?](#what-are-socket-options)
* [Cross-Platform Support](#cross-platform-support)
* [Common Options by Socket Type](#common-options-by-socket-type)

    * [1. Options for All Sockets](#1-options-for-all-sockets)
    * [2. TCP Sockets (`Socket`, `ServerSocket`)](#2-tcp-sockets-socket-serversocket)
    * [3. UDP Sockets (`DatagramSocket`, `MulticastSocket`)](#3-udp-sockets-datagramsocket-multicastsocket)
    * [4. Unix Domain Sockets (`UnixSocket`)](#4-unix-domain-sockets-unixsocket)
    * [5. Summary Table](#5-summary-table)
* [Dedicated Methods vs. Generic Methods](#dedicated-methods-vs-generic-methods)
* [Examples](#examples)
* [Design Recommendations](#design-recommendations)
* [Further Reading](#further-reading)

---

## What Are Socket Options?

**Socket options** allow you to change the behavior of a socket at runtime, such as buffer sizes, address reuse, and
special TCP or UDP features. They are configured using the `setsockopt()` and `getsockopt()` system calls, but
`jsocketpp` provides convenient, type-safe, and well-documented wrappers.

---

## Cross-Platform Support

jsocketpp aims to abstract away the differences between Linux, macOS, and Windows.
Most options are available on all major platforms, but some are platform-specific.
Where relevant, this is noted below.

---

## Common Options by Socket Type

### 1. Options for All Sockets

| Option                    | Description                                              |
|---------------------------|----------------------------------------------------------|
| `setReuseAddress(bool)`   | Allow multiple sockets to bind to the same address/port. |
| `getReuseAddress() const` | Query if address reuse is enabled.                       |

On some Unix systems, you may also find `SO_REUSEPORT` for advanced load balancing, but it is not universally available.

---

### 2. TCP Sockets (`Socket`, `ServerSocket`)

| Option                         | Description                                                     |
|--------------------------------|-----------------------------------------------------------------|
| `setNoDelay(bool)`             | Disable Nagle’s algorithm for lower latency (TCP\_NODELAY).     |
| `getNoDelay() const`           | Query if Nagle’s algorithm is disabled.                         |
| `setKeepAlive(bool)`           | Enable TCP keepalive packets (helps detect broken connections). |
| `getKeepAlive() const`         | Query if keepalive is enabled.                                  |
| `setLinger(bool, int)`         | Control close() behavior: wait N seconds to send unsent data.   |
| `getLinger() const`            | Get linger status and timeout.                                  |
| `setSendBufferSize(int)`       | Set the size of the send buffer.                                |
| `getSendBufferSize() const`    | Get the size of the send buffer.                                |
| `setReceiveBufferSize(int)`    | Set the size of the receive buffer.                             |
| `getReceiveBufferSize() const` | Get the size of the receive buffer.                             |
| `setTimeout(int)`              | Set socket timeouts (already in your API).                      |

---

### 3. UDP Sockets (`DatagramSocket`, `MulticastSocket`)

| Option                         | Description                                               |
|--------------------------------|-----------------------------------------------------------|
| `setBroadcast(bool)`           | Enable sending to broadcast addresses (SO\_BROADCAST).    |
| `getBroadcast() const`         | Query if broadcast is enabled.                            |
| `setSendBufferSize(int)`       | Set the send buffer size.                                 |
| `getSendBufferSize() const`    | Get the send buffer size.                                 |
| `setReceiveBufferSize(int)`    | Set the receive buffer size.                              |
| `getReceiveBufferSize() const` | Get the receive buffer size.                              |
| `setReuseAddress(bool)`        | Allow reuse of local address/port (useful for multicast). |
| `getReuseAddress() const`      | Query if address reuse is enabled.                        |

#### For Multicast:

| Option                                      | Description                                        |
|---------------------------------------------|----------------------------------------------------|
| `setMulticastLoopback(bool)`                | Control whether outgoing multicast is looped back. |
| `getMulticastLoopback() const`              | Query loopback status.                             |
| `setMulticastTTL(int)`                      | Set the multicast time-to-live (scope).            |
| `getMulticastTTL() const`                   | Get the current multicast TTL.                     |
| `setMulticastInterface(const std::string&)` | Set outgoing multicast interface by name/address.  |
| `getMulticastInterface() const`             | Query the multicast interface.                     |

---

### 4. Unix Domain Sockets (`UnixSocket`)

| Option                         | Description                                           |
|--------------------------------|-------------------------------------------------------|
| `setSendBufferSize(int)`       | Set the send buffer size.                             |
| `getSendBufferSize() const`    | Get the send buffer size.                             |
| `setReceiveBufferSize(int)`    | Set the receive buffer size.                          |
| `getReceiveBufferSize() const` | Get the receive buffer size.                          |
| `setPassCred(bool)`            | Linux: pass credentials with messages (SO\_PASSCRED). |
| `getPassCred() const`          | Query if credential passing is enabled.               |
| `setReuseAddress(bool)`        | Rarely used, but available.                           |

---

### 5. Summary Table

| Option                | Socket | ServerSocket | DatagramSocket | MulticastSocket | UnixSocket |
|-----------------------|:------:|:------------:|:--------------:|:---------------:|:----------:|
| setReuseAddress       |   ✓    |      ✓       |       ✓        |        ✓        |     ✓      |
| setNoDelay            |   ✓    |              |                |                 |            |
| setKeepAlive          |   ✓    |      ✓       |                |                 |            |
| setLinger             |   ✓    |      ✓       |                |                 |            |
| setBroadcast          |        |              |       ✓        |        ✓        |            |
| setSendBufferSize     |   ✓    |      ✓       |       ✓        |        ✓        |     ✓      |
| setReceiveBufferSize  |   ✓    |      ✓       |       ✓        |        ✓        |     ✓      |
| setTimeout            |   ✓    |      ✓       |       ✓        |        ✓        |     ✓      |
| setMulticastLoopback  |        |              |                |        ✓        |            |
| setMulticastTTL       |        |              |                |        ✓        |            |
| setMulticastInterface |        |              |                |        ✓        |            |
| setPassCred           |        |              |                |                 |     ✓      |

---

## Dedicated Methods vs. Generic Methods

While jsocketpp provides generic `setOption(int level, int optname, int value)` and `getOption(int level, int optname)`
methods for full flexibility, **dedicated methods** offer:

* Type safety
* Better documentation
* IDE autocompletion
* Protection against misuse (e.g., not all options are meaningful for all socket types)

---

## Examples

```cpp
Socket tcpSocket;
tcpSocket.setNoDelay(true);
tcpSocket.setKeepAlive(true);
tcpSocket.setSendBufferSize(32768);

DatagramSocket udpSocket;
udpSocket.setBroadcast(true);

MulticastSocket mcastSock;
mcastSock.setMulticastTTL(4);
mcastSock.setMulticastLoopback(false);

UnixSocket unixSock;
unixSock.setPassCred(true);
```

---

## Design Recommendations

* Use **dedicated option methods** for the most common and semantically important settings.
* Keep the generic option methods for advanced or less common socket options.
* Clearly document OS-specific options and behaviors in Doxygen and in the public API.
* When in doubt, consult [man 7 socket](https://man7.org/linux/man-pages/man7/socket.7.html) and your platform’s
  documentation.

---

## Further Reading

* [Beej’s Guide to Network Programming: Using Internet Sockets](https://beej.us/guide/bgnet/)
* [man 7 socket](https://man7.org/linux/man-pages/man7/socket.7.html)
* [Winsock Reference](https://learn.microsoft.com/en-us/windows/win32/winsock/socket-options)

---

> **Questions, suggestions, or improvements?**
> Please open an [issue](https://github.com/MangaD/jsocketpp/issues) or contribute directly!
