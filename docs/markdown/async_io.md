# Blocking vs Non-Blocking Sockets in jsocketpp

Modern socket programming requires a solid understanding of how blocking, non-blocking, and asynchronous I/O work. These
modes of operation directly impact performance, resource usage, and API design.

This article provides a comprehensive guide on blocking vs non-blocking sockets, timeouts, their interaction with system
calls (`connect`, `accept`, `recv`, `send`), and how `jsocketpp` implements these behaviors for safety and flexibility.

---

## 🚦 Blocking vs Non-Blocking Sockets

### What is Blocking?

In blocking mode (the default), a system call will pause the current thread until it completes:

* `connect()` blocks until the connection is established or fails.
* `accept()` blocks until a new connection is ready.
* `recv()` blocks until data arrives.
* `send()` blocks until the data can be written to the socket buffer.

### What is Non-Blocking?

In non-blocking mode, system calls **return immediately**:

* If the operation cannot proceed (e.g. no data to read), it fails with `EWOULDBLOCK` or `EAGAIN`.
* It becomes your job to retry or use a mechanism like `select()` or `poll()` to check readiness.

Non-blocking mode is often used in event-driven programming, such as GUI applications, game servers, and
high-performance web servers.

---

## 🕐 Socket Timeouts

Timeouts define how long a blocking operation will wait before aborting. There are two ways to apply timeouts:

### OS-Level Timeouts

* Use `SO_RCVTIMEO` and `SO_SNDTIMEO` socket options.
* Not reliably portable: behavior varies across platforms and is known to be inconsistent on Windows.
* They only affect system calls like `recv()`, `send()`, and `accept()`, but **do not apply to `select()`, `poll()`,
  or `epoll()`**.
* Often less flexible compared to using `select()` explicitly.

### Custom Timeouts via `select()`

* Preferred method in `jsocketpp`.
* Portable across Windows and POSIX systems.
* Works for any operation by waiting for readiness before executing the syscall.
* Enables better error reporting and control.

---

## ⏱ How jsocketpp Handles Blocking and Timeouts

`jsocketpp` offers:

* Default **blocking mode** for safety and simplicity.
* `setNonBlocking(bool)` to enable non-blocking behavior.
* Custom timeouts using `select()` internally.
* Exceptions like `SocketTimeoutException` when timeouts occur.
* Dedicated methods (`acceptBlocking`, `accept(timeout)`, `tryAccept()`) for each strategy.

### Design Rationale

| Feature            | jsocketpp Approach                    |
|--------------------|---------------------------------------|
| Blocking mode      | Default                               |
| Timeout support    | via `select()`, not OS socket options |
| Non-blocking mode  | Opt-in via `setNonBlocking(true)`     |
| Timeout exceptions | Yes, using `SocketTimeoutException`   |
| Event loop support | Yes, with `waitReady()` and polling   |

---

## 🔄 Operation-Specific Behavior: `connect`, `accept`, `recv`, and `send`

Each socket operation behaves differently under non-blocking mode or when paired with timeout mechanisms like
`select()`. Understanding these distinctions is essential for designing reliable I/O logic.

### 🔌 `connect`

#### Blocking mode:

* Blocks until the connection is fully established or fails.
* Uses `SO_SNDTIMEO` if supported, but better to use `select()` with a timeout.

#### Non-blocking mode:

* Returns immediately; if connection is in progress, `errno` = `EINPROGRESS`.
* Use `select()` to wait for **writability**, then check `SO_ERROR` with `getsockopt()`.

#### Java:

* Uses a dedicated `connect(SocketAddress, timeout)` method.
* Internally sets the socket to non-blocking, uses `select()`, then reverts to blocking mode.

### 📞 `accept`

#### Blocking mode:

* Blocks until a new connection is ready.
* Timeout managed via `SO_RCVTIMEO` or `select()` before calling `accept()`.

#### Non-blocking mode:

* Returns immediately.
* Use `select()` to wait for **readability** before calling `accept()`.

#### Java:

* Uses `ServerSocket.setSoTimeout()`.
* Blocks with a timeout; exception thrown on timeout.

### 📥 `recv` / `read`

#### Blocking mode:

* Blocks until data is available or the connection closes.
* Timeout managed via `SO_RCVTIMEO` or `select()`.

#### Non-blocking mode:

* Returns immediately.
* Returns `-1` with `EWOULDBLOCK` if no data.
* Use `select()` to wait for **readability**.

### 📤 `send` / `write`

#### Blocking mode:

* Blocks until data is sent or buffered.
* Controlled by `SO_SNDTIMEO` or `select()`.

#### Non-blocking mode:

* Returns immediately.
* Returns `-1` with `EWOULDBLOCK` if the buffer is full.
* Use `select()` to wait for **writability**.

---

## ✅ Summary Table: Waiting for Readiness

| Operation | Blocking           | Non-blocking w/ `select()` | Wait For            |
|-----------|--------------------|----------------------------|---------------------|
| `connect` | Waits to establish | Wait for **writability**   | Connection complete |
| `accept`  | Waits for client   | Wait for **readability**   | Client pending      |
| `recv`    | Waits for data     | Wait for **readability**   | Data ready          |
| `send`    | Waits for buffer   | Wait for **writability**   | Buffer availability |

---

## 📚 Java Comparison

Java socket classes wrap native sockets and hide non-blocking logic. Java uses:

* Blocking I/O by default.
* Timeouts with `connect(timeout)` and `ServerSocket.setSoTimeout()`.
* Internally uses non-blocking mode with `select()` then reverts to blocking.
* Does **not expose** non-blocking sockets directly.

### Limitations:

* Less flexibility.
* Not well-suited for high-performance event loops.

`jsocketpp` improves on this by:

* Offering both blocking and non-blocking APIs.
* Providing portable timeouts via `select()`.
* Supporting `waitReady()` for fine-grained control.

---

## 🧠 Final Thoughts and Best Practices

* Use **blocking mode** by default unless you're building a custom event loop.
* Use `waitReady()` to wait for a socket to become readable/writable.
* Avoid OS-level timeout options (`SO_RCVTIMEO`, `SO_SNDTIMEO`) for portability.
* Throw or handle timeout errors using exceptions (`SocketTimeoutException`).
* Prefer select-based timeouts even for blocking sockets.

### Example Use:

```cpp
if (server.waitReady(5000)) {
    Socket client = server.acceptBlocking();
} else {
    std::cerr << "No connection within timeout" << std::endl;
}
```

---

For deeper technical references:

* [Linux `select(2)`](https://man7.org/linux/man-pages/man2/select.2.html)
* [POSIX `fcntl`](https://man7.org/linux/man-pages/man2/fcntl.2.html)
* [Winsock Non-Blocking I/O](https://learn.microsoft.com/en-us/windows/win32/winsock/)
* [Java Socket API Docs](https://docs.oracle.com/javase/8/docs/api/java/net/Socket.html)

---

By designing `jsocketpp` with thoughtful separation of blocking, timeouts, and async mechanisms, your applications gain
robustness and flexibility across platforms.
