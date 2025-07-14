# Address Reuse in Sockets: `SO_REUSEADDR` vs `SO_EXCLUSIVEADDRUSE`

<!--!
\defgroup address_reuse Address Reuse in Sockets: `SO_REUSEADDR` vs `SO_EXCLUSIVEADDRUSE`
\ingroup docs
\hidegroupgraph
[TOC]
-->

Cross-platform socket programming requires careful handling of address reuse settings to avoid unexpected behavior,
especially when dealing with server sockets that are frequently restarted.

This article explains the behavior and usage of `SO_REUSEADDR`, `SO_EXCLUSIVEADDRUSE`, and `SO_REUSEPORT` on different
operating systems, highlights the key differences, analyzes Java’s design decisions, and proposes a robust strategy for
use in this cross-platform C++ socket library.

---

## 🔍 In-Depth Overview of Address Reuse Options

### `SO_REUSEADDR`

* **Definition**: Allows a socket to bind to an address that is in the `TIME_WAIT` state or may still be lingering.
  See [socket_states.md](./socket_states.md) to learn about socket states.
* **Purpose**: Enables servers to restart and bind to the same port without waiting for previous connections to fully
  terminate.
* **Platform-Specific Behavior**:

  #### On Unix/Linux:

  `SO_REUSEADDR` on Unix-like systems behaves differently than on Windows and is generally considered safe and useful —
  but only when its semantics are well understood.

  ##### 🔄 What it actually does

    * `SO_REUSEADDR` allows a socket to **bind** to an address even if:

        * A previous socket bound to the same address is in the `TIME_WAIT` state.
        * The same port is already bound **by another socket**, but only under specific conditions.

    * It does **not** allow arbitrary multiple sockets to bind to the **same full address (IP + port)** simultaneously.
      It will return `EADDRINUSE` unless:

        * All sockets set `SO_REUSEADDR` *before* binding.
        * The sockets bind to different interfaces (e.g., `127.0.0.1` vs `0.0.0.0` vs a specific IP).
        * The kernel allows it (varies slightly between BSD/Linux implementations).

  ##### 📦 Common use cases

    * Restarting a server (e.g., NGINX, HAProxy, Redis) that previously closed a socket still in `TIME_WAIT`.
    * Rebinding to an ephemeral port after a client disconnects.
    * Binding a socket that needs to share a port with another **non-conflicting** binding (e.g., a broadcast/multicast
      listener).

  ##### ⚠️ Misconceptions and caveats

    * It does **not** permit multiple listeners on the exact same `(ip, port)` to receive traffic at the same time —
      that is the purpose of `SO_REUSEPORT`.
    * If multiple sockets successfully bind with `SO_REUSEADDR`, only **one** of them will receive incoming connections.
      This is typically the last one to bind, but behavior is not guaranteed.
    * When improperly used, it can lead to one process inadvertently "stealing" traffic from another.

  ##### 🧠 Practical implications

    * Without `SO_REUSEADDR`, restarting a server may fail with `EADDRINUSE` until the OS releases the port (typically
      after 60 seconds in `TIME_WAIT`).
    * With it, server restarts can be smooth and quick, avoiding port conflicts.
    * It does **not** override exclusive binds from other users (permissions apply).
    * Used by almost every server daemon and framework on Unix.

  ##### 🧪 Socket state handling

  | Previous socket state | Can bind with `SO_REUSEADDR`? |
         | --------------------- | ----------------------------- |
  | `TIME_WAIT`           | ✅ Yes                         |
  | `CLOSE_WAIT`          | ✅ Yes                         |
  | `ESTABLISHED`         | ❌ No                          |
  | `LISTEN`              | ❌ No                          |
  | Unbound               | ✅ N/A                         |

  In essence: `SO_REUSEADDR` **does not allow stealing a live port from another process.** It enables rebinding in
  *transitional* states (e.g., `TIME_WAIT`) and makes controlled reuse easier within cooperating processes.

  ##### 🚀 Combining with `SO_REUSEPORT`

    * When **used together**, `SO_REUSEADDR` and `SO_REUSEPORT` allow multiple sockets (in the same or different
      processes) to listen concurrently on the **same IP\:port** and receive connections in parallel.

    * This is common in load-balanced multi-process servers:

      ```cpp
      int sock = socket(...);
      setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, ...);
      setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, ...);
      bind(sock, ...);
      listen(sock, SOMAXCONN);
      ```

    * Without `SO_REUSEPORT`, `SO_REUSEADDR` is *not* sufficient for true concurrent binding or parallel connection
      handling.

  #### On Windows:

    * `SO_REUSEADDR` allows binding to a port **even if it is still actively bound by another socket**.
    * This means two sockets in different processes could both bind to the same port and **both receive data**, leading
      to race conditions, data loss, or application errors.
    * Hence, this is **not safe for use in server applications** on Windows.

#### On Windows:

    The behavior of `SO_REUSEADDR` on Windows diverges significantly from its Unix counterpart — and **can be unsafe if misunderstood**.

    ##### 🧨 What it actually does

    * On Windows, setting `SO_REUSEADDR` **before `bind()`** allows a socket to:

        * Bind to a port that is still in `TIME_WAIT`.
        * **Bind to a port already in use by another process or socket**, even if that socket is in `LISTEN` or `ESTABLISHED`.

    ##### ❗ Dangerous implications

    * **Multiple sockets (even in different processes)** can bind to the **exact same IP and port**, provided they all use `SO_REUSEADDR`.
    * Windows will not prevent this, nor guarantee which socket receives traffic.
    * This leads to **non-deterministic behavior**, where:

        * Incoming data may be delivered to any of the bound sockets.
        * TCP connections may be accepted by either process.
        * UDP datagrams can be lost, duplicated, or processed out of order.

    ##### 🚫 Not safe for servers

    This behavior **breaks the assumption** that only one server process can bind to a specific port, and is considered unsafe for:

    * Production services
    * Daemons
    * Applications requiring exclusive access

    ##### 🔐 Correct approach on Windows: `SO_EXCLUSIVEADDRUSE`

    To ensure a server has **exclusive access to its listening port**, Windows introduced `SO_EXCLUSIVEADDRUSE`:

    * Must be set before `bind()`.
    * Prevents **any** other socket (in any process) from binding to the same IP\:port.
    * Protects against port hijacking and unintended parallel bindings.

    ##### 🛠️ Example behavior

    | Socket 1                   | Socket 2                   | Both use SO\_REUSEADDR | Result                              |
    | -------------------------- | -------------------------- | ---------------------- | ----------------------------------- |
    | Bound                      | Tries to bind to same port | ✅ Yes                  | **Both succeed**, traffic split     |
    | Bound                      | Tries to bind to same port | ❌ No                   | `bind()` fails with `WSAEADDRINUSE` |
    | Uses `SO_EXCLUSIVEADDRUSE` | Any socket                 | N/A                    | Only one bind allowed               |

    ##### 🧪 TCP state implications

    * Windows does allow re-binding to sockets in `TIME_WAIT` when `SO_REUSEADDR` is used — similar to Unix.
    * **But it also permits re-binding to active or listening sockets** — which Unix forbids — making it **much more permissive and risky**.

    ##### 🧠 Summary

    | Feature                             | Unix `SO_REUSEADDR` | Windows `SO_REUSEADDR`  |
    | ----------------------------------- | ------------------- | ----------------------- |
    | Bind to port in `TIME_WAIT`         | ✅ Yes               | ✅ Yes                   |
    | Bind when another socket is bound   | ❌ No                | ✅ Yes (unsafe!)         |
    | Allow multiple sockets on same port | ❌ No                | ✅ Yes (risky)           |
    | Port hijacking risk                 | ❌ No                | ⚠️ Yes                  |
    | Use for server binding              | ✅ Yes               | ❌ No                    |
    | Safer alternative                   | —                   | ✅ `SO_EXCLUSIVEADDRUSE` |

    > 💡 Therefore, for **cross-platform server code**, it is essential to map `setReuseAddress(true)` to `SO_EXCLUSIVEADDRUSE` on Windows.

### `SO_EXCLUSIVEADDRUSE` (Windows only)

#### 🔐 Definition

`SO_EXCLUSIVEADDRUSE` is a Windows-specific socket option that **guarantees exclusive access to a given IP and port**.
When enabled, **no other socket** — in the same process or in other processes — can bind to the same address and port
combination **until the socket is closed**.

#### 🎯 Purpose

Its goal is to prevent:

* **Port hijacking** — where another process binds to a port that should be owned by a server.
* **Accidental or malicious duplication** — where multiple server instances interfere with each other.
* **Ambiguity** — where incoming data could be routed to any of multiple listeners.

This option is especially important on Windows because its default behavior is much more permissive than on Unix-based
systems.

#### ⚙️ Behavior

* Must be set **before** calling `bind()`.
* Ensures **only one socket** may bind to a given port at a time.
* Even if another socket attempts to bind using `SO_REUSEADDR`, it will **fail** if `SO_EXCLUSIVEADDRUSE` is in effect
  on the bound socket.
* Once the socket is closed, the port becomes available again.

> 🔄 Unlike Unix `SO_REUSEADDR`, it does **not** allow rebinding while in `TIME_WAIT`. You must wait for the OS to
> release the port or restart the process. If you close a socket and immediately try to bind a new one to the same
> port (
> e.g., on server restart), it will fail if the previous socket was not properly closed or if Windows still holds it in
`TIME_WAIT`. To ensure a smooth restart: Properly close sockets before shutdown; Avoid forcibly terminating the server
> process; Consider using `netsh int tcp set global MaxUserPort=...` and related commands to manage port exhaustion if
> relevant; You could provide a fallback mechanism: e.g., if `bind()` fails due to `WSAEADDRINUSE`, show a clear error
> or
> delay retry.

#### ⚠️ Incompatibility

* **Cannot be combined meaningfully with `SO_REUSEADDR`**.

    * If both are set, `SO_EXCLUSIVEADDRUSE` takes precedence.
    * In practice, this combination is discouraged because `SO_REUSEADDR` serves no useful purpose when exclusivity is
      enforced.

#### ✅ Use Case

This is the **recommended option** for **server applications on Windows**, especially those that:

* Need strong security guarantees (e.g., preventing malicious binding).
* Expect to run only a single instance.
* Must avoid data races or contention between multiple sockets.

#### 🛠️ How it works internally

* The Windows kernel enforces a strict binding policy for sockets with `SO_EXCLUSIVEADDRUSE`.
* It denies subsequent bind attempts from **any** socket that doesn't own the port, regardless of `SO_REUSEADDR`.
* Once the socket is closed, the address is fully released.

#### 🧠 Summary

| Behavior                                   | `SO_EXCLUSIVEADDRUSE` |
|--------------------------------------------|-----------------------|
| Exclusive control over port                | ✅ Yes                 |
| Prevents other binds to same port          | ✅ Yes                 |
| Binds allowed while in `TIME_WAIT`         | ❌ No                  |
| Safer alternative to `SO_REUSEADDR` on Win | ✅ Yes                 |
| Useful for production servers              | ✅ Absolutely          |

> 💡 `jsocketpp` automatically enables `SO_EXCLUSIVEADDRUSE` on Windows when `setReuseAddress(true)` is called (which
> happens by default in `ServerSocket`).

### `SO_REUSEPORT` (Unix/Linux only)

* **Definition**:
  `SO_REUSEPORT` allows multiple sockets within the **same host** to bind to the **same port and IP address**, enabling
  **kernel-level load balancing** between them.

* **Primary Purpose**:
  Improve scalability and performance of **multi-threaded** or **multi-process** server applications. Each worker thread
  or process can bind its own socket to the same port, and the kernel distributes incoming packets.

* **How It Works**:

    * All sockets **must set `SO_REUSEPORT` before `bind()`**.
    * All sockets **must bind to the exact same port and address**.
    * The kernel performs **fair distribution** of incoming connections or datagrams among the bound sockets.
    * This avoids the need for synchronization between threads/processes in user-space.

* **Typical Use Cases**:

    * High-performance web servers like **NGINX**, **HAProxy**, and **systemd socket activation**.
    * Servers with **multiple threads or processes** listening on the same port to parallelize work.

* **Compatibility**:

    * Available on **Linux ≥ 3.9** and **FreeBSD ≥ 10**.
    * Not available on **macOS**, unless using the newer `SO_REUSEPORT_LB` extension (not portable).
    * **Not available on Windows**.

* **Key Advantages**:

    * **No need for accept-mutex or lock-based load balancing** in user space.
    * **Improves cache locality**: each thread handles its own clients.
    * **Scales better** on multicore systems under heavy load.

* **Limitations & Cautions**:

    * Requires **careful design**: all sockets must be set up identically.
    * If one process crashes or behaves incorrectly, it can still affect shared traffic.
    * **Security risk** if multiple untrusted processes bind to the same port.

* **Example**:

  ```cpp
  int sock1 = socket(AF_INET, SOCK_STREAM, 0);
  int sock2 = socket(AF_INET, SOCK_STREAM, 0);

  int optval = 1;
  setsockopt(sock1, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
  setsockopt(sock2, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

  bind(sock1, ...);
  bind(sock2, ...);

  listen(sock1, SOMAXCONN);
  listen(sock2, SOMAXCONN);
  ```

* **Comparison with `SO_REUSEADDR`**:

  | Feature               | `SO_REUSEADDR`           | `SO_REUSEPORT`                    |
          | --------------------- | ------------------------ | --------------------------------- |
  | Multiple bind allowed | Sometimes (see OS rules) | ✅ Always if `SO_REUSEPORT` is set |
  | Load balancing        | ❌ Only last wins         | ✅ Kernel distributes load         |
  | Thread/process safety | ❌ Risk of collisions     | ✅ Independent sockets             |
  | Available on Windows? | ✅ Yes                    | ❌ No                              |

* **Best Practice**:
  Use `SO_REUSEPORT` only if:

    * You are targeting **Linux 3.9+ or FreeBSD 10+**.
    * Your architecture benefits from high parallelism.
    * You want **manual control over per-core listeners**.
    * You fully understand the implications.

> 🛑 **Not portable!** Avoid making it the default behavior in a cross-platform library. Instead, provide a way for
> advanced users to enable it via `setOption(...)`.

---

## 🧪 What Happens When Multiple Sockets Bind to the Same Port?

This behavior depends heavily on which socket options are used and which operating system the code is running on. Here’s
a breakdown of the possibilities:

---

### Unix/Linux

#### **With `SO_REUSEADDR` only**:

* Multiple sockets in the same process or in different processes can bind to the same port, **but only if all of them
  set `SO_REUSEADDR` before binding**.
* However, **only the last socket to bind will receive incoming connections**.
* The others remain bound but receive **no data**.
* This behavior is often **misunderstood** and may lead to subtle bugs if used incorrectly.
* Example use case: Restarting a server that was listening on a port stuck in `TIME_WAIT`.

#### **With `SO_REUSEADDR` + `SO_REUSEPORT`**:

* All sockets that bind to the same port **and set both options** before binding can **receive connections concurrently
  **.
* The kernel performs **load balancing** among the sockets.
* Typically used by high-performance servers with **multi-threaded** or **multi-process** architectures.

> ⚠️ You must still ensure all sockets bind to the same address and port with matching options. Any inconsistency can
> lead to undefined behavior or silent failure.

---

### Windows

#### **With `SO_REUSEADDR`**:

* **Highly permissive**, and potentially dangerous.
* Allows **multiple processes** to bind to the **same port** even if another process is already using it.
* If multiple sockets bind to the same port (even in different processes), **they may all receive the same data**,
  leading to:

    * Data duplication
    * Message interleaving
    * Connection hijacking
* This is **unsafe for server sockets** and is **not recommended**.

#### **With `SO_EXCLUSIVEADDRUSE` (default in jsocketpp)**:

* Prevents any other socket or process from binding to the same port **until the current socket is closed**.
* The safest and most predictable behavior on Windows.
* If another process tries to bind to the port, `bind()` fails with `WSAEADDRINUSE`.

---

### Summary Table

| Platform   | Socket Option(s)           | Multiple Binds | Who Receives Data?                | Safe?      |
|------------|----------------------------|----------------|-----------------------------------|------------|
| Unix/Linux | `SO_REUSEADDR`             | ✅ (if all set) | ❌ Only the last one to bind       | ⚠️ Caution |
| Unix/Linux | `SO_REUSEADDR + REUSEPORT` | ✅              | ✅ All sockets (load balanced)     | ✅ Yes      |
| Windows    | `SO_REUSEADDR`             | ✅              | ❌ All sockets (data race, unsafe) | ❌ No       |
| Windows    | `SO_EXCLUSIVEADDRUSE`      | ❌              | ✅ One socket exclusively          | ✅ Yes      |

---

## ⚠️ Java’s Approach and Its Limitations

Java’s networking API attempts to simplify socket configuration across platforms, but this comes at a cost —
particularly on Windows.

### What Java Does

* The `ServerSocket` class in Java **automatically enables `SO_REUSEADDR`** by default.
* This allows quick server restarts without waiting for the previous socket to fully leave the `TIME_WAIT` state.
* This behavior matches Unix/Linux expectations but **does not distinguish between platforms**.

### Why This is Problematic on Windows

On Windows, enabling `SO_REUSEADDR` **does not behave the same way**:

* It allows **multiple sockets across different processes** to bind to the **same port** concurrently.
* This can cause:

    * **Race conditions**: Two servers listening on the same port may both receive traffic unpredictably.
    * **Security risks**: Malicious or accidental hijacking of network traffic.
    * **Debugging nightmares**: Symptoms may not appear until runtime, and are often non-deterministic.

Java **does not use** `SO_EXCLUSIVEADDRUSE`, which would prevent this issue and is considered the safe default for
Windows server sockets.

---

### Summary of Java’s Design Trade-Off

| Platform | Behavior                    | Consequence                                             |
|----------|-----------------------------|---------------------------------------------------------|
| Unix     | Enables `SO_REUSEADDR`      | Matches expectations. Allows restarts. Safe.            |
| Windows  | Enables `SO_REUSEADDR` only | Unsafe. Allows multiple bindings. May cause data races. |

* Java’s choice prioritizes **restartability and simplicity** over **strict correctness and security**.
* While convenient on Linux, it leads to **risky behavior on Windows**, especially for long-running or concurrent server
  applications.

---

### A Better Alternative: What `jsocketpp` Does

| Feature                 | Java           | jsocketpp                 |
|-------------------------|----------------|---------------------------|
| OS-aware address reuse  | ❌ No           | ✅ Yes                     |
| Safe default on Windows | ❌ Uses `REUSE` | ✅ Uses `EXCLUSIVEADDRUSE` |
| Allows override via API | ✅ Yes          | ✅ Yes                     |
| Explains trade-offs     | ❌ No           | ✅ Comprehensive docs      |

By handling `SO_REUSEADDR` and `SO_EXCLUSIVEADDRUSE` appropriately for each platform, `jsocketpp` ensures:

* Safe, predictable binding behavior.
* Compatibility with modern OS expectations.
* Restartable servers without risk of hijacking.

> ✅ This strategy avoids the shortcomings of Java’s one-size-fits-all approach while preserving flexibility for advanced
> users.

---

## ✅ Smarter Strategy for jsocketpp

### Recommended Behavior

| OS      | Reuse Option          | Behavior                                                        |
|---------|-----------------------|-----------------------------------------------------------------|
| Windows | `SO_EXCLUSIVEADDRUSE` | Safe: prevents any other process from binding the port.         |
| Unix    | `SO_REUSEADDR`        | Allows port reuse in `TIME_WAIT`, safe within a single process. |

### Constructor Defaults

* `ServerSocket` automatically enables safe defaults:

    * `setReuseAddress(true)` maps to:

        * `SO_EXCLUSIVEADDRUSE` on Windows
        * `SO_REUSEADDR` on Unix/Linux

### Developer Control

* `setReuseAddress(bool)` remains available to developers.
* Must be called **before `bind()`**.
* Does nothing once the socket is bound.

### Why Not Expose Windows `SO_REUSEADDR`?

* It’s simply unsafe and not aligned with best practices.
* Allowing it would cause undefined behavior that’s hard to debug.

---

## 💡 Summary Table

| Option                | Platform | Safe Default | Allows Multiple Binds | Allows TIME\_WAIT Reuse | Notes                                |
|-----------------------|----------|--------------|-----------------------|-------------------------|--------------------------------------|
| `SO_REUSEADDR`        | Unix     | ✅ Yes        | ⚠️ Last bind wins     | ✅ Yes                   | OK for restarts                      |
| `SO_REUSEADDR`        | Windows  | ❌ No         | ❌ Unsafe              | ✅ Yes                   | Do not use for servers               |
| `SO_REUSEPORT`        | Unix     | Optional     | ✅ Yes (load balanced) | ✅ Yes                   | For multi-thread/process performance |
| `SO_EXCLUSIVEADDRUSE` | Windows  | ✅ Yes        | ❌ No                  | ❌ No                    | Ensures exclusive use                |

---

## 📌 Notes for Library Authors

* Always apply reuse settings **before** calling `bind()`.
* Prefer safe defaults tailored to each OS.
* Make `setReuseAddress()` explicit in the API.
* Optionally add warnings if unsafe options are requested.

---

## 🔗 References

* [socket(7) - Linux Man Pages](https://man7.org/linux/man-pages/man7/socket.7.html)
* [Microsoft Docs on
  `SO_EXCLUSIVEADDRUSE`](https://learn.microsoft.com/en-us/windows/win32/winsock/using-so-exclusiveaddruse)
* [Java Socket Options](https://docs.oracle.com/javase/8/docs/api/java/net/ServerSocket.html)
* [Linux `SO_REUSEPORT`](https://lwn.net/Articles/542629/)

---

## ✅ Conclusion

Correct handling of socket address reuse options is essential for writing reliable and portable server applications,
especially when targeting both Unix-like systems and Windows.

By default, many programming environments (like Java) apply `SO_REUSEADDR` without considering the platform-specific
nuances. While convenient on Unix, this can introduce serious issues on Windows due to how `SO_REUSEADDR` behaves
there — enabling unintentional port sharing and data delivery ambiguity.

### ✔ What jsocketpp Gets Right

The `jsocketpp` library provides **safe, OS-aware defaults** that follow best practices:

* On **Windows**, `setReuseAddress(true)` internally applies `SO_EXCLUSIVEADDRUSE`, ensuring exclusive port ownership
  and eliminating race conditions.
* On **Unix/Linux**, `setReuseAddress(true)` enables `SO_REUSEADDR`, allowing fast socket rebinding in typical server
  restart scenarios.
* The library **does not expose unsafe behavior** such as `SO_REUSEADDR` on Windows, avoiding security and stability
  risks.
* It provides clear, documented methods for advanced control (`setOption`, `getOption`), while giving safe defaults to
  newcomers.

### 🧠 Beyond the Basics

* Developers working on high-performance, multi-threaded servers can use `SO_REUSEPORT` (on Unix/Linux) for concurrency
  and graceful reloads.
* With helper methods like `setReuseAddress()`, `isBound()`, and platform-adaptive behavior, `jsocketpp` strikes a
  thoughtful balance between **Java-style simplicity** and **system-level flexibility**.

---

### ✅ Best Practices Recap

| Platform | Safe Reuse Option     | Default Behavior in jsocketpp       |
|----------|-----------------------|-------------------------------------|
| Unix     | `SO_REUSEADDR`        | Enabled via `setReuseAddress(true)` |
| Windows  | `SO_EXCLUSIVEADDRUSE` | Enabled via `setReuseAddress(true)` |

| Feature                       | Unix (`SO_REUSEADDR`)             | Windows (`SO_EXCLUSIVEADDRUSE`) |
|-------------------------------|-----------------------------------|---------------------------------|
| Fast restart after TIME\_WAIT | ✅ Yes                             | ✅ Yes (if port is truly free)   |
| Prevent other binds           | ⚠️ No                             | ✅ Yes                           |
| Multiple sockets on one port  | ❌ No (unless with `SO_REUSEPORT`) | ❌ No                            |

---

By designing with platform-specific behaviors in mind and exposing a clear, Java-inspired API, `jsocketpp` helps
developers avoid subtle pitfalls while remaining flexible and powerful.
