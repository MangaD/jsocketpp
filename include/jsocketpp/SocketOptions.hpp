/**
 * @file SocketOptions.hpp
 * @brief Defines the SocketOptions base class for cross-platform socket option access.
 *
 * This header is part of the jsocketpp library and declares the `SocketOptions` class, a reusable,
 * lightweight base interface for setting and retrieving low-level socket options.
 *
 * The implementation is portable across POSIX and Windows platforms.
 * It is designed to be inherited by socket classes such as `Socket`, `ServerSocket`, `DatagramSocket`,
 * and `UnixSocket`, enabling shared access to `setsockopt()` and `getsockopt()` functionality.
 *
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

#pragma once

#include "common.hpp"

namespace jsocketpp
{

/**
 * @class SocketOptions
 * @brief Public base class for raw socket option access via `setsockopt()` and `getsockopt()`.
 * @ingroup socketopts
 *
 * The `SocketOptions` class provides a unified, cross-platform interface for working with
 * socket-level configuration options (e.g., `SO_REUSEADDR`, `SO_RCVBUF`, `TCP_NODELAY`).
 * It is designed to be inherited by classes that manage socket descriptors, enabling those
 * classes to expose low-level tunables without duplicating boilerplate code.
 *
 * ## Intended Usage
 * - Derived classes must pass a valid socket descriptor to the constructor of `SocketOptions`.
 * - After move construction or assignment, they must call `setSocketFd()` to update the descriptor.
 * - This class does **not** own the socket or manage its lifecycle (open, close, etc.).
 *
 * ## Supported Classes
 * - `Socket`
 * - `ServerSocket`
 * - `DatagramSocket`
 * - `UnixSocket`
 *
 * @note This class is **not thread-safe**. Access should be externally synchronized if needed.
 *
 * @see setOption()
 * @see getOption()
 */
class SocketOptions
{
  public:
    /**
     * @brief Initializes the socket option interface with a specific socket descriptor.
     * @ingroup socketopts
     *
     * Constructs a `SocketOptions` object that uses the provided socket file descriptor for all
     * subsequent `setOption()` and `getOption()` calls. This constructor is intended to be used
     * by derived socket classes (e.g., `Socket`, `ServerSocket`, `UnixSocket`) to delegate
     * option-handling responsibilities without duplicating system-specific logic.
     *
     * @param[in] sock A valid socket descriptor (e.g., from `socket()`, `accept()`, etc.), or
     *                 `INVALID_SOCKET` if the object is not yet initialized.
     *
     * @note It is the responsibility of the derived class to ensure that the socket descriptor
     *       remains valid for the lifetime of the `SocketOptions` object.
     *
     * @warning This class does not manage or close the socket descriptor. It assumes the derived
     *          class owns and controls the lifetime of the socket.
     *
     * @see setSocketFd()
     * @see setOption()
     * @see getOption()
     */
    explicit SocketOptions(const SOCKET sock) noexcept : _sockFd(sock) {}

    /**
     * @brief Virtual destructor for safe polymorphic destruction.
     * @ingroup socketopts
     *
     * Ensures that derived classes can be safely destroyed through a pointer to `SocketOptions`.
     * This is important for preventing resource leaks and undefined behavior when using
     * socket option interfaces polymorphically (e.g., in unit tests or abstraction layers).
     *
     * This destructor does **not** perform any cleanup of the socket descriptor itself,
     * as `SocketOptions` does not own or manage the socket lifecycle. Derived classes are
     * responsible for closing or releasing the socket if needed.
     *
     * @note Declared `virtual` to support inheritance and polymorphism.
     */
    virtual ~SocketOptions() = default;

    /**
     * @brief Retrieves the native socket handle (file descriptor or OS-level handle).
     * @ingroup socketopts
     *
     * This method provides low-level access to the socket’s operating system identifier:
     * - On **POSIX** systems, this is an integer file descriptor (`int`)
     * - On **Windows**, this is a `SOCKET` handle (`uintptr_t`-like)
     *
     * It is intended for **advanced use cases only**, such as:
     * - Integrating with external event loops (`select()`, `poll()`, `epoll`, `kqueue`)
     * - Passing the socket to platform APIs or native libraries
     * - Monitoring socket state or readiness using system primitives
     *
     * ---
     *
     * ### ⚠️ HANDLE WITH EXTREME CARE
     * This method exposes the raw handle **without ownership transfer**. Misuse may lead to:
     * - Resource leaks (if manually closed)
     * - Double-close or shutdown
     * - Corruption of internal state
     * - Thread-safety issues in multithreaded environments
     * - Broken invariants inside the jsocketpp abstraction
     *
     * ---
     *
     * ### ✅ Safe Usage Guidelines
     * - **DO** use this handle for non-destructive introspection (e.g., `poll()`, `select()`)
     * - **DO NOT** call `close()`, `shutdown()`, or `setsockopt()` directly unless you fully understand the
     * consequences
     * - **DO NOT** store the handle beyond the lifetime of the socket object
     * - **DO NOT** share across threads without synchronization
     *
     * ---
     *
     * ### Example: Use with `select()`
     * @code
     * fd_set readSet;
     * FD_ZERO(&readSet);
     * FD_SET(socket.getSocketFd(), &readSet);
     *
     * if (::select(socket.getSocketFd() + 1, &readSet, nullptr, nullptr, &timeout) > 0) {
     *     std::cout << "Socket is readable!\n";
     * }
     * @endcode
     *
     * @return The underlying OS-level socket identifier (`SOCKET` or file descriptor).
     *         Returns `INVALID_SOCKET` if the socket is not initialized.
     *
     * @see setSocketFd() – Internal method for synchronizing base state
     * @see setOption(), getOption()
     * @see waitReady(), setNonBlocking()
     */
    [[nodiscard]] SOCKET getSocketFd() const noexcept { return _sockFd; }

    /**
     * @brief Sets a low-level socket option on the underlying socket.
     * @ingroup socketopts
     *
     * This method configures a socket option using the system-level `setsockopt()` interface.
     * Socket options allow advanced users to fine-tune behavior related to performance,
     * protocol semantics, reliability, and resource usage.
     *
     * This API is available on all socket types provided by the library, including:
     * - `Socket` (TCP stream sockets)
     * - `ServerSocket` (listening sockets)
     * - `DatagramSocket` (UDP)
     * - `UnixSocket` (UNIX domain stream or datagram sockets)
     *
     * It supports options at multiple protocol levels, such as `SOL_SOCKET`, `IPPROTO_TCP`, and `IPPROTO_UDP`.
     *
     * ---
     *
     * ### 🔧 Common Use Cases
     * - Enable address reuse (`SO_REUSEADDR`) for TCP/UDP servers
     * - Enable TCP keepalive (`SO_KEEPALIVE`) for long-lived connections
     * - Tune send/receive buffer sizes (`SO_SNDBUF`, `SO_RCVBUF`)
     * - Disable Nagle’s algorithm (`TCP_NODELAY`) for latency-sensitive TCP clients
     * - Enable broadcast mode (`SO_BROADCAST`) on UDP sockets
     *
     * ---
     *
     * ### ⚠️ ServerSocket-specific Notes
     * - Setting options on a **listening socket** only affects the acceptor socket.
     * - Options do **not** propagate to client sockets returned by `accept()`.
     * - Common use: `SO_REUSEADDR` before `bind()`.
     *
     * @code
     * serverSocket.setOption(SOL_SOCKET, SO_REUSEADDR, 1);
     * @endcode
     *
     * ---
     *
     * ### 📡 DatagramSocket-specific Notes
     * - Useful for enabling `SO_BROADCAST`, tuning buffer sizes, or applying timeouts (`SO_RCVTIMEO`, `SO_SNDTIMEO`)
     * - For connected UDP sockets, options affect the single remote peer.
     * - For unconnected sockets, they apply to all traffic.
     *
     * @code
     * datagramSocket.setOption(SOL_SOCKET, SO_BROADCAST, 1);
     * @endcode
     *
     * ---
     *
     * ### 🧿 UnixSocket-specific Notes
     * - Although not TCP/IP-based, many `SOL_SOCKET` options apply:
     *   - `SO_RCVBUF`, `SO_SNDBUF` (buffer tuning)
     *   - `SO_PASSCRED` (credential passing)
     *   - `SO_RCVTIMEO`, `SO_SNDTIMEO` (timeouts)
     * - TCP-level options like `TCP_NODELAY` are **not supported**.
     *
     * @code
     * unixSocket.setOption(SOL_SOCKET, SO_SNDBUF, 65536);
     * @endcode
     *
     * ---
     *
     * @param[in] level   The protocol level at which the option resides (e.g., `SOL_SOCKET`, `IPPROTO_TCP`).
     * @param[in] optName The name of the socket option (e.g., `SO_REUSEADDR`, `TCP_NODELAY`).
     * @param[in] value   Integer value to assign to the option.
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - `setsockopt()` fails due to unsupported option or platform error
     *
     * @see getOption(int, int) For querying current option values
     * @see setOption(int, int, const void*, socklen_t) For structured or binary options
     * @see https://man7.org/linux/man-pages/man2/setsockopt.2.html
     */
    void setOption(int level, int optName, int value);

    /**
     * @brief Sets a low-level socket option using a structured or binary value.
     * @ingroup socketopts
     *
     * This overload of `setOption()` configures advanced socket options that require
     * passing complex values via a memory buffer, such as `struct linger`, protocol-specific
     * flags, or platform-defined structures.
     *
     * Socket options allow advanced users to customize performance, transport behavior,
     * and resource usage. This method is applicable to all supported socket types:
     *
     * - `Socket` (TCP stream sockets)
     * - `ServerSocket` (listening TCP sockets)
     * - `DatagramSocket` (UDP)
     * - `UnixSocket` (UNIX domain stream or datagram)
     *
     * It supports options at various protocol levels, including:
     * - `SOL_SOCKET` for generic socket behavior
     * - `IPPROTO_TCP` for TCP-specific options
     * - `IPPROTO_UDP`, `IPPROTO_IP`, etc.
     *
     * ---
     *
     * ### 🧩 Use Cases
     * - Configure linger behavior with `SO_LINGER` using `struct linger`
     * - Apply platform-specific options that require binary data
     * - Set multicast interface (`IP_MULTICAST_IF`) or IPv6 options
     *
     * ---
     *
     * ### Example: Set `SO_LINGER` to linger for 5 seconds on close
     * @code
     * linger lin;
     * lin.l_onoff = 1;
     * lin.l_linger = 5;
     * socket.setOption(SOL_SOCKET, SO_LINGER, &lin, sizeof(linger));
     * @endcode
     *
     * ---
     *
     * ### ⚠️ ServerSocket-specific Notes
     * - Only affects the passive/listening socket itself.
     * - Accepted sockets must be configured separately.
     *
     * ### 📡 DatagramSocket-specific Notes
     * - Can be used for multicast, broadcast, timeouts, etc.
     * - Accepts raw buffers for protocol-level socket options.
     *
     * ### 🧿 UnixSocket-specific Notes
     * - Common options include `SO_PASSCRED`, `SO_RCVBUF`, `SO_SNDTIMEO`
     * - TCP-specific options (e.g., `TCP_NODELAY`) are not supported.
     *
     * ---
     *
     * @param[in] level   Protocol level (e.g., `SOL_SOCKET`, `IPPROTO_TCP`)
     * @param[in] optName Option name (e.g., `SO_LINGER`, `IP_TOS`)
     * @param[in] value   Pointer to a buffer containing the option value
     * @param[in] len     Size of the buffer in bytes
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - `setsockopt()` fails due to invalid parameters or unsupported option
     *
     * @note This method is intended for structured or binary values.
     * For simple integer options, use `setOption(int, int, int)` instead.
     *
     * @see setOption(int, int, int)
     * @see getOption(int, int, void*, socklen_t*)
     * @see https://man7.org/linux/man-pages/man2/setsockopt.2.html
     */
    void setOption(int level, int optName, const void* value, socklen_t len);

    /**
     * @brief Retrieves the current value of a low-level socket option.
     * @ingroup socketopts
     *
     * This method queries a socket option using the system-level `getsockopt()` interface.
     * It returns the current value as an integer and is useful for inspecting platform
     * defaults, runtime configuration, or verifying changes made via `setOption()`.
     *
     * This method is supported by all socket types provided by the library:
     * - `Socket` (TCP)
     * - `ServerSocket` (listening)
     * - `DatagramSocket` (UDP)
     * - `UnixSocket` (UNIX domain)
     *
     * It supports standard protocol levels such as `SOL_SOCKET`, `IPPROTO_TCP`, and `IPPROTO_UDP`.
     *
     * ---
     *
     * ### 🔧 Use Cases
     * - Verify receive/send buffer sizes (`SO_RCVBUF`, `SO_SNDBUF`)
     * - Check if keepalive or broadcast is enabled (`SO_KEEPALIVE`, `SO_BROADCAST`)
     * - Inspect linger state (`SO_LINGER`) via the structured overload
     *
     * ---
     *
     * ### Example: Read configured receive buffer size
     * @code
     * int rcvBuf = socket.getOption(SOL_SOCKET, SO_RCVBUF);
     * @endcode
     *
     * ---
     *
     * ### ⚠️ ServerSocket-specific Notes
     * - Applies only to the listening socket.
     * - Use this to inspect pre-accept behavior such as:
     *   - `SO_REUSEADDR`, `SO_LINGER`, `SO_RCVBUF`, etc.
     * - Accepted client sockets must be queried individually.
     *
     * @code
     * int reuse = serverSocket.getOption(SOL_SOCKET, SO_REUSEADDR);
     * @endcode
     *
     * ---
     *
     * ### 📡 DatagramSocket-specific Notes
     * - Useful for checking `SO_BROADCAST`, `SO_RCVTIMEO`, `SO_SNDTIMEO`, and buffer sizes.
     * - Behavior is consistent across connected and unconnected UDP sockets.
     *
     * @code
     * int timeout = datagramSocket.getOption(SOL_SOCKET, SO_RCVTIMEO);
     * @endcode
     *
     * ---
     *
     * ### 🧿 UnixSocket-specific Notes
     * - `SO_RCVBUF`, `SO_SNDBUF`, `SO_PASSCRED`, and timeout options are supported.
     * - Can be used for IPC performance diagnostics.
     *
     * @code
     * int bufSize = unixSocket.getOption(SOL_SOCKET, SO_SNDBUF);
     * @endcode
     *
     * ---
     *
     * @param[in] level   Protocol level (e.g., `SOL_SOCKET`, `IPPROTO_TCP`)
     * @param[in] optName Socket option name (e.g., `SO_KEEPALIVE`, `SO_SNDBUF`)
     * @return            Integer value of the requested option
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - The option is unsupported or improperly configured
     * - The `getsockopt()` call fails
     *
     * @see setOption(int, int, int)
     * @see getOption(int, int, void*, socklen_t*) For retrieving structured options
     * @see https://man7.org/linux/man-pages/man2/getsockopt.2.html
     */
    [[nodiscard]] int getOption(int level, int optName) const;

    /**
     * @brief Retrieves a socket option into a structured or binary buffer.
     * @ingroup socketopts
     *
     * This overload of `getOption()` allows querying complex or platform-specific socket options
     * that require structured output (e.g., `struct linger`, `struct timeval`, etc.).
     * It wraps the system-level `getsockopt()` call using a raw memory buffer and returns
     * the result through the provided pointer.
     *
     * The semantics are identical to the `getOption(int, int)` overload, but this version
     * is used for retrieving non-integer options.
     *
     * This method works uniformly across:
     * - `Socket` (TCP)
     * - `ServerSocket` (listening TCP)
     * - `DatagramSocket` (UDP)
     * - `UnixSocket` (local IPC)
     *
     * ---
     *
     * ### 🔧 Use Cases
     * - Query `SO_LINGER` using `struct linger`
     * - Read timeout values into `struct timeval`
     * - Inspect platform-specific flags or metadata structures
     * - Use with low-level protocol options that return binary data
     *
     * ---
     *
     * ### Example: Read `SO_LINGER` setting
     * @code
     * linger lin{};
     * socklen_t len = sizeof(lin);
     * socket.getOption(SOL_SOCKET, SO_LINGER, &lin, &len);
     *
     * if (lin.l_onoff)
     *     std::cout << "Linger enabled for " << lin.l_linger << " seconds.\n";
     * else
     *     std::cout << "Linger is disabled.\n";
     * @endcode
     *
     * ---
     *
     * ### ⚠️ ServerSocket-specific Notes
     * - Applies to the listening socket only (not accepted clients).
     * - Common for querying `SO_LINGER`, `SO_RCVBUF`, etc.
     *
     * ---
     *
     * ### 📡 DatagramSocket-specific Notes
     * - Use to inspect structured timeouts (`SO_RCVTIMEO`) or multicast settings.
     * - Valid on both connected and unconnected sockets.
     *
     * ---
     *
     * ### 🧿 UnixSocket-specific Notes
     * - Retrieve `SO_PASSCRED`, buffer sizes, and timeouts using appropriate structures.
     * - Useful for debugging IPC characteristics and system resource tuning.
     *
     * ---
     *
     * @param[in]  level   Protocol level (e.g., `SOL_SOCKET`, `IPPROTO_TCP`)
     * @param[in]  optName Socket option name (e.g., `SO_LINGER`)
     * @param[out] result  Pointer to a buffer to receive the option value
     * @param[out] len     Pointer to the size of the buffer. On return, holds the actual size used.
     *
     * @throws SocketException if:
     * - The socket is invalid or closed
     * - `result` or `len` is null
     * - The option is unsupported or the system call fails
     *
     * @note The caller is responsible for allocating and sizing the buffer.
     * The `len` parameter must be set to the size of the buffer before the call.
     *
     * @see getOption(int, int) For integer-based options
     * @see setOption(int, int, const void*, socklen_t) For setting structured values
     * @see https://man7.org/linux/man-pages/man2/getsockopt.2.html
     */
    void getOption(int level, int optName, void* result, socklen_t* len) const;

    /**
     * @brief Enables or disables the `SO_REUSEADDR` socket option.
     * @ingroup socketopts
     *
     * This option controls whether the socket is permitted to bind to a local address and port
     * that is already in use or in the `TIME_WAIT` state. It is commonly used in both server
     * and client contexts, and is applicable to TCP, UDP, and Unix domain sockets.
     *
     * Internally, this method invokes:
     * @code
     * setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, ...)
     * @endcode
     *
     * ### Platform Behavior
     * - **POSIX (Linux, BSD, macOS):**
     *   - Permits multiple sockets to bind the same address/port as long as all use `SO_REUSEADDR`.
     *   - Commonly used with multicast UDP, and for servers that need to restart quickly.
     * - **Windows:**
     *   - Allows rebinding to a port in `TIME_WAIT`, but does **not** allow simultaneous binds.
     *   - Microsoft recommends `SO_EXCLUSIVEADDRUSE` for exclusive ownership, but `SO_REUSEADDR` works for most cases.
     *
     * ### Protocol Use Cases
     * - **TCP Server Sockets**: Enables re-binding to a port immediately after shutdown.
     * - **TCP Client Sockets**: Useful when binding to a fixed local port to reconnect rapidly.
     * - **UDP Sockets**:
     *   - Allows multiple sockets to bind to the same port, often required for multicast listeners.
     *   - Useful in P2P setups where port reuse is desired across short-lived sessions.
     * - **Unix Domain Sockets**:
     *   - On some platforms, `SO_REUSEADDR` may be ignored or unnecessary for `AF_UNIX`, since
     *     file system semantics already handle path reusability. However, setting it is harmless and may
     *     be supported by your platform for consistency.
     *
     * ### Example
     * @code
     * socket.setReuseAddress(true); // Enable port/path reuse before calling bind()
     * @endcode
     *
     * @warning This method must be called **after socket creation** and **before bind()**.
     *          Improper use can lead to security issues or interference between applications.
     *
     * @param[in] on If `true`, enables address reuse; if `false`, disables it.
     *
     * @throws SocketException if the socket is invalid or the option cannot be set.
     *
     * @see getReuseAddress()
     * @see bind()
     * @see https://man7.org/linux/man-pages/man7/socket.7.html
     */
    void setReuseAddress(bool on);

    /**
     * @brief Queries whether the socket is currently configured to allow address reuse.
     * @ingroup socketopts
     *
     * This method retrieves the state of the `SO_REUSEADDR` socket option using `getsockopt()`.
     * When enabled, it allows the socket to bind to a local address/port that is already in use
     * or still in the `TIME_WAIT` state. This is particularly useful in scenarios such as:
     *
     * - Restartable TCP servers that must rebind quickly
     * - UDP or multicast receivers sharing the same port
     * - Clients using fixed local ports across reconnects
     *
     * ### Platform Behavior
     * - **POSIX:** Multiple sockets can bind the same local address/port if all use `SO_REUSEADDR`.
     * - **Windows:** `SO_REUSEADDR` allows rebinding to a port in `TIME_WAIT`, but does **not**
     *   allow simultaneous binds. Additionally, `SO_EXCLUSIVEADDRUSE` is enabled by default
     *   and disables reuse — this method reflects the effective state of `SO_REUSEADDR`, not
     *   `SO_EXCLUSIVEADDRUSE`.
     *
     * ### Protocol Applicability
     * - **TCP:** Useful for both client and server sockets.
     * - **UDP:** Required for multicast and shared-port scenarios.
     * - **Unix domain sockets:** `SO_REUSEADDR` may be supported but is typically a no-op;
     *   Unix path conflicts are governed by the file system.
     *
     * ### Example
     * @code
     * if (!socket.getReuseAddress()) {
     *     socket.setReuseAddress(true);
     * }
     * @endcode
     *
     * @return `true` if address reuse is currently enabled; `false` otherwise.
     *
     * @throws SocketException if the socket is invalid or the option cannot be retrieved.
     *
     * @note Always call this after socket creation and before `bind()` to ensure valid results.
     *
     * @see setReuseAddress()
     * @see https://man7.org/linux/man-pages/man7/socket.7.html
     */
    [[nodiscard]] bool getReuseAddress() const;

    /**
     * @brief Sets the socket's receive buffer size (SO_RCVBUF).
     * @ingroup socketopts
     *
     * Configures the size of the kernel-level receive buffer for the underlying socket.
     * This buffer controls how much data the operating system can queue before the
     * application reads from the socket. Increasing this value can reduce packet loss
     * and improve throughput under high load or network delay.
     *
     * ### Applicability
     * This method is valid for all supported socket types:
     * - **TCP (`Socket`)**: Recommended for high-latency or high-throughput connections.
     * - **UDP (`DatagramSocket`)**: Critical for avoiding packet drops in high-volume flows.
     * - **UNIX domain sockets**: Supported on most platforms.
     * - **ServerSocket**: Affects the passive socket itself, not accepted clients.
     *
     * ### Platform Behavior
     * - **Linux**: The kernel typically doubles the requested size to account for internal overhead.
     * - **Windows**: Value may be rounded to OS-specific granularity.
     * - **macOS/BSD**: Follows BSD semantics; may enforce stricter limits.
     * - In all cases, system-wide limits apply (e.g., `/proc/sys/net/core/rmem_max` on Linux).
     *
     * ### Example
     * @code
     * datagramSocket.setReceiveBufferSize(256 * 1024); // Set 256 KB buffer
     * int actual = datagramSocket.getReceiveBufferSize();
     * std::cout << "OS applied buffer size: " << actual << " bytes\n";
     * @endcode
     *
     * @param[in] size Desired receive buffer size in bytes. Actual value may be adjusted by the OS.
     *
     * @throws SocketException if:
     * - The socket is invalid or uninitialized
     * - The call to `setsockopt()` fails
     * - The requested size exceeds system limits
     * - Insufficient permissions to increase buffer limits
     *
     * @note Always call this after socket creation and before heavy I/O.
     * @see getReceiveBufferSize()
     * @see setSendBufferSize()
     * @see setInternalBufferSize()
     */
    void setReceiveBufferSize(std::size_t size);

    /**
     * @brief Retrieves the current receive buffer size (SO_RCVBUF) of the socket.
     * @ingroup socketopts
     *
     * Queries the operating system for the size of the receive buffer allocated to
     * this socket. This buffer temporarily holds incoming data before it is read
     * by the application. The size is determined by system policy, user configuration,
     * and the value set via `setReceiveBufferSize()`.
     *
     * ### Applicability
     * This method works for:
     * - **TCP sockets (`Socket`)**: Indicates how much inbound data can be queued before blocking or drop.
     * - **UDP sockets (`DatagramSocket`)**: Critical for burst tolerance and loss avoidance.
     * - **UNIX domain sockets**: Supported on most platforms.
     * - **ServerSocket**: Reflects configuration of the passive socket itself (not accepted sockets).
     *
     * ### Platform Notes
     * - **Linux**: Returned value is typically *twice* the requested size due to internal accounting.
     * - **Windows**: Rounded to alignment boundaries; value reflects what was granted, not requested.
     * - **BSD/macOS**: Exact behavior may vary, but returned size reflects the OS’s accepted setting.
     *
     * ### Example
     * @code
     * int size = socket.getReceiveBufferSize();
     * std::cout << "Current OS receive buffer: " << size << " bytes\n";
     * @endcode
     *
     * @return The actual receive buffer size in bytes as allocated by the operating system.
     *
     * @throws SocketException if:
     * - The socket is invalid (e.g., not open or moved-from)
     * - The system call fails (e.g., due to low-level error)
     * - Permissions are insufficient
     *
     * @note To verify that `setReceiveBufferSize()` took effect, compare the returned value here.
     * @see setReceiveBufferSize()
     * @see getSendBufferSize()
     * @see setInternalBufferSize()
     */
    [[nodiscard]] int getReceiveBufferSize() const;

    /**
     * @brief Sets the socket's send buffer size (SO_SNDBUF).
     * @ingroup socketopts
     *
     * Configures the size of the kernel-level send buffer for this socket. This buffer controls
     * how much data the operating system can queue for transmission before blocking the sender
     * or returning a partial write. Larger buffers help improve throughput, especially in high-bandwidth
     * or high-latency network environments.
     *
     * ### Applicability
     * This method is valid for all socket types:
     * - **TCP (`Socket`)**: Helps reduce application-level blocking and improves write throughput.
     * - **UDP (`DatagramSocket`)**: Allows higher burst rate; excess data is dropped if full.
     * - **UNIX domain sockets**: Supported for local IPC buffering on most systems.
     * - **ServerSocket**: Setting this has no meaningful effect on passive sockets.
     *
     * ### Platform Behavior
     * - **Linux**: Kernel may double the size for internal overhead; limited by `/proc/sys/net/core/wmem_max`.
     * - **Windows**: Rounded to system-specific page sizes or segment alignment.
     * - **BSD/macOS**: Honors the request up to system-imposed limits.
     * - All platforms may silently apply a different size than requested — verify with `getSendBufferSize()`.
     *
     * ### Example
     * @code
     * socket.setSendBufferSize(128 * 1024); // 128 KB buffer
     * int actual = socket.getSendBufferSize();
     * std::cout << "Effective send buffer: " << actual << " bytes\n";
     * @endcode
     *
     * @param[in] size Desired send buffer size in bytes. The actual buffer size may be adjusted by the system.
     *
     * @throws SocketException if:
     * - The socket is invalid or uninitialized
     * - The call to `setsockopt()` fails
     * - The requested size exceeds system-imposed limits
     * - Insufficient privileges are present
     *
     * @note Applies to future sends only; does not affect buffered data already enqueued.
     * @see getSendBufferSize()
     * @see setReceiveBufferSize()
     * @see setInternalBufferSize()
     */
    void setSendBufferSize(std::size_t size);

    /**
     * @brief Retrieves the current send buffer size (SO_SNDBUF) of the socket.
     * @ingroup socketopts
     *
     * Queries the operating system for the size of the send buffer allocated to this socket.
     * This buffer holds outgoing data that has been written by the application but not yet
     * transmitted over the network or IPC channel. A larger send buffer allows the socket
     * to tolerate bursts of data or temporary network slowdowns without blocking the writer.
     *
     * ### Applicability
     * This method is supported for:
     * - **TCP sockets (`Socket`)**: Improves write throughput and non-blocking behavior.
     * - **UDP sockets (`DatagramSocket`)**: Enables larger bursts of outgoing datagrams.
     * - **UNIX domain sockets**: Applies to stream or datagram sockets on most platforms.
     * - **ServerSocket**: Returns the buffer size of the passive socket, but has no effect on accepted sockets.
     *
     * ### Platform Notes
     * - **Linux**: May return a value twice the requested size due to kernel bookkeeping.
     * - **Windows**: Returns the actual allocated size, rounded internally by Winsock.
     * - **BSD/macOS**: Behavior follows BSD socket semantics.
     * - System-wide limits may apply, and the kernel may clamp, align, or ignore extreme values.
     *
     * ### Example
     * @code
     * int size = socket.getSendBufferSize();
     * std::cout << "Send buffer size: " << size << " bytes\n";
     * @endcode
     *
     * @return Actual send buffer size in bytes, as reported by the kernel.
     *
     * @throws SocketException if:
     * - The socket is invalid or closed
     * - The `getsockopt()` call fails
     * - Permissions are insufficient to query socket settings
     *
     * @note The value returned may differ from what was passed to `setSendBufferSize()`.
     *       Always query after setting to determine the effective size.
     *
     * @see setSendBufferSize()
     * @see getReceiveBufferSize()
     * @see setReceiveBufferSize()
     */
    [[nodiscard]] int getSendBufferSize() const;

    /**
     * @brief Configures the socket's linger behavior (`SO_LINGER`) during close.
     * @ingroup socketopts
     *
     * This method sets the `SO_LINGER` socket option, which controls how the socket behaves
     * when it is closed and unsent data remains in the transmission buffer.
     * It determines whether `close()` will return immediately (discarding unsent data),
     * or block until the data is transmitted or a timeout expires.
     *
     * ### 🔁 Applicability
     * - ✅ **TCP (`Socket`)**: Fully supported; governs graceful vs. abortive close behavior.
     * - ✅ **UNIX stream sockets (`UnixSocket`)**: Supported on most platforms.
     * - ⚠️ **UDP or UNIX datagram sockets (`DatagramSocket`)**: Technically accepted by some systems,
     *   but `SO_LINGER` has **no effect** (ignored by the kernel).
     * - ⚠️ **ServerSocket (listening)**: Affects only the listening socket itself; accepted
     *   sockets must be configured separately.
     *
     * ### Behavior
     * - **Linger enabled (`enable == true`)**:
     *   - The OS will try to send remaining data on close.
     *   - If data cannot be sent within `seconds`, the socket is closed forcibly.
     *   - A timeout of `0` causes an **abortive close** (TCP RST is sent immediately).
     *
     * - **Linger disabled (`enable == false`)** (default):
     *   - `close()` returns immediately.
     *   - Unsent data may be discarded depending on system behavior.
     *
     * ### Example
     * @code
     * // Wait up to 5 seconds for graceful shutdown
     * socket.setSoLinger(true, 5);
     *
     * // Immediately discard unsent data on close (send TCP RST)
     * socket.setSoLinger(true, 0);
     *
     * // Standard non-blocking close behavior
     * socket.setSoLinger(false, 0);
     * @endcode
     *
     * @param[in] enable  Whether to enable lingering behavior on close.
     * @param[in] seconds Linger timeout (in seconds). Only meaningful if `enable == true`.
     *                    - Must be ≥ 0. `0` means abortive close.
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - `setsockopt()` fails
     * - The operation is not supported on the socket type (platform-dependent)
     *
     * @see getSoLinger()
     * @see shutdown()
     * @see close()
     *
     * @note Behavior is OS-specific. On POSIX, applies only to stream-oriented sockets.
     * On Windows, similar semantics apply via Winsock.
     */
    void setSoLinger(bool enable, int seconds);

    /**
     * @brief Retrieves the current `SO_LINGER` configuration of the socket.
     * @ingroup socketopts
     *
     * Queries the operating system for the current linger behavior configured
     * on this socket. Linger settings determine whether the socket blocks on
     * `close()` to allow unsent data to be transmitted, or terminates the
     * connection immediately (potentially discarding unsent data).
     *
     * ### 🔁 Applicability
     * - ✅ **TCP sockets (`Socket`)**: Fully supported; reflects actual linger settings.
     * - ✅ **UNIX domain stream sockets**: Supported on most POSIX platforms.
     * - ⚠️ **UDP / datagram sockets (`DatagramSocket`)**: `SO_LINGER` is silently ignored by most systems.
     * - ⚠️ **ServerSocket**: Applies only to the passive (listening) socket. To inspect accepted clients,
     *   query each returned `Socket` individually.
     *
     * ### Return Value
     * Returns a pair `{enabled, timeout}`:
     * - `first` (`bool`): Whether `SO_LINGER` is currently enabled.
     * - `second` (`int`): Timeout value in seconds (typically `0` if disabled).
     *
     * ### Example
     * @code
     * auto [enabled, timeout] = socket.getSoLinger();
     * if (enabled)
     *     std::cout << "Linger is enabled for " << timeout << " seconds.\n";
     * else
     *     std::cout << "Linger is disabled.\n";
     * @endcode
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - The system call fails
     * - The option is unsupported or not implemented on the platform
     *
     * @see setSoLinger()
     * @see shutdown()
     * @see close()
     *
     * @note On many platforms, this option is only meaningful for stream-oriented sockets.
     * Behavior for datagram sockets is undefined or silently ignored.
     */
    [[nodiscard]] std::pair<bool, int> getSoLinger() const;

    /**
     * @brief Enables or disables TCP-level keepalive behavior on the socket (`SO_KEEPALIVE`).
     * @ingroup socketopts
     *
     * Configures the `SO_KEEPALIVE` socket option, which instructs the operating system
     * to periodically send keepalive probes on otherwise idle **stream-oriented sockets**.
     * This helps detect half-open connections where the remote peer has silently disconnected,
     * crashed, or become unreachable without sending a FIN or RST.
     *
     * ### 🔁 Applicability
     * - ✅ **TCP sockets (`Socket`)**: Fully supported and commonly used.
     * - ✅ **UNIX domain stream sockets (`UnixSocket`)**: Supported on most POSIX platforms.
     * - ✅ **Accepted client sockets in `ServerSocket`**: Enable per-socket after `accept()`.
     * - ⚠️ **UDP or datagram sockets (`DatagramSocket`)**: Not applicable. Most systems ignore this flag.
     *
     * ### Platform Behavior
     * - **Linux**: Keepalive timing is controlled by `/proc/sys/net/ipv4/tcp_keepalive_*` parameters.
     *   Per-socket tuning (e.g., idle, interval, probes) is available using advanced options (not exposed here).
     * - **Windows**: Default timeout is ~2 hours. Fine-tuning requires `WSAIoctl` with `SIO_KEEPALIVE_VALS`.
     * - **macOS/BSD**: Controlled by system-level TCP settings; per-socket control is limited.
     *
     * ### Effects
     * - If enabled (`on == true`), the OS sends periodic probes on idle connections.
     *   If the peer does not respond after several attempts, the connection is forcefully closed.
     * - If disabled (`on == false`), no keepalive probes are sent. The socket may remain open indefinitely.
     *
     * ### Example
     * @code
     * socket.setKeepAlive(true);  // Enable periodic liveness checks on TCP
     * @endcode
     *
     * @param[in] on Set to `true` to enable keepalive, or `false` to disable it.
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - `SO_KEEPALIVE` is unsupported for this socket type
     * - The system call fails due to platform-level constraints
     *
     * @see getKeepAlive()
     * @see setOption()
     * @see https://man7.org/linux/man-pages/man7/tcp.7.html
     *
     * @note This method does not configure the keepalive **timing parameters** (interval, probes, timeout).
     *       Use platform-specific APIs for advanced tuning.
     */
    void setKeepAlive(bool on);

    /**
     * @brief Checks whether TCP-level keepalive (`SO_KEEPALIVE`) is currently enabled.
     * @ingroup socketopts
     *
     * This method queries the socket's current `SO_KEEPALIVE` setting, which determines
     * whether the operating system sends periodic keepalive probes to verify that
     * idle, stream-oriented connections are still alive.
     *
     * Keepalive is especially useful for detecting silent disconnects in long-lived
     * connections (e.g., crash, power loss, cable pull) where no TCP FIN/RST is received.
     *
     * ### 🔁 Applicability
     * - ✅ **TCP sockets (`Socket`)**: Fully supported. Returns the current setting.
     * - ✅ **UNIX domain stream sockets (`UnixSocket`)**: Supported on most POSIX platforms.
     * - ✅ **Accepted sockets in `ServerSocket`**: Each accepted socket can be queried individually.
     * - ⚠️ **UDP / datagram sockets (`DatagramSocket`)**: Most systems ignore this option;
     *   querying may return `false` or raise an error depending on platform behavior.
     *
     * ### Platform Behavior
     * - **Linux/macOS/BSD**: Returns `true` if `SO_KEEPALIVE` is enabled at the socket level.
     *   Probe intervals and behavior are system-controlled unless overridden via additional options.
     * - **Windows**: Returns `true` if enabled. Advanced tuning requires use of `WSAIoctl`.
     *
     * ### Example
     * @code
     * if (!socket.getKeepAlive()) {
     *     socket.setKeepAlive(true);
     *     std::cout << "Keepalive enabled.\n";
     * }
     * @endcode
     *
     * @return `true` if keepalive is enabled; `false` if disabled.
     *
     * @throws SocketException if:
     * - The socket is invalid or closed
     * - `getsockopt()` fails
     * - The platform does not support the option on this socket type
     *
     * @see setKeepAlive()
     * @see setOption()
     * @see https://man7.org/linux/man-pages/man7/tcp.7.html
     *
     * @note This method only checks whether `SO_KEEPALIVE` is enabled.
     *       It does **not** report the configured keepalive timing parameters
     *       (interval, probes, idle time), which are system-defined.
     */
    [[nodiscard]] bool getKeepAlive() const;

    /**
     * @brief Sets the socket receive timeout (`SO_RCVTIMEO`) in milliseconds.
     * @ingroup socketopts
     *
     * This method configures how long a blocking read operation may wait for incoming data
     * before timing out. It affects all socket types that perform blocking I/O, including:
     * - `Socket` (e.g., `read()`, `readExact()`, `readUntil()`)
     * - `DatagramSocket` (e.g., `receiveFrom()`)
     * - `UnixSocket` (e.g., `read()` or IPC reads)
     *
     * A timeout of `0` disables the timeout entirely, causing read operations to block
     * indefinitely until data is available, the connection is closed, or an error occurs.
     * A negative timeout is invalid and will result in an exception.
     *
     * ---
     *
     * ### 🔀 Platform Behavior
     * - **Windows**: The timeout is passed as an `int` in milliseconds.
     * - **POSIX**: The timeout is passed as a `struct timeval` (`seconds` + `microseconds`).
     *
     * ---
     *
     * ### Example: Set a 3-second receive timeout
     * @code
     * socket.setSoRecvTimeout(3000);  // Timeout after 3000 ms if no data arrives
     * std::string line = socket.readUntil('\n');
     * @endcode
     *
     * ---
     *
     * @param[in] millis Timeout in milliseconds. Must be ≥ 0. Use `0` to disable the timeout.
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - The timeout is negative
     * - `setsockopt()` fails due to unsupported option or system-level error
     *
     * @see getSoRecvTimeout()
     * @see setSoSendTimeout()
     * @see https://man7.org/linux/man-pages/man7/socket.7.html
     */
    void setSoRecvTimeout(int millis);

    /**
     * @brief Sets the socket send timeout (`SO_SNDTIMEO`) in milliseconds.
     * @ingroup socketopts
     *
     * This method configures how long a blocking send operation (e.g., `write()`, `writeAll()`)
     * may block before timing out. It affects all socket types that support writing:
     * - `Socket` (TCP stream writes)
     * - `DatagramSocket` (UDP send operations)
     * - `UnixSocket` (interprocess writes)
     *
     * A timeout of `0` disables the timeout entirely, allowing send operations to block
     * indefinitely until buffer space is available. A negative timeout is invalid and
     * will result in an exception.
     *
     * ---
     *
     * ### 🔀 Platform Behavior
     * - **Windows**: Timeout is set as a plain `int` in milliseconds.
     * - **POSIX**: Timeout is specified via a `struct timeval` with second/microsecond precision.
     *
     * ---
     *
     * ### Example: Set a 5-second send timeout
     * @code
     * socket.setSoSendTimeout(5000);  // Timeout if write blocks for more than 5 seconds
     * socket.writeAll(request);
     * @endcode
     *
     * ---
     *
     * @param[in] millis Timeout in milliseconds. Must be ≥ 0. Use `0` to disable the timeout.
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - The timeout is negative
     * - `setsockopt()` fails due to system-level error or unsupported option
     *
     * @see getSoSendTimeout()
     * @see setSoRecvTimeout()
     * @see https://man7.org/linux/man-pages/man7/socket.7.html
     */
    void setSoSendTimeout(int millis);

    /**
     * @brief Retrieves the socket receive timeout (`SO_RCVTIMEO`) in milliseconds.
     * @ingroup socketopts
     *
     * This method queries the configured timeout for blocking read operations,
     * such as `read()`, `readExact()`, `receiveFrom()`, etc. The value returned
     * represents the number of milliseconds a read will block before failing with a timeout.
     *
     * ---
     *
     * ### 🔀 Platform Behavior
     * - **Windows**: Returns the timeout directly as an `int` in milliseconds.
     * - **POSIX**: Retrieves a `struct timeval` and converts it to milliseconds.
     *
     * ---
     *
     * ### Example
     * @code
     * int timeout = socket.getSoRecvTimeout();
     * std::cout << "Read timeout is " << timeout << " ms\n";
     * @endcode
     *
     * ---
     *
     * @return The current receive timeout in milliseconds.
     *         A value of `0` indicates no timeout (i.e., blocking indefinitely).
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - `getsockopt()` fails
     * - The timeout cannot be retrieved due to system-level error
     *
     * @see setSoRecvTimeout()
     * @see getSoSendTimeout()
     */
    [[nodiscard]] int getSoRecvTimeout() const;

    /**
     * @brief Retrieves the socket send timeout (`SO_SNDTIMEO`) in milliseconds.
     * @ingroup socketopts
     *
     * This method queries the configured timeout for blocking send operations,
     * such as `write()`, `writeAll()`, or `sendTo()`. The returned value
     * represents how long a send operation will block before timing out.
     *
     * ---
     *
     * ### 🔀 Platform Behavior
     * - **Windows**: Timeout is retrieved as a plain `int` (milliseconds).
     * - **POSIX**: The timeout is stored in a `struct timeval` and converted back.
     *
     * ---
     *
     * ### Example
     * @code
     * int timeout = socket.getSoSendTimeout();
     * std::cout << "Send timeout is " << timeout << " ms\n";
     * @endcode
     *
     * ---
     *
     * @return The current send timeout in milliseconds.
     *         A value of `0` indicates no timeout (i.e., blocking indefinitely).
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - `getsockopt()` fails
     * - The system reports an unexpected or unsupported configuration
     *
     * @see setSoSendTimeout()
     * @see getSoRecvTimeout()
     */
    [[nodiscard]] int getSoSendTimeout() const;

    /**
     * @brief Enables or disables non-blocking mode on the socket.
     * @ingroup socketopts
     *
     * This method configures the I/O blocking behavior of the socket. In **non-blocking mode**,
     * system calls such as `read()`, `write()`, `connect()`, and `accept()` return immediately
     * if they cannot proceed, rather than blocking the calling thread. This enables asynchronous
     * or event-driven designs where the application explicitly manages readiness.
     *
     * In **blocking mode** (the default), these calls block the thread until they complete,
     * time out (if configured), or fail.
     *
     * ---
     *
     * ### 🔀 Platform Behavior
     * - **POSIX**: Uses `fcntl()` to set or clear the `O_NONBLOCK` flag.
     * - **Windows**: Uses `ioctlsocket()` with the `FIONBIO` control code.
     * - **Effect**: Changes apply immediately and persist for the life of the socket.
     *
     * ---
     *
     * ### 📦 Applicable Socket Types
     * This operation is available on all socket types in this library:
     * - `Socket` (TCP client)
     * - `ServerSocket` (listening/acceptor)
     * - `DatagramSocket` (UDP)
     * - `UnixSocket` (UNIX domain stream/datagram)
     *
     * ---
     *
     * ### ⏱️ Behavior Summary by Operation
     * | Operation     | In Blocking Mode                     | In Non-Blocking Mode                               |
     * |---------------|--------------------------------------|----------------------------------------------------|
     * `connect()`     | Blocks until connected or timeout    | Returns immediately; may require polling `write`   |
     * `read()`        | Waits for data or EOF                | Returns `-1` with `EWOULDBLOCK` if no data         |
     * `write()`       | Waits for buffer availability        | Returns `-1` if buffer full                        |
     * `accept()`      | Waits for pending connection         | Returns `-1` with `EWOULDBLOCK` if none available  |
     * `recvfrom()`    | Waits for incoming datagram          | Returns `-1` if no datagram received               |
     *
     * ---
     *
     * ### ✅ Use Cases
     * - Event-driven I/O (with `select`, `poll`, `epoll`, or `kqueue`)
     * - GUI or game loops that must avoid blocking the main thread
     * - High-performance servers handling thousands of concurrent clients
     * - Real-time systems with custom scheduling and retry logic
     *
     * ---
     *
     * ### Example: Enable non-blocking mode on a client socket
     * @code
     * Socket sock("example.com", 80);
     * sock.setNonBlocking(true);
     *
     * if (!sock.waitReady(true, 3000)) {
     *     throw TimeoutException("Connection timed out");
     * }
     * @endcode
     *
     * ---
     *
     * @param[in] nonBlocking `true` to enable non-blocking mode, `false` to restore blocking mode.
     *
     * @throws SocketException if:
     * - The socket is invalid (`EBADF`)
     * - Platform-specific system calls fail
     * - Permissions or capabilities are insufficient
     *
     * @note This setting only affects the current socket instance. It does **not** affect sockets
     * returned by `accept()` (you must call `setNonBlocking()` on each accepted socket).
     *
     * @see getNonBlocking()
     * @see waitReady() To wait for readiness in non-blocking mode
     * @see setSoRecvTimeout() For timeout control in blocking mode
     */
    void setNonBlocking(bool nonBlocking);

    /**
     * @brief Queries whether the socket is currently in non-blocking mode.
     * @ingroup socketopts
     *
     * This method inspects the I/O mode of the socket and returns `true` if it is configured
     * for non-blocking operations. In non-blocking mode, I/O system calls return immediately
     * if they cannot complete, rather than blocking the calling thread.
     *
     * ---
     *
     * ### 🔀 Platform Behavior
     * - **POSIX**: Checks the `O_NONBLOCK` flag using `fcntl(F_GETFL)`.
     * - **Windows**: No direct API exists to query non-blocking state. This function
     *   will conservatively return `false` and emit a warning if queried.
     *
     * ---
     *
     * ### Use Cases
     * - Debugging or inspecting socket configuration
     * - Adaptive I/O logic based on runtime state
     *
     * ---
     *
     * ### Example
     * @code
     * if (!socket.getNonBlocking()) {
     *     socket.setNonBlocking(true);
     * }
     * @endcode
     *
     * ---
     *
     * @return `true` if the socket is in non-blocking mode, `false` otherwise.
     *         On Windows, always returns `false` due to lack of system support.
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - The system query fails (e.g., `fcntl` returns error on POSIX)
     *
     * @note On Windows, there is **no way to detect** the current non-blocking state via public APIs.
     * You must track it manually in application logic if needed.
     *
     * @see setNonBlocking()
     */
    [[nodiscard]] bool getNonBlocking() const;

    /**
     * @brief Enables or disables Nagle’s algorithm (`TCP_NODELAY`) on TCP sockets.
     * @ingroup socketopts
     *
     * This method configures the `TCP_NODELAY` option, which controls the behavior of **Nagle's algorithm**
     * for stream-oriented TCP sockets (`SOCK_STREAM`). Nagle's algorithm aims to reduce network congestion
     * by delaying small outgoing packets until previous ones are acknowledged — effectively "coalescing" writes.
     *
     * Disabling Nagle’s algorithm (i.e., setting `TCP_NODELAY = 1`) allows small packets to be sent immediately,
     * which is ideal for latency-sensitive applications. Enabling it (i.e., setting `TCP_NODELAY = 0`) improves
     * throughput by batching writes, but introduces potential delays.
     *
     * ---
     *
     * ### 💡 Use Cases
     * - **Latency-sensitive applications**:
     *   - Games, remote control, real-time messaging, RPC
     *   - Disabling Nagle (`on = true`) minimizes delay in sending small payloads
     * - **Bulk transfer or high-throughput services**:
     *   - Enabling Nagle (`on = false`) reduces packet count and improves network efficiency
     *
     * ---
     *
     * ### 🔀 Platform Behavior
     * - Universally available on TCP/IP sockets (`AF_INET`, `AF_INET6` with `SOCK_STREAM`)
     * - **POSIX**: Uses `setsockopt()` with `IPPROTO_TCP` and `TCP_NODELAY`
     * - **Windows**: Uses Winsock with same option names
     * - Has **no effect** on:
     *   - UDP sockets (`DatagramSocket`)
     *   - UNIX domain sockets (`UnixSocket`)
     *   - Non-TCP protocols or non-stream sockets
     *
     * ---
     *
     * ### Example: Disable Nagle’s algorithm to reduce latency
     * @code
     * Socket sock("example.com", 443);
     * sock.connect();
     *
     * sock.setTcpNoDelay(true);  // Disable Nagle — send immediately
     * sock.write("small interactive packet");
     * @endcode
     *
     * ---
     *
     * @param[in] on If `true`, disables Nagle's algorithm (`TCP_NODELAY = 1`), enabling immediate sends.
     *               If `false`, enables Nagle's algorithm (`TCP_NODELAY = 0`) to batch small writes.
     *
     * @throws SocketException if:
     * - The socket is invalid or closed
     * - The option is unsupported on the current socket type
     * - The `setsockopt()` call fails due to system error or permission issue
     *
     * @note Calling this method on a non-TCP socket will raise an exception.
     *       You can catch `SocketException` to handle cross-socket behavior gracefully.
     *
     * @see getTcpNoDelay()
     * @see https://en.wikipedia.org/wiki/Nagle%27s_algorithm
     * @see https://man7.org/linux/man-pages/man7/tcp.7.html
     */
    void setTcpNoDelay(bool on);

    /**
     * @brief Queries whether Nagle's algorithm (`TCP_NODELAY`) is currently disabled.
     * @ingroup socketopts
     *
     * This method checks if `TCP_NODELAY` is set on the socket, which controls the behavior
     * of **Nagle's algorithm** for TCP connections. When `TCP_NODELAY` is enabled (`true`),
     * small writes are sent immediately without waiting to coalesce packets. When disabled (`false`),
     * the system may buffer small packets to improve network efficiency.
     *
     * ---
     *
     * ### 🔍 Applicability
     * This option applies only to:
     * - TCP stream sockets (`SOCK_STREAM` with `AF_INET` or `AF_INET6`)
     *
     * It does **not** apply to:
     * - UDP sockets (`DatagramSocket`)
     * - UNIX domain sockets (`UnixSocket`)
     * - Listening sockets (`ServerSocket`) — check accepted `Socket` objects instead
     *
     * ---
     *
     * ### Use Cases
     * - Determine latency behavior at runtime
     * - Validate low-latency configuration in unit tests
     * - Debug unexpected TCP buffering issues
     *
     * ---
     *
     * ### Example
     * @code
     * if (!socket.getTcpNoDelay()) {
     *     socket.setTcpNoDelay(true);  // Reduce latency
     * }
     * @endcode
     *
     * ---
     *
     * @return `true` if Nagle's algorithm is disabled (`TCP_NODELAY = 1`),
     *         `false` if Nagle’s algorithm is enabled (`TCP_NODELAY = 0`)
     *
     * @throws SocketException if:
     * - The socket is invalid or closed
     * - The option is not supported on the current socket type
     * - A system-level query fails (`getsockopt()` error)
     *
     * @see setTcpNoDelay()
     * @see https://en.wikipedia.org/wiki/Nagle%27s_algorithm
     * @see https://man7.org/linux/man-pages/man7/tcp.7.html
     */
    [[nodiscard]] bool getTcpNoDelay() const;

#if defined(IPV6_V6ONLY)

    /**
     * @brief Enables or disables `IPV6_V6ONLY` mode for IPv6-capable sockets.
     * @ingroup socketopts
     *
     * This method configures the `IPV6_V6ONLY` socket option, which determines whether an IPv6 socket
     * can accept **only** IPv6 connections or **both** IPv6 and IPv4-mapped addresses (e.g., `::ffff:a.b.c.d`).
     *
     * ---
     *
     * ### 🌍 Applicability by Socket Type
     * - `ServerSocket`: ✅ Common — determines which protocols are accepted via `bind()`
     * - `Socket`: ✅ Optional — may control behavior of outgoing connections or bound local endpoint
     * - `DatagramSocket`: ✅ Applies when binding to `::` for dual-stack multicast or receiving
     * - `UnixSocket`: ❌ Not applicable — local domain sockets do not use IP protocols
     *
     * ---
     *
     * ### 🔀 Platform Behavior
     * - **Linux**: `IPV6_V6ONLY` defaults to `0` (dual-stack enabled)
     * - **Windows**: Defaults to `1` (IPv6-only); must disable explicitly for dual-stack
     * - **macOS / BSD**: Often defaults to `1`, some systems disallow disabling
     *
     * ⚠️ On all platforms, this option **must be set before `bind()`**. Changing it afterward is undefined or ignored.
     *
     * ---
     *
     * ### 💡 Use Cases
     * - Enforce strict IPv6-only policy (security or protocol compliance)
     * - Enable dual-stack sockets that handle both IPv6 and IPv4 transparently
     * - Configure UDP or multicast receivers to receive from both protocol families
     *
     * ---
     *
     * ### Example
     * @code
     * ServerSocket server(AF_INET6);
     * server.setIPv6Only(true);      // Only allow IPv6 clients
     * server.bind("::", 8080);
     * server.listen();
     * @endcode
     *
     * ---
     *
     * @param[in] enable `true` to enable IPv6-only mode (`IPV6_V6ONLY = 1`),
     *                   `false` to allow dual-stack operation (`IPV6_V6ONLY = 0`).
     *
     * @throws SocketException if:
     * - The socket is not valid
     * - The option is unsupported on the platform or socket type
     * - The system call (`setsockopt()`) fails
     *
     * @note This setting has no effect on UNIX domain sockets, and calling it on one will throw.
     *
     * @see getIPv6Only()
     * @see setOption()
     * @see https://man7.org/linux/man-pages/man7/ipv6.7.html
     */
    void setIPv6Only(bool enable);

    /**
     * @brief Queries whether the `IPV6_V6ONLY` option is enabled on this socket.
     * @ingroup socketopts
     *
     * This method checks whether the socket is currently restricted to IPv6-only traffic,
     * or whether it allows dual-stack operation (accepting both IPv6 and IPv4-mapped addresses).
     *
     * The `IPV6_V6ONLY` flag is primarily relevant to sockets using the `AF_INET6` address family.
     *
     * ---
     *
     * ### 🌍 Applicability by Socket Type
     * - `ServerSocket`: ✅ Affects accept() behavior for IPv6 listeners
     * - `Socket`: ✅ Optional, useful for bound clients or diagnostic purposes
     * - `DatagramSocket`: ✅ Relevant when binding to IPv6 multicast or wildcard addresses
     * - `UnixSocket`: ❌ Not applicable; throws if called
     *
     * ---
     *
     * ### ⚠️ Platform Behavior
     * - **Linux**: Defaults to `false` (dual-stack), but configurable
     * - **Windows**: Defaults to `true` (IPv6-only), must be explicitly disabled
     * - **macOS/BSD**: May disallow toggling at runtime or enforce `true`
     *
     * ---
     *
     * ### Example: Check if socket is IPv6-only
     * @code
     * if (socket.getIPv6Only()) {
     *     std::cout << "This socket is IPv6-only.\n";
     * } else {
     *     std::cout << "Dual-stack mode is active.\n";
     * }
     * @endcode
     *
     * ---
     *
     * @return `true` if `IPV6_V6ONLY` is enabled (IPv6-only mode), `false` if dual-stack is allowed.
     *
     * @throws SocketException if:
     * - The socket is invalid or closed
     * - The system call fails (`getsockopt()` error)
     * - The socket is not an IPv6 socket (i.e., `AF_INET6`)
     *
     * @note Always check that the socket is using the `AF_INET6` family before interpreting this flag.
     * Calling this method on a non-IPv6 socket will throw.
     *
     * @see setIPv6Only()
     * @see https://man7.org/linux/man-pages/man7/ipv6.7.html
     */
    [[nodiscard]] bool getIPv6Only() const;

#endif

#if defined(SO_REUSEPORT)

    /**
     * @brief Enables or disables the `SO_REUSEPORT` socket option.
     * @ingroup socketopts
     *
     * This method configures the `SO_REUSEPORT` option, which allows multiple sockets to bind
     * to the **same IP address and port combination**, enabling parallelism in multi-threaded or
     * multi-process applications. Unlike `SO_REUSEADDR`, which allows re-binding during `TIME_WAIT`,
     * `SO_REUSEPORT` permits **simultaneous binding** by multiple sockets.
     *
     * ---
     *
     * ### 🌍 Applicability
     * - `ServerSocket`: ✅ Enables load-balanced accept loops across threads or processes
     * - `DatagramSocket`: ✅ Permits shared reception on a multicast port (platform-dependent)
     * - `Socket`: ✅ Technically allowed, but rarely used in clients
     * - `UnixSocket`: ❌ Not supported; this method is not compiled on unsupported platforms
     *
     * ---
     *
     * ### 🔀 Platform Support
     * - ✅ Linux (kernel ≥ 3.9)
     * - ✅ BSD-based systems (FreeBSD, macOS)
     * - ❌ Windows: Not available — this method is excluded at compile time
     *
     * ---
     *
     * ### Example: Enable shared port binding
     * @code
     * #if defined(SO_REUSEPORT)
     *     serverSocket.setReusePort(true);
     *     serverSocket.bind("0.0.0.0", 8080);
     *     serverSocket.listen();
     * #endif
     * @endcode
     *
     * ---
     *
     * @param[in] enable `true` to allow multiple sockets to bind the same port (`SO_REUSEPORT = 1`),
     *                   or `false` to disable the shared binding behavior (`SO_REUSEPORT = 0`).
     *
     * @throws SocketException if:
     * - The socket is invalid
     * - The system call fails (`setsockopt()` error)
     * - The option is used improperly (e.g., after `bind()`)
     *
     * @note This option must be set **before** calling `bind()`. Behavior is undefined if changed after binding.
     *
     * @see getReusePort()
     * @see setReuseAddress()
     * @see https://lwn.net/Articles/542629/
     */
    void setReusePort(const bool enable);

    /**
     * @brief Checks whether the `SO_REUSEPORT` option is currently enabled on the socket.
     * @ingroup socketopts
     *
     * This method queries the `SO_REUSEPORT` setting via `getsockopt()` to determine
     * whether the socket is configured to allow multiple bindings to the same address/port.
     *
     * ---
     *
     * ### 🌍 Applicability
     * - `ServerSocket`: ✅ Used to verify shared accept-mode configuration
     * - `DatagramSocket`: ✅ Useful for multicast receivers or redundant UDP listeners
     * - `Socket`: ✅ Rare, but valid for bound client sockets
     * - `UnixSocket`: ❌ Not applicable — method excluded at compile time
     *
     * ---
     *
     * ### 🔀 Platform Support
     * - ✅ **Linux (≥ 3.9)**, FreeBSD, macOS (some BSDs require extra sysctls)
     * - ❌ **Windows**: Not supported — this method is not compiled
     *
     * ---
     *
     * ### Example
     * @code
     * #if defined(SO_REUSEPORT)
     * if (socket.getReusePort()) {
     *     std::cout << "Port reuse is enabled.\n";
     * } else {
     *     std::cout << "Port reuse is not enabled.\n";
     * }
     * #endif
     * @endcode
     *
     * ---
     *
     * @return `true` if `SO_REUSEPORT` is enabled on the socket; `false` otherwise.
     *
     * @throws SocketException if:
     * - The socket is invalid or uninitialized
     * - The system call fails (`getsockopt()` error)
     * - The option is not supported or the socket type is incompatible
     *
     * @see setReusePort()
     * @see https://man7.org/linux/man-pages/man7/socket.7.html
     */
    [[nodiscard]] bool getReusePort() const;

#endif

  protected:
    /**
     * @brief Updates the socket descriptor used by this object.
     * @ingroup socketopts
     *
     * This method sets the internal socket file descriptor used by the `SocketOptions` interface.
     * It is typically called by derived classes after the socket has been moved, reassigned, or
     * otherwise changed during the object's lifetime.
     *
     * This does **not** open, close, or validate the socket descriptor. It simply updates the
     * internal `_sockFd` used by methods such as `setOption()` and `getOption()`.
     *
     * ### When to Use
     * - After a move constructor or move assignment updates the underlying socket
     * - After explicitly re-binding or re-creating a socket file descriptor
     * - When adapting an externally provided descriptor (e.g., from `accept()` or `socketpair()`)
     *
     * ### Example: Move assignment
     * @code
     * _sockFd = rhs._sockFd;
     * setSocketFd(_sockFd); // keeps SocketOptions base class in sync
     * @endcode
     *
     * @param[in] sock The new socket file descriptor to associate with this object. May be `INVALID_SOCKET`
     *                 to mark the socket as uninitialized or closed.
     *
     * @note This method does not perform error checking. It is the caller's responsibility to ensure
     *       the provided descriptor is valid and consistent with the derived class state.
     *
     * @see getOption()
     * @see setOption()
     * @see SocketOptions
     */
    void setSocketFd(const SOCKET sock) noexcept { _sockFd = sock; }

    /**
     * @brief Indicates whether the socket behaves as a passive (listening) socket.
     * @ingroup socketopts
     *
     * This virtual method is used internally by the `SocketOptions` interface to determine whether
     * the socket is operating in a *passive role* — that is, it is a listening socket typically created
     * by a server to accept incoming connections. This distinction is particularly relevant on platforms
     * like Windows, where different socket options apply to passive vs. active sockets.
     *
     * Specifically, this affects how address reuse logic is applied:
     * - On Windows, `SO_EXCLUSIVEADDRUSE` is used for passive sockets to control port reuse semantics.
     * - On POSIX systems, all sockets use `SO_REUSEADDR`, but server sockets may still follow different best practices.
     *
     * ### Usage
     * - Override this method in subclasses like `ServerSocket` to return `true`.
     * - Leave it as `false` (default) in client sockets, UDP sockets, or Unix domain sockets.
     *
     * @return `true` if the socket is used in a listening or acceptor role; `false` otherwise.
     *
     * @note This method has no direct side effects. It exists purely to guide conditional logic in reusable
     *       base class operations such as `setReuseAddress()` and `getReuseAddress()`.
     *
     * @see setReuseAddress()
     * @see getReuseAddress()
     * @see ServerSocket
     */
    [[nodiscard]] virtual bool isPassiveSocket() const noexcept { return false; }

  private:
    SOCKET _sockFd = INVALID_SOCKET; ///< Underlying socket file descriptor
};

} // namespace jsocketpp
