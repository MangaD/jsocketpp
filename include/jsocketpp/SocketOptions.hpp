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
#include "SocketException.hpp"

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

  protected:
    /**
     * @brief Update the socket descriptor used by this object.
     * @ingroup socketopts
     * @param sock New socket file descriptor.
     */
    void setSocketFd(const SOCKET sock) noexcept { _sockFd = sock; }

  private:
    SOCKET _sockFd = INVALID_SOCKET; ///< Underlying socket file descriptor
};

} // namespace jsocketpp
