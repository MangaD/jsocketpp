# Understanding `bind()`: Binding to All Interfaces vs Specific Interfaces

<!--!
\defgroup bind_interfaces Understanding `bind()`: Binding to All Interfaces vs Specific Interfaces
\ingroup docs
\hidegroupgraph
[TOC]
-->

Socket programming often involves choosing **which local addresses and interfaces** your server will listen on. This
article explains what it means to “bind to all interfaces,” how it works under the hood, and how to select specific
interfaces if needed.

## What Does “Binding to All Interfaces” Mean?

When you create a listening socket (e.g., for a server) you must “bind” it to a local address and port. This address
determines:

- Which incoming connections or datagrams will be delivered to your socket.
- Which network interfaces (e.g., Ethernet, Wi-Fi, loopback) will be monitored.

### Wildcard Addresses

- **IPv4:** `0.0.0.0`
- **IPv6:** `::`

These addresses, often called **wildcard addresses**, are a special signal to the OS:
> “Accept connections or datagrams on *any* address of this machine, on the specified port, for this protocol family.”

When you bind your socket to a wildcard address, the OS will deliver to you any incoming traffic on that port, no matter
which network interface it arrives on.

### Example: Binding to All Interfaces

```cpp
// Using getaddrinfo with node=nullptr and AI_PASSIVE hint
addrinfo hints{};
hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
hints.ai_socktype = SOCK_STREAM; // TCP
hints.ai_flags = AI_PASSIVE;     // For wildcard binding

addrinfo* res;
getaddrinfo(nullptr, "8080", &hints, &res);

// Pick the first result and bind
int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
bind(sockfd, res->ai_addr, res->ai_addrlen);
````

This binds to **all local addresses** (both IPv4 and IPv6 if dual-stack is enabled), on port 8080, with one socket per
address family.

---

## How Does jsocketpp Handle This?

jsocketpp, like most modern libraries, does this:

1. Calls `getaddrinfo(nullptr, ...)` with the `AI_PASSIVE` flag.
2. Iterates the resulting list of wildcard addresses (e.g., `0.0.0.0`, `::`).
3. Prefers to bind to the IPv6 wildcard address if possible, which can accept both IPv6 and IPv4 (if dual-stack is
   enabled).
4. Binds a socket to that wildcard address—so *one socket* receives all incoming connections for that protocol family,
   on all interfaces.

**You do NOT need to create a socket per interface.**

---

## Binding to a Specific Address or Interface

Sometimes you want your server to only accept connections on a specific interface or address, such as:

* Only on the loopback interface (localhost, `127.0.0.1`)
* Only on an external IP address
* Only on a specific network interface (advanced; see below)

**How?**
Instead of passing `nullptr` to `getaddrinfo`, you specify the address you want to bind to:

```cpp
// Only listen on localhost
getaddrinfo("127.0.0.1", "8080", &hints, &res);
// ... bind as usual

// Only listen on a specific IP
getaddrinfo("192.168.1.123", "8080", &hints, &res);
```

The kernel will only deliver incoming connections or datagrams **addressed to that specific IP/interface** to your
socket.

---

## What About Binding to All Interfaces on All Protocols?

* **Dual-Stack (IPv6 and IPv4):**
  On modern systems, you can bind a single IPv6 socket to `"::"` and disable `IPV6_V6ONLY`.
  This one socket then receives both IPv4 and IPv6 traffic.

* **Older Systems:**
  You may need two sockets—one for `"::"` (IPv6) and one for `"0.0.0.0"` (IPv4).

---

## Table: Summary of Common Bind Addresses

| Address         | Binds to...                                           | Use Case                       |
|-----------------|-------------------------------------------------------|--------------------------------|
| `0.0.0.0`       | All IPv4 interfaces                                   | Most servers (IPv4 only)       |
| `::`            | All IPv6 interfaces, often also all IPv4 (dual-stack) | Most servers (IPv6+IPv4)       |
| `127.0.0.1`     | Loopback only (localhost, IPv4)                       | Local testing, admin-only APIs |
| `::1`           | Loopback only (localhost, IPv6)                       | Local testing, admin-only APIs |
| `<specific IP>` | Only that IP/interface                                | Multi-homed servers, advanced  |

---

## Advanced: Binding to a Named Interface

On some OSes, you can bind to a specific interface by using its address, or, with advanced socket options (
`SO_BINDTODEVICE` on Linux), by name. This is rarely needed for most server applications.

```cpp
// Linux only: restrict socket to eth0
setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, "eth0", strlen("eth0"));
```

*Note: Not portable!*

---

## Best Practices

* For most servers, **binding to the wildcard address** (`0.0.0.0` or `::`) is best. This allows clients to connect
  regardless of which network they use to reach your server.
* Bind to a specific address **only** when you have a reason (security, access control, multiple processes, etc.).
* Let jsocketpp’s default behavior handle this for you unless you have advanced requirements.

---

## jsocketpp API Notes

* By default, jsocketpp binds to all local addresses for the protocol family (one socket).
* You can customize the address passed to the constructor to bind only to a specific interface/address if desired.
* The library prefers dual-stack (IPv6) when possible for maximum reach.

---

## Conclusion

> **Binding to all interfaces** is simple, robust, and sufficient for most server applications.
> Advanced use-cases (multi-homing, interface binding) are supported but rarely needed.

If you need to bind to a specific address/interface in jsocketpp, simply pass the address string to the constructor or
to your bind logic. Otherwise, let the default do the right thing.
