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
     * @param sock A valid socket descriptor (e.g., from `socket()`, `accept()`, etc.), or
     *             `INVALID_SOCKET` if the object is not yet initialized.
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
     * @brief Sets a low-level socket option on the underlying socket.
     * @ingroup socketopts
     *
     * This method configures a socket option using the system-level `setsockopt()` interface.
     * Socket options allow advanced users to fine-tune behavior related to performance,
     * protocol semantics, and resource usage.
     *
     * This API is common to all socket types (e.g., `Socket`, `ServerSocket`, `DatagramSocket`, `UnixSocket`)
     * and supports options at multiple protocol levels, such as `SOL_SOCKET` and `IPPROTO_TCP`.
     *
     * Typical use cases include:
     * - Enabling address reuse (`SO_REUSEADDR`)
     * - Adjusting receive/send buffer sizes (`SO_RCVBUF`, `SO_SNDBUF`)
     * - Enabling/disabling keepalive (`SO_KEEPALIVE`)
     * - Disabling Nagle’s algorithm for latency-sensitive TCP (`TCP_NODELAY`)
     *
     * ### ⚠️ ServerSocket-specific Notes
     * - Setting options on a **listening socket** (e.g., `ServerSocket`) only affects that socket, not the
     *   client sockets returned by `accept()`. Per-connection tuning must be applied to each accepted `Socket`.
     * - Not all options are meaningful on passive (listening) sockets. Invalid combinations may result in
     *   `SocketException` or system errors.
     *
     * ### Example: Enable port reuse (recommended for servers)
     * @code
     * serverSocket.setOption(SOL_SOCKET, SO_REUSEADDR, 1);
     * @endcode
     *
     * ### Example: Disable Nagle’s algorithm (for latency-sensitive clients)
     * @code
     * socket.setOption(IPPROTO_TCP, TCP_NODELAY, 1);
     * @endcode
     *
     * ---
     *
     * ### 📡 DatagramSocket-specific Notes
     * - You can use this method to enable **broadcast mode** (`SO_BROADCAST`) or adjust buffer sizes for
     *   high-throughput UDP workloads.
     * - It also supports setting timeouts via `SO_RCVTIMEO` and `SO_SNDTIMEO`.
     * - For unconnected UDP sockets, option changes affect all outgoing/incoming packets.
     * - For connected UDP sockets, the changes affect only the single destination connection.
     *
     * #### Example: Enable broadcast for a datagram socket
     * @code
     * datagramSocket.setOption(SOL_SOCKET, SO_BROADCAST, 1);
     * @endcode
     *
     * ### 🧿 UnixSocket-specific Notes
     * - Although Unix domain sockets are local and do not use TCP/IP, many `SOL_SOCKET` options still apply:
     *   - `SO_RCVBUF`, `SO_SNDBUF` for buffer control
     *   - `SO_RCVTIMEO`, `SO_SNDTIMEO` for timeouts
     *   - `SO_PASSCRED` to enable credential passing (on supported platforms)
     * - Options like `TCP_NODELAY` and `SO_KEEPALIVE` do not apply to Unix sockets.
     *
     * #### Example: Set a larger send buffer for inter-process communication
     * @code
     * unixSocket.setOption(SOL_SOCKET, SO_SNDBUF, 65536);
     * @endcode
     *
     * @param level    The protocol level at which the option resides (e.g., `SOL_SOCKET`, `IPPROTO_TCP`).
     * @param optName  The name of the option to set (e.g., `SO_REUSEADDR`, `SO_RCVBUF`).
     * @param value    Integer value to assign to the option.
     *
     * @throws SocketException if the option is invalid, unsupported, or if `setsockopt()` fails.
     *
     * @see getOption()
     * @see https://man7.org/linux/man-pages/man2/setsockopt.2.html
     */
    void setOption(int level, int optName, int value);

    /**
     * @brief Retrieves the current value of a low-level socket option.
     * @ingroup socketopts
     *
     * This method queries the current value of a socket option via `getsockopt()` on the
     * underlying socket descriptor. It is useful for verifying runtime socket configuration,
     * inspecting platform defaults, and performing conditional logic based on system-level state.
     *
     * This interface applies uniformly across all socket types (e.g., `Socket`, `ServerSocket`,
     * `DatagramSocket`, `UnixSocket`) and supports standard protocol levels and option names.
     *
     * ### Example: Read the configured receive buffer size
     * @code
     * int rcvBuf = socket.getOption(SOL_SOCKET, SO_RCVBUF);
     * @endcode
     *
     * ---
     *
     * ### ⚠️ ServerSocket-specific Notes
     * - `getOption()` returns the value of the option as applied to the **listening socket only**.
     * - Options like `SO_RCVBUF`, `SO_REUSEADDR`, and `SO_LINGER` are meaningful before or during `listen()`.
     * - To inspect options for a client connection, call `getOption()` on the accepted `Socket` returned by `accept()`.
     *
     * ---
     *
     * ### 📡 DatagramSocket-specific Notes
     * - Use `getOption()` to inspect options like buffer sizes (`SO_RCVBUF`, `SO_SNDBUF`) and timeouts
     *   (`SO_RCVTIMEO`, `SO_SNDTIMEO`) for diagnostic or tuning purposes.
     * - On broadcast-enabled sockets (`SO_BROADCAST`), this method can verify that the option was set successfully.
     * - Behavior is the same whether the socket is connected or unconnected.
     *
     * #### Example: Get current timeout for receiving
     * @code
     * int timeout = datagramSocket.getOption(SOL_SOCKET, SO_RCVTIMEO);
     * @endcode
     *
     * ---
     *
     * ### 🧿 UnixSocket-specific Notes
     * - Although Unix domain sockets are local, many `SOL_SOCKET` options are still supported and report useful values:
     *   - `SO_RCVBUF`, `SO_SNDBUF`: Report actual system-allocated buffer sizes
     *   - `SO_RCVTIMEO`, `SO_SNDTIMEO`: Inspect timeouts for IPC calls
     *   - `SO_PASSCRED`: Check whether credential passing is enabled
     * - This method is particularly useful for debugging IPC performance characteristics.
     *
     * #### Example: Read the actual receive buffer size used for a Unix socket
     * @code
     * int size = unixSocket.getOption(SOL_SOCKET, SO_RCVBUF);
     * @endcode
     *
     * @param level   The protocol level (e.g., `SOL_SOCKET`, `IPPROTO_TCP`).
     * @param optName The name of the socket option to retrieve (e.g., `SO_RCVBUF`, `SO_KEEPALIVE`).
     * @return        The integer value of the specified socket option.
     *
     * @throws SocketException if the operation fails, the socket is invalid, or the option is not supported.
     *
     * @see setOption()
     * @see https://man7.org/linux/man-pages/man2/getsockopt.2.html
     */
    [[nodiscard]] int getOption(int level, int optName) const;

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
     * @param on If `true`, enables address reuse; if `false`, disables it.
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
     * @param enable Whether to enable lingering behavior on close.
     * @param seconds Linger timeout (in seconds). Only meaningful if `enable == true`.
     *                - Must be ≥ 0. `0` means abortive close.
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
     * @param sock The new socket file descriptor to associate with this object. May be `INVALID_SOCKET`
     *             to mark the socket as uninitialized or closed.
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
