/**
 * @file DatagramSocket.hpp
 * @brief UDP datagram socket abstraction for jsocketpp.
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

#pragma once

#include "BufferView.hpp"
#include "common.hpp"
#include "DatagramPacket.hpp"
#include "detail/buffer_traits.hpp"
#include "SocketOptions.hpp"

#include <bit>
#include <optional>
#include <span>
#include <string>

namespace jsocketpp
{

/**
 * @brief Receive-time sizing policy for UDP datagrams.
 * @ingroup udp
 *
 * @details
 * Controls how `DatagramSocket::read(...)` chooses the number of bytes
 * to request from the OS for the **next** UDP datagram. This affects both
 * the likelihood of truncation and the number of syscalls performed per receive.
 *
 * **General behavior**
 * - In all modes, exactly **one** datagram is consumed per call.
 * - The OS may truncate the payload if the destination buffer is smaller than the datagram.
 * - For `DatagramPacket&` variants, the `resizeBuffer` parameter still applies and determines
 *   whether the packet’s buffer may be **grown/shrunk** by the call (see individual modes).
 * - For raw-buffer variants (e.g., `readInto()`), the buffer size is fixed by the caller;
 *   no resizing is possible, but truncation may occur.
 *
 * **Performance note**
 * - Modes that preflight the exact datagram size typically issue an **extra syscall**
 *   (e.g., `FIONREAD` or `MSG_PEEK|MSG_TRUNC`) before the actual receive. This improves
 *   sizing accuracy at a small cost in throughput.
 *
 * @code
 * DatagramPacket pkt;
 * pkt.resize(8192); // provision capacity
 *
 * // Default (no preflight): fastest path; may truncate if a datagram > 8192 arrives
 * std::size_t n = sock.read(pkt, true, jsocketpp::DatagramReceiveMode::NoPreflight);
 *
 * // Preflight: probe exact size and (if allowed) grow pkt before receiving to avoid truncation
 * n = sock.read(pkt, true, jsocketpp::DatagramReceiveMode::PreflightSize);
 *
 * // PreflightMax: probe size but never request more than current capacity (or provided length)
 * n = sock.read(pkt, false, jsocketpp::DatagramReceiveMode::PreflightMax);
 * @endcode
 *
 * @since 1.0
 */
enum class DatagramReceiveMode : std::uint8_t
{
    /**
     * @brief Do not probe the datagram size; call `recvfrom()` directly.
     *
     * @details
     * - **Syscalls:** 1 (fast path).
     * - **Resizing:** If `resizeBuffer == true`, the packet may be **shrunk** *after* the receive
     *   to match the actual byte count; it is **not** grown beforehand.
     * - **Truncation:** If the incoming datagram exceeds the current buffer size, the OS returns
     *   only the first `buffer.size()` bytes and discards the rest (standard UDP behavior).
     *
     * Use this for fixed-size protocols or hot paths where every syscall counts.
     */
    NoPreflight = 0,

    /**
     * @brief Probe the exact size of the next datagram and size the receive accordingly.
     *
     * @details
     * - **Syscalls:** up to 2 (probe, then receive).
     * - **Resizing (DatagramPacket):** If `resizeBuffer == true` and the probed size is greater
     *   than `packet.size()`, the implementation may **grow** the packet (clamped to
     *   `MaxDatagramPayloadSafe`) to avoid truncation. If `resizeBuffer == false`, the packet is
     *   **not grown** and truncation may still occur.
     * - **Raw buffer:** Reads exactly the probed size if it fits in `len`, otherwise truncates to `len`.
     * - **Accuracy:** Uses platform facilities such as `FIONREAD` or a POSIX `MSG_PEEK|MSG_TRUNC`
     *   probe via `nextDatagramSize()`. On platforms where the probe is unavailable or unreliable,
     *   behavior gracefully degrades to `NoPreflight`.
     *
     * Choose this when datagram sizes vary widely and avoiding truncation is more important
     * than minimizing syscalls.
     */
    PreflightSize = 1,

    /**
     * @brief Probe the size of the next datagram but cap it at the current buffer length.
     *
     * @details
     * - **Syscalls:** up to 2 (probe, then receive).
     * - **Resizing (DatagramPacket):** If `resizeBuffer == true` and the probed size is less than
     *   or equal to `packet.size()`, the packet may be shrunk to the exact size after the receive.
     *   If the probed size is greater than `packet.size()`, truncation occurs; the packet is never grown.
     * - **Raw buffer:** Reads exactly `min(probedSize, len)`.
     * - **Accuracy:** Same probe mechanism as `PreflightSize`.
     *
     * Use this to avoid oversizing reads while still skipping unnecessary extra bytes for
     * smaller datagrams.
     */
    PreflightMax = 2
};

/**
 * @brief Options controlling a single UDP receive operation.
 * @ingroup udp
 *
 * This structure encapsulates the configurable parameters that control how a UDP datagram
 * is received and processed by the DatagramSocket class. It provides fine-grained control
 * over buffer management, system flags, and post-receive behavior.
 *
 * ### Key Features:
 * - Controls datagram sizing policy
 * - Manages buffer resizing behavior
 * - Sets system receive flags
 * - Controls remote peer tracking
 * - Configures address resolution
 *
 * @see DatagramSocket::read()
 * @see DatagramReceiveMode
 * @since 1.0
 */
struct DatagramReadOptions
{
    /**
     * @brief Datagram sizing policy to use during receive.
     *
     * Controls whether to probe the datagram size before receiving (preflight) or
     * directly read into the provided buffer. See @ref DatagramReceiveMode for details.
     */
    DatagramReceiveMode mode = DatagramReceiveMode::NoPreflight;

    /**
     * @brief Whether the packet buffer may grow before receive (applies to DatagramPacket).
     *
     * When true and using PreflightSize mode, allows the implementation to grow the
     * packet buffer if the probed datagram size exceeds current capacity. This can
     * prevent truncation at the cost of potential allocation.
     */
    bool allowGrow = true;

    /**
     * @brief Whether the packet buffer may shrink after receive (applies to DatagramPacket).
     *
     * When true, allows the implementation to shrink the packet buffer to exactly match
     * the received datagram size after a successful receive. This can reduce memory usage
     * but may trigger reallocation on subsequent receives.
     */
    bool allowShrink = true;

    /**
     * @brief Extra flags passed to recv/recvfrom (e.g., MSG_PEEK).
     *
     * Platform-specific flags that modify the behavior of the underlying receive operation.
     * Common values include:
     * - MSG_PEEK: Look at data without removing it from the receive queue
     * - MSG_WAITALL: Block until the full request can be satisfied
     * - MSG_DONTWAIT: Non-blocking operation
     */
    int recvFlags = 0;

    /**
     * @brief Whether to persist sender into the socket's "last remote" (unconnected sockets).
     *
     * When true on an unconnected socket, updates the internal state tracking the most
     * recent peer address. This enables getRemoteIp() and getRemotePort() to return
     * information about the last datagram sender.
     */
    bool updateLastRemote = true;

    /**
     * @brief Whether to resolve numeric host/port into DatagramPacket after receive.
     *
     * When true, attempts to convert the sender's address into a human-readable IP string
     * and port number. This may incur additional syscalls but provides convenient access
     * to peer information via DatagramPacket::address and DatagramPacket::port.
     */
    bool resolveNumeric = true;

    /**
     * @brief When `true`, the read **fails** if the incoming datagram would be truncated.
     *
     * @details
     * This option determines whether a receive operation should **throw** when the
     * destination buffer cannot hold the entire datagram:
     *
     * - With `DatagramReceiveMode::PreflightSize`, size is probed before reading. If the
     *   probed size exceeds the destination capacity and `errorOnTruncate == true`, the
     *   function throws **before** consuming the datagram. If `false`, the read proceeds
     *   and the datagram is truncated to the provided capacity.
     *
     * - With `DatagramReceiveMode::NoPreflight`, truncation can only be detected **after**
     *   the receive. If truncation occurs and `errorOnTruncate == true`, the function
     *   throws **after** the datagram has been consumed by the kernel (the tail is lost).
     *   If `false`, it returns the truncated bytes without error.
     *
     * @note This flag primarily affects APIs that read into a **fixed-size buffer** (e.g.,
     *       `readAvailable(std::span<char> ...)`, `readInto(void*, size_t, ...)`). It has
     *       no practical effect for string-returning APIs that allocate exactly to fit the
     *       datagram after a successful size preflight; however, if size probing is not
     *       available on the platform and a **safe cap** is used, a datagram larger than
     *       that cap will trigger this check.
     *
     * @warning With `NoPreflight`, a thrown error occurs **after** data consumption; the
     *          datagram tail cannot be recovered. Prefer `PreflightSize` if you must avoid
     *          truncation entirely.
     */
    bool errorOnTruncate{true};
};

/**
 * @brief Telemetry data about a single UDP datagram receive operation.
 * @ingroup udp
 *
 * The `DatagramReadResult` structure encapsulates comprehensive details about the result of a UDP datagram receive
 * operation, including payload size, truncation status, and sender information. It enables monitoring and control of
 * datagram handling behavior.
 *
 * Key features:
 * - Tracks actual bytes received and copied to the destination buffer
 * - Reports full size of the original datagram when available via preflight
 * - Indicates truncation status for datagrams exceeding buffer capacity
 * - Captures raw sender address information for unconnected sockets in a platform-independent format
 *
 * ### Use Cases:
 * - Monitoring datagram sizes and truncation
 * - Tracking sender information without DNS resolution
 * - Implementing custom routing or filtering logic
 * - Performance profiling and diagnostics
 *
 * @see DatagramSocket::read()
 * @see DatagramReceiveMode
 * @see DatagramReadOptions
 * @since 1.0
 */
struct DatagramReadResult
{
    /**
     * @brief Number of bytes successfully copied into the destination buffer.
     *
     * This field indicates how many bytes were actually stored in the caller's buffer during the receive
     * operation. If the datagram was larger than the provided buffer capacity and truncation occurred,
     * this value will be less than `datagramSize`.
     *
     * @note For zero-length datagrams, this will be 0 even though the receive was successful.
     */
    std::size_t bytes = 0;

    /**
     * @brief Full size of the original datagram when it can be determined.
     *
     * This field represents the complete size of the received datagram before any truncation,
     * when it can be determined through:
     * - Preflight size probing (`DatagramReceiveMode::PreflightSize`)
     * - Platform-specific mechanisms (e.g., MSG_TRUNC on Linux)
     *
     * @note A value of 0 indicates the size could not be determined (no preflight or platform support).
     */
    std::size_t datagramSize = 0;

    /**
     * @brief Indicates whether datagram truncation occurred.
     *
     * Set to true if the incoming datagram was larger than the provided destination buffer's capacity,
     * causing the excess bytes to be discarded. This is a common occurrence in UDP when:
     * - The sender's message exceeds the receiver's buffer size
     * - Buffer sizing policy (`DatagramReceiveMode`) doesn't match actual datagram sizes
     * - Platform MTU or fragmentation limits are exceeded
     */
    bool truncated = false;

    /**
     * @brief Raw storage for the sender's address information.
     *
     * This field contains the platform-independent sockaddr_storage structure holding
     * the complete address information of the datagram sender. This is only valid for:
     * - Unconnected sockets accepting datagrams from any source
     * - When srcLen > 0 indicates valid sender info was captured
     *
     * @note For connected sockets or when sender info isn't needed, this may be empty.
     */
    sockaddr_storage src{};

    /**
     * @brief Length in bytes of the valid address data in src.
     *
     * This field indicates how many bytes in the src structure contain valid sender
     * address information. A value of 0 means either:
     * - The socket is in connected mode (sender info not captured)
     * - The receive operation didn't capture sender details
     * - No sender information was available
     */
    socklen_t srcLen = 0;
};

/**
 * @brief Policy for enforcing an exact-byte receive on a single UDP datagram.
 * @ingroup udp
 *
 * This structure defines the policy for reading a UDP datagram that must match a specific
 * size requirement. It provides fine-grained control over the handling of datagrams that
 * are larger or smaller than the expected size, including padding behavior and error handling.
 *
 * ### Key Features
 * - Enforces exact datagram size matching
 * - Controls zero-padding behavior for undersized datagrams
 * - Manages truncation vs error behavior for oversized datagrams
 * - Supports automatic buffer resizing for dynamic containers
 *
 * @see DatagramSocket::readExact()
 * @see DatagramSocket::readIntoExact()
 * @since 1.0
 */
struct ReadExactOptions
{
    /**
     * @brief Base receive options for controlling preflight behavior, system flags, and side effects.
     *
     * This field inherits the core receive options from @ref DatagramReadOptions, including:
     * - Datagram sizing mode (@ref DatagramReceiveMode)
     * - Buffer growth/shrink policies
     * - Additional receive flags (e.g., MSG_PEEK)
     * - Remote peer tracking and address resolution
     *
     * @see DatagramReadOptions
     */
    DatagramReadOptions base{};

    /**
     * @brief Controls whether the datagram size must match exactly.
     *
     * When true, the datagram's payload size must match the requested size exactly;
     * otherwise, a @ref SocketException is thrown. This enforces strict size matching
     * for protocols that require fixed-size messages.
     *
     * @note This is independent of @ref padIfSmaller and @ref errorOnTruncate, which
     * control how size mismatches are handled when this is false.
     */
    bool requireExact = true;

    /**
     * @brief Controls zero-padding behavior for undersized datagrams.
     *
     * When true and the received datagram is smaller than the requested size:
     * - For raw buffers: fill remaining bytes with zeros
     * - For fixed-size containers: zero-initialize unused space
     * - For dynamic containers: no effect (container is sized to actual data)
     *
     * @note Only meaningful when @ref requireExact is false.
     */
    bool padIfSmaller = false;

    /**
     * @brief Controls error handling for oversized datagrams.
     *
     * When true, receiving a datagram larger than the requested size will throw
     * a @ref SocketException. When false, the datagram is silently truncated to
     * the requested size.
     *
     * @note Only meaningful when @ref requireExact is false.
     */
    bool errorOnTruncate = true;

    /**
     * @brief Controls automatic resizing of dynamic containers.
     *
     * When true and receiving into a dynamic container (e.g., std::string,
     * std::vector), the container will be automatically resized to the exact
     * size before the receive operation.
     *
     * This ensures efficient memory use but may trigger reallocation.
     *
     * @note Has no effect on fixed-size containers or raw buffers.
     */
    bool autoResizeDynamic = true;
};

/**
 * @enum Direction
 * @brief I/O readiness selector used by waitReady().
 * @ingroup udp
 *
 * @details
 * Specifies which readiness condition to wait for on a datagram socket when calling
 * `waitReady()`. On POSIX systems this corresponds to polling for `POLLIN` (readable)
 * and/or `POLLOUT` (writable). On Windows, it maps to `POLLRDNORM` and/or `POLLWRNORM`
 * via `WSAPoll`. For UDP sockets:
 *
 * - **Read** means at least one complete datagram is queued and can be received without blocking.
 * - **Write** generally indicates the socket is writable (datagrams can be sent without blocking),
 *   though UDP sockets are often immediately writable unless the send buffer is full or an error is pending.
 * - **ReadWrite** requests readiness for either read or write (logical OR of the above conditions).
 *
 * @note This selector controls *which* condition is awaited; timeout behavior and error
 *       reporting are defined by `waitReady()` itself. Exceptional poll conditions
 *       (e.g., error flags) are surfaced by `waitReady()` as exceptions.
 *
 * @see waitReady()
 */
enum class Direction : std::uint8_t
{
    Read,     ///< Wait until the socket is readable (one or more datagrams available).
    Write,    ///< Wait until the socket is writable (can send a datagram without blocking).
    ReadWrite ///< Wait until the socket is readable **or** writable (logical OR).
};

/**
 * @class DatagramSocket
 * @ingroup udp
 * @brief Cross-platform UDP socket class with Java-style interface.
 *
 * The `DatagramSocket` class provides a convenient, cross-platform abstraction for sending and receiving UDP datagrams,
 * inspired by the Java API. It supports both IPv4 and IPv6, and works on Windows and Linux.
 *
 * ## What is UDP?
 * UDP (User Datagram Protocol) is a lightweight, connectionless protocol for sending packets over the network.
 * Unlike TCP, UDP does **not** guarantee delivery, ordering, or duplicate protection—packets may be lost,
 * arrive out of order, or duplicated. However, UDP is fast and simple, and widely used for real-time applications
 * (such as online games, video streaming, and VoIP).
 *
 * ## Key Features
 * - **Connectionless and connected modes:** You can send datagrams to any address/port, or "connect" the socket
 *   to a default destination for simpler sending/receiving.
 * - **Custom buffer size:** Easily set the size of the internal buffer for large or small datagrams.
 * - **Broadcast support:** Easily enable broadcast packets.
 * - **Timeouts and non-blocking mode:** Set timeouts and switch between blocking/non-blocking operations.
 * - **Java-style interface:** Familiar to those who have used Java networking.
 *
 * ## Example: Simple UDP Echo Server and Client
 * @code{.cpp}
 * // --- Server ---
 * #include <jsocketpp/DatagramSocket.hpp>
 * #include <iostream>
 *
 * int main() {
 *     jsocketpp::DatagramSocket server(12345); // Bind to port 12345
 *     jsocketpp::DatagramPacket packet(2048);
 *     while (true) {
 *         size_t received = server.read(packet);
 *         std::cout << "Received: " << std::string(packet.buffer.begin(), packet.buffer.end())
 *                   << " from " << packet.address << ":" << packet.port << std::endl;
 *         // Echo back
 *         server.write(packet);
 *     }
 * }
 * @endcode
 *
 * @code{.cpp}
 * // --- Client ---
 * #include <jsocketpp/DatagramSocket.hpp>
 * #include <iostream>
 *
 * int main() {
 *     jsocketpp::DatagramSocket client;
 *     std::string message = "Hello UDP!";
 *     client.write(message, "127.0.0.1", 12345); // Send to server
 *     jsocketpp::DatagramPacket response(2048);
 *     client.read(response);
 *     std::cout << "Server replied: " << std::string(response.buffer.begin(), response.buffer.end()) << std::endl;
 * }
 * @endcode
 *
 * ## Notes
 * - Not thread-safe. Use each `DatagramSocket` instance from only one thread at a time.
 * - Use the `DatagramPacket` class to store both the data and the address/port of the sender/receiver.
 * - To receive the sender’s address and port, use the `read(DatagramPacket&)` method.
 *
 * @see jsocketpp::DatagramPacket
 * @see jsocketpp::SocketException
 */
class DatagramSocket : public SocketOptions
{
  public:
    /**
     * @brief Creates a UDP socket, optionally binds to a local address, and optionally connects to a remote peer.
     * @ingroup udp
     *
     * This constructor supports both server-style and client-style UDP sockets:
     *
     * - **Server mode**: Binds to a local address and receives datagrams from any source
     * - **Client mode**: Optionally connects to a remote peer, enabling `send()`/`recv()` and automatic ICMP error
     * handling
     *
     * The socket supports:
     * - IPv4 and IPv6 (with optional dual-stack fallback)
     * - Local binding (`bind()`), optionally done during construction
     * - Connection to a remote host/port using `::connect()` semantics
     * - OS-level socket configuration (`SO_RCVBUF`, `SO_SNDBUF`, timeouts, etc.)
     * - Optional non-blocking mode
     *
     * ---
     *
     * ### Usage Modes
     *
     * - **Bound UDP Server**
     *   Useful for listening on a specific port/interface for datagrams from any peer.
     *
     *   @code
     *   DatagramSocket s(53, "0.0.0.0", ..., true, false); // bind only
     *   @endcode
     *
     * - **Connected UDP Client**
     *   Binds (optional) and then connects to a fixed remote peer.
     *   Enables `write()` / `read()`, ICMP error propagation, and performance improvements.
     *
     *   @code
     *   DatagramSocket s(0, "", ..., true, true, "1.2.3.4", 9000, 3000); // bind and connect with timeout
     *   @endcode
     *
     * ---
     *
     * ### Configuration Parameters
     *
     * @param[in] localPort The local port to bind from. Use `0` for ephemeral.
     * @param[in] localAddress The local IP to bind from (e.g., `"192.168.1.100"` or `"::"`). Empty for wildcard.
     * @param[in] recvBufferSize Optional socket receive buffer size (`SO_RCVBUF`)
     * @param[in] sendBufferSize Optional socket send buffer size (`SO_SNDBUF`)
     * @param[in] internalBufferSize Optional internal buffer size used by high-level read methods
     * @param[in] reuseAddress If `true`, enables `SO_REUSEADDR` to allow rebinding the port
     * @param[in] soRecvTimeoutMillis Timeout for receive operations in milliseconds (`-1` disables)
     * @param[in] soSendTimeoutMillis Timeout for send operations in milliseconds (`-1` disables)
     * @param[in] nonBlocking If `true`, sets the socket to non-blocking mode immediately
     * @param[in] dualStack If `true`, enables IPv6 sockets to accept IPv4-mapped addresses
     * @param[in] autoBind If `true`, performs a `bind()` using `localAddress` and `localPort` after construction
     * @param[in] autoConnect If `true`, immediately connects to the remote peer using `remoteAddress` and `remotePort`
     * @param[in] remoteAddress The remote host/IP to connect to (used only if `autoConnect == true`)
     * @param[in] remotePort The remote UDP port to connect to (used only if `autoConnect == true`)
     * @param[in] connectTimeoutMillis Timeout (in ms) for the connection attempt when `autoConnect == true`:
     *                                 - `< 0` performs a traditional blocking `connect()`
     *                                 - `>= 0` uses a timeout-aware non-blocking connect
     *
     * ---
     *
     * ### Throws
     *
     * @throws SocketException If any step of socket resolution, creation, binding, configuration, or connection fails
     * @throws SocketTimeoutException If the `connect()` call exceeds the specified `connectTimeoutMillis` timeout
     *
     * ---
     *
     * @see bind(), connect(), setSoRecvTimeout(), setSoSendTimeout(), isConnected(), write(), writeTo()
     */
    explicit DatagramSocket(Port localPort = 0, std::string_view localAddress = "",
                            std::optional<std::size_t> recvBufferSize = std::nullopt,
                            std::optional<std::size_t> sendBufferSize = std::nullopt,
                            std::optional<std::size_t> internalBufferSize = std::nullopt, bool reuseAddress = true,
                            int soRecvTimeoutMillis = -1, int soSendTimeoutMillis = -1, bool nonBlocking = false,
                            bool dualStack = true, bool autoBind = true, bool autoConnect = false,
                            std::string_view remoteAddress = "", Port remotePort = 0, int connectTimeoutMillis = -1);

    /**
     * @brief Destructor for DatagramSocket. Ensures socket resources are released.
     * @ingroup udp
     *
     * Automatically closes the underlying UDP socket and releases all associated system resources.
     * This follows RAII principles, guaranteeing cleanup when the object goes out of scope.
     *
     * - Closes the socket file descriptor (using `close()` or platform equivalent)
     * - Suppresses all exceptions during cleanup to maintain noexcept guarantees
     * - Prevents resource leaks even in exception scenarios
     *
     * @note Errors during destruction are ignored. For explicit error handling, call `close()` manually.
     * @note Do not use a DatagramSocket after destruction or move.
     *
     * @see close()
     */
    ~DatagramSocket() noexcept override;

    /**
     * @brief Copy constructor (deleted) for DatagramSocket.
     * @ingroup udp
     *
     * The copy constructor is explicitly deleted to prevent copying of `DatagramSocket`
     * instances. This class wraps a native socket file descriptor (`SOCKET`) and manages
     * system-level resources that cannot be safely or meaningfully duplicated.
     *
     * Copying would result in multiple objects referring to the same underlying socket,
     * which would violate ownership semantics and could lead to:
     * - Double closure of the same socket
     * - Undefined behavior in concurrent scenarios
     * - Silent data corruption if two sockets share internal buffers
     *
     * Instead, use move semantics to transfer ownership safely between instances.
     *
     * @note All socket classes in the library are non-copyable by design to preserve
     * RAII guarantees and strict resource control.
     *
     * @code{.cpp}
     * DatagramSocket a(12345);
     * DatagramSocket b = a; // ❌ Compilation error (copy constructor is deleted)
     * @endcode
     *
     * @see DatagramSocket(DatagramSocket&&) noexcept
     */
    DatagramSocket(const DatagramSocket&) = delete;

    /**
     * @brief Copy assignment operator (deleted) for DatagramSocket.
     * @ingroup udp
     *
     * The copy assignment operator is explicitly deleted to prevent assignment between
     * `DatagramSocket` instances. This class manages a native socket file descriptor
     * and internal buffers, which must have clear ownership semantics.
     *
     * Allowing copy assignment would result in:
     * - Multiple objects referring to the same underlying socket
     * - Risk of double-close or concurrent misuse
     * - Broken RAII guarantees and undefined behavior
     *
     * This deletion enforces safe, move-only usage patterns consistent across the socket library.
     *
     * @note Use move assignment (`operator=(DatagramSocket&&)`) if you need to transfer ownership.
     *
     * @code{.cpp}
     * DatagramSocket a(12345);
     * DatagramSocket b;
     * b = a; // ❌ Compilation error (copy assignment is deleted)
     * @endcode
     *
     * @see operator=(DatagramSocket&&) noexcept
     */
    DatagramSocket& operator=(const DatagramSocket&) = delete;

    /**
     * @brief Move constructor for DatagramSocket.
     * @ingroup udp
     *
     * Transfers ownership of the underlying socket file descriptor and internal state
     * from another `DatagramSocket` instance. After the move, the source object is left
     * in a valid but unspecified state and should not be used.
     *
     * ### What Is Transferred
     * - Native socket handle (`_sockFd`)
     * - Address resolution pointers (`_addrInfoPtr`, `_selectedAddrInfo`)
     * - Local address metadata
     * - Internal receive buffer
     * - Port number
     *
     * ### Rationale
     * - Enables safe transfer of socket ownership
     * - Preserves resource integrity without duplicating system handles
     * - Supports RAII and container use (e.g., `std::vector<DatagramSocket>`)
     *
     * @note The moved-from object is invalidated and cannot be reused.
     *
     * @code{.cpp}
     * DatagramSocket a(12345);
     * DatagramSocket b = std::move(a); // ✅ b now owns the socket
     * @endcode
     *
     * @see operator=(DatagramSocket&&) noexcept
     */
    DatagramSocket(DatagramSocket&& rhs) noexcept
        : SocketOptions(rhs.getSocketFd()), _remoteAddr(rhs._remoteAddr), _remoteAddrLen(rhs._remoteAddrLen),
          _localAddr(rhs._localAddr), _localAddrLen(rhs._localAddrLen), _internalBuffer(std::move(rhs._internalBuffer)),
          _port(rhs._port), _isBound(rhs._isBound), _isConnected(rhs._isConnected)
    {
        rhs.setSocketFd(INVALID_SOCKET);
        rhs._remoteAddrLen = 0;
        rhs._localAddrLen = 0;
        rhs._port = 0;
        rhs._isBound = false;
        rhs._isConnected = false;
    }

    /**
     * @brief Move assignment operator for DatagramSocket.
     * @ingroup udp
     *
     * Releases any resources owned by this socket instance and transfers ownership
     * from another `DatagramSocket` object (`rhs`). After the operation:
     *
     * - This socket adopts the file descriptor, buffer, and internal state from `rhs`
     * - The source object is left in a valid but unspecified state (`INVALID_SOCKET`)
     * - Existing resources in the destination socket are released via `close()`
     *
     * ### Ownership Transferred
     * - Native socket handle (`_sockFd`)
     * - Local address metadata (`_localAddr`, `_localAddrLen`)
     * - Internal read buffer
     * - Port number (for tracking purposes)
     *
     * ### Notes
     * @note This method is `noexcept` and safe to use in container scenarios (e.g., `std::vector<DatagramSocket>`).
     * @note After the move, `rhs` will be left in an uninitialized state and should not be used except for destruction
     * or reassignment.
     * @note If `close()` throws during cleanup, the exception is suppressed to preserve noexcept guarantees.
     *
     * ### Example
     * @code{.cpp}
     * DatagramSocket a(12345);
     * DatagramSocket b;
     * b = std::move(a); // b now owns the socket previously held by a
     * assert(!a.isValid());
     * assert(b.isValid());
     * @endcode
     *
     * @param[in,out] rhs The source `DatagramSocket` to move from.
     * @return Reference to the updated `*this` object.
     *
     * @see DatagramSocket(DatagramSocket&&) noexcept
     * @see close(), isValid()
     */
    DatagramSocket& operator=(DatagramSocket&& rhs) noexcept
    {
        if (this != &rhs)
        {
            // Clean up current socket
            try
            {
                close(); // Clean up existing resources
            }
            catch (...)
            {
                // CloseSocket error is ignored.
                // TODO: Consider adding an internal flag, nested exception, or user-configurable error handler
                //       to report errors in future versions.
            }

            // Transfer ownership
            setSocketFd(rhs.getSocketFd());
            _remoteAddr = rhs._remoteAddr;
            _remoteAddrLen = rhs._remoteAddrLen;
            _localAddr = rhs._localAddr;
            _localAddrLen = rhs._localAddrLen;
            _internalBuffer = std::move(rhs._internalBuffer);
            _port = rhs._port;
            _isBound = rhs._isBound;
            _isConnected = rhs._isConnected;

            // Reset source
            rhs.setSocketFd(INVALID_SOCKET);
            rhs._remoteAddrLen = 0;
            rhs._localAddrLen = 0;
            rhs._port = 0;
            rhs._isBound = false;
            rhs._isConnected = false;
        }
        return *this;
    }

    /**
     * @brief Binds the datagram socket to all available interfaces on an ephemeral port.
     * @ingroup udp
     *
     * This method binds the `DatagramSocket` to an ephemeral (auto-assigned) local port
     * on all local network interfaces (`0.0.0.0` for IPv4 or `::` for IPv6), depending on
     * system configuration and address resolution.
     *
     * ### Common Use Cases
     * - UDP client sockets that do not require a specific local port
     * - Transient sockets used for fire-and-forget messages, RPC, or NAT traversal
     * - Applications that allow the OS to choose a source port dynamically
     *
     * ### Behavior
     * - Uses `getaddrinfo()` with `AI_PASSIVE` and a wildcard local address (`0.0.0.0` or `::`)
     * - Binds to the first successfully resolved and compatible local address
     * - On success, updates internal `_isBound` flag and enables receiving datagrams
     *
     * @note This method may only be called once per socket instance. Rebinding is not supported.
     * @note If the socket was constructed with an already-resolved address, this will override it.
     *
     * @throws SocketException if address resolution or binding fails.
     *
     * @code{.cpp}
     * DatagramSocket sock;
     * sock.bind(); // Binds to all interfaces on an ephemeral port (e.g., 0.0.0.0:49512 or [::]:49512)
     * @endcode
     *
     * @see bind(Port localPort), bind(std::string_view localAddress, Port localPort), getLocalSocketAddress()
     */
    void bind();

    /**
     * @brief Binds the datagram socket to a specific local port on all network interfaces.
     * @ingroup udp
     *
     * This overload binds the socket to the given UDP `localPort` across all available network
     * interfaces, using a wildcard address (`0.0.0.0` for IPv4 or `::` for IPv6).
     *
     * ### Common Use Cases
     * - Server-side sockets that need to receive packets on a known port
     * - P2P or NAT traversal clients using fixed source ports
     * - Test setups or replay systems where the port number must be predictable
     *
     * ### Behavior
     * - Uses `getaddrinfo()` with `AI_PASSIVE` and a null host to resolve wildcard binding addresses
     * - Attempts all resolved addresses until `bind()` succeeds
     * - If successful, sets `_isBound = true` and enables subsequent `read()` or `recvFrom()` operations
     *
     * @param[in] localPort UDP port number to bind to. Must be in the range [1, 65535], or 0 to request an ephemeral
     * port.
     *
     * @note This method may only be called once per socket instance. Rebinding is not supported.
     * @note If the specified port is already in use, a `SocketException` will be thrown.
     *
     * @throws SocketException if address resolution or binding fails, or if the socket is already bound.
     *
     * @code{.cpp}
     * DatagramSocket server;
     * server.bind(12345); // Bind to port 12345 on all interfaces
     * @endcode
     *
     * @see bind(), bind(std::string_view, Port), getLocalSocketAddress(), setReuseAddress()
     */
    void bind(Port localPort);

    /**
     * @brief Binds the datagram socket to a specific local IP address and port.
     * @ingroup udp
     *
     * This method allows full control over the local binding interface by specifying
     * both the local IP address (`host`) and port. It supports IPv4 and IPv6 addresses,
     * including loopback, multicast-capable interfaces, and link-local addresses.
     *
     * ### Common Use Cases
     * - Multihomed systems binding to a specific NIC/interface
     * - Clients or servers requiring fixed local IP-port pairing
     * - Binding to loopback or link-local addresses
     * - Low-level networking tools (e.g., packet sniffers, trace clients)
     *
     * ### Behavior
     * - Uses `getaddrinfo()` to resolve the provided IP/hostname and port
     * - Tries all resolved addresses until one binds successfully
     * - Updates internal state to reflect the binding result
     *
     * @param[in] localAddress Local IP address or hostname to bind to (e.g., "127.0.0.1", "::1", "eth0.local").
     *             Use "0.0.0.0" or "::" to bind to all interfaces (same as `bind(port)`).
     * @param[in] localPort Local UDP port number to bind to. Use 0 for ephemeral port assignment.
     *
     * @note This method may only be called once. Rebinding is not supported.
     * @note If resolution fails, or no address can be bound, a `SocketException` is thrown.
     *
     * @throws SocketException if address resolution or binding fails.
     *
     * @code{.cpp}
     * DatagramSocket sock;
     * sock.bind("192.168.1.42", 9000); // Bind to specific interface and port
     * @endcode
     *
     * @see bind(), bind(Port), getLocalSocketAddress(), setReuseAddress()
     */
    void bind(std::string_view localAddress, Port localPort);

    /**
     * @brief Indicates whether the datagram socket has been explicitly bound to a local address or port.
     * @ingroup udp
     *
     * Returns `true` if the socket has successfully completed a call to one of the `bind()` overloads.
     * Binding is optional for UDP sockets, but is commonly required for:
     *
     * - Receiving datagrams on a specific port (e.g., UDP server or listener)
     * - Specifying a fixed source port (e.g., for NAT traversal or P2P scenarios)
     * - Selecting a specific local interface in multihomed systems
     * - Participating in multicast or broadcast communication
     *
     * @note A datagram socket can only be bound once. Attempting to bind again will throw.
     * @note If the socket was constructed with a port but `bind()` is never called, it remains unbound.
     * @note Binding must occur before calling `connect()` or sending datagrams from an unconnected socket.
     *
     * ### Example
     * @code
     * DatagramSocket sock;
     * sock.bind(12345);
     * if (sock.isBound()) {
     *     std::cout << "Listening on " << sock.getLocalSocketAddress() << "\n";
     * }
     * @endcode
     *
     * @return `true` if the socket has been bound to a local address/port, `false` otherwise.
     *
     * @see bind(), isConnected(), getLocalSocketAddress()
     */
    [[nodiscard]] bool isBound() const noexcept { return _isBound; }

    /**
     * @brief Resolves and connects the datagram socket to a remote UDP peer with optional timeout.
     * @ingroup udp
     *
     * Although UDP is a connectionless protocol, calling `connect()` on a datagram socket sets a default remote
     * peer. This operation internally performs a `::connect()` syscall, which offers the following advantages:
     *
     * ---
     *
     * ### 🔌 What Is Connected UDP?
     *
     * A UDP socket normally uses `sendto()` to send data to any arbitrary destination and `recvfrom()` to receive
     * from any source. When a socket is "connected" to a fixed peer:
     *
     * - It can use `send()` / `recv()` instead of `sendto()` / `recvfrom()`
     * - Incoming datagrams are filtered to only accept those from the connected peer
     * - Outgoing datagrams are always sent to the connected peer
     * - ICMP errors (e.g., port unreachable) are reliably reported via `recv()` and `send()`
     * - You may benefit from slightly faster I/O due to simplified kernel bookkeeping
     *
     * This mode is ideal for client-side UDP protocols like DNS, QUIC, RTP, STUN/TURN, or custom request/response
     * protocols.
     *
     * ---
     *
     * ### 🧠 Example Use Cases
     *
     * - **DNS client**:
     *   - Connect to `"8.8.8.8:53"` and send queries using `write()`
     *
     * - **UDP echo or ping client**:
     *   - Use `write("ping")` / `read()` loop with a known server
     *
     * - **QUIC or DTLS over UDP**:
     *   - Secure transport over a connected datagram channel
     *
     * - **Firewall/NAT diagnostics**:
     *   - By connecting, ICMP errors are reliably propagated for unreachable peers
     *
     * ---
     *
     * ### ⚙️ Behavior
     *
     * This method resolves the target `host:port` using DNS (`getaddrinfo()`), selects the best available
     * address, and performs a `::connect()` syscall. After success:
     *
     * - The socket is considered **connected**
     * - Only datagrams from the connected peer are received
     * - `write()` and `read()` may be used
     * - Internal `_isConnected` is set to `true`
     *
     * ---
     *
     * ### ⏱ Timeout Handling
     *
     * This method supports both blocking and timeout-aware non-blocking connect:
     *
     * - `timeoutMillis < 0`: Performs a standard blocking `connect()`
     * - `timeoutMillis >= 0`: Temporarily switches to non-blocking mode and uses `select()` to wait
     *
     * After timeout-based connect, the socket’s original blocking mode is restored automatically.
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock(0, "", ..., true, false); // bind to ephemeral port, no connect yet
     * sock.connect("8.8.8.8", 53);                  // blocking connect
     * sock.write("dns query");                     // implicit sendto(8.8.8.8:53)
     * auto response = sock.read();                 // only accepts from 8.8.8.8
     * @endcode
     *
     * @param[in] host     Hostname or IP address of the remote UDP peer
     * @param[in] port     Port number of the remote UDP peer
     * @param[in] timeoutMillis Optional timeout for connect:
     *                          - `< 0` performs a blocking `connect()`
     *                          - `>= 0` uses non-blocking `connect()` with timeout
     *
     * @throws SocketTimeoutException If the connection times out before completion
     * @throws SocketException If resolution, socket creation, or `connect()` fails
     *
     * @note This operation does not perform any UDP handshaking — it simply sets the default destination.
     *       The UDP socket remains datagram-based and unreliable.
     *
     * @note Any existing connection will be overwritten.
     *
     * @see read(), write(), disconnect(), isConnected(), ScopedBlockingMode, resolveAddress()
     */
    void connect(std::string_view host, Port port, int timeoutMillis = -1);

    /**
     * @brief Disconnects the datagram socket from its currently connected peer.
     * @ingroup udp
     *
     * This method disassociates the datagram socket from the previously connected
     * remote host and port, returning it to an unconnected state. After disconnection:
     *
     * - The socket can receive datagrams from any remote source.
     * - You must specify a destination when calling `write()` or `sendto()`.
     * - The internal `_isConnected` flag is cleared.
     *
     * This is useful for switching from connected-mode (e.g., unicast-only) to
     * connectionless mode (e.g., dynamic peer-to-peer or server-mode behavior).
     *
     * ### Behavior
     * - Internally calls `::connect()` with a null address (or `AF_UNSPEC`) to break the association.
     * - This is supported on most platforms (e.g., Linux, Windows).
     * - No data is lost or flushed—this only affects connection state.
     *
     * @note This method is a no-op if the socket is already unconnected.
     * @note After disconnection, calling `write()` without a destination will throw.
     *
     * ### Example
     * @code{.cpp}
     * DatagramSocket sock("example.com", 9999);
     * sock.connect();
     * sock.write("hello"); // connected mode
     * sock.disconnect();
     * sock.write("hello", "example.org", 8888); // connectionless mode
     * @endcode
     *
     * @throws SocketException if the underlying disconnect operation fails.
     *
     * @see connect(), isConnected(), write(), read<T>()
     */
    void disconnect();

    /**
     * @brief Indicates whether the datagram socket is connected to a specific remote peer.
     * @ingroup udp
     *
     * Returns `true` if the socket has been successfully connected to a remote address and port
     * using the `connect()` method. While UDP is a connectionless protocol, invoking `connect()`
     * on a datagram socket enables connection-oriented semantics:
     *
     * - Filters incoming datagrams to only accept from the connected peer
     * - Allows use of `send()` / `recv()` instead of `sendto()` / `recvfrom()`
     * - Enables simplified calls like `write("message")` or `read<T>()`
     *
     * @note This method reflects the internal connection state as tracked by the library.
     * @note It does not verify whether the remote host is reachable or alive.
     * @note Unconnected sockets may still send and receive using `write(host, port)` or `write(DatagramPacket)`.
     *
     * ### Example
     * @code
     * DatagramSocket sock("example.com", 9999);
     * sock.connect();
     * if (sock.isConnected()) {
     *     sock.write("ping");
     * }
     * @endcode
     *
     * @return `true` if the socket has been logically connected to a peer, `false` otherwise.
     *
     * @see connect(), write(std::string_view), read<T>(), disconnect()
     */
    [[nodiscard]] bool isConnected() const noexcept { return _isConnected; }

    /**
     * @brief Retrieves the local IP address this socket is bound to.
     * @ingroup udp
     *
     * This method returns the IP address that this socket is bound to on the local system.
     * The address may have been set explicitly via `bind()` or automatically assigned
     * by the operating system.
     *
     * ---
     *
     * ### Behavior
     * - For explicitly bound sockets, returns the address specified in `bind()`
     * - For auto-bound sockets, returns the OS-assigned interface address
     * - For unbound sockets or if `getsockname()` fails, throws `SocketException`
     * - For IPv6 sockets, may return IPv4-mapped addresses (e.g., "::ffff:127.0.0.1")
     *
     * ---
     *
     * @param[in] convertIPv4Mapped If `true`, IPv4-mapped IPv6 addresses (e.g., `::ffff:192.0.2.1`)
     *                              will be returned as plain IPv4 strings (`192.0.2.1`). If `false`,
     *                              the raw mapped form is preserved.
     *
     * @return A string representation of the local IP address (e.g., "192.168.1.10" or "::1").
     *
     * @throws SocketException If:
     * - The socket is not open
     * - The socket is not bound
     * - The system call `getsockname()` fails
     * - Address conversion fails
     *
     * @see getLocalPort() Get the local port number
     * @see getLocalSocketAddress() Get the combined "IP:port" string
     * @see bind() Explicitly bind to an address
     */
    [[nodiscard]] std::string getLocalIp(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieves the local port number this datagram socket is bound to.
     * @ingroup udp
     *
     * This method returns the local UDP port that the socket is currently bound to,
     * either explicitly via `bind()` or implicitly assigned by the operating system.
     *
     * ---
     *
     * ### ⚙️ Behavior
     * - Uses `getsockname()` to query the underlying bound port
     * - Returns the port in host byte order
     * - Works for both IPv4 and IPv6 sockets
     * - Safe for auto-assigned ephemeral ports (`port = 0` in bind)
     *
     * ---
     *
     * ### 🧪 Example
     * @code
     * DatagramSocket sock;
     * sock.bind("0.0.0.0", 0);  // Let OS pick an ephemeral port
     *
     * Port port = sock.getLocalPort();
     * std::cout << "Bound to port: " << port << "\n";
     * @endcode
     *
     * ---
     *
     * @return The local UDP port number in host byte order.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - The socket is not bound
     * - The `getsockname()` call fails
     *
     * @see getLocalIp() Get the bound IP address
     * @see getLocalSocketAddress() Get the combined "IP:port" string
     * @see bind() Explicitly bind to an address/port
     */
    [[nodiscard]] Port getLocalPort() const;

    /**
     * @brief Retrieves the local socket address as a formatted string in the form `"IP:port"`.
     * @ingroup udp
     *
     * This method returns the local IP address and port that this socket is bound to, formatted
     * as a human-readable string (e.g., `"192.168.1.42:12345"` or `"[::1]:9999"`). It works for
     * both explicitly bound sockets and those where the operating system has auto-assigned an
     * ephemeral port.
     *
     * ---
     *
     * ### ⚙️ Core Behavior
     * - Uses `getsockname()` to query the bound local address and port
     * - Formats IPv4 addresses as `"ip:port"` (e.g., "127.0.0.1:8080")
     * - Formats IPv6 addresses with square brackets: `"[ipv6]:port"` (e.g., "[::1]:8080")
     * - For unbound sockets, throws `SocketException`
     * - Works with both IPv4 and IPv6 address families
     * - Safe to call after automatic port assignment
     *
     * ---
     *
     * ### 🧪 Example
     * @code
     * DatagramSocket sock;
     * sock.bind("0.0.0.0", 0);  // Let OS pick ephemeral port
     *
     * std::cout << "Bound to: " << sock.getLocalSocketAddress() << "\n";
     * // Output: "0.0.0.0:50432"
     * @endcode
     *
     * ---
     *
     * @param[in] convertIPv4Mapped Whether to convert IPv4-mapped IPv6 addresses to IPv4
     *                             format (e.g., "::ffff:127.0.0.1" → "127.0.0.1").
     *                             Default is true.
     *
     * @return A string in the format `"IP:port"` or `"[IPv6]:port"`.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - The socket is not bound
     * - `getsockname()` fails
     * - Address conversion fails
     *
     * @see getLocalIp() Get just the IP portion
     * @see getLocalPort() Get just the port number
     * @see bind() Explicitly bind to an address/port
     * @since 1.0
     */
    [[nodiscard]] std::string getLocalSocketAddress(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieves the IP address of the remote peer from the socket's current state.
     * @ingroup udp
     *
     * Returns the remote peer's IP address based on the socket's connection state and prior
     * communication history. The address is returned as a human-readable string (e.g., "192.168.1.42"
     * or "::1").
     *
     * ---
     *
     * ### 🔁 Behavior Based on Connection Mode
     * - **Connected Socket:**
     *   - Returns the connected peer's IP as set by `connect()`
     *   - Uses `getpeername()` to obtain the fixed peer address
     *
     * - **Unconnected Socket:**
     *   - Returns the most recently active peer's IP, updated after:
     *     - Receiving via `read()` or `readInto()`
     *     - Receiving via `readFrom()`
     *     - Sending via `writeTo()` or `write(DatagramPacket)`
     *
     * ---
     *
     * ### IPv6 Handling
     * If the peer address is an IPv4-mapped IPv6 address (e.g., "::ffff:192.0.2.1") and
     * `convertIPv4Mapped` is true (default), it is simplified to standard IPv4 form ("192.0.2.1").
     *
     * ---
     *
     * ### Example
     * @code
     * // Unconnected mode
     * DatagramSocket sock(12345);
     * sock.bind();
     *
     * DatagramPacket packet(1024);
     * sock.read(packet);
     * std::cout << "Last sender: " << sock.getRemoteIp() << "\n";
     *
     * // Connected mode
     * sock.connect("example.com", 9999);
     * std::cout << "Connected to: " << sock.getRemoteIp() << "\n";
     * @endcode
     *
     * ---
     *
     * @param[in] convertIPv4Mapped If true, simplify IPv4-mapped IPv6 addresses to IPv4 form.
     * @return The remote peer's IP address as a string.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - In connected mode, `getpeername()` fails
     * - In unconnected mode, no peer information is available yet
     * - Address conversion fails
     *
     * @see getRemotePort()
     * @see getRemoteSocketAddress()
     * @see isConnected()
     * @see connect()
     * @since 1.0
     */
    [[nodiscard]] std::string getRemoteIp(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieves the remote peer's UDP port number in host byte order.
     * @ingroup udp
     *
     * Returns the port number of the remote peer associated with this socket, depending on its connection state:
     *
     * - **Connected socket:** Returns the port of the peer set via `connect()`, using `getpeername()`.
     * - **Unconnected socket:** Returns the port of the most recent sender or destination, as updated by
     *   `read()`, `readInto()`, `readFrom()`, `writeTo()`, or `write(DatagramPacket)`.
     *
     * If no communication has occurred yet in unconnected mode, this method throws a `SocketException`.
     *
     * @return The remote UDP port number in host byte order.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - In connected mode, `getpeername()` fails
     * - In unconnected mode, no peer information is available yet
     *
     * @see getRemoteIp()
     * @see getRemoteSocketAddress()
     * @see isConnected()
     * @see connect()
     * @since 1.0
     */
    [[nodiscard]] Port getRemotePort() const;

    /**
     * @brief Retrieves the remote peer’s socket address in the form `"IP:port"`.
     * @ingroup udp
     *
     * This method returns the formatted address of the remote peer that this datagram socket
     * is either:
     * - **Connected to** via `connect()` —or—
     * - **Most recently communicated with** using `recvfrom()`, `read()`, `writeTo()`, or `write(DatagramPacket)`
     *   in unconnected mode.
     *
     * ---
     *
     * ### 🔁 Behavior Based on Connection Mode
     * - **Connected DatagramSocket**: Uses `getpeername()` to retrieve the connected remote address.
     * - **Unconnected DatagramSocket**: Returns the IP and port of the most recent sender or destination,
     *   as updated by:
     *   - `read()` or `read<T>()`
     *   - `recvFrom()`
     *   - `writeTo()` or `write(DatagramPacket)`
     *
     *   If no such operation has occurred yet, the method throws a `SocketException`.
     *
     * ---
     *
     * ### IPv6 Handling
     * If the returned IP is an IPv4-mapped IPv6 address (e.g., `::ffff:192.0.2.1`) and
     * `@p convertIPv4Mapped` is `true` (default), the IP portion is simplified to IPv4 form.
     *
     * ---
     *
     * ### Example (Unconnected Mode)
     * @code
     * DatagramSocket sock(12345);
     *
     * DatagramPacket packet(1024);
     * sock.read(packet); // Wait for a sender
     *
     * std::string remote = sock.getRemoteSocketAddress();
     * std::cout << "Received from: " << remote << "\n"; // e.g., "192.168.0.42:56789"
     * @endcode
     *
     * ---
     *
     * @param[in] convertIPv4Mapped Whether to simplify IPv4-mapped IPv6 addresses (default: true).
     * @return Remote socket address as a string in the format `"IP:port"`.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - In unconnected mode, no datagram has been sent or received yet
     * - In connected mode, if `getpeername()` fails
     *
     * @see getRemoteIp()
     * @see getRemotePort()
     * @see isConnected()
     * @see writeTo(), write(DatagramPacket), read()
     */
    [[nodiscard]] std::string getRemoteSocketAddress(bool convertIPv4Mapped = true) const;

    /**
     * @brief Send one UDP datagram to the currently connected peer (no pre-wait).
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram containing the bytes in @p message. This method performs
     * an immediate send attempt:
     *
     * 1. Validates that the socket is **open** and **connected**.
     * 2. If the payload is empty, it returns immediately (zero-length datagrams are valid
     *    per UDP, but this implementation avoids the syscall as a micro-optimization).
     * 3. Enforces protocol-level size limits for the connected peer’s address family via
     *    `enforceSendCapConnected(message.size())` (prevents guaranteed `EMSGSIZE` failures).
     * 4. Calls the internal single-syscall sender to transmit the datagram.
     *
     * This function does **not** poll or wait for writability. On non-blocking sockets,
     * if the send buffer is temporarily full, the underlying send may fail (e.g.,
     * `EWOULDBLOCK`/`WSAEWOULDBLOCK`) and a `SocketException` is thrown. If you need the
     * method to wait until the socket is writable, use `writeAll()` or one of the timeout
     * variants instead.
     *
     * **Atomicity:** UDP is message-oriented; on success, the *entire* payload is sent in
     * one datagram (no partial sends). Failures are reported via exceptions—no bytes are
     * considered transmitted on exception.
     *
     * @param[in] message  Bytes to send as a single datagram. Accepts `std::string`,
     *                     string literals, and other contiguous character sequences via
     *                     implicit conversion to `std::string_view`.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @post On success, exactly `message.size()` bytes are handed to the kernel as a
     *       single datagram. The connection state and socket options are unchanged.
     *
     * @throws SocketException
     *   - **Logical errors (error code = 0):**
     *     - Socket is not open or not connected.
     *     - Payload exceeds the permitted maximum for the connected peer’s family
     *       (detected by `enforceSendCapConnected()`).
     *   - **System errors (OS error code + `SocketErrorMessage(...)`):**
     *     - Transient non-blocking condition (`EWOULDBLOCK` / `WSAEWOULDBLOCK`).
     *     - Resource/network issues (`ENOBUFS`, `ENETUNREACH`, `EHOSTUNREACH`, etc.).
     *     - Other send failures surfaced by the OS.
     *
     * @note Zero-length payloads: this method returns without sending. If you need to
     *       intentionally transmit an empty datagram as a signal, use an explicit flag
     *       in your protocol or send a 1-byte marker instead.
     *
     * @see writeAll(), writeWithTimeout(), writeTo(), enforceSendCapConnected()
     *
     * @code
     * DatagramSocket sock;
     * // ... socket opened and connected elsewhere ...
     * sock.write("ping");                      // immediate attempt; may throw on EWOULDBLOCK
     * sock.write(std::string_view{"hello"});   // same, using string_view explicitly
     * @endcode
     *
     * @since 1.0
     */
    void write(std::string_view message) const;

    /**
     * @brief Send one UDP datagram whose payload is the raw object representation of @p value.
     * @ingroup udp
     *
     * @tparam T
     *   Trivially copyable, standard-layout type whose in-memory object representation
     *   will be sent verbatim as the datagram payload. (See Implementation notes.)
     *
     * @details
     * Emits exactly one datagram containing the bytes of @p value. This method performs
     * an **immediate** send attempt (no pre-wait). Use `writeAll()` or `writeWithTimeout()`
     * if you need the call to block until the socket is writable.
     *
     * Processing steps:
     *  1. Verifies the socket is **open** and **connected**.
     *  2. Computes `sizeof(T)` and enforces address-family maxima via
     *     `enforceSendCapConnected(sizeof(T))` to preempt guaranteed `EMSGSIZE` failures.
     *  3. Transmits the payload in a **single** system call; on success, the *entire* object
     *     representation is handed to the kernel as one UDP datagram (no partial sends).
     *
     * ⚠️ **Serialization & portability**
     * - The bytes sent are the raw object representation of `T` (including padding).
     *   Endianness, padding, and ABI differences make this suitable mainly for homogeneous
     *   peers or debugging. For interoperable protocols, prefer explicit serialization
     *   (fixed-width integers in network byte order, packed layouts, etc.).
     * - If `T` contains padding, ensure it is initialized to avoid leaking stale memory.
     *
     * **Atomicity:** UDP is message-oriented; on success, there are no partial sends.
     * Any failure is reported via exception and **no** bytes are considered transmitted.
     *
     * @param[in] value
     *   The value to send; its bytes (object representation) become the datagram payload.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @post On success, exactly `sizeof(T)` bytes are queued to the kernel as a single datagram.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):**
     *     - Socket is not open or not connected.
     *     - Payload exceeds the permitted maximum for the connected peer’s family
     *       (detected by `enforceSendCapConnected()`).
     *   - **System (OS error code + `SocketErrorMessage(...)`):**
     *     - Transient non-blocking condition (`EWOULDBLOCK` / `WSAEWOULDBLOCK`).
     *     - Resource/network issues (`ENOBUFS`, `ENETUNREACH`, `EHOSTUNREACH`, etc.).
     *
     * @note This method does **not** wait for writability. Use `writeAll()` (infinite wait)
     *       or `writeWithTimeout()` (bounded wait) for backpressure handling.
     *
     * @since 1.0
     *
     * @see write(std::string_view), writeAll(), writeWithTimeout(),
     *      writeTo(std::string_view, Port, std::string_view), enforceSendCapConnected()
     *
     * @par Implementation notes
     * Uses `std::bit_cast<std::array<std::byte, sizeof(T)>>` (C++20) to capture the exact
     * object representation. Compile-time constraints:
     * @code
     * static_assert(std::is_trivially_copyable_v<T>);
     * static_assert(std::is_standard_layout_v<T>);
     * @endcode
     */
    template <typename T> void write(const T& value) const
    {
        static_assert(std::is_trivially_copyable_v<T>, "DatagramSocket::write<T>() requires trivially copyable type");
        static_assert(std::is_standard_layout_v<T>, "DatagramSocket::write<T>() requires standard layout type");

        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::write<T>(): socket is not open.");

        if (!_isConnected)
            throw SocketException("DatagramSocket::write<T>(): socket is not connected. Use writeTo() instead.");

        enforceSendCapConnected(sizeof(T));

        const auto buffer = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
        internal::sendExact(getSocketFd(), buffer.data(), buffer.size());
    }

    /**
     * @brief Send one UDP datagram to the connected peer from a raw byte span (no pre-wait).
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram containing the bytes referenced by @p data. This method
     * performs an **immediate** send attempt:
     *
     * 1. Verifies the socket is **open** and **connected**.
     * 2. If the span is empty, it returns immediately (zero-length datagrams are valid in UDP,
     *    but this implementation skips the syscall as a micro-optimization).
     * 3. Enforces protocol-level size limits for the connected peer’s address family via
     *    `enforceSendCapConnected(data.size())` to preempt guaranteed `EMSGSIZE` failures.
     * 4. Transmits the payload in a **single** system call (no partial sends).
     *
     * This function does **not** poll for writability. On non-blocking sockets, if the send
     * buffer is full, the send may fail (e.g., `EWOULDBLOCK`/`WSAEWOULDBLOCK`) and a
     * `SocketException` is thrown. If you need backpressure handling, use `writeAll()` (infinite
     * wait) or `writeWithTimeout()` (bounded wait) instead.
     *
     * **Atomicity:** UDP is message-oriented; on success the *entire* span is sent as one
     * datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] data  Contiguous read-only bytes to send as a single datagram.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @post On success, exactly `data.size()` bytes are handed to the kernel as one datagram.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):**
     *     - Socket is not open or not connected.
     *     - Payload exceeds the permitted maximum for the connected peer’s family
     *       (detected by `enforceSendCapConnected()`).
     *   - **System (OS error code + `SocketErrorMessage(...)`):**
     *     - Transient non-blocking condition (`EWOULDBLOCK` / `WSAEWOULDBLOCK`).
     *     - Resource/network issues (`ENOBUFS`, `ENETUNREACH`, `EHOSTUNREACH`, etc.).
     *
     * @note This overload is ideal when you already have binary data as bytes. For textual
     *       data, prefer `write(std::string_view)`. For POD types, see `write<T>(const T&)`.
     * @note Zero-length payloads: this method returns without sending. If you must signal an
     *       “empty message,” consider a protocol marker instead of an empty datagram.
     *
     * @since 1.0
     *
     * @see write(std::string_view), writeAll(), writeWithTimeout(),
     *      write(const T&), writeFrom(const void*, std::size_t),
     *      writev(std::span<const std::string_view>), enforceSendCapConnected()
     *
     * @code
     * // Example: send raw binary using std::byte
     * std::array<std::byte, 4> magic{ std::byte{0xDE}, std::byte{0xAD},
     *                                 std::byte{0xBE}, std::byte{0xEF} };
     * sock.write(std::span<const std::byte>{magic});
     *
     * // From an existing buffer:
     * const std::byte* p = reinterpret_cast<const std::byte*>(buffer.data());
     * sock.write(std::span<const std::byte>{p, buffer.size()});
     * @endcode
     */
    void write(std::span<const std::byte> data) const;

    /**
     * @brief Send one UDP datagram to the connected peer, waiting indefinitely for writability.
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram containing @p message. Unlike `write()`, this method
     * **pre-waits** for the socket to become writable so it behaves consistently on
     * both blocking and non-blocking sockets:
     *
     * 1. Verifies the socket is **open** and **connected**.
     * 2. If the payload is empty, returns immediately (zero-length UDP datagrams are valid,
     *    but this implementation skips the syscall as a micro-optimization).
     * 3. Enforces protocol-level size limits for the connected peer via
     *    `enforceSendCapConnected(message.size())`.
     * 4. Calls `waitReady(Direction::Write, -1)` to wait **without timeout** until writable.
     * 5. Transmits the datagram in a **single** syscall (no partial sends).
     *
     * **Atomicity:** On success the *entire* payload is sent in one datagram. On exception,
     * no bytes are considered transmitted.
     *
     * @param[in] message  Bytes to send as a single datagram.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @post On success, exactly `message.size()` bytes are handed to the kernel as one datagram.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):** socket not open/connected; payload exceeds permitted maximum
     *     for the connected family (via `enforceSendCapConnected()`).
     *   - **System (OS error + `SocketErrorMessage(...)`):** polling or send failures.
     *
     * @note This method never throws `SocketTimeoutException` because it waits indefinitely.
     *       For a bounded wait, use `writeWithTimeout()`.
     *
     * @since 1.0
     *
     * @see write(std::string_view), writeWithTimeout(std::string_view, int),
     *      writevAll(std::span<const std::string_view>), enforceSendCapConnected(), waitReady()
     */
    void writeAll(std::string_view message) const;

    /**
     * @brief Send a length-prefixed UDP datagram to the connected peer from text bytes (no pre-wait).
     * @ingroup udp
     *
     * @tparam T
     *   Unsigned integral type for the length prefix (e.g., `std::uint8_t`, `std::uint16_t`,
     *   `std::uint32_t`, `std::uint64_t`). A compile-time check in the helper enforces this.
     *
     * @details
     * Wraps the payload as **[ prefix(T, big-endian) | payload ]** and forwards to
     * `sendPrefixedConnected<T>()`. Zero-length payloads are valid: a prefix-only
     * datagram is sent. This method performs an **immediate** send attempt (no pre-wait).
     *
     * @param[in] payload  Text bytes to send after the length prefix.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @throws SocketException
     *   - **Logical (error=0):** socket not open/connected; `payload.size()` exceeds
     *     `std::numeric_limits<T>::max()`; total size exceeds the connected family’s limit.
     *   - **System (OS error + `SocketErrorMessage(...)`):** send failures (e.g., `EWOULDBLOCK`, `ENOBUFS`).
     *
     * @since 1.0
     *
     * @see sendPrefixedConnected<T>(std::span<const std::byte>),
     *      writePrefixedTo<T>(std::string_view, Port, std::string_view),
     *      encodeLengthPrefixBE<T>(std::size_t),
     *      enforceSendCapConnected(std::size_t)
     */
    template <typename T> void writePrefixed(const std::string_view payload) const
    {
        sendPrefixedConnected<T>(asBytes(payload));
    }

    /**
     * @brief Send a length-prefixed UDP datagram to the connected peer from a byte span (no pre-wait).
     * @ingroup udp
     *
     * @tparam T  Unsigned integral type for the length prefix.
     *
     * @details
     * Builds **[ prefix(T, big-endian) | payload ]** and forwards to
     * `sendPrefixedConnected<T>()`. Zero-length payloads yield a prefix-only datagram.
     * This method performs an **immediate** send attempt (no pre-wait).
     *
     * @param[in] payload  Binary payload to append after the length prefix.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @throws SocketException  (same categories as the `std::string_view` overload).
     *
     * @since 1.0
     *
     * @see sendPrefixedConnected<T>(std::span<const std::byte>),
     *      writePrefixed<T>(std::string_view),
     *      encodeLengthPrefixBE<T>(std::size_t),
     *      enforceSendCapConnected(std::size_t)
     */
    template <typename T> void writePrefixed(const std::span<const std::byte> payload) const
    {
        sendPrefixedConnected<T>(payload);
    }

    /**
     * @brief Send a length-prefixed UDP datagram to (host, port) from text bytes (unconnected path).
     * @ingroup udp
     *
     * @tparam T  Unsigned integral type for the length prefix.
     *
     * @details
     * Builds **[ prefix(T, big-endian) | payload ]** and forwards to
     * `sendPrefixedUnconnected<T>()`, which resolves A/AAAA, skips families that cannot
     * carry the frame, and sends once to the first compatible destination. Zero-length
     * payloads produce a prefix-only datagram.
     *
     * @param[in] host     Destination hostname or numeric address (IPv4/IPv6).
     * @param[in] port     Destination UDP port (> 0).
     * @param[in] payload  Text bytes to append after the length prefix.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     *
     * @throws SocketException
     *   - **Logical (error=0):** socket not open; `payload.size()` exceeds `std::numeric_limits<T>::max()`;
     *     or no address family can carry the total size (surfaced by the helper).
     *   - **System (OS error + `SocketErrorMessage(...)`):** resolution/send failures.
     *
     * @since 1.0
     *
     * @see sendPrefixedUnconnected<T>(std::string_view, Port, std::span<const std::byte>),
     *      writePrefixedTo<T>(std::string_view, Port, std::span<const std::byte>),
     *      encodeLengthPrefixBE<T>(std::size_t),
     *      sendUnconnectedTo(std::string_view, Port, const void*, std::size_t)
     */
    template <typename T>
    void writePrefixedTo(const std::string_view host, const Port port, const std::string_view payload) const
    {
        sendPrefixedUnconnected<T>(host, port, asBytes(payload));
    }

    /**
     * @brief Send a length-prefixed UDP datagram to (host, port) from a byte span (unconnected path).
     * @ingroup udp
     *
     * @tparam T  Unsigned integral type for the length prefix.
     *
     * @details
     * Builds **[ prefix(T, big-endian) | payload ]** and forwards to
     * `sendPrefixedUnconnected<T>()`. The helper resolves and selects a compatible
     * destination (skipping families that can’t carry the frame) and sends once.
     *
     * @param[in] host     Destination hostname or numeric address (IPv4/IPv6).
     * @param[in] port     Destination UDP port (> 0).
     * @param[in] payload  Binary payload to append after the length prefix.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     *
     * @throws SocketException  (same categories as the text overload).
     *
     * @since 1.0
     *
     * @see sendPrefixedUnconnected<T>(std::string_view, Port, std::span<const std::byte>),
     *      writePrefixedTo<T>(std::string_view, Port, std::string_view),
     *      encodeLengthPrefixBE<T>(std::size_t),
     *      sendUnconnectedTo(std::string_view, Port, const void*, std::size_t)
     */
    template <typename T>
    void writePrefixedTo(const std::string_view host, const Port port, const std::span<const std::byte> payload) const
    {
        sendPrefixedUnconnected<T>(host, port, payload);
    }

    /**
     * @brief Send one UDP datagram to the connected peer by concatenating multiple fragments (no pre-wait).
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram whose payload is the concatenation of all strings in @p buffers.
     * This method performs an **immediate** send attempt (it does not wait for writability):
     *
     * 1. Verifies the socket is **open** and **connected**.
     * 2. Computes the total byte count `sum(buffers[i].size())`. If the sum is zero, returns immediately
     *    (zero-length UDP datagrams are valid, but this implementation skips the syscall).
     * 3. Enforces protocol-level maxima for the connected peer’s family via
     *    `enforceSendCapConnected(total)` to preempt guaranteed `EMSGSIZE`.
     * 4. Sends in a **single** syscall:
     *    - Fast path: if there is exactly one fragment, it is sent directly.
     *    - Otherwise, the fragments are coalesced into a temporary contiguous buffer and sent once.
     *
     * This function does **not** pre-wait for writability. On non-blocking sockets, if the send
     * buffer is full, the send may fail (e.g., `EWOULDBLOCK` / `WSAEWOULDBLOCK`) and a
     * `SocketException` is thrown. Use `writevAll()` or `writeWithTimeout()` for blocking behavior.
     *
     * **Atomicity:** UDP is message-oriented; on success the *entire* concatenated payload is sent
     * as a single datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] buffers  Sequence of string fragments that will be concatenated in-order into the
     *                     datagram payload. Individual elements may be empty.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @post On success, exactly the sum of all fragment sizes is handed to the kernel as one datagram.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):** socket not open/connected; total payload exceeds the
     *     permitted maximum for the connected peer’s family (detected by `enforceSendCapConnected()`).
     *   - **System (OS error + `SocketErrorMessage(...)`):** send failures such as
     *     `EWOULDBLOCK`, `ENOBUFS`, `ENETUNREACH`, `EHOSTUNREACH`, etc.
     *
     * @note If you later add a scatter/gather `internal::sendExactv(...)`, this method can switch
     *       to a vectored send to avoid the temporary copy in the multi-fragment case.
     *
     * @since 1.0
     *
     * @see writevAll(std::span<const std::string_view>), write(std::string_view),
     *      writeAll(std::string_view), writeWithTimeout(std::string_view, int),
     *      enforceSendCapConnected(std::size_t)
     *
     * @code
     * // Example: send header + body as one UDP datagram
     * std::string_view header = "HDR:";
     * std::string_view body   = "payload";
     * sock.writev({header, body});  // immediate attempt; may throw on EWOULDBLOCK
     * @endcode
     */
    void writev(std::span<const std::string_view> buffers) const;

    /**
     * @brief Send one UDP datagram to the connected peer by concatenating multiple fragments, waiting indefinitely.
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram whose payload is the concatenation of all strings in @p buffers.
     * Unlike `writev()`, this method **pre-waits** for writability so it behaves consistently on
     * both blocking and non-blocking sockets:
     *
     * 1. Verifies the socket is **open** and **connected**.
     * 2. Computes the total byte count `sum(buffers[i].size())`. If the sum is zero, returns immediately
     *    (zero-length UDP datagrams are valid, but this implementation skips the syscall).
     * 3. Enforces protocol-level maxima for the connected peer’s family via
     *    `enforceSendCapConnected(total)` to preempt guaranteed `EMSGSIZE`.
     * 4. Calls `waitReady(Direction::Write, -1)` to wait **without timeout** until the socket is writable
     *    (poll errors are surfaced as `SocketException`).
     * 5. Sends in a **single** syscall:
     *    - Fast path: if there is exactly one fragment, it is sent directly.
     *    - Otherwise, the fragments are coalesced into a temporary contiguous buffer and sent once.
     *
     * **Atomicity:** UDP is message-oriented; on success, the *entire* concatenated payload is sent as a
     * single datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] buffers  Sequence of string fragments concatenated in-order into the datagram payload.
     *                     Individual elements may be empty.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @post On success, exactly the sum of all fragment sizes is handed to the kernel as one datagram.
     *       Socket state and options are unchanged.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):** socket not open/connected; total payload exceeds the permitted
     *     maximum for the connected peer’s family (detected by `enforceSendCapConnected()`).
     *   - **System (OS error + `SocketErrorMessage(...)`):** polling or send failures
     *     (e.g., `POLLERR`, `POLLNVAL`, `POLLHUP`, `ENOBUFS`, `ENETUNREACH`, `EHOSTUNREACH`, etc.).
     *
     * @note This method never throws `SocketTimeoutException` because it waits indefinitely. For a bounded
     *       wait, use `writeWithTimeout()`.
     * @note If you later add a scatter/gather `internal::sendExactv(...)`, this method can switch to a
     *       vectored send to avoid the temporary copy in the multi-fragment case.
     *
     * @since 1.0
     *
     * @see writev(std::span<const std::string_view>), write(std::string_view), writeAll(std::string_view),
     *      writeWithTimeout(std::string_view, int), enforceSendCapConnected(std::size_t), waitReady(Direction, int)
     *
     * @code
     * // Example: send header + separator + body as one UDP datagram, blocking until writable
     * std::string_view hdr = "HDR";
     * std::string_view sep = ":";
     * std::string_view body = "payload";
     * sock.writevAll({hdr, sep, body});
     * @endcode
     */
    void writevAll(std::span<const std::string_view> buffers) const;

    /**
     * @brief Send one UDP datagram to the connected peer from a raw memory buffer (no pre-wait).
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram containing the first @p len bytes starting at @p data.
     * This method performs an **immediate** send attempt (it does not poll for writability):
     *
     * 1. Verifies the socket is **open** and **connected**.
     * 2. If @p len is zero, returns immediately (zero-length UDP datagrams are valid,
     *    but this implementation skips the syscall as a micro-optimization).
     * 3. Enforces protocol-level size limits for the connected peer’s family via
     *    `enforceSendCapConnected(len)` to preempt guaranteed `EMSGSIZE`.
     * 4. Transmits the datagram in a **single** system call (no partial sends).
     *
     * This function does **not** pre-wait for writability. On non-blocking sockets, if the send
     * buffer is temporarily full, the underlying send may fail (e.g., `EWOULDBLOCK` / `WSAEWOULDBLOCK`)
     * and a `SocketException` is thrown. For blocking behavior, use `writeAll()` (infinite wait)
     * or `writeWithTimeout()` (bounded wait).
     *
     * **Ownership/Lifetime:** The memory referenced by @p data is not copied or retained beyond
     * the duration of the call; it must remain valid and readable until the call returns.
     *
     * **Atomicity:** UDP is message-oriented; on success, the *entire* buffer is sent as one
     * datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] data  Pointer to the first byte of the payload. May be `nullptr` iff @p len == 0.
     * @param[in] len   Number of bytes to send as a single datagram.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     * @pre `data != nullptr || len == 0`
     *
     * @post On success, exactly @p len bytes are handed to the kernel as one datagram. Socket state
     *       and options are unchanged.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):**
     *     - Socket is not open or not connected.
     *     - Payload exceeds the permitted maximum for the connected peer’s family
     *       (detected by `enforceSendCapConnected()`).
     *   - **System (OS error + `SocketErrorMessage(...)`):**
     *     - Transient non-blocking condition (`EWOULDBLOCK`, `WSAEWOULDBLOCK`).
     *     - Resource/network issues (`ENOBUFS`, `ENETUNREACH`, `EHOSTUNREACH`, etc.).
     *
     * @note If your payload is already in `std::byte` form, prefer `write(std::span<const std::byte>)`.
     *       For text, prefer `write(std::string_view)`. For POD objects, consider `write<T>(const T&)`.
     *
     * @since 1.0
     *
     * @see write(std::string_view), write(std::span<const std::byte>),
     *      writeAll(std::string_view), writeWithTimeout(std::string_view, int),
     *      writev(std::span<const std::string_view>), enforceSendCapConnected(std::size_t)
     *
     * @code
     * // Example: send a raw buffer
     * std::array<std::uint8_t, 4> buf{0xDE, 0xAD, 0xBE, 0xEF};
     * sock.writeFrom(buf.data(), buf.size());
     *
     * // From a std::vector<char>
     * std::vector<char> payload = get_payload();
     * sock.writeFrom(payload.data(), payload.size());
     * @endcode
     */
    void writeFrom(const void* data, std::size_t len) const;

    /**
     * @brief Send one UDP datagram to the connected peer, waiting up to @p timeoutMillis for writability.
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram containing @p data. This method **bounds** the wait for socket
     * writability, then performs a single send:
     *
     * 1. Verifies the socket is **open** and **connected**.
     * 2. If the payload is empty, returns immediately (zero-length UDP datagrams are valid, but this
     *    implementation skips the syscall as a micro-optimization).
     * 3. Enforces protocol-level size limits for the connected peer’s address family via
     *    `enforceSendCapConnected(data.size())` to preempt guaranteed `EMSGSIZE`.
     * 4. Calls `waitReady(Direction::Write, timeoutMillis)`; if the socket does not become writable
     *    before the timeout, a `SocketTimeoutException` is thrown.
     * 5. On readiness, transmits the datagram in a **single** system call (no partial sends).
     *
     * **Timeout semantics** (delegated to `waitReady()`):
     * - `timeoutMillis > 0`  — wait up to the specified milliseconds.
     * - `timeoutMillis == 0` — non-blocking poll (immediate).
     * - `timeoutMillis < 0`  — wait indefinitely (equivalent to `writeAll()` behavior).
     *
     * **Atomicity:** UDP is message-oriented; on success, the *entire* payload is sent as one
     * datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] data           Bytes to send as a single datagram (accepts `std::string`,
     *                           string literals, etc., via implicit conversion to `std::string_view`).
     * @param[in] timeoutMillis  See timeout semantics above.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @post On success, exactly `data.size()` bytes are handed to the kernel as one datagram.
     *       Socket state and options are unchanged.
     *
     * @throws SocketTimeoutException
     *   If the socket does not become writable within @p timeoutMillis (when it is >= 0).
     *
     * @throws SocketException
     *   - **Logical (error code = 0):** socket not open/connected; payload exceeds the permitted
     *     maximum for the connected peer’s family (detected by `enforceSendCapConnected()`).
     *   - **System (OS error + `SocketErrorMessage(...)`):** polling or send failures
     *     (e.g., `POLLERR`, `POLLNVAL`, `POLLHUP`, `EWOULDBLOCK`, `ENOBUFS`, `ENETUNREACH`, etc.).
     *
     * @note Prefer `writeAll()` when you intentionally want an infinite wait; pass a negative
     *       @p timeoutMillis to mimic that behavior if you must stick to a single API.
     * @note For an immediate, no-wait attempt, use `write()` instead.
     *
     * @since 1.0
     *
     * @see write(std::string_view), writeAll(std::string_view), writevAll(std::span<const std::string_view>),
     *      enforceSendCapConnected(std::size_t), waitReady(Direction, int)
     *
     * @code
     * // Example: send with a 500 ms bound
     * sock.writeWithTimeout("payload", 500);   // throws SocketTimeoutException on timeout
     *
     * // Immediate attempt (non-blocking poll)
     * sock.writeWithTimeout("ping", 0);
     *
     * // Equivalent to writeAll() (infinite wait)
     * sock.writeWithTimeout("large message", -1);
     * @endcode
     */
    void writeWithTimeout(std::string_view data, int timeoutMillis) const;

    /**
     * @brief Send one UDP datagram using a packet’s buffer and optional explicit destination.
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram whose payload is `packet.buffer`. Behavior depends on whether
     * the packet specifies a destination:
     *
     * - **Packet has a destination (`packet.hasDestination()` is true):**
     *   Uses the **unconnected** send path via `sendUnconnectedTo(packet.address, packet.port, ...)`.
     *   That helper resolves A/AAAA, **skips** families that cannot carry the payload size, attempts
     *   one send to the first compatible candidate, and caches the last destination (without marking
     *   the socket as connected).
     *
     * - **Packet has no destination:**
     *   Requires the socket to be **already connected**. The method enforces the connected peer’s
     *   protocol maxima via `enforceSendCapConnected(packet.buffer.size())` and sends once.
     *
     * Processing steps:
     *  1) Verify the socket is **open**.
     *  2) If `packet.buffer.empty()`, return immediately (zero-length datagrams are valid but skipped).
     *  3) If the packet carries a destination, forward to `sendUnconnectedTo(...)`.
     *  4) Otherwise, require `isConnected() == true`, guard size with `enforceSendCapConnected(...)`, and send.
     *
     * **Atomicity:** UDP is message-oriented; on success, the *entire* `packet.buffer` is sent as one
     * datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] packet
     *   The packet whose `buffer` supplies the payload and whose `address`/`port` (if present)
     *   select the destination for unconnected sends.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre If `!packet.hasDestination()`, then `isConnected() == true`
     *
     * @post On success, exactly `packet.buffer.size()` bytes are handed to the kernel as one datagram.
     *       Socket state and options are unchanged. Unconnected sends may update the cached last-remote
     *       endpoint for diagnostics, but do not connect the socket.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):**
     *     - Socket is not open.
     *     - No destination in the packet **and** socket is not connected.
     *     - For connected sends, payload exceeds the permitted maximum for the peer’s family
     *       (detected by `enforceSendCapConnected()`).
     *     - For unconnected sends, no address family can carry the payload size (surfaced by `sendUnconnectedTo()`).
     *   - **System (OS error + `SocketErrorMessage(...)`):**
     *     - Resolution or send failures reported by the OS (e.g., `ENETUNREACH`, `EHOSTUNREACH`, `ENOBUFS`,
     *       `EWOULDBLOCK` / `WSAEWOULDBLOCK` on non-blocking sockets), or poll-derived errors inside helpers.
     *
     * @note Zero-length payloads: this method returns without sending. If your protocol needs to signal
     *       an empty frame, consider a prefix/marker instead of an empty datagram.
     *
     * @since 1.0
     *
     * @see write(std::string_view), writeFrom(const void*, std::size_t),
     *      writeTo(std::string_view, Port, std::string_view),
     *      sendUnconnectedTo(std::string_view, Port, const void*, std::size_t),
     *      enforceSendCapConnected(std::size_t)
     */
    void write(const DatagramPacket& packet) const;

    /**
     * @brief Send one unconnected UDP datagram to (host, port) from text bytes (no pre-wait).
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram containing @p message to the specified destination. This
     * overload uses the unconnected send path and delegates the heavy lifting to
     * `sendUnconnectedTo()`, which:
     *
     *  - Resolves A/AAAA candidates for @p host.
     *  - **Skips** any address family whose theoretical UDP maximum cannot carry the payload
     *    size (IPv4 vs IPv6 caps).
     *  - Attempts a single send to the first compatible candidate, returning on success.
     *  - Caches the last destination endpoint for diagnostics (does **not** mark the socket
     *    as connected).
     *
     * Steps performed by this method:
     *  1) Verify the socket is **open**.
     *  2) If `message.empty()`, return immediately (zero-length UDP datagrams are valid, but
     *     this implementation skips the syscall as a micro-optimization).
     *  3) Forward to `sendUnconnectedTo(host, port, message.data(), message.size())`.
     *
     * This function does **not** poll for writability. On non-blocking sockets, if an attempt
     * is made and the send buffer is full, the underlying send may fail
     * (`EWOULDBLOCK` / `WSAEWOULDBLOCK`) and will be surfaced as a `SocketException`.
     * For blocking semantics on a *connected* socket, prefer `writeAll()` or
     * `writeWithTimeout()`.
     *
     * **Atomicity:** UDP is message-oriented; on success, the *entire* payload is sent as one
     * datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] host     Destination hostname or numeric address (IPv4/IPv6).
     * @param[in] port     Destination UDP port (> 0).
     * @param[in] message  Bytes to send as a single datagram (accepts `std::string`,
     *                     string literals, etc., via implicit conversion to `std::string_view`).
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     *
     * @post On success, exactly `message.size()` bytes are handed to the kernel as one datagram.
     *       Socket state and options are unchanged. The last-remote endpoint may be cached
     *       internally for diagnostics.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):**
     *     - Socket is not open.
     *     - No address family can carry the datagram size (e.g., payload exceeds IPv4 limit and
     *       only A records are available; surfaced by `sendUnconnectedTo()` with a clear message).
     *   - **System (OS error + `SocketErrorMessage(...)`):**
     *     - Resolution or send failures reported by the OS (e.g., `EAI_*`, `ENETUNREACH`,
     *       `EHOSTUNREACH`, `ENOBUFS`, `EWOULDBLOCK`).
     *
     * @note Family-specific size enforcement (IPv4 vs IPv6) and destination selection are handled
     *       inside `sendUnconnectedTo()`. This method performs no pre-wait; if you need bounded or
     *       indefinite waiting, connect the socket and use `writeWithTimeout()` or `writeAll()`.
     *
     * @since 1.0
     *
     * @see write(std::string_view), writeAll(std::string_view), writeWithTimeout(std::string_view, int),
     *      writePrefixedTo<T>(std::string_view, Port, std::string_view),
     *      sendUnconnectedTo(std::string_view, Port, const void*, std::size_t)
     *
     * @code
     * // Example: send a message to a host/port without connecting the socket
     * sock.writeTo("example.com", 8125, "metric:1|c");
     *
     * // IPv6 numeric address is fine too:
     * sock.writeTo("2001:db8::1", 5000, "hello");
     * @endcode
     */
    void writeTo(std::string_view host, Port port, std::string_view message) const;

    /**
     * @brief Send one unconnected UDP datagram to (host, port) from a raw byte span (no pre-wait).
     * @ingroup udp
     *
     * @details
     * Emits exactly one datagram containing @p data to the specified destination. This overload
     * uses the unconnected send path and delegates to `sendUnconnectedTo()`, which:
     *
     *  - Resolves A/AAAA candidates for @p host.
     *  - Skips any address family whose theoretical UDP maximum cannot carry the payload size
     *    (IPv4 vs IPv6 caps).
     *  - Attempts a single send to the first compatible candidate, returning on success.
     *  - Caches the last destination endpoint for diagnostics (does not connect the socket).
     *
     * Steps performed by this method:
     *  1) Verify the socket is open.
     *  2) If `data.empty()`, return immediately (zero-length UDP datagrams are valid, but this
     *     implementation skips the syscall as a micro-optimization).
     *  3) Forward to `sendUnconnectedTo(host, port, data.data(), data.size())`.
     *
     * This function does not poll for writability. On non-blocking sockets, if an attempt is
     * made and the send buffer is full, the underlying send may fail (`EWOULDBLOCK` /
     * `WSAEWOULDBLOCK`) and will be surfaced as a `SocketException`. For blocking semantics on a
     * connected socket, prefer `writeAll()` or `writeWithTimeout()`.
     *
     * **Atomicity:** UDP is message-oriented; on success, the entire span is sent as one
     * datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] host  Destination hostname or numeric address (IPv4/IPv6).
     * @param[in] port  Destination UDP port (> 0).
     * @param[in] data  Contiguous read-only bytes to send as a single datagram.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     *
     * @post On success, exactly `data.size()` bytes are handed to the kernel as one datagram.
     *       Socket state and options are unchanged. The last-remote endpoint may be cached
     *       internally for diagnostics.
     *
     * @throws SocketException
     *   - Logical (error code = 0):
     *     - Socket is not open.
     *     - No address family can carry the datagram size (for example, payload exceeds IPv4
     *       limit and only A records are available; surfaced by `sendUnconnectedTo()` with a
     *       clear message).
     *   - System (OS error + `SocketErrorMessage(...)`):
     *     - Resolution or send failures reported by the OS (for example, `EAI_*`, `ENETUNREACH`,
     *       `EHOSTUNREACH`, `ENOBUFS`, `EWOULDBLOCK`).
     *
     * @note Family-specific size enforcement (IPv4 vs IPv6) and destination selection are handled
     *       inside `sendUnconnectedTo()`. This method performs no pre-wait; if you need bounded or
     *       indefinite waiting, connect the socket and use `writeWithTimeout()` or `writeAll()`.
     *
     * @since 1.0
     *
     * @see writeTo(std::string_view, Port, std::string_view),
     *      write(std::span<const std::byte>), writeAll(std::string_view),
     *      writeWithTimeout(std::string_view, int),
     *      sendUnconnectedTo(std::string_view, Port, const void*, std::size_t)
     *
     * @code
     * // Example: send binary payload to a destination without connecting the socket
     * std::array<std::byte, 4> magic{std::byte{0xDE}, std::byte{0xAD},
     *                                std::byte{0xBE}, std::byte{0xEF}};
     * sock.writeTo("239.0.0.1", 5000, std::span<const std::byte>{magic});
     *
     * // From a dynamic buffer
     * std::vector<std::byte> payload(3);
     * payload[0] = std::byte{0x01};
     * payload[1] = std::byte{0x02};
     * payload[2] = std::byte{0x03};
     * sock.writeTo("2001:db8::1", 6000, std::span<const std::byte>{payload.data(), payload.size()});
     * @endcode
     */
    void writeTo(std::string_view host, Port port, std::span<const std::byte> data) const;

    /**
     * @brief Send one unconnected UDP datagram to (host, port) containing the raw bytes of @p value.
     * @ingroup udp
     *
     * @tparam T
     *   Trivially copyable, standard-layout type whose in-memory object representation
     *   will be sent verbatim as the datagram payload. Compile-time checks in the
     *   implementation enforce these constraints.
     *
     * @details
     * Emits exactly one datagram whose payload is the raw bytes of @p value. This overload
     * uses the unconnected send path and delegates to `sendUnconnectedTo()`, which:
     *
     *  - Resolves A/AAAA candidates for @p host.
     *  - Skips any address family whose theoretical UDP maximum cannot carry `sizeof(T)`.
     *  - Attempts a single send to the first compatible candidate, returning on success.
     *  - Caches the last destination endpoint for diagnostics (does not connect the socket).
     *
     * Steps performed by this method:
     *  1) Verify the socket is open.
     *  2) If `sizeof(T) == 0`, return immediately.
     *  3) Bit-cast @p value to a contiguous byte buffer.
     *  4) Forward to `sendUnconnectedTo(host, port, buf, sizeof(T))`.
     *
     * This function does not poll for writability. On non-blocking sockets, if a send is
     * attempted and the send buffer is full, the underlying send may fail
     * (`EWOULDBLOCK` / `WSAEWOULDBLOCK`) and will be surfaced as a `SocketException`.
     * For blocking semantics on a connected socket, prefer `writeAll()` or `writeWithTimeout()`.
     *
     * Serialization and portability:
     *  - The bytes sent are the raw object representation of `T` (including any padding).
     *    Endianness, padding, and ABI differences mean this is typically suitable only for
     *    homogeneous peers or debugging. For interoperable protocols, use explicit
     *    serialization (fixed-width integers in network byte order, packed layouts, and so on).
     *  - If `T` contains padding, ensure it is initialized to avoid leaking stale memory.
     *
     * Atomicity:
     *  - UDP is message-oriented; on success, the entire `sizeof(T)` bytes are sent as one
     *    datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] host  Destination hostname or numeric address (IPv4/IPv6).
     * @param[in] port  Destination UDP port (> 0).
     * @param[in] value The value whose bytes form the datagram payload.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     *
     * @post On success, exactly `sizeof(T)` bytes are handed to the kernel as one datagram.
     *       Socket state and options are unchanged. The last-remote endpoint may be cached
     *       internally for diagnostics.
     *
     * @throws SocketException
     *   - Logical (error code = 0):
     *     - Socket is not open.
     *     - No address family can carry `sizeof(T)` (for example, payload exceeds IPv4 limit
     *       and only A records are available; surfaced by `sendUnconnectedTo()` with a clear message).
     *   - System (OS error + `SocketErrorMessage(...)`):
     *     - Resolution or send failures reported by the OS (for example, `EAI_*`, `ENETUNREACH`,
     *       `EHOSTUNREACH`, `ENOBUFS`, `EWOULDBLOCK`).
     *
     * @since 1.0
     *
     * @see writeTo(std::string_view, Port, std::string_view),
     *      write(std::string_view), writeFrom(const void*, std::size_t),
     *      sendUnconnectedTo(std::string_view, Port, const void*, std::size_t)
     *
     * @code
     * // Example: send a POD header to a destination without connecting the socket
     * struct Header {
     *   std::uint32_t magic;
     *   std::uint16_t version;
     *   std::uint16_t flags;
     * };
     * static_assert(std::is_trivially_copyable_v<Header> && std::is_standard_layout_v<Header>);
     *
     * Header h{0xABCD1234u, 1u, 0u};
     * sock.writeTo("example.com", 5000, h);
     * @endcode
     */
    template <typename T> void writeTo(const std::string_view host, const Port port, const T& value) const
    {
        static_assert(std::is_trivially_copyable_v<T>, "DatagramSocket::writeTo<T>() requires trivially copyable type");
        static_assert(std::is_standard_layout_v<T>, "DatagramSocket::writeTo<T>() requires standard layout type");

        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::writeTo<T>(): socket is not open.");

        if constexpr (sizeof(T) == 0)
            return;

        const auto buf = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
        sendUnconnectedTo(host, port, buf.data(), buf.size());
    }

    /**
     * @brief Read one UDP datagram into a DatagramPacket with optional growth/shrink and strict truncation policy.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Receives exactly one datagram and copies its payload into @p packet.buffer. Behavior is governed by @p opts:
     * - If a size preflight succeeds and @p opts.allowGrow is true, the buffer may be resized up front
     *   (clamped to MaxDatagramPayloadSafe) to fit the datagram exactly.
     * - If the buffer is smaller than the datagram and @p opts.errorOnTruncate is true, the call throws
     *   before reading when size is known; otherwise it throws after the read if truncation occurred.
     * - If @p opts.allowShrink is true and no truncation occurred, the buffer may be resized down to the
     *   exact number of bytes received.
     * - If @p opts.updateLastRemote is true, the internally tracked “last remote” is updated to the sender.
     * - If @p opts.resolveNumeric is true, @p packet.address and @p packet.port are filled with the sender’s
     *   numeric host and port.
     * Exactly one kernel receive is performed for the payload copy; all requests are clamped to MaxDatagramPayloadSafe.
     *
     * @par Implementation notes
     * Uses a size probe when available (internal::nextDatagramSize). When the size is known and
     * @p opts.errorOnTruncate is true, the method fails early without consuming the datagram. The actual copy
     * is performed with a single recv/recvmsg call via the internal backbone.
     *
     * @param[in,out] packet  Destination datagram container. On input, its buffer capacity limits the copy unless
     *                        @p opts.allowGrow enables pre-read resizing. On success, the first @c result.bytes
     *                        bytes contain the payload; address/port fields are updated when requested.
     * @param[in]     opts    Read options controlling preflight, capacity growth/shrink, truncation policy, sender
     *                        bookkeeping, and numeric address resolution.
     *
     * @return A DatagramReadResult with:
     *         - @c bytes        Number of bytes copied into @p packet.buffer.
     *         - @c datagramSize Full datagram size when known (0 if unknown on this platform/path).
     *         - @c truncated    True if the datagram did not fully fit the destination buffer.
     *
     * @throws SocketException
     *         If the socket is not open, arguments are invalid, OS-level receive errors occur (message via
     *         SocketErrorMessage), or truncation is disallowed by @p opts.errorOnTruncate.
     * @throws SocketTimeoutException
     *         If a configured receive timeout elapses before any datagram is received, or when the socket is
     *         non-blocking and no data is available.
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, @p packet.buffer holds the received payload (possibly resized per @p opts.allowShrink).
     *       When @p opts.updateLastRemote is true, the internal “last remote” reflects the sender.
     *
     * @note When size cannot be probed and @p opts.errorOnTruncate is false, oversized datagrams are truncated by
     *       the kernel and the tail is lost. Enable @p opts.errorOnTruncate for strict no-truncation behavior.
     *
     * @par Example: grow to exact size, reject truncation
     * @code{.cpp}
     * DatagramPacket pkt(0);
     * DatagramReadOptions ro{};
     * ro.allowGrow = true;
     * ro.errorOnTruncate = true;
     * auto r = sock.read(pkt, ro);
     * std::string_view payload(pkt.buffer.data(), r.bytes);
     * @endcode
     *
     * @par Example: fixed buffer, allow truncation
     * @code{.cpp}
     * DatagramPacket small(512);
     * DatagramReadOptions op{};
     * op.errorOnTruncate = false;
     * auto r2 = sock.read(small, op);
     * @endcode
     *
     * @see readInto(std::span<char>, const DatagramReadOptions&) for zero-allocation reads into caller buffers.
     * @see readAvailable() for returning the full datagram as a string (tries to avoid truncation).
     * @see readAtMost(std::span<char>, const DatagramReadOptions&) for single-recv best-effort reads without preflight.
     * @see DatagramReadOptions for all configurable policies.
     */
    DatagramReadResult read(DatagramPacket& packet, const DatagramReadOptions& opts) const;

    /**
     * @brief Read one UDP datagram into a caller-provided buffer with explicit truncation policy.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Performs exactly one receive to copy the next datagram into @p buffer, applying the policies in
     * @p opts:
     * - If `opts.mode != DatagramReceiveMode::NoPreflight` or `opts.errorOnTruncate == true`, the method
     *   attempts to probe the next datagram size first. When the size is known and the buffer is too small
     *   and `opts.errorOnTruncate == true`, it throws **before** reading so the datagram remains queued.
     * - If size cannot be probed (platform/path) or `opts.mode == NoPreflight`, a single receive is performed.
     *   If truncation occurs and `opts.errorOnTruncate == true`, it throws **after** the read (the tail is
     *   lost by UDP semantics). If `opts.errorOnTruncate == false`, the truncated bytes are returned.
     * - When `opts.updateLastRemote == true`, the internally tracked “last remote” endpoint is updated from
     *   the sender address of this datagram.
     * The request size is clamped to `MaxDatagramPayloadSafe`.
     *
     * @param[out] buffer  Caller-provided destination buffer receiving up to @p len bytes.
     * @param[in]  len     Capacity of @p buffer in bytes.
     * @param[in]  opts    Read options controlling preflight, truncation policy, and “last remote” updates.
     *
     * @return DatagramReadResult with:
     *         - `bytes` — number of bytes copied into @p buffer,
     *         - `datagramSize` — full datagram size when known (0 if unknown on this platform/path),
     *         - `truncated` — `true` if the datagram did not fully fit in @p buffer.
     *
     * @throws SocketException
     *         If the socket is not open, arguments are invalid, OS-level receive errors occur
     *         (message via `SocketErrorMessage`), or truncation is disallowed by `opts.errorOnTruncate`.
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or when the socket is
     *         non-blocking and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened. If @p buffer is non-null, @p len > 0.
     * @post On success, the first `result.bytes` bytes of @p buffer contain the payload. When
     *       `opts.updateLastRemote == true`, the internal “last remote” reflects the sender.
     *
     * @note Prefer the span overload for zero-allocation ergonomics:
     *       `readInto(std::span<char>, const DatagramReadOptions&)`.
     * @note For strict no-truncation behavior across platforms, set `opts.errorOnTruncate = true`
     *       and (optionally) `opts.mode = DatagramReceiveMode::PreflightSize`.
     *
     * @par Example
     * @code{.cpp}
     * std::array<char, 2048> buf{};
     * DatagramReadOptions ro{};
     * ro.errorOnTruncate = true;
     * auto res = sock.readInto(buf.data(), buf.size(), ro);
     * std::string_view payload(buf.data(), res.bytes);
     * @endcode
     *
     * @see readInto(std::span<char>, const DatagramReadOptions&) for the span overload.
     * @see read(DatagramPacket&, const DatagramReadOptions&) when you also need the sender address.
     * @see readAvailable() for returning the entire datagram as a string.
     * @see readAtMost(std::span<char>, const DatagramReadOptions&) for best-effort single-recv reads.
     */
    [[nodiscard]] DatagramReadResult readInto(void* buffer, std::size_t len,
                                              const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Read one UDP datagram into a caller-provided span (zero allocation) with explicit truncation policy.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Performs exactly one receive to copy the next datagram into @p out, applying the policies in @p opts:
     * - If `opts.mode != DatagramReceiveMode::NoPreflight` or `opts.errorOnTruncate == true`, the method attempts
     *   to probe the next datagram size first. When the size is known and the span is too small and
     *   `opts.errorOnTruncate == true`, it throws **before** reading so the datagram remains queued.
     * - If size cannot be probed (platform/path) or `opts.mode == NoPreflight`, a single receive is performed.
     *   If truncation occurs and `opts.errorOnTruncate == true`, it throws **after** the read (the tail is lost
     *   by UDP semantics). If `opts.errorOnTruncate == false`, the truncated bytes are returned.
     * - When `opts.updateLastRemote == true`, the internally tracked “last remote” endpoint is updated.
     * The request size is clamped to `MaxDatagramPayloadSafe`. If `out.size() == 0`, the function returns immediately.
     *
     * @param[out] out   Destination span receiving up to `out.size()` bytes.
     * @param[in]  opts  Read options controlling preflight, truncation policy, and “last remote” updates.
     *
     * @return DatagramReadResult with:
     *         - `bytes` — number of bytes copied into @p out,
     *         - `datagramSize` — full datagram size when known (0 if unknown on this platform/path),
     *         - `truncated` — `true` if the datagram did not fully fit in @p out.
     *
     * @throws SocketException
     *         If the socket is not open, OS-level receive errors occur (message via `SocketErrorMessage`),
     *         or truncation is disallowed by `opts.errorOnTruncate`.
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or when the socket is non-blocking
     *         and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, the first `result.bytes` elements of @p out contain the payload. When
     *       `opts.updateLastRemote == true`, the internal “last remote” reflects the sender.
     *
     * @note Prefer this overload for **zero-allocation** reads. For pointer-based interop with legacy APIs,
     *       use `readInto(void*, std::size_t, const DatagramReadOptions&)`.
     * @note For strict no-truncation behavior across platforms, set `opts.errorOnTruncate = true`
     *       and (optionally) `opts.mode = DatagramReceiveMode::PreflightSize`.
     *
     * @par Example
     * @code{.cpp}
     * std::array<char, 2048> buf{};
     * DatagramReadOptions ro{};
     * ro.errorOnTruncate = true;
     * auto res = sock.readInto(std::span<char>(buf.data(), buf.size()), ro);
     * std::string_view payload(buf.data(), res.bytes);
     * @endcode
     *
     * @see readInto(void*, std::size_t, const DatagramReadOptions&) for the pointer overload.
     * @see read(DatagramPacket&, const DatagramReadOptions&) when you also need the sender address.
     * @see readAvailable() for returning the entire datagram as a string.
     * @see readAtMost(std::span<char>, const DatagramReadOptions&) for best-effort single-recv reads.
     */
    [[nodiscard]] DatagramReadResult readInto(std::span<char> out, const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Read one UDP datagram into a dynamically resizable, contiguous byte container (zero-copy into caller
     * storage).
     * @ingroup udp
     * @since 1.0
     *
     * @tparam T  A contiguous, dynamically resizable byte-like container (e.g., std::string, std::vector<char>,
     *            std::vector<std::byte>, std::pmr::string) that provides `data()`, `size()`, and `resize()`.
     *
     * @details
     * Allocates a buffer in @p T, receives exactly one datagram into it, and then shrinks the container to the number
     * of bytes actually received. Capacity is chosen as follows:
     * - Start from @p minCapacity, clamped to `MaxDatagramPayloadSafe`. If @p minCapacity is 0, it is treated as 1
     *   to allow clean reception of zero-length datagrams.
     * - If size preflight is requested (see `opts.mode`) or strict no-truncation is desired (`opts.errorOnTruncate ==
     * true`), the next datagram size is probed. On success, the container is grown to the **exact** datagram size
     * (clamped to `MaxDatagramPayloadSafe`) to avoid truncation and extra copies.
     *
     * The actual copy is performed with a single kernel receive via `readInto(...)`. If truncation occurs and
     * `opts.errorOnTruncate == true`, `readInto(...)` will throw according to its policy (either before the read,
     * when size is known, or after the read otherwise). On success, the container is shrunk to the number of bytes
     * received. No null terminator is appended automatically.
     *
     * @param[in]  opts         Read options (preflight mode, truncation policy, sender bookkeeping).
     * @param[in]  minCapacity  Minimum initial capacity to allocate before reading. Clamped to
     * `MaxDatagramPayloadSafe`.
     *
     * @return The container @p T holding the received payload (size equals the number of bytes received).
     *
     * @throws SocketException
     *         If the socket is not open, OS-level errors occur (message via `SocketErrorMessage`),
     *         or truncation is disallowed by `opts.errorOnTruncate` and would occur.
     * @throws SocketTimeoutException
     *         If a configured receive timeout elapses before any datagram is received, or when the socket is
     *         non-blocking and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, the returned container’s `size()` equals the number of bytes received; its content is the
     * payload.
     *
     * @note Prefer enabling strict no-truncation (`opts.errorOnTruncate = true`) together with size preflight
     *       (`opts.mode = DatagramReceiveMode::PreflightSize`) when available, so overlarge datagrams are rejected
     *       **before** data is consumed.
     *
     * @par Example
     * @code{.cpp}
     * DatagramReadOptions ro{};
     * ro.errorOnTruncate = true;
     * ro.mode = DatagramReceiveMode::PreflightSize;
     * std::string payload = sock.read<std::string>(ro); // exact-size allocation when preflight succeeds
     * @endcode
     */
    template <typename T, std::enable_if_t<detail::is_dynamic_buffer_v<T>, int> = 0>
    [[nodiscard]] T read(const DatagramReadOptions& opts = {},
                         std::size_t minCapacity = DefaultDatagramReceiveSize) const
    {
        using Ptr = decltype(std::declval<T&>().data());
        using Elem = std::remove_pointer_t<Ptr>;
        static_assert(std::is_same_v<Elem, char> || std::is_same_v<Elem, unsigned char> ||
                          std::is_same_v<Elem, std::byte>,
                      "T must be a contiguous byte-like container of char, unsigned char, or std::byte");

        if (minCapacity == 0)
            minCapacity = 1; // allow clean reception of zero-length datagrams

        std::size_t capacity = (std::min) (minCapacity, MaxDatagramPayloadSafe);

        // Prefer an exact allocation when we can learn the size (either explicitly requested,
        // or because strict no-truncation is desired).
        std::size_t probed = 0;
        const bool wantPreflight = (opts.mode != DatagramReceiveMode::NoPreflight) || opts.errorOnTruncate;

        if (wantPreflight)
        {
            try
            {
                if (const std::size_t exact = internal::nextDatagramSize(getSocketFd()); exact > 0)
                {
                    probed = (std::min) (exact, MaxDatagramPayloadSafe);
                    capacity = (std::max) (capacity, probed); // grow to fit the datagram exactly
                }
            }
            catch (const SocketException&)
            {
                // graceful degradation: keep current capacity
            }
        }

        T out;
        out.resize(capacity);

        DatagramReadOptions local = opts;
        if (probed > 0)
            local.mode = DatagramReceiveMode::NoPreflight; // avoid redundant preflight on the actual recv

        // Use the policy-aware backbone; it will enforce errorOnTruncate as documented.
        if (auto res = readInto(reinterpret_cast<void*>(out.data()), out.size(), local); res.bytes < out.size())
            out.resize(res.bytes); // shrink to actual bytes received (no null terminator implied)

        return out;
    }

    /**
     * @brief Read one UDP datagram into a fixed-size, contiguous byte container (zero allocation).
     * @ingroup udp
     * @since 1.0
     *
     * @tparam T  A fixed-size, contiguous byte-like container (e.g., @c std::array<char, N>,
     *            @c std::array<unsigned char, N>, @c std::array<std::byte, N>) exposing
     *            @c data() and @c size().
     *
     * @details
     * Performs exactly one receive and copies up to @c T::size() bytes into the returned container.
     * Trailing bytes remain value-initialized (typically zero) if the datagram is shorter.
     * Truncation behavior is controlled by @p opts.errorOnTruncate:
     * - If a reliable size probe is available and the datagram would not fit while @p opts.errorOnTruncate is @c true,
     *   the function throws **before** reading so the datagram stays queued.
     * - Otherwise, if truncation occurs and @p opts.errorOnTruncate is @c true, it throws **after** the read.
     * - If @p opts.errorOnTruncate is @c false, the payload is truncated to @c T::size() without error.
     * When @p opts.updateLastRemote is @c true, the internally tracked “last remote” is updated.
     * The requested copy length is always clamped to @c MaxDatagramPayloadSafe.
     *
     * @param[in] opts  Read options controlling preflight, truncation policy, and sender bookkeeping.
     *
     * @return The fixed-size container @p T containing the received bytes in its leading elements. Any unused
     *         tail remains value-initialized.
     *
     * @throws SocketException
     *         If the socket is not open, on OS-level errors (message via @c SocketErrorMessage),
     *         or when truncation is disallowed by @p opts.errorOnTruncate.
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or when the socket is non-blocking
     *         and no data is available.
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, the first @c n bytes of the returned container hold the payload, where
     *       @c n ≤ @c T::size(). If @p opts.updateLastRemote is @c true, the internal “last remote”
     *       reflects the sender.
     *
     * @note Prefer @c opts.errorOnTruncate = true with @c DatagramReceiveMode::PreflightSize for strict
     *       no-truncation behavior when supported by the platform.
     *
     * @par Example
     * @code{.cpp}
     * std::array<char, 1500> buf = sock.read<std::array<char,1500>>({
     *     .errorOnTruncate = true,
     *     .mode = DatagramReceiveMode::PreflightSize
     * });
     * @endcode
     */
    template <typename T, std::enable_if_t<detail::is_fixed_buffer_v<T>, int> = 0>
    [[nodiscard]] T read(const DatagramReadOptions& opts = {}) const
    {
        using Ptr = decltype(std::declval<T&>().data());
        using Elem = std::remove_pointer_t<Ptr>;
        static_assert(std::is_same_v<Elem, char> || std::is_same_v<Elem, unsigned char> ||
                          std::is_same_v<Elem, std::byte>,
                      "T must be a contiguous byte-like container of char, unsigned char, or std::byte");
        static_assert(sizeof(Elem) == 1, "T element type must be 1 byte wide");

        T out{}; // value-init so any unused tail is predictable (typically zero)
        if (out.size() == 0)
            return out;

        // Zero-allocation, policy-aware read
        (void) readInto(std::span<char>(reinterpret_cast<char*>(out.data()), out.size()), opts);
        return out;
    }

    /**
     * @brief Read one UDP datagram into a caller-provided byte container and optionally return the sender address/port.
     * @ingroup udp
     * @since 1.0
     *
     * @tparam T  A contiguous byte container with `data()` and `size()`. Supports both
     *            dynamic (e.g., `std::string`, `std::vector<char>`) and fixed-size
     *            (e.g., `std::array<char, N>`) containers.
     *
     * @details
     * Receives exactly one datagram and copies it into @p buffer. If @p T is dynamically
     * resizable and a reliable size probe is available, the function **grows** @p buffer
     * up front to the exact datagram size (clamped to `MaxDatagramPayloadSafe`) to avoid
     * truncation and extra copies. Truncation behavior follows @p opts.errorOnTruncate:
     * - If a size probe succeeds and the datagram would not fit:
     *   - Dynamic container: the buffer is grown automatically (up to the safety cap).
     *   - Fixed container: if `opts.errorOnTruncate == true`, the function throws **before**
     *     reading (datagram remains queued); otherwise it will be truncated.
     * - If no size probe is available, a single receive is performed. If truncation occurs
     *   and `opts.errorOnTruncate == true`, the function throws **after** the read.
     *
     * When @p senderAddr and/or @p senderPort are non-null, the function resolves the
     * numeric host and port of the datagram’s sender and writes them to the provided outputs.
     * This works for both connected and unconnected sockets. If you only need the sender
     * information without copying the payload into a `DatagramPacket`, this overload is
     * convenient and zero-alloc for fixed buffers.
     *
     * @param[in,out] buffer       Caller-provided destination container. For dynamic containers,
     *                             it may be resized to fit the datagram. For fixed containers,
     *                             its capacity limits the copy.
     * @param[out]    senderAddr   Optional. Receives the sender’s numeric IP string (e.g., "203.0.113.10").
     * @param[out]    senderPort   Optional. Receives the sender’s port.
     * @param[in]     opts         Read options controlling preflight, truncation policy, and bookkeeping.
     *
     * @return DatagramReadResult with:
     *         - `bytes` — number of bytes copied into @p buffer,
     *         - `datagramSize` — full datagram size when known (0 if unknown on this platform/path),
     *         - `truncated` — `true` if the datagram did not fully fit in @p buffer,
     *         - `src` / `srcLen` — sender socket address (when available in the build; included in this result type).
     *
     * @throws SocketException
     *         If the socket is not open, OS-level receive errors occur (message via `SocketErrorMessage`),
     *         or truncation is disallowed by `opts.errorOnTruncate` and would occur.
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or when the socket is non-blocking
     *         and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened.
     * @pre  For fixed-size containers, `T::size() > 0`.
     * @post On success, the first `result.bytes` elements of @p buffer contain the payload. If @p senderAddr and/or
     *       @p senderPort are provided, they contain the numeric sender address and port.
     *
     * @note Requests are clamped to `MaxDatagramPayloadSafe`. For strict no-truncation behavior across platforms,
     *       prefer `opts.errorOnTruncate = true`; when available, a preflight size probe avoids consuming the datagram
     *       on failure.
     *
     * @par Example: dynamic buffer (auto-grow) and capture sender
     * @code{.cpp}
     * std::vector<char> buf;
     * std::string fromIp;
     * Port fromPort{};
     * DatagramReadOptions ro{};
     * ro.errorOnTruncate = true;
     * auto r = sock.readFrom(buf, &fromIp, &fromPort, ro);
     * @endcode
     *
     * @par Example: fixed buffer (truncate allowed)
     * @code{.cpp}
     * std::array<char, 1024> buf{};
     * auto r = sock.readFrom(buf, nullptr, nullptr, DatagramReadOptions{ .errorOnTruncate = false });
     * @endcode
     */
    template <typename T>
    [[nodiscard]] DatagramReadResult readFrom(T& buffer, std::string* senderAddr, Port* senderPort,
                                              const DatagramReadOptions& opts) const
    {
        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::readFrom(): socket is not open.");

        DatagramReadResult result{};

        // Determine current capacity and whether we can grow.
        constexpr bool isDynamic = detail::is_dynamic_buffer_v<T>;
        constexpr bool isFixed = detail::is_fixed_buffer_v<T>;
        static_assert(isDynamic || isFixed, "T must be a supported dynamic or fixed-size contiguous byte container");

        auto currSize = static_cast<std::size_t>(buffer.size());

        // For dynamic containers, ensure at least 1 byte to properly consume zero-length datagrams.
        if constexpr (isDynamic)
        {
            if (currSize == 0)
                currSize = (std::min) (DefaultDatagramReceiveSize, MaxDatagramPayloadSafe);
        }

        // Optional preflight: if we can learn the size, either grow (dynamic) or enforce policy (fixed).
        std::size_t probed = 0;
        const bool wantPreflight = (opts.mode != DatagramReceiveMode::NoPreflight) || opts.errorOnTruncate;

        if (wantPreflight)
        {
            try
            {
                const std::size_t exact = internal::nextDatagramSize(getSocketFd());
                if (exact > 0)
                {
                    probed = (std::min) (exact, MaxDatagramPayloadSafe);

                    if constexpr (isDynamic)
                    {
                        if (probed > currSize)
                            currSize = probed; // grow to fit the datagram exactly
                    }
                    else
                    { // fixed
                        if (opts.errorOnTruncate && probed > currSize)
                        {
                            // Early, non-destructive failure: datagram remains queued.
                            throw SocketException("DatagramSocket::readFrom(): fixed buffer too small for incoming "
                                                  "datagram (preflight).");
                        }
                    }
                }
            }
            catch (const SocketException&)
            {
                // graceful degradation; fall back to single-recv path
                probed = 0;
            }
        }

        // Clamp to safety cap.
        currSize = (std::min) (currSize, MaxDatagramPayloadSafe);

        // Resize dynamic container as needed.
        if constexpr (isDynamic)
            buffer.resize(currSize);

        // Perform exactly one receive using the low-level backbone to obtain sender info.
        sockaddr_storage src{};
        auto srcLen = static_cast<socklen_t>(sizeof(src));
        std::size_t datagramSize = 0;
        bool truncated = false;

        const DatagramReceiveMode effectiveMode = (probed > 0 ? DatagramReceiveMode::NoPreflight : opts.mode);

        const std::size_t n = readIntoBuffer(reinterpret_cast<char*>(buffer.data()), currSize, effectiveMode,
                                             opts.recvFlags, &src, &srcLen, &datagramSize, &truncated);

        // Enforce strict no-truncation after the read if we couldn't fail early.
        if (opts.errorOnTruncate && (truncated || (datagramSize > 0 && datagramSize > currSize)))
        {
            throw SocketException("DatagramSocket::readFrom(): datagram truncated into destination buffer.");
        }

        // Shrink dynamic container to the actual bytes received.
        if constexpr (isDynamic)
        {
            if (n < buffer.size())
                buffer.resize(n);
        }

        // Fill the result structure.
        result.bytes = n;
        result.datagramSize = (datagramSize > 0 ? datagramSize : n);
        result.truncated = truncated;
        result.src = src;
        result.srcLen = srcLen;

        // Optional: update last-remote if requested by options.
        if (opts.updateLastRemote)
            rememberRemote(src, srcLen);

        // Resolve numeric sender info if requested.
        if ((senderAddr || senderPort) && srcLen > 0)
        {
            std::string addrStr;
            Port portNum{};
            internal::resolveNumericHostPort(reinterpret_cast<const sockaddr*>(&src), srcLen, addrStr, portNum);
            if (senderAddr)
                *senderAddr = std::move(addrStr);
            if (senderPort)
                *senderPort = portNum;
        }

        return result;
    }

    /**
     * @brief Receive exactly @p exactLen bytes from a single UDP datagram into @p buffer, with strict policy control.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Attempts to ensure the caller ends up with exactly @p exactLen bytes in @p buffer from the **next datagram**:
     * - If a size probe succeeds and `opts.requireExact == true`, the call **throws before reading** unless the
     *   datagram size equals @p exactLen. If the datagram is smaller and `opts.padIfSmaller == true`, the read proceeds
     *   and the tail is zero-padded after copying.
     * - If a size probe is unavailable, a single receive is performed. If the datagram is larger than @p exactLen and
     *   `opts.requireExact == true`, the call throws **after** the read (the kernel truncated the payload). If the
     *   datagram is smaller than @p exactLen and `opts.padIfSmaller == true`, the tail is zero-padded; otherwise the
     *   call throws due to size mismatch.
     * - `opts.base.updateLastRemote` controls whether the internally tracked “last remote” is updated.
     *
     * All requests are clamped to `MaxDatagramPayloadSafe`. This function performs exactly one kernel receive for the
     * payload copy (no dummy read).
     *
     * @param[out] buffer       [out] Destination buffer receiving exactly @p exactLen bytes on success.
     * @param[in]  exactLen     [in]  Required logical length. Must be > 0.
     * @param[in]  opts         [in]  Exact-read policy (`requireExact`, `padIfSmaller`) and base receive options.
     *
     * @return DatagramReadResult with:
     *         - `bytes`        Number of bytes copied (equals min(datagramSize, exactLen)).
     *         - `datagramSize` Full datagram size when known (0 if unknown on this platform/path).
     *         - `truncated`    True if the datagram was larger than @p exactLen and the kernel truncated the payload.
     *         - `src`/`srcLen` Sender address information (when available).
     *
     * @throws SocketException
     *         If the socket is not open, arguments are invalid, OS errors occur (message via `SocketErrorMessage`),
     *         or the size policy fails (e.g., mismatch with `requireExact`, or undersize without `padIfSmaller`).
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or in non-blocking mode with no data.
     *
     * @pre  Socket is open; @p buffer is non-null; @p exactLen > 0.
     * @post On success, @p buffer holds exactly @p exactLen bytes (payload plus optional zero-padding).
     *
     * @note For strictly rejecting any truncation **before** reading, prefer `opts.base.mode =
     * DatagramReceiveMode::PreflightSize` (or leave `NoPreflight`; this method will still attempt to probe when
     * useful).
     */
    [[nodiscard]] DatagramReadResult readExact(void* buffer, std::size_t exactLen,
                                               const ReadExactOptions& opts = {}) const;

    /**
     * @brief Span overload of readExact(...) (derives @p exactLen from @p out.size()).
     * @ingroup udp
     * @since 1.0
     *
     * @param[out] out   [out] Destination span; on success, exactly `out.size()` bytes are written.
     * @param[in]  opts  [in]  Exact-read policy and base receive options.
     *
     * @return DatagramReadResult; see pointer overload.
     */
    [[nodiscard]] DatagramReadResult readExact(std::span<char> out, const ReadExactOptions& opts = {}) const;

    /**
     * @brief Receive exactly @p exactLen bytes from a single UDP datagram into a contiguous byte container.
     * @ingroup udp
     * @since 1.0
     *
     * @tparam T  A contiguous byte-like container with `data()` and `size()`.
     *            Supported: dynamic (e.g., std::string, std::vector<char/unsigned char/std::byte>)
     *            and fixed-size (e.g., std::array<char/unsigned char/std::byte, N>).
     *
     * @details
     * Enforces **exact-length** semantics on top of a single datagram receive, governed by @p opts:
     * - If a reliable size probe is available and `opts.requireExact == true`, the call throws **before** reading
     *   unless the datagram size equals @p exactLen. If the datagram is smaller and `opts.padIfSmaller == true`,
     *   the read proceeds and the tail is zero-padded after copying.
     * - If size cannot be probed, one receive is performed. If the datagram is larger than @p exactLen and
     *   `opts.requireExact == true`, the call throws **after** the read (the kernel truncated the payload).
     *   If the datagram is smaller than @p exactLen and `opts.padIfSmaller == true`, the tail is zero-padded;
     *   otherwise the call throws due to size mismatch.
     * - For dynamic containers:
     *   - When `opts.autoResizeDynamic == true`, the container is resized to @p exactLen up front
     *     (after validating `exactLen <= MaxDatagramPayloadSafe`).
     *   - When `opts.autoResizeDynamic == false`, the container must already have `size() >= exactLen`
     *     or the call throws.
     *   After the read, the container is shrunk or extended to exactly @p exactLen (new bytes are value-initialized).
     * - For fixed-size containers, `T::size()` must be `>= exactLen`; the size cannot change.
     *
     * Internally delegates to the pointer/span `readExact(...)` overload, which performs at most one kernel receive,
     * attempts early failure via a size probe, applies zero-padding when requested, updates “last remote” per options,
     * and returns a `DatagramReadResult`.
     *
     * @param[in,out] buffer   Destination container. Must be contiguous and byte-sized.
     * @param[in]     exactLen Required logical length (> 0).
     * @param[in]     opts     Exact-read policy (`requireExact`, `padIfSmaller`, `autoResizeDynamic`) and base options.
     *
     * @return DatagramReadResult:
     *         - `bytes`        Number of bytes copied from the datagram (≤ @p exactLen; equals @p exactLen when padded
     * or exact).
     *         - `datagramSize` Full datagram size when known (0 if unknown on this platform/path).
     *         - `truncated`    True if the datagram was larger than @p exactLen and the kernel truncated the payload.
     *         - `src`/`srcLen` Sender address (when available).
     *
     * @throws SocketException
     *         If the socket is not open; arguments are invalid; `exactLen` exceeds `MaxDatagramPayloadSafe`
     *         (when auto-resizing); the dynamic container is too small and auto-resize is disabled; OS receive
     *         errors occur (message via `SocketErrorMessage`); or size policy fails (e.g., `requireExact` mismatch).
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or in non-blocking mode with no data.
     *
     * @pre  Socket is open; @p exactLen > 0. For fixed-size containers, `T::size() >= exactLen`.
     * @post On success, @p buffer contains exactly @p exactLen bytes of payload (plus zero-padding when applicable).
     *
     * @note For strictly rejecting any truncation *before* reading, set `opts.base.mode =
     * DatagramReceiveMode::PreflightSize` (this function will still attempt to probe when useful).
     *
     * @par Example: dynamic buffer, auto-resize, strict exact
     * @code{.cpp}
     * std::vector<char> buf;
     * ReadExactOptions ex{};
     * ex.requireExact      = true;
     * ex.padIfSmaller      = false;
     * ex.autoResizeDynamic = true;
     * auto r = sock.readExact(buf, 1024, ex); // buf.size() becomes 1024 on success
     * @endcode
     *
     * @par Example: fixed buffer, allow padding
     * @code{.cpp}
     * std::array<char, 2048> buf{};
     * ReadExactOptions ex{};
     * ex.requireExact = true;
     * ex.padIfSmaller = true;
     * auto r = sock.readExact(buf, 1500, ex); // tail zero-padded up to 1500 bytes
     * @endcode
     */
    template <typename T>
    [[nodiscard]] DatagramReadResult readExact(T& buffer, const std::size_t exactLen,
                                               const ReadExactOptions& opts = {}) const
    {
        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::readExact(T&,size_t): socket is not open.");
        if (exactLen == 0)
            throw SocketException("DatagramSocket::readExact(T&,size_t): exactLen must be > 0.");

        // Validate container element type and contiguity at compile time
        using Ptr = decltype(std::declval<T&>().data());
        using Elem = std::remove_pointer_t<Ptr>;
        static_assert(sizeof(Elem) == 1, "T element type must be 1 byte wide");
        static_assert(std::is_same_v<Elem, char> || std::is_same_v<Elem, unsigned char> ||
                          std::is_same_v<Elem, std::byte>,
                      "T must be a contiguous byte-like container of char, unsigned char, or std::byte");

        DatagramReadResult res{};

        if constexpr (detail::is_dynamic_buffer_v<T>)
        {
            // Ensure capacity matches policy
            if (opts.autoResizeDynamic)
            {
                if (exactLen > MaxDatagramPayloadSafe)
                    throw SocketException(
                        "DatagramSocket::readExact(T&,size_t): exactLen exceeds MaxDatagramPayloadSafe.");
                buffer.resize(exactLen); // value-initializes any new bytes
            }
            else
            {
                if (buffer.size() < exactLen)
                    throw SocketException(
                        "DatagramSocket::readExact(T&,size_t): buffer too small and autoResizeDynamic=false.");
            }

            // Perform policy-aware exact read into the first exactLen bytes
            res = readExact(std::span(reinterpret_cast<char*>(buffer.data()), exactLen), opts);

            // Normalize container size to the logical exact length (keeps API guarantees)
            if (buffer.size() != exactLen)
                buffer.resize(exactLen); // any extension is value-initialized (zeros)
        }
        else if constexpr (detail::is_fixed_buffer_v<T>)
        {
            if (buffer.size() < exactLen)
                throw SocketException("DatagramSocket::readExact(T&,size_t): fixed buffer smaller than exactLen.");

            // Exact read directly into the fixed-size storage (only the first exactLen bytes are touched)
            res = readExact(std::span(reinterpret_cast<char*>(buffer.data()), exactLen), opts);
            // Size is fixed; if padding was requested, the span path already zero-filled the tail.
        }
        else
        {
            static_assert(detail::is_dynamic_buffer_v<T> || detail::is_fixed_buffer_v<T>,
                          "T must be a supported dynamic or fixed-size contiguous byte container");
        }

        return res;
    }

    /**
     * @brief Read up to @p out.size() bytes from the next UDP datagram into a caller-provided buffer (no allocation).
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Performs exactly one receive with **at-most** semantics:
     * - If the incoming datagram is smaller than @p out, only that many bytes are written.
     * - If the datagram is larger than @p out, only the first @p out.size() bytes are written and the remainder
     *   of the datagram is discarded by the kernel (standard UDP truncation).
     *
     * This API enforces a single `recv(from)` with no extra syscalls by using `DatagramReceiveMode::NoPreflight`
     * for the actual receive (any `opts.mode` setting is overridden for this method). On unconnected sockets,
     * when `opts.updateLastRemote == true`, the internally tracked “last remote” endpoint is updated to the sender.
     *
     * Blocking/timeout behavior:
     * - In blocking mode, the call waits until a datagram arrives or a configured receive timeout elapses.
     *   On timeout, `SocketTimeoutException` is thrown.
     * - In non-blocking mode with no data available, a `SocketTimeoutException` is also thrown (would-block mapped).
     *
     * @param[out] out   Destination span receiving at most `out.size()` bytes.
     * @param[in]  opts  Read options; fields other than `mode` are honored. The effective mode is forced to
     *                   `DatagramReceiveMode::NoPreflight` to preserve single-recv semantics.
     *
     * @return Number of bytes actually written to @p out (0 if @p out is empty).
     *
     * @throws SocketException
     *         If the socket is not open, on argument errors, or on other OS-level receive errors
     *         (message via `SocketErrorMessage`).
     * @throws SocketTimeoutException
     *         If a configured receive timeout elapses before any datagram is received, or when the socket
     *         is non-blocking and no data is available.
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, exactly the returned number of bytes have been written to @p out. For unconnected sockets,
     *       when `opts.updateLastRemote == true`, the internal “last remote” reflects the sender of this datagram.
     *
     * @note This function never allocates and never enlarges the caller’s buffer.
     * @warning If @p out is smaller than the datagram, the tail of that datagram is dropped (cannot be recovered).
     *
     * @par Example
     * @code{.cpp}
     * std::array<char, 1500> buf{};
     * DatagramReadOptions ro{};
     * const std::size_t n = sock.readAtMost(std::span<char>(buf.data(), buf.size()), ro);
     * std::string_view payload(buf.data(), n);
     * @endcode
     *
     * @see readAtMost(std::size_t) for a convenience overload that allocates and returns a string.
     * @see readAvailable(std::span<char>, const DatagramReadOptions&) when you need no-truncation semantics.
     * @see readInto(void*, std::size_t, const DatagramReadOptions&) for the lower-level primitive.
     */
    std::size_t readAtMost(std::span<char> out, const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Read up to @p n bytes from the next UDP datagram and return them as a newly allocated string.
     *
     * @details
     * Performs exactly one receive operation (single `recvfrom`/`recv`) with
     * **best-effort, at-most** semantics:
     * - If the incoming datagram is **smaller** than @p n, the returned string’s size equals the datagram size.
     * - If the datagram is **larger** than @p n, only the first @p n bytes are returned and the **remainder is
     * discarded** (standard UDP truncation semantics).
     *
     * This convenience overload enforces `DatagramReceiveMode::NoPreflight` (no size preflight, no extra syscalls).
     * It works for both connected and unconnected sockets. On unconnected sockets, the internally tracked “last remote”
     * endpoint is updated to the sender of the received datagram.
     *
     * Blocking/timeout behavior:
     * - If the socket is blocking and no datagram is available, the call blocks until one arrives or a configured
     *   receive timeout elapses.
     * - If a receive timeout applies and expires before any datagram is received, a `SocketTimeoutException` is thrown.
     * - If the socket is non-blocking and no data is available, behavior follows the lower-level `readInto(...)` policy
     *   (typically throwing on would-block).
     *
     * @param[in] n  Maximum number of bytes to read (and to appear in the returned string). If 0, returns an empty
     * string.
     *
     * @return The payload bytes read (size ≤ @p n). Marked `[[nodiscard]]` to help prevent accidental loss of data.
     *
     * @throws SocketException
     *         If the socket is not open, not readable, or on other OS-level errors.
     * @throws SocketTimeoutException
     *         If a configured receive timeout elapses before any datagram is received.
     *
     * @pre The socket has been successfully created/opened.
     * @post On success, the returned string contains the bytes received; for unconnected sockets, the internal
     *       “last remote” is updated to the sender of the received datagram.
     *
     * @note This overload allocates and may shrink the string after receive. For zero-allocation reads, prefer
     *       `readAtMost(std::span<char>, const DatagramReadOptions&)`.
     * @warning If @p n is smaller than the incoming datagram, the remainder of that datagram is dropped.
     *
     * @par Example
     * @code{.cpp}
     * // Read up to 1400 bytes from the next datagram
     * std::string payload = sock.readAtMost(1400);
     * // use payload...
     * @endcode
     *
     * @see readAtMost(std::span<char>, const DatagramReadOptions&) for a span-first, zero-allocation overload.
     * @see readInto(void*, std::size_t, const DatagramReadOptions&) for the lower-level primitive.
     * @see read(DatagramPacket&, const DatagramReadOptions&) when you also need the sender address.
     */
    [[nodiscard]] std::string readAtMost(std::size_t n) const;

    /**
     * @brief Receive the next UDP datagram and return its payload as a string, attempting to avoid truncation.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Tries to return the **entire** next datagram:
     * - First attempts a size probe (e.g., via an OS-specific query). If successful, it allocates exactly that
     *   many bytes (clamped to MaxDatagramPayloadSafe) and performs a single receive.
     * - If a reliable size probe is not available, it allocates a safe maximum buffer (MaxDatagramPayloadSafe)
     *   and performs exactly one receive. In this fallback, extremely large datagrams may be truncated.
     *
     * On unconnected sockets, the internally tracked “last remote” endpoint is updated to the sender.
     * This convenience overload forces a single-recv path and **does not throw on truncation** in the rare
     * fallback case (truncation is accepted and the received bytes are returned).
     *
     * Blocking/timeout behavior:
     * - In blocking mode, waits until a datagram arrives or a configured receive timeout elapses, then throws
     *   SocketTimeoutException.
     * - In non-blocking mode with no data available, also throws SocketTimeoutException (would-block mapped).
     *
     * @return The payload of the next datagram as a string. May be empty for a zero-length datagram.
     *
     * @throws SocketException
     *         On socket not open or other OS-level receive errors (message via SocketErrorMessage).
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is received, or when the socket is non-blocking
     *         and no data is available.
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, the returned string contains the bytes received from exactly one datagram; for unconnected
     *       sockets, the internal “last remote” reflects the sender of that datagram.
     *
     * @note For strict no-truncation behavior on all platforms, prefer the span overload
     *       readAvailable(std::span<char>, const DatagramReadOptions&) and set opts.errorOnTruncate = true.
     *
     * @warning On platforms without a reliable size probe, datagrams larger than MaxDatagramPayloadSafe will be
     *          truncated in this convenience API.
     *
     * @par Example
     * @code{.cpp}
     * [[nodiscard]] std::string payload = sock.readAvailable();
     * // use payload
     * @endcode
     *
     * @see readAvailable(std::span<char>, const DatagramReadOptions&) for zero-allocation reads with explicit policy.
     * @see readAtMost(std::size_t) for best-effort single-recv reads without size preflight.
     * @see read(DatagramPacket&, const DatagramReadOptions&) when you also need the sender address.
     */
    [[nodiscard]] std::string readAvailable() const;

    /**
     * @brief Receive the entire next UDP datagram into a caller-provided buffer, with explicit truncation policy.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Attempts to copy the **full datagram** into @p out in a single receive:
     * - When the platform can report the next datagram size up front, the method probes it and:
     *   - If `opts.errorOnTruncate == true` and @p out is smaller, it throws **before** reading (datagram remains
     * queued).
     *   - Otherwise it performs exactly one receive sized appropriately to avoid redundant syscalls.
     * - When the platform cannot preflight size, it still performs **exactly one receive** into @p out and:
     *   - If truncation occurs and `opts.errorOnTruncate == true`, it throws **after** the read (the tail is lost).
     *   - If `opts.errorOnTruncate == false`, it returns the truncated bytes without error.
     *
     * On unconnected sockets, when `opts.updateLastRemote == true`, the internally tracked “last remote”
     * endpoint is updated to the sender of this datagram.
     *
     * Blocking/timeout behavior:
     * - In blocking mode, waits until a datagram arrives or a configured receive timeout elapses,
     *   then throws `SocketTimeoutException`.
     * - In non-blocking mode with no data, also throws `SocketTimeoutException` (would-block mapped).
     *
     * @param[out] out   Destination span that will receive up to `out.size()` bytes.
     * @param[in]  opts  Read options controlling size preflight, truncation policy, sender bookkeeping,
     *                   and receive flags. Other fields are honored; this method may override the internal
     *                   mode to avoid redundant preflight after a successful probe.
     *
     * @return Number of bytes written to @p out. Equals the datagram size when not truncated.
     *
     * @throws SocketException
     *         If the socket is not open; on OS-level receive errors (message via `SocketErrorMessage`);
     *         or when @p out is too small and truncation is disallowed by `opts.errorOnTruncate`.
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or when the socket is non-blocking
     *         and no data is available.
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, exactly the returned number of bytes have been written to @p out. For unconnected sockets,
     *       when `opts.updateLastRemote == true`, the internal “last remote” reflects the sender.
     *
     * @note For strict no-truncation behavior across platforms, set `opts.errorOnTruncate = true`.
     * @warning If @p out is smaller than the datagram and truncation is allowed, the remainder of that datagram
     *          is irretrievably dropped by the kernel per UDP semantics.
     *
     * @par Example
     * @code{.cpp}
     * std::array<char, 2048> buf{};
     * DatagramReadOptions ro{};
     * ro.errorOnTruncate = true; // enforce no-truncation policy
     * const std::size_t n = sock.readAvailable(std::span<char>(buf.data(), buf.size()), ro);
     * std::string_view payload(buf.data(), n);
     * @endcode
     *
     * @see readAvailable() for the convenience string-returning overload.
     * @see readAtMost(std::span<char>, const DatagramReadOptions&) for best-effort, single-recv reads.
     * @see readInto(void*, std::size_t, const DatagramReadOptions&) for the lower-level primitive.
     */
    std::size_t readAvailable(std::span<char> out, const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Strict exact-length UDP receive into a caller-provided buffer (single datagram).
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Copies the next datagram into @p buffer and enforces that the datagram size matches @p len
     * exactly. The method prefers an early, non-destructive failure: it attempts to probe the next
     * datagram size first and throws before reading if the size is not exactly @p len. If a probe
     * is not available on the platform, it performs one receive and throws afterward if the payload
     * was truncated or shorter than @p len.
     *
     * Behavior is equivalent to calling `readExact(buffer, len, ReadExactOptions{ .requireExact = true,
     * .padIfSmaller = false, .base.mode = DatagramReceiveMode::PreflightSize })`.
     * A single kernel receive is used for the payload copy; no allocation occurs.
     *
     * @param[out] buffer  [out] Destination buffer that must have capacity for exactly @p len bytes.
     * @param[in]  len     [in]  Required length in bytes; the datagram must be exactly this size.
     *
     * @return Number of bytes written to @p buffer. On success this equals @p len.
     *
     * @throws SocketException
     *         If the socket is not open; arguments are invalid; OS-level receive errors occur
     *         (message via SocketErrorMessage); or the datagram size does not equal @p len.
     * @throws SocketTimeoutException
     *         If a configured receive timeout elapses before any datagram is available, or when the
     *         socket is non-blocking and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened. @p buffer is non-null. @p len > 0.
     * @post On success, exactly @p len bytes have been written to @p buffer and the datagram queue
     *       advanced by one. On unconnected sockets, the internal “last remote” reflects the sender.
     *
     * @note For variable-length reads or padding behavior, use the more general
     *       `readExact(void*, std::size_t, const ReadExactOptions&)` overload.
     * @warning If the platform cannot preflight size, an oversized datagram will be consumed by the
     *          kernel and this function will throw after the read; the excess bytes are lost per UDP semantics.
     *
     * @par Example
     * @code{.cpp}
     * std::array<char, 512> buf{};
     * // Expect exactly 512 bytes in the next datagram; throw otherwise.
     * const std::size_t n = sock.readIntoExact(buf.data(), buf.size());
     * // n == 512 on success
     * @endcode
     */
    std::size_t readIntoExact(void* buffer, std::size_t len) const;

    /**
     * @brief Read up to @p n bytes from the next UDP datagram, waiting up to @p timeoutMillis for data.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Blocks the caller for at most @p timeoutMillis milliseconds until the socket is readable,
     * then performs a **single-recv, best-effort** read (no size preflight), returning up to @p n bytes.
     * If no datagram arrives before the timeout, a `SocketTimeoutException` is thrown.
     *
     * Implementation notes:
     * - Uses a readiness wait (`poll` on POSIX, `WSAPoll` on Windows) so the per-call timeout
     *   does **not** alter the socket’s global receive-timeout state.
     * - After readiness, delegates to the `readAtMost(std::size_t)` helper which enforces
     *   `DatagramReceiveMode::NoPreflight` (one recv) and updates the internally tracked
     *   “last remote” on unconnected sockets.
     *
     * @param[in] n              Maximum number of bytes to return. If 0, returns an empty string.
     * @param[in] timeoutMillis  Timeout in milliseconds. Must be >= 0. A value of 0 performs a
     *                           non-blocking check and throws `SocketTimeoutException` if no data is ready.
     *
     * @return The bytes received (size ≤ @p n). Marked `[[nodiscard]]` to avoid accidental loss.
     *
     * @throws SocketException
     *         If the socket is not open or an OS-level error occurs while waiting/receiving
     *         (message via `SocketErrorMessage`).
     * @throws SocketTimeoutException
     *         If no datagram becomes available within @p timeoutMillis, or if the socket is non-blocking
     *         and no data is available at the moment of the call.
     *
     * @pre  The socket has been successfully created/opened. @p timeoutMillis ≥ 0.
     * @post On success, the returned string contains up to @p n bytes from exactly one datagram; for
     *       unconnected sockets, the internal “last remote” reflects that datagram’s sender.
     *
     * @note This is a convenience overload. For zero-allocation reads with the same timeout behavior,
     *       wait for readability externally, then call `readAtMost(std::span<char>, ...)`.
     */
    [[nodiscard]] std::string readAtMostWithTimeout(std::size_t n, int timeoutMillis) const;

    /**
     * @brief Read a length-prefixed UDP datagram and return the payload (prefix type @p T).
     * @ingroup udp
     * @since 1.0
     *
     * @tparam T  Unsigned, trivially copyable integer type used for the length prefix
     *            (e.g., @c uint16_t, @c uint32_t, @c uint64_t).
     *
     * @details
     * Expects the next datagram’s payload to begin with a @p T-sized length field followed
     * by exactly that many payload bytes. The prefix is interpreted in the specified byte
     * order and validated against the actual datagram size. The function:
     * - Reads the **entire** datagram (throws on truncation) using a single receive.
     * - Verifies that the datagram size is at least @c sizeof(T) and that the prefix
     *   equals the remaining payload length.
     * - Enforces @p maxPayloadLen to guard against oversized frames.
     *
     * Works for connected and unconnected sockets; on unconnected sockets the internally
     * tracked “last remote” endpoint is updated to the sender.
     *
     * @param[in] maxPayloadLen   Maximum allowed payload length (not counting the prefix).
     *                            Clamped to @c MaxDatagramPayloadSafe. Use to prevent
     *                            pathological allocations. Defaults to @c MaxDatagramPayloadSafe.
     * @param[in] prefixEndian    Byte order of the length prefix. Defaults to big-endian.
     *
     * @return The decoded payload bytes as a @c std::string. Marked @c [[nodiscard]].
     *
     * @throws SocketException
     *         If the socket is not open; the datagram is smaller than @c sizeof(T);
     *         the prefix/payload sizes mismatch; the payload exceeds @p maxPayloadLen;
     *         or on OS-level receive errors (message via @c SocketErrorMessage).
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or when the
     *         socket is non-blocking and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, the returned string contains exactly the bytes declared by the prefix.
     *
     * @note This function throws rather than silently truncating. If you want best-effort
     *       reads, use @c readAtMost(...) instead.
     *
     * @par Example
     * @code{.cpp}
     * // Datagram format: [u32_be length][payload...]
     * std::string payload = sock.readPrefixed<std::uint32_t>();
     * @endcode
     */
    template <
        typename T,
        std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && std::is_trivially_copyable_v<T>, int> = 0>
    [[nodiscard]] std::string readPrefixed(std::size_t maxPayloadLen = MaxDatagramPayloadSafe,
                                           const std::endian prefixEndian = std::endian::big) const
    {
        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::readPrefixed<T>(): socket is not open.");

        // Clamp guard values
        if (maxPayloadLen == 0)
        {
            // Only valid if the incoming prefix is exactly zero; we’ll validate after the read.
            // Keep as 0 here; we still read the datagram and then enforce.
        }
        constexpr std::size_t safeMax = MaxDatagramPayloadSafe;
        if (maxPayloadLen > safeMax)
            maxPayloadLen = safeMax;

        // Prefer allocating exactly the datagram size when we can probe.
        std::size_t probed = 0;
        try
        {
            if (const std::size_t sz = internal::nextDatagramSize(getSocketFd()); sz > 0)
                probed = (std::min) (sz, safeMax);
        }
        catch (const SocketException&)
        {
            // Fallback to safe maximum below.
            probed = 0;
        }

        const std::size_t cap = (probed > 0) ? probed : safeMax;
        if (cap < sizeof(T))
        {
            // We’ll still read and then fail with a precise error if needed.
        }

        std::string frame(cap, '\0');

        // Read the entire datagram (no truncation); a single recv is used internally.
        DatagramReadOptions ro{};
        ro.errorOnTruncate = true; // enforce full-datagram semantics
        const std::size_t n = readAvailable(std::span<char>(frame.data(), frame.size()), ro);
        frame.resize(n);

        // Validate minimal size (must contain the prefix)
        if (frame.size() < sizeof(T))
            throw SocketException("DatagramSocket::readPrefixed<T>(): datagram smaller than length prefix.");

        // Decode prefix in requested endian
        const auto* p = reinterpret_cast<const unsigned char*>(frame.data());
        std::uint64_t decl = 0;

        if (prefixEndian == std::endian::big)
        {
            for (std::size_t i = 0; i < sizeof(T); ++i)
                decl = (decl << 8) | static_cast<std::uint64_t>(p[i]);
        }
        else
        { // little-endian
            for (std::size_t i = 0; i < sizeof(T); ++i)
                decl |= (static_cast<std::uint64_t>(p[i]) << (8 * i));
        }

        const std::size_t declared = decl;

        // Enforce declared size == actual payload length
        if (const std::size_t actual = frame.size() - sizeof(T); declared != actual)
            throw SocketException("DatagramSocket::readPrefixed<T>(): prefix/payload size mismatch.");

        // Enforce application-level max payload guard
        if (declared > maxPayloadLen)
            throw SocketException("DatagramSocket::readPrefixed<T>(): payload exceeds maxPayloadLen.");

        // Return payload slice
        return {frame.data() + sizeof(T), frame.data() + sizeof(T) + declared};
    }

    /**
     * @brief Discard the next UDP datagram without copying it out.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Consumes exactly one datagram from the socket and discards its bytes. This method ignores the
     * datagram size and never throws due to truncation. Timeouts and OS-level receive errors still apply.
     * On unconnected sockets, when @p opts.updateLastRemote is true, the internally tracked “last remote”
     * endpoint is updated to the sender of the discarded datagram.
     *
     * @param[in] opts  Read options controlling sender bookkeeping and receive flags. The @c mode field is
     *                  ignored (a single receive is always performed). Truncation reporting is ignored.
     *
     * @throws SocketException
     *         If the socket is not open or an OS-level receive error occurs (message via @c SocketErrorMessage).
     * @throws SocketTimeoutException
     *         If a configured receive timeout elapses before any datagram is available, or when the socket is
     *         non-blocking and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened.
     * @post Exactly one datagram has been removed from the socket’s receive queue. For unconnected sockets,
     *       when @p opts.updateLastRemote is true, the internal “last remote” reflects that sender.
     *
     * @par Example
     * @code{.cpp}
     * // Drop the next datagram, whatever its size
     * sock.discard();
     * @endcode
     */
    void discard(const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Discard the next UDP datagram only if its payload size is exactly @p n bytes.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Enforces **exact-length** semantics while draining the datagram:
     * - If a reliable size probe is available, the method verifies the next datagram size equals @p n and
     *   throws **before** reading when it does not, so the datagram remains queued.
     * - If size cannot be probed, it performs exactly one receive of up to @p n bytes. If the datagram is
     *   larger than @p n, the kernel truncates it; this method then throws. If the datagram is smaller than
     *   @p n, it also throws. In other words, the datagram must be exactly @p n bytes or the call fails.
     *
     * The discard operation minimizes copying: when the size is known to be @p n, the method reads with a
     * tiny scratch buffer to consume the datagram (the kernel drops the remainder).
     *
     * On unconnected sockets, when @p opts.updateLastRemote is true, the internally tracked “last remote”
     * endpoint is updated to the sender of the discarded datagram.
     *
     * @param[in] n     Required datagram payload size (> 0).
     * @param[in] opts  Read options controlling sender bookkeeping and receive flags. Truncation policy in @p opts
     *                  is ignored; this method enforces its own exact-length checks.
     *
     * @throws SocketException
     *         If the socket is not open; @p n == 0; OS-level receive errors occur (message via @c SocketErrorMessage);
     *         or the next datagram’s size is not exactly @p n bytes.
     * @throws SocketTimeoutException
     *         If a configured receive timeout elapses before any datagram is available, or when the socket is
     *         non-blocking and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened. @p n > 0.
     * @post Exactly one datagram has been removed from the socket’s receive queue if and only if its size was @p n.
     *       For unconnected sockets, when @p opts.updateLastRemote is true, the internal “last remote” reflects that
     * sender.
     *
     * @par Example
     * @code{.cpp}
     * // Drop the next datagram only if it is exactly 512 bytes
     * sock.discardExact(512);
     * @endcode
     */
    void discardExact(std::size_t n, const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Scatter-gather receive: read one UDP datagram into multiple non-contiguous buffers.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Copies the next datagram across the spans in @p buffers using a single kernel receive:
     * - Computes total capacity = sum of buffer sizes (clamped to MaxDatagramPayloadSafe).
     * - If size preflight succeeds and `opts.errorOnTruncate == true`, throws **before** reading when
     *   the datagram would not fit. Otherwise performs exactly one receive and reports truncation.
     * - On POSIX, uses `recvmsg` with `iovec[]` (Linux adds `MSG_TRUNC` to learn the full size even
     *   when truncated). On Windows, uses `WSARecvFrom` with `WSABUF[]`.
     * - On unconnected sockets, when `opts.updateLastRemote == true`, updates the internally tracked
     *   “last remote” to the sender of this datagram.
     *
     * @param[in,out] buffers  Scatter list of writable buffers; each entry must have a valid pointer when its size > 0.
     * @param[in]     opts     Read options controlling preflight, truncation policy, sender bookkeeping, and flags.
     *
     * @return DatagramReadResult { bytes, datagramSize, truncated, src, srcLen }.
     *
     * @throws SocketException
     *         If the socket is not open; buffer arguments are invalid; OS-level errors occur
     *         (message via SocketErrorMessage); or truncation is disallowed by `opts.errorOnTruncate`.
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or when the socket is non-blocking
     *         and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, the first `result.bytes` bytes have been written across the buffers in order. For unconnected
     *       sockets, when `opts.updateLastRemote == true`, the internal “last remote” reflects the sender.
     *
     * @note This method never allocates payload storage; it only references your buffers. For single-buffer cases,
     *       prefer `readInto(std::span<char>, ...)`.
     */
    [[nodiscard]] DatagramReadResult readv(std::span<BufferView> buffers, const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Scatter-gather receive that guarantees the entire next datagram fits the provided buffers.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Performs exactly one kernel receive to copy the **full** datagram across @p buffers in order.
     * If the datagram would not fit in the aggregate capacity, the call fails **before** reading when
     * a size probe is available; otherwise, it performs one receive and fails **after** the read if
     * truncation occurred. This is a strict, no-truncation variant of @c readv(...).
     *
     * On unconnected sockets, when @p opts.updateLastRemote == true, the internally tracked “last remote”
     * endpoint is updated to the sender of this datagram.
     *
     * @param[in,out] buffers  Scatter list of writable buffers. Each view must have a valid pointer when its size > 0.
     * @param[in]     opts     Read options. This method forces `errorOnTruncate = true` and prefers a size preflight
     *                         (`mode = PreflightSize` when the caller left it as `NoPreflight`).
     *
     * @return DatagramReadResult with:
     *         - `bytes`        Number of bytes copied across @p buffers (equals the datagram size on success),
     *         - `datagramSize` Full datagram size when known (0 if unknown on this platform/path),
     *         - `truncated`    Always `false` on success (method throws on truncation),
     *         - `src`/`srcLen` Sender address information.
     *
     * @throws SocketException
     *         If the socket is not open; buffer arguments are invalid; OS-level errors occur
     *         (message via `SocketErrorMessage`); or the datagram does not fit the provided capacity.
     * @throws SocketTimeoutException
     *         If a receive timeout elapses before any datagram is available, or when the socket is non-blocking
     *         and no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened.
     * @post On success, the entire datagram has been written across @p buffers and removed from the queue.
     *
     * @note This function never allocates payload storage. If you need automatic resizing, use a dynamic container
     *       API (e.g., `read(std::string&)` or `readExact` with `autoResizeDynamic`).
     *
     * @par Example
     * @code{.cpp}
     * std::array<char, 8>  hdr{};
     * std::array<char, 64> body{};
     * BufferView views[] = {
     *   {hdr.data(),  hdr.size()},
     *   {body.data(), body.size()}
     * };
     * DatagramReadOptions ro{};
     * auto r = sock.readvAll(std::span<BufferView>(views, 2), ro); // throws if datagram > 72 bytes
     * @endcode
     */
    [[nodiscard]] DatagramReadResult readvAll(std::span<BufferView> buffers,
                                              const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Back-compat convenience returning only the number of bytes copied.
     * @ingroup udp
     * @since 1.0
     *
     * @param[in,out] buffers  See @ref readvAll(std::span<BufferView>, const DatagramReadOptions&) const
     * @param[in]     opts     See @ref readvAll(std::span<BufferView>, const DatagramReadOptions&) const
     * @return Bytes copied (equals the datagram size on success).
     */
    std::size_t readvAllBytes(std::span<BufferView> buffers, const DatagramReadOptions& opts = {}) const
    {
        return readvAll(buffers, opts).bytes;
    }

    /**
     * @brief Scatter-gather, best-effort read of the next datagram with a per-call timeout.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Waits up to @p timeoutMillis milliseconds for the socket to become readable, then performs a
     * **single-recv, at-most** scatter-gather read into @p buffers:
     * - Copies up to the aggregate capacity of @p buffers (clamped by MaxDatagramPayloadSafe).
     * - If the datagram is larger than the aggregate capacity, the excess is discarded (standard UDP truncation).
     * - No size preflight is performed; truncation is **accepted** (no exception) to preserve best-effort semantics.
     *
     * On unconnected sockets, when `opts.updateLastRemote == true`, the internally tracked “last remote”
     * endpoint is updated to the sender of this datagram.
     *
     * @param[in,out] buffers      Scatter list of writable buffers; each entry must have a valid pointer if its size >
     * 0.
     * @param[in]     timeoutMillis Timeout in milliseconds (>= 0). 0 means non-blocking readiness check.
     * @param[in]     opts         Read options; fields other than `mode` and `errorOnTruncate` are honored.
     *                             This method forces `mode = DatagramReceiveMode::NoPreflight` and
     *                             `errorOnTruncate = false` to ensure single-recv, best-effort behavior.
     *
     * @return DatagramReadResult { bytes, datagramSize (best effort), truncated, src, srcLen }.
     *
     * @throws SocketException
     *         If the socket is not open; buffer arguments are invalid; or an OS-level error occurs
     *         (message via SocketErrorMessage).
     * @throws SocketTimeoutException
     *         If no datagram becomes ready within @p timeoutMillis, or when the socket is non-blocking and
     *         no data is available (would-block).
     *
     * @pre  The socket has been successfully created/opened. @p timeoutMillis >= 0.
     * @post On success, up to the aggregate buffer capacity has been written across @p buffers.
     *
     * @note For strict no-truncation scatter-gather reads, use @ref readvAll.
     */
    [[nodiscard]] DatagramReadResult readvAtMostWithTimeout(std::span<BufferView> buffers, int timeoutMillis,
                                                            const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Convenience wrapper returning only the number of bytes read.
     * @ingroup udp
     * @since 1.0
     */
    std::size_t readvAtMostWithTimeout(std::span<BufferView> buffers, int timeoutMillis) const
    {
        return readvAtMostWithTimeout(buffers, timeoutMillis, DatagramReadOptions{}).bytes;
    }

    /**
     * @brief Scatter-gather, strict no-truncation receive with a per-call total timeout.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Waits up to @p totalTimeoutMillis milliseconds for the socket to become readable, then performs a
     * **single** scatter-gather receive that **must** copy the entire next datagram into @p buffers. If the
     * datagram would not fit in the aggregate capacity, the call fails (either **before** reading when a
     * size probe is available, or **after** the read if not). This is a timed variant of `readvAll(...)`.
     *
     * The readiness wait uses `poll`/`WSAPoll` via `hasPendingData(...)`, so this per-call timeout does **not**
     * modify the socket’s global receive timeout. On unconnected sockets, when `opts.updateLastRemote == true`,
     * the internally tracked “last remote” endpoint is updated to the sender of this datagram.
     *
     * @param[in,out] buffers            Scatter list of writable buffers. Each view must have a valid pointer when its
     * size > 0.
     * @param[in]     totalTimeoutMillis Total timeout in milliseconds (>= 0). 0 performs a non-blocking readiness
     * check.
     * @param[in]     opts               Read options. This method enforces strict no-truncation by setting
     *                                   `errorOnTruncate = true` and preferring a size preflight
     *                                   (`mode = PreflightSize` when left as `NoPreflight`). Other fields are honored.
     *
     * @return DatagramReadResult with:
     *         - `bytes`        Number of bytes copied (equals the datagram size on success),
     *         - `datagramSize` Full datagram size when known (0 if unknown on this platform/path),
     *         - `truncated`    Always `false` on success (the method throws on truncation),
     *         - `src`/`srcLen` Sender address information.
     *
     * @throws SocketException
     *         If the socket is not open; buffer arguments are invalid; OS-level errors occur (message via
     * `SocketErrorMessage`); or the datagram does not fit the provided capacity.
     * @throws SocketTimeoutException
     *         If no datagram becomes ready within @p totalTimeoutMillis, or when the socket is non-blocking and no data
     * is available.
     *
     * @pre  The socket has been successfully created/opened. @p totalTimeoutMillis >= 0.
     * @post On success, the entire datagram has been written across @p buffers and removed from the queue. For
     * unconnected sockets, when `opts.updateLastRemote == true`, the internal “last remote” reflects the sender.
     *
     * @note If you don’t need a timeout, use `readvAll(...)`. If best-effort truncation is acceptable, see
     * `readvAtMostWithTimeout(...)`.
     *
     * @par Example
     * @code{.cpp}
     * std::array<char, 8>  hdr{};
     * std::array<char, 64> body{};
     * BufferView views[] = {
     *   {hdr.data(),  hdr.size()},
     *   {body.data(), body.size()}
     * };
     * DatagramReadOptions ro{};
     * auto r = sock.readvAllWithTotalTimeout(std::span<BufferView>(views, 2), 2500, ro);
     * @endcode
     */
    [[nodiscard]] DatagramReadResult readvAllWithTotalTimeout(std::span<BufferView> buffers, int totalTimeoutMillis,
                                                              const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Convenience wrapper returning only the number of bytes copied.
     * @ingroup udp
     * @since 1.0
     */
    std::size_t readvAllWithTotalTimeoutBytes(std::span<BufferView> buffers, int totalTimeoutMillis,
                                              const DatagramReadOptions& opts = {}) const
    {
        return readvAllWithTotalTimeout(buffers, totalTimeoutMillis, opts).bytes;
    }

    /**
     * @brief Peek at the next UDP datagram without consuming it (single receive with MSG_PEEK).
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Copies up to @p packet.buffer.size() bytes from the next datagram into @p packet.buffer
     * using a single kernel receive with MSG_PEEK so the datagram remains queued. The method:
     * - Never removes the datagram from the kernel queue.
     * - Can optionally grow @p packet.buffer before peeking when @p allowResize is true:
     *   it first tries to probe the next datagram size and, if successful, resizes exactly
     *   to that size (clamped to MaxDatagramPayloadSafe); otherwise it uses a safe default.
     * - Reports whether the datagram would be truncated via `DatagramReadResult::truncated`
     *   and, when available, returns the full datagram size (`datagramSize`).
     * - Does **not** update the internally tracked “last remote.” If you need that, do a real read.
     *
     * On POSIX, uses `recvmsg` with `MSG_PEEK`; on Linux it also sets `MSG_TRUNC` so the return
     * value reflects the **full datagram size** even when the user buffer is smaller. On Windows,
     * uses `recvfrom` with `MSG_PEEK`.
     *
     * @param[in,out] packet       Destination DatagramPacket. Its buffer is the peek target; when @p allowResize
     *                             is true and the size is known, it may be resized prior to the peek. When
     *                             `opts.resolveNumeric` is true, `packet.address` and `packet.port` are filled.
     * @param[in]     allowResize  When true and a size probe succeeds, grow @p packet.buffer to fit exactly
     *                             (clamped to MaxDatagramPayloadSafe). If false and the buffer is empty,
     *                             the call throws.
     * @param[in]     opts         Read options. `resolveNumeric` is honored; `recvFlags` is OR’ed with `MSG_PEEK`.
     *                             Other fields (e.g., `updateLastRemote`) are ignored for peek (no side-effects).
     *
     * @return DatagramReadResult with:
     *         - `bytes`        Number of bytes copied into `packet.buffer` (≤ its capacity),
     *         - `datagramSize` Full datagram size when known (0 if unknown on this platform/path),
     *         - `truncated`    True if the datagram is larger than the provided capacity,
     *         - `src`/`srcLen` Sender address (not stored as “last remote”).
     *
     * @throws SocketException
     *         If the socket is not open; `packet.buffer` is empty and @p allowResize is false; OS-level errors
     *         occur (message via `SocketErrorMessage`).
     * @throws SocketTimeoutException
     *         If the socket is non-blocking and no data is available (would-block), or a configured receive timeout
     *         elapses (platform-mapped).
     *
     * @pre  The socket has been successfully created/opened.
     * @post No datagram has been removed from the receive queue; `packet.buffer` contains a copy of the leading bytes.
     *
     * @note A zero-length datagram is valid: peeking it yields `bytes == 0` and does not indicate “connection closed.”
     */
    [[nodiscard]] DatagramReadResult peek(DatagramPacket& packet, bool allowResize = true,
                                          const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Check if the socket is readable within a timeout (no data is consumed).
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Waits until the socket becomes readable and returns whether at least one datagram
     * is ready to be received. This call does not read or consume data; it only checks
     * readiness. A negative @p timeoutMillis blocks indefinitely. A zero timeout performs
     * a non-blocking poll.
     *
     * Implementation uses `poll` on POSIX and `WSAPoll` on Windows (preferred over `select`
     * to avoid FD_SETSIZE limits and provide better EINTR semantics).
     *
     * @param[in] timeoutMillis  Timeout in milliseconds. < 0 = infinite, 0 = immediate, > 0 = bounded wait.
     *
     * @return true if the socket is readable within the timeout; false on timeout (no data ready).
     *
     * @throws SocketException
     *         If the socket is not open or if the readiness API reports an error
     *         (error code/message via `SocketErrorMessage`).
     *
     * @pre The socket has been successfully created/opened.
     * @post No data is consumed from the socket; the readability state may change due to races typical
     *       of readiness APIs (i.e., data could be gone by the time you attempt to read).
     *
     * @note Prefer calling this as a guard before best-effort reads with per-call timeouts
     *       (e.g., `readAtMostWithTimeout` can delegate to this check).
     */
    bool hasPendingData(int timeoutMillis) const;

    /**
     * @brief Retrieves the Maximum Transmission Unit (MTU) of the local interface associated with the socket.
     * @ingroup udp
     *
     * This method attempts to query the MTU of the network interface to which the socket is currently bound.
     * It supports both Windows and POSIX platforms using appropriate system APIs and returns the result
     * as an optional integer.
     *
     * ---
     *
     * ### 🔍 What is the MTU?
     * The MTU is the largest size (in bytes) of a datagram that can be sent over a network interface
     * without fragmentation. For example, Ethernet IPv4 commonly uses an MTU of 1500 bytes,
     * while the payload limit for UDP is typically 1472 bytes after IP/UDP headers.
     *
     * ---
     *
     * ### ⚙️ Platform Behavior
     *
     * - **Windows:**
     *   - Uses `GetAdaptersAddresses()` to enumerate system interfaces.
     *   - Uses `getsockname()` to determine the bound local IP.
     *   - Compares adapter unicast IPs to the bound address using normalized logic that handles
     *     IPv4 and IPv4-mapped IPv6.
     *   - Returns the MTU for the matched adapter.
     *
     * - **POSIX (Linux, macOS, etc.):**
     *   - Uses `getsockname()` to retrieve the bound IP.
     *   - Uses `getifaddrs()` to map the IP to a named interface.
     *   - Uses `ioctl(SIOCGIFMTU)` to retrieve the MTU for the interface.
     *
     * ---
     *
     * ### ✅ Use Cases
     * - Enforce maximum UDP payload size to avoid fragmentation
     * - Tune protocol chunk sizes dynamically based on interface limits
     * - Avoid silent datagram drops on MTU-exceeding payloads
     *
     * ---
     *
     * @return
     * - `std::optional<int>` containing the MTU value if successfully determined.
     * - `std::nullopt` if:
     *   - The socket is not bound
     *   - The local interface cannot be resolved
     *   - The OS query fails
     *
     * @throws SocketException If a low-level socket operation (e.g., `getsockname()`) fails.
     *
     * @note This method returns the MTU of the **local sending interface**, not of any remote peer.
     * @note On some platforms, this requires the socket to be explicitly bound or connected.
     *
     * @see write(), bind(), connect(), getLocalSocketAddress(), getBoundLocalIp(), ipAddressesEqual()
     */
    [[nodiscard]] std::optional<int> getMTU() const;

    /**
     * @brief Block until the socket is ready for I/O or a timeout occurs.
     * @ingroup udp
     *
     * @details
     * Waits for the connected UDP socket to become ready for the requested operation(s)
     * using a portable polling primitive (`poll` on POSIX, `WSAPoll` on Windows).
     * This method does **not** change the socket’s blocking mode and is safe to call
     * on both blocking and non-blocking sockets.
     *
     * **Timeout semantics**
     * - `timeoutMillis < 0`  → wait indefinitely (no timeout).
     * - `timeoutMillis == 0` → non-blocking poll (immediate). If not ready, throws `SocketTimeoutException`.
     * - `timeoutMillis > 0`  → wait up to the specified milliseconds.
     *
     * If an exceptional condition is reported by the OS (e.g., `POLLERR`, `POLLNVAL`,
     * `POLLHUP`), this function retrieves `SO_ERROR` when available and throws a
     * `SocketException` with that error code/message.
     *
     * @param[in] dir            Readiness to wait for: `Direction::Read`, `Direction::Write`,
     *                           or `Direction::ReadWrite`.
     * @param[in] timeoutMillis  Timeout in milliseconds (see semantics above).
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     *
     * @throws SocketTimeoutException  If the requested readiness is not achieved before the timeout.
     * @throws SocketException         On polling failure or detected socket error
     *                                 (uses `GetSocketError()` and `SocketErrorMessage()`).
     *
     * @code
     * // Example: wait up to 500 ms to become writable, then send one datagram
     * waitReady(Direction::Write, 500);
     * internal::sendExact(getSocketFd(), data, len);
     * @endcode
     */
    void waitReady(Direction dir, int timeoutMillis) const;

    /**
     * @brief Sets the size of the internal buffer used for string-based UDP receive operations.
     * @ingroup udp
     *
     * This method controls the size of the internal buffer used internally by
     * `read<std::string>()`. It does **not** affect the operating system's socket-level
     * receive buffer (`SO_RCVBUF`), nor does it apply to fixed-size `read<T>()` calls.
     *
     * ---
     *
     * ### 🧠 Purpose
     * - Limits the maximum number of bytes `read<std::string>()` can receive in a single call
     * - Controls the size of the internally managed `std::vector<char>` buffer
     * - Affects only high-level string reads from **connected** UDP sockets
     *
     * ---
     *
     * ### ⚙️ Implementation Details
     * - Resizes an internal `std::vector<char>` used exclusively by `read<std::string>()`
     * - **Does not** impact `read<T>()` or `receiveFrom()` which use their own fixed-size buffers
     * - Thread-safe with respect to other `DatagramSocket` instances
     * - Safe to call at any time after construction
     *
     * ---
     *
     * ### 🧪 Example
     * @code
     * DatagramSocket udp(12345);
     * udp.setInternalBufferSize(8192); // set 8 KB for string reads
     *
     * udp.connect("192.168.1.100", 12345);
     * std::string message = udp.read<std::string>();
     * @endcode
     *
     * ---
     *
     * @param[in] newLen New size (in bytes) for the internal buffer
     *
     * @throws std::bad_alloc If resizing the buffer fails due to memory allocation
     *
     * @see read<std::string>() Uses this buffer for connected-mode string reads
     * @see receiveFrom() Uses its own internal buffer, not affected by this setting
     * @see setReceiveBufferSize() Sets the OS-level socket buffer
     * @see setSendBufferSize() For tuning datagram send capacity
     */
    void setInternalBufferSize(std::size_t newLen);

    /**
     * @brief Closes the datagram socket and releases its underlying system resources.
     * @ingroup udp
     *
     * This method performs a full teardown of the datagram socket, closing the underlying file
     * descriptor/handle and releasing all associated resources. After closure, the socket becomes
     * invalid and cannot be reused.
     *
     * ---
     *
     * ### ⚙️ Core Behavior
     * - Invalidates the socket descriptor (`_sockFd = INVALID_SOCKET`)
     * - Releases system-level socket resources via `::close()` or `::closesocket()`
     * - Resets all internal state flags (`_isBound`, `_isConnected`)
     * - Clears address resolution data and remote peer information
     * - Makes the socket unsuitable for further I/O operations
     *
     * ---
     *
     * ### 🔒 Safety
     * - Safe to call multiple times (idempotent)
     * - Thread-safe with respect to other instances
     * - Ensures no resource leaks even after errors
     * - Preserves exception safety guarantees
     *
     * ---
     *
     * ### 🧪 Example
     * ```cpp
     * DatagramSocket sock;
     * sock.bind(12345);
     *
     * // ... use socket ...
     *
     * sock.close(); // Release all resources
     * assert(sock.isClosed());
     * ```
     *
     * @throws SocketException If the underlying close operation fails unexpectedly.
     *
     * @note
     * - Any pending operations on the socket will be aborted
     * - Subsequent operations will throw `SocketException`
     * - Use `isClosed()` to check socket state
     *
     * @see isClosed() To verify socket state
     * @see isValid() Alternative check for usability
     * @see cleanup() Internal cleanup helper
     * @since 1.0
     */
    void close();

    /**
     * @brief Checks whether the datagram socket is valid and ready for use.
     * @ingroup udp
     *
     * Returns `true` if the socket has a valid file descriptor and has not been closed
     * or moved-from. A valid datagram socket can be used for sending and receiving UDP
     * packets, though it may or may not be bound or connected.
     *
     * This method performs a quick, local check:
     * - It does **not** verify whether the socket is bound (see `isBound()`)
     * - It does **not** verify whether the socket is connected (see `isConnected()`)
     * - It does **not** query the operating system or socket state
     *
     * ### Use Cases
     * - Guarding against use-after-close
     * - Early validation in test or utility code
     * - Precondition checks before `bind()` or `connect()`
     *
     * ### Implementation Details
     * - Returns `true` if `getSocketFd() != INVALID_SOCKET`
     * - Constant-time, thread-safe, no system calls
     *
     * @return `true` if the socket is open and usable; `false` if it was closed, moved-from, or failed to initialize.
     *
     * @note A valid socket may be unbound or unconnected. Use `isBound()` and `isConnected()` to query those states.
     *
     * ### Example
     * @code{.cpp}
     * DatagramSocket sock("example.com", 9999);
     * assert(sock.isValid());
     * sock.connect();
     * sock.close();
     * assert(!sock.isValid()); // ✅ socket no longer usable
     * @endcode
     *
     * @see isBound(), isConnected(), close(), getSocketFd()
     */
    [[nodiscard]] bool isValid() const noexcept { return getSocketFd() != INVALID_SOCKET; }

    /**
     * @brief Checks whether the datagram socket has been closed or is otherwise invalid.
     * @ingroup udp
     *
     * Returns `true` if the socket is no longer usable—either because it was explicitly closed
     * via `close()`, or because it was never successfully initialized (i.e., holds an invalid
     * file descriptor).
     *
     * This method does **not** perform any system-level query. It simply checks whether the internal
     * socket descriptor equals `INVALID_SOCKET`. This is the standard invariant for resource management
     * in the jsocketpp UDP API.
     *
     * ### Common Scenarios
     * - The socket was default-initialized or failed during construction
     * - The socket was explicitly closed via `close()`
     * - The socket was moved-from, leaving the source in a valid but unusable state
     *
     * @note Once a datagram socket is closed, it cannot be reused. Create a new instance to open a new socket.
     *
     * @return `true` if the socket is closed or invalid, `false` if it is open and usable.
     *
     * ### Example
     * @code
     * jsocketpp::DatagramSocket sock(12345);
     * assert(!sock.isClosed());
     * sock.close();
     * assert(sock.isClosed()); // ✅
     * @endcode
     *
     * @see close(), isConnected(), isBound(), getSocketFd()
     */
    [[nodiscard]] bool isClosed() const noexcept { return getSocketFd() == INVALID_SOCKET; }

    /**
     * @brief Retrieves the raw socket address of the last known remote peer.
     * @ingroup udp
     *
     * This method exposes the internal low-level `sockaddr_storage` structure representing the
     * last known remote peer that this socket communicated with. It is useful in advanced use
     * cases where direct access to address structures is required — for example:
     *
     * - Custom routing or connection tracking
     * - Creating new sockets targeting the same peer
     * - Implementing security checks or access controls
     * - Avoiding repeated DNS resolution
     *
     * ---
     *
     * ### 🔁 Behavior Based on Mode
     *
     * - **Connected Socket:**
     *   - Reflects the peer specified via `connect()`.
     *   - Remains constant until the socket is closed or reset.
     *
     * - **Unconnected Socket:**
     *   - Reflects the peer involved in the most recent:
     *     - `read()`
     *     - `recvFrom()`
     *     - `writeTo()`
     *     - `write(DatagramPacket)`
     *   - If no such operation has occurred, the result is empty.
     *
     * ---
     *
     * ### Example
     * @code
     * auto remote = sock.getLastPeerSockAddr();
     * if (remote) {
     *     const sockaddr* sa = reinterpret_cast<const sockaddr*>(&remote->first);
     *     // Now you can use sendto(), bind(), connect(), etc.
     * }
     * @endcode
     *
     * ---
     *
     * @return
     * - `std::optional<std::pair<sockaddr_storage, socklen_t>>`:
     *     - `.first` contains the raw peer address
     *     - `.second` is the valid length of the address
     * - Returns `std::nullopt` if the socket has not communicated with any peer.
     *
     * @note This is a low-level method. Use `getRemoteIp()` or `getRemoteSocketAddress()` for string-formatted access.
     * @note The returned structure is a **copy**; modifying it has no effect on socket state.
     *
     * @see getRemoteIp(), getRemotePort(), writeTo(), read(), isConnected(), connect()
     */
    [[nodiscard]] std::optional<std::pair<sockaddr_storage, socklen_t>> getLastPeerSockAddr() const
    {
        if (_remoteAddrLen == 0)
        {
            // No communication has occurred; no peer info available
            return std::nullopt;
        }

        return std::make_pair(_remoteAddr, _remoteAddrLen);
    }

  protected:
    /**
     * @brief Low-level, single-recv primitive that copies one UDP datagram into a caller buffer.
     * @ingroup udp
     * @since 1.0
     *
     * @details
     * Performs exactly one kernel receive. On POSIX, uses `recvmsg` and sets `MSG_TRUNC` (on Linux) to
     * obtain precise truncation signaling; on Linux the return value with `MSG_TRUNC` reflects the full
     * datagram size even when the buffer is smaller. When a size preflight is requested via @p mode, the
     * next datagram size is queried (if supported) and the requested copy length is clamped accordingly.
     * This function is policy-free: it never throws solely because truncation happened; instead it reports
     * size and truncation through @p outDatagramSz and @p outTruncated so higher-level APIs can decide.
     *
     * @param[in,out] buf            Destination buffer receiving up to @p len bytes.
     * @param[in]     len            Capacity of @p buf in bytes; the function never writes more than this.
     * @param[in]     mode           Receive mode. If not @c DatagramReceiveMode::NoPreflight, a size probe
     *                               is attempted to refine the requested copy length.
     * @param[in]     recvFlags      Flags passed to the underlying receive. May be OR’ed with platform
     *                               flags internally (e.g., `MSG_TRUNC` on Linux).
     * @param[out]    outSrc         Optional. Sender address as returned by the kernel.
     * @param[in,out] outSrcLen      Optional. On input, capacity of @p outSrc. On output, actual size written.
     * @param[out]    outDatagramSz  Optional. Full datagram size when known (preflight or Linux+`MSG_TRUNC`);
     *                               otherwise set to the copied byte count (best effort).
     * @param[out]    outTruncated   Optional. Set to @c true if the datagram did not fully fit in @p buf.
     *
     * @return Number of bytes actually written to @p buf (≤ @p len). May be 0 for a zero-length datagram.
     *
     * @throws SocketException
     *         On invalid arguments, socket not open, or other OS-level errors (message via @c SocketErrorMessage).
     * @throws SocketTimeoutException
     *         If a configured receive timeout elapses before any datagram is received, or when the socket is
     *         non-blocking and no data is available (would-block).
     *
     * @pre  The socket handle is valid (open). If @p outSrc is non-null, @p outSrcLen points to a writable size.
     * @post On success, exactly the returned number of bytes are stored in @p buf. When provided, @p outSrc/@p
     * outSrcLen reflect the sender; @p outDatagramSz and @p outTruncated report size and truncation status.
     *
     * @note The requested copy length is always clamped to @c MaxDatagramPayloadSafe for safety.
     * @warning On platforms without a reliable size probe and without `MSG_TRUNC` semantics, truncation detection may
     *          be heuristic (e.g., a full buffer implies possible truncation).
     *
     * @par Example
     * @code{.cpp}
     * std::array<char, 2048> buf{};
     * sockaddr_storage src{};
     * socklen_t srcLen = sizeof(src);
     * std::size_t pktSize = 0;
     * bool wasTrunc = false;
     *
     * const std::size_t n = sock.readIntoBuffer(buf.data(), buf.size(),
     *                                           DatagramReceiveMode::NoPreflight,
     *                                           0, &src, &srcLen, &pktSize, &wasTrunc);
     * // n bytes are in buf; pktSize is the full size when known; wasTrunc indicates truncation.
     * @endcode
     */
    std::size_t readIntoBuffer(char* buf, std::size_t len, DatagramReceiveMode mode, int recvFlags,
                               sockaddr_storage* outSrc, socklen_t* outSrcLen, std::size_t* outDatagramSz,
                               bool* outTruncated) const;

    /**
     * @brief Internal helper that releases socket resources and resets all internal state.
     * @ingroup udp
     *
     * This method safely resets the `DatagramSocket` to an uninitialized state. It is used
     * during error recovery to release partially constructed socket state, and ensures the object
     * no longer appears bound or connected after failure.
     *
     * ---
     *
     * ### ⚙️ Behavior
     * - Closes the socket if it is valid (`getSocketFd() != INVALID_SOCKET`)
     * - Sets the socket descriptor to `INVALID_SOCKET`
     * - Resets internal flags:
     *   - `_isBound = false`
     *   - `_isConnected = false`
     * - Clears cached local and remote address structures:
     *   - `_localAddr`, `_localAddrLen`
     *   - `_remoteAddr`, `_remoteAddrLen`
     *
     * ---
     *
     * ### 🔒 Safety
     * - Safe to call multiple times
     * - Never throws
     * - Used internally by `cleanupAndThrow()` and `cleanupAndRethrow()` for consistent recovery
     *
     * @note This method is private and should not be exposed to users directly. It is designed
     *       for internal lifecycle management and exception-safe cleanup.
     *
     * @see cleanupAndThrow(), cleanupAndRethrow(), CloseSocket()
     */
    void cleanup();

    /**
     * @brief Releases all socket resources and throws a `SocketException` with the given error code.
     * @ingroup udp
     *
     * This method performs complete internal cleanup of the datagram socket and then throws
     * a `SocketException`. It is typically invoked when construction, binding, or configuration
     * of the socket fails, ensuring the object is left in a safe, uninitialized state.
     *
     * ---
     *
     * ### ⚙️ Behavior
     * Internally delegates to `cleanup()`, which:
     * - Closes the socket if open
     * - Resets the socket descriptor to `INVALID_SOCKET`
     * - Resets internal state flags:
     *   - `_isBound = false`
     *   - `_isConnected = false`
     * - Clears cached local/remote address structures
     *
     * After cleanup, throws a `SocketException` with:
     * - The provided `errorCode`
     * - A descriptive message from `SocketErrorMessage(errorCode)`
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * if (::bind(getSocketFd(), ...) == SOCKET_ERROR)
     * {
     *     cleanupAndThrow(GetSocketError());
     * }
     * @endcode
     *
     * @param[in] errorCode The system or application-level error code to report.
     *
     * @throws SocketException Always throws after cleanup.
     *
     * @see cleanup()
     * @see cleanupAndRethrow()
     * @see SocketException, SocketErrorMessage()
     */
    void cleanupAndThrow(int errorCode);

    /**
     * @brief Cleans up the datagram socket and rethrows the currently active exception.
     * @ingroup udp
     *
     * This method is intended to be used inside a `catch` block when an exception occurs
     * during socket construction or setup. It ensures all internal resources are safely
     * released before rethrowing the original exception, leaving the socket in an
     * uninitialized and safe state.
     *
     * ---
     *
     * ### ⚙️ Behavior
     * Internally calls `cleanup()`, which:
     * - Closes the socket if it is open
     * - Sets the socket descriptor to `INVALID_SOCKET`
     * - Resets connection state flags:
     *   - `_isBound = false`
     *   - `_isConnected = false`
     * - Clears cached local/remote address structures
     *
     * Then rethrows the active exception using `throw;`.
     *
     * ---
     *
     * ### ⚠️ Usage Notes
     * - Must be invoked only from within a valid `catch` block
     * - If no exception is currently being handled, calling this method causes undefined behavior
     * - Never accepts an exception object — `throw;` preserves full type and context
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * try {
     *     setNonBlocking(true);
     *     setSoRecvTimeout(2000);
     * } catch (...) {
     *     cleanupAndRethrow(); // Fully resets state and rethrows original exception
     * }
     * @endcode
     *
     * @throws SocketException or the original active exception
     *
     * @see cleanup(), cleanupAndThrow()
     * @see SocketException
     */
    void cleanupAndRethrow();

    /**
     * @brief Decide how many bytes to attempt to receive for the next UDP datagram.
     * @ingroup udp
     *
     * @details
     * This method centralizes the library’s **Java-like sizing policy** so all UDP `read()` variants
     * behave consistently while keeping `_internalBuffer` as reusable storage only (never resized here).
     *
     * **Policy order:**
     *  1. **Prefer exact size** from `jsocketpp::internal::nextDatagramSize(getSocketFd())`.
     *     If the platform reports the pending datagram length, use that value.
     *  2. **Otherwise, use caller capacity**: if `_internalBuffer.size() > 0`, treat it as the
     *     caller-provided maximum (Java semantics).
     *  3. If the internal buffer is **unset/empty**, fall back to `DefaultDatagramReceiveSize`.
     *  4. **Clamp** the chosen value to `MaxDatagramPayloadSafe` (65,507 bytes) to avoid oversizing.
     *
     * **Semantics:**
     *  - This method performs **no I/O** and **never resizes** `_internalBuffer`.
     *  - The returned value is a **target** receive size; the actual bytes read may be less
     *    (e.g., zero-length datagrams, short reads, or errors).
     *
     * @return Proposed byte count to pass to `recv()` / `recvfrom()` for the next datagram.
     *
     * @pre `getSocketFd()` is valid if the result will be used immediately for I/O.
     * @note If the OS cannot report exact size and the sender’s datagram exceeds both the chosen
     *       value and available destination storage, the payload may be truncated (standard UDP behavior).
     * @see jsocketpp::internal::nextDatagramSize()
     * @see DefaultDatagramReceiveSize
     * @see MaxDatagramPayloadSafe
     * @since 1.0
     */
    std::size_t chooseReceiveSize() const
    {
        if (const std::size_t exact = internal::nextDatagramSize(getSocketFd()); exact > 0)
            return (exact > MaxDatagramPayloadSafe) ? MaxDatagramPayloadSafe : exact;

        const std::size_t fallback = _internalBuffer.empty() ? DefaultDatagramReceiveSize : _internalBuffer.size();

        return (fallback > MaxDatagramPayloadSafe) ? MaxDatagramPayloadSafe : fallback;
    }

    /**
     * @brief Remember the last remote peer after an unconnected receive.
     *
     * @details
     * Copies @p src into @_remoteAddr and sets @_remoteAddrLen to @p len. This updates the values
     * returned by @c getRemoteIp(), @c getRemotePort(), and @c getRemoteSocketAddress() to reflect
     * the most recent sender on unconnected sockets.
     *
     * @param[in] src Sender address as returned by @c recvfrom().
     * @param[in] len Length of @p src.
     *
     * @note Callers should only invoke this after a successful receive with @c recvfrom().
     * @since 1.0
     */
    void rememberRemote(const sockaddr_storage& src, const socklen_t len) const noexcept
    {
        _remoteAddr = src;
        _remoteAddrLen = len;
    }

    /**
     * @brief Throw a descriptive exception when a UDP datagram’s size differs from what was expected.
     * @ingroup udp
     *
     * @details
     * Emits a uniform, user-friendly error when the size of the next datagram does not match the
     * caller’s expectation. This is typically used after either:
     *  - A **size probe** (e.g., via `FIONREAD` or `MSG_PEEK|MSG_TRUNC`) indicated an expected size, or
     *  - The caller required an **exact** payload size (e.g., fixed-size read into a POD object).
     *
     * The exception message intentionally distinguishes whether the `actual` value came from a prior
     * **probe** versus a **receive** operation, to aid debugging (reordering, truncation, or races
     * between probe and read).
     *
     * This function does not perform any I/O and does not modify socket state; it simply constructs
     * and throws a `SocketException` with a clear message.
     *
     * @param[in] expected        The number of bytes the caller expected to be present in the next datagram.
     * @param[in] actual          The number of bytes observed (either probed or received).
     * @param[in] isProbedKnown   Set to `true` if @p actual comes from a size probe; set to `false` if it
     *                            reflects the size actually received from a `recv`/`recvfrom` call.
     *
     * @throws SocketException
     * - Always throws (logical error; not a system error).
     * - Uses error code `0` and a message of the form:
     *   `"UDP datagram size mismatch: expected <E>, <probed|received> <A>"`.
     *
     * @note This function **always throws** and never returns.
     * @note No system error is consulted or reported; this represents a violated size invariant in the
     *       caller’s logic rather than a kernel failure.
     *
     * @code
     * // Example: after probing size, enforce exact-size semantics
     * const std::size_t probed = probeNextDatagramSize();
     * const std::size_t received = recvExact(buf, probed);
     * if (received != probed) {
     *     DatagramSocket::throwSizeMismatch(probed, received, true);
     * }
     * @endcode
     *
     * @see readIntoBuffer(), DefaultDatagramReceiveSize, MaxDatagramPayloadSafe
     */
    static void throwSizeMismatch(const std::size_t expected, const std::size_t actual, const bool isProbedKnown)
    {
        // Two-arg pattern with wrapped message for consistency across the project
        constexpr int err = 0; // logical (not a system error)
        const std::string msg =
            "UDP datagram size mismatch: expected " + std::to_string(expected) +
            (isProbedKnown ? (", probed " + std::to_string(actual)) : (", received " + std::to_string(actual)));
        throw SocketException(err, msg);
    }

    /**
     * @brief Enforce UDP payload size limits for **connected** datagram sends.
     * @ingroup udp
     *
     * @details
     * Validates that a single datagram of @p payloadSize bytes can be sent to the
     * **currently connected** peer without exceeding protocol-level maxima.
     *
     * The check is conservative by default:
     * - Always allows up to `MaxDatagramPayloadSafe` (65,507 bytes), which is safe on both IPv4 and IPv6.
     * - If the connected peer is IPv6 (`AF_INET6`), allows the IPv6 theoretical maximum
     *   (`MaxUdpPayloadIPv6` = 65,527) while continuing to reject anything larger.
     *
     * If the socket is connected to IPv4 (`AF_INET`) and @p payloadSize exceeds the IPv4
     * limit (`MaxUdpPayloadIPv4` = 65,507), an exception is thrown **before** calling
     * `send()`, providing a precise, user-friendly error message.
     *
     * The remote address family is determined via:
     *  1) cached last-peer address when available, otherwise
     *  2) a `getpeername()` query on the connected socket.
     *
     * ---
     *
     * ### Rationale
     * Proactive validation yields deterministic, actionable errors (instead of a vague
     * `EMSGSIZE` from the kernel) and avoids futile syscalls.
     *
     * ---
     *
     * ### Example (internal use)
     * @code
     * // Inside write(std::string_view):
     * enforceSendCapConnected(value.size());
     * internal::sendExact(getSocketFd(), value.data(), value.size());
     * @endcode
     *
     * ---
     *
     * @param[in] payloadSize  Number of bytes intended for a single datagram.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @throws SocketException
     * - If the socket is not open or not connected (logical error).
     * - If `getpeername()` fails while determining the connected peer family
     *   (OS error; uses `GetSocketError()`/`SocketErrorMessage()`).
     * - If @p payloadSize exceeds the permitted maximum for the connected peer’s family.
     *
     * @note This method does **not** enforce path-MTU/non-fragmenting policies; it only
     *       enforces absolute protocol maxima to prevent guaranteed failures.
     *
     * @see MaxDatagramPayloadSafe, MaxUdpPayloadIPv4, MaxUdpPayloadIPv6, write(), writeTo()
     */
    void enforceSendCapConnected(const std::size_t payloadSize) const
    {
        // Zero-length datagrams are valid (often used as keep-alives); nothing to enforce.
        if (payloadSize == 0)
            return;

        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException(0, "DatagramSocket::enforceSendCapConnected(): socket is not open.");

        if (!_isConnected)
            throw SocketException(0, "DatagramSocket::enforceSendCapConnected(): socket is not connected.");

        // Determine remote family: prefer cached last-peer, else query getpeername().
        int family = AF_UNSPEC;
        if (_remoteAddrLen > 0)
        {
            family = reinterpret_cast<const sockaddr*>(&_remoteAddr)->sa_family;
        }
        else
        {
            sockaddr_storage peer{};
            auto len = static_cast<socklen_t>(sizeof(peer));
            if (::getpeername(getSocketFd(), reinterpret_cast<sockaddr*>(&peer), &len) == SOCKET_ERROR)
            {
                const int err = GetSocketError();
                throw SocketException(err, SocketErrorMessage(err));
            }
            family = reinterpret_cast<const sockaddr*>(&peer)->sa_family;
        }

        // Always allow the conservative, cross-stack "safe" ceiling.
        if (payloadSize <= MaxDatagramPayloadSafe)
            return;

        // If the connected peer is IPv6, allow the IPv6 theoretical headroom.
        if (family == AF_INET6 && payloadSize <= MaxUdpPayloadIPv6)
            return;

        // Compose a precise, helpful error message and throw a logical (non-OS) error.
        std::string msg = "UDP datagram payload (" + std::to_string(payloadSize) + " bytes) ";

        if (payloadSize > MaxUdpPayloadIPv6)
        {
            msg += "exceeds IPv6 theoretical maximum (" + std::to_string(MaxUdpPayloadIPv6) + "). ";
        }
        else if (family == AF_INET)
        {
            msg += "exceeds IPv4 maximum (" + std::to_string(MaxUdpPayloadIPv4) + "). ";
        }
        else
        {
            // AF_UNSPEC or other: fell through the safe ceiling and not eligible for IPv6 headroom.
            msg += "exceeds MaxDatagramPayloadSafe (" + std::to_string(MaxDatagramPayloadSafe) + "). ";
        }

        msg += "Split the payload across multiple datagrams or switch to a stream protocol.";

        // Two-arg form for consistency with other logical throws in the project.
        throw SocketException(0, msg);
    }

    /**
     * @brief Resolve and send one unconnected UDP datagram to the first compatible destination.
     * @ingroup udp
     *
     * @details
     * Resolves @p host and @p port (AF_UNSPEC, UDP), iterates all A/AAAA candidates, and:
     *  - **Skips** any family whose theoretical UDP maximum would be exceeded by @p len
     *    (`MaxUdpPayloadIPv4`/`MaxUdpPayloadIPv6`).
     *  - Sends the datagram with a single syscall (`internal::sendExactTo`) to the **first**
     *    compatible candidate that succeeds.
     *  - On success, remembers the last remote endpoint for helpers (without flipping the
     *    socket into a connected state).
     *
     * If no candidates can even be *attempted* (e.g., payload > IPv6 max or only A records
     * for a payload that requires IPv6), throws a **logical** `SocketException` with a clear
     * diagnostic. If at least one attempt is made and all fail, throws a `SocketException`
     * with the last OS error (`GetSocketError()`/`SocketErrorMessage()`).
     *
     * This helper is intended for all **unconnected** write paths (e.g., `writeTo(...)`,
     * `write(const DatagramPacket&)` when the packet specifies a destination).
     *
     * @param[in] host  Destination hostname or numeric address (IPv4/IPv6).
     * @param[in] port  Destination UDP port (> 0).
     * @param[in] data  Pointer to payload (may be null iff @p len == 0).
     * @param[in] len   Number of bytes to send in a single UDP datagram.
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     *
     * @throws SocketException
     *   - With code `0` and an explanatory message if no address family can carry the payload.
     *   - With the last OS error code/message if all attempted sends fail.
     *
     * @note Zero-length datagrams are valid; this helper returns `0` without sending.
     * @see writeTo(), write(const DatagramPacket&), MaxUdpPayloadIPv4, MaxUdpPayloadIPv6
     */
    void sendUnconnectedTo(std::string_view host, Port port, const void* data, std::size_t len) const;

    /**
     * @brief View a textual buffer as raw bytes without copying.
     * @ingroup udp
     *
     * @details
     * Returns a `std::span<const std::byte>` that references the same memory as the input
     * `std::string_view` @p sv. This is an **O(1)**, zero-allocation conversion intended
     * for APIs that operate on binary payloads (e.g., UDP datagram sends) while callers
     * may naturally hold data as text.
     *
     * - The resulting span **does not** include any null terminator; it covers exactly
     *   `sv.size()` bytes starting at `sv.data()`.
     * - The span is **non-owning**; its lifetime and validity are tied to the lifetime
     *   of the storage viewed by @p sv. Callers must ensure that storage remains alive
     *   until any I/O using the span completes.
     * - Aliasing/strict-aliasing: viewing `char`/`unsigned char` storage as `std::byte`
     *   for read-only purposes is well-formed; this helper never writes through the span.
     *
     * @param[in] sv   The textual data to re-interpret as bytes. May be empty.
     *
     * @return A byte-span referencing the same memory as @p sv (possibly empty).
     *
     * @note This function performs no validation or transcoding; it is a shallow view
     *       conversion only. For structured data, prefer explicit serialization.
     *
     * @since 1.0
     *
     * @see write(std::span<const std::byte>), write(std::string_view),
     *      writePrefixed<T>(std::string_view), writePrefixed<T>(std::span<const std::byte>),
     *      sendPrefixedConnected<T>(std::span<const std::byte>),
     *      sendPrefixedUnconnected<T>(std::string_view, Port, std::span<const std::byte>)
     *
     * @code
     * // Treat a string payload as raw bytes for binary-oriented send paths:
     * std::string payload = "hello";
     * auto bytes = DatagramSocket::asBytes(std::string_view{payload});
     * sock.write(bytes); // sends as a single UDP datagram
     * @endcode
     */
    static std::span<const std::byte> asBytes(const std::string_view sv) noexcept
    {
        return {reinterpret_cast<const std::byte*>(sv.data()), sv.size()};
    }

    /**
     * @brief Encode a length value into a fixed-size, big-endian (network-order) byte array.
     * @ingroup udp
     *
     * @tparam T
     *   Unsigned integral type used for the length prefix. Typical choices are
     *   `std::uint8_t`, `std::uint16_t`, `std::uint32_t`, or `std::uint64_t`.
     *   A compile-time check enforces that `T` is an unsigned integral.
     *
     * @details
     * Converts @p n into its big-endian (network byte order) representation using exactly
     * `sizeof(T)` bytes and returns the result as `std::array<std::byte, sizeof(T)>`.
     * The function is O(`sizeof(T)`) and allocation-free.
     *
     * - When `sizeof(T) == 1`, the result is a single byte: `n & 0xFF`.
     * - For larger `T`, the most significant byte appears first in the array.
     * - Only the low `sizeof(T) * 8` bits of @p n are representable; values that do not
     *   fit are rejected with a logical exception (see Throws).
     *
     * This helper is intended for building length-prefixed UDP frames with a prefix followed
     * by payload bytes, where the prefix must be in network byte order for interoperability.
     *
     * @param[in] n  The length value to encode. Must satisfy `n <= std::numeric_limits<T>::max()`.
     *
     * @return A `std::array<std::byte, sizeof(T)>` containing @p n encoded in big-endian order.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):** if @p n exceeds `std::numeric_limits<T>::max()`.
     *
     * @since 1.0
     *
     * @see writePrefixed<T>(std::string_view),
     *      writePrefixed<T>(std::span<const std::byte>),
     *      writePrefixedTo<T>(std::string_view, Port, std::string_view),
     *      writePrefixedTo<T>(std::string_view, Port, std::span<const std::byte>)
     *
     * @code
     * // Example: encode a 16-bit length (42) as network-order bytes { 0x00, 0x2A }.
     * auto be = DatagramSocket::encodeLengthPrefixBE<std::uint16_t>(42);
     * assert(static_cast<unsigned>(be[0]) == 0x00);
     * assert(static_cast<unsigned>(be[1]) == 0x2A);
     * @endcode
     */
    template <typename T> static std::array<std::byte, sizeof(T)> encodeLengthPrefixBE(std::size_t n)
    {
        static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>,
                      "T must be an unsigned integral type for the length prefix.");
        if (n > static_cast<std::size_t>((std::numeric_limits<T>::max)()))
            throw SocketException("writePrefixed<T>(): payload too large for prefix type T.");
        std::array<std::byte, sizeof(T)> out{};
        T v = static_cast<T>(n);
        for (std::size_t i = 0; i < sizeof(T); ++i)
        {
            out[sizeof(T) - 1 - i] = static_cast<std::byte>(v & static_cast<T>(0xFF));
            v = static_cast<T>(v >> 8);
        }
        return out;
    }

    /**
     * @brief Build and send a length-prefixed UDP datagram to the connected peer (no pre-wait).
     * @ingroup udp
     *
     * @tparam T
     *   Unsigned integral type used for the length prefix (e.g., `std::uint8_t`, `std::uint16_t`,
     *   `std::uint32_t`, `std::uint64_t`). A compile-time check enforces that `T` is an
     *   unsigned integral.
     *
     * @details
     * Constructs a single datagram with layout **[ prefix(T, big-endian) | payload ]** where
     * the prefix encodes `payload.size()` in **network byte order**. The method:
     *  1) verifies the socket is **open** and **connected**;
     *  2) encodes the prefix via `encodeLengthPrefixBE<T>(payload.size())`;
     *  3) computes `total = sizeof(T) + payload.size()` and enforces the connected peer’s
     *     protocol maxima via `enforceSendCapConnected(total)` (prevents guaranteed `EMSGSIZE`);
     *  4) coalesces `[prefix|payload]` into one contiguous buffer and performs **one** send.
     *
     * This function does **not** poll for writability. On non-blocking sockets, if the send
     * buffer is temporarily full, the underlying send may fail (e.g., `EWOULDBLOCK` /
     * `WSAEWOULDBLOCK`) and will be surfaced as a `SocketException`. For blocking behavior,
     * prefer `writeAll()` or `writeWithTimeout()`.
     *
     * **Atomicity:** UDP is message-oriented; on success, the *entire* `[prefix|payload]`
     * frame is sent in a single datagram. On exception, no bytes are considered transmitted.
     *
     * Zero-length payloads are valid: a datagram containing only the prefix is sent.
     *
     * @param[in] payload  Binary bytes to append after the length prefix (may be empty).
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     * @pre `isConnected() == true`
     *
     * @post On success, exactly `sizeof(T) + payload.size()` bytes are handed to the kernel
     *       as one UDP datagram. Socket state and options are unchanged.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):**
     *     - Socket is not open or not connected.
     *     - `payload.size()` exceeds `std::numeric_limits<T>::max()` (from `encodeLengthPrefixBE<T>()`).
     *     - Total frame exceeds the permitted maximum for the connected peer’s family
     *       (from `enforceSendCapConnected()`).
     *   - **System (OS error + `SocketErrorMessage(...)`):**
     *     - Send failures such as `EWOULDBLOCK`, `ENOBUFS`, `ENETUNREACH`, `EHOSTUNREACH`, etc.
     *
     * @since 1.0
     *
     * @see writePrefixed<T>(std::string_view),
     *      writePrefixed<T>(std::span<const std::byte>),
     *      writeWithTimeout(std::string_view, int),
     *      writeAll(std::string_view),
     *      encodeLengthPrefixBE<T>(std::size_t),
     *      enforceSendCapConnected(std::size_t)
     *
     * @code
     * // Example: send a length-prefixed frame with a 16-bit prefix
     * std::string_view text = "hello";
     * sock.sendPrefixedConnected<std::uint16_t>(DatagramSocket::asBytes(text));
     * @endcode
     */
    template <typename T> void sendPrefixedConnected(const std::span<const std::byte> payload) const
    {
        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::sendPrefixedConnected<T>(): socket is not open.");
        if (!isConnected())
            throw SocketException(
                "DatagramSocket::sendPrefixedConnected<T>(): socket is not connected. Use write*To().");

        const std::size_t n = payload.size();
        const auto prefix = encodeLengthPrefixBE<T>(n);
        const std::size_t total = sizeof(T) + n;

        enforceSendCapConnected(total);

        // Coalesce [prefix | payload] into one datagram and send once.
        std::vector<std::byte> datagram(total);
        std::memcpy(datagram.data(), prefix.data(), sizeof(T));
        if (n != 0)
        {
            std::memcpy(datagram.data() + sizeof(T), payload.data(), n);
        }
        internal::sendExact(getSocketFd(), datagram.data(), datagram.size());
    }

    /**
     * @brief Build and send a length-prefixed UDP datagram to (host, port) on the unconnected path.
     * @ingroup udp
     *
     * @tparam T
     *   Unsigned integral type for the length prefix (e.g., `std::uint8_t`, `std::uint16_t`,
     *   `std::uint32_t`, `std::uint64_t`). A compile-time check enforces that `T` is unsigned integral.
     *
     * @details
     * Constructs a single frame **[ prefix(T, big-endian) | payload ]**, where the prefix encodes
     * `payload.size()` in **network byte order**, then transmits it to the specified destination
     * using `sendUnconnectedTo()`. That helper resolves A/AAAA records, **skips** address families
     * whose theoretical UDP maximum cannot carry the frame, attempts a single send to the first
     * compatible candidate, and caches the last destination (without marking the socket connected).
     *
     * Processing steps:
     *  1) Verify socket is **open**.
     *  2) Encode prefix via `encodeLengthPrefixBE<T>(payload.size())` (validates that size fits in `T`).
     *  3) Coalesce `[prefix|payload]` once and dispatch through `sendUnconnectedTo(host, port, ...)`.
     *
     * Zero-length payloads are valid: a datagram containing only the prefix is sent.
     *
     * **Atomicity:** UDP is message-oriented; on success the entire `[prefix|payload]` frame is sent
     * in one datagram. On exception, no bytes are considered transmitted.
     *
     * @param[in] host     Destination hostname or numeric address (IPv4/IPv6).
     * @param[in] port     Destination UDP port (> 0).
     * @param[in] payload  Binary bytes to append after the length prefix (may be empty).
     *
     * @pre `getSocketFd() != INVALID_SOCKET`
     *
     * @post On success, exactly `sizeof(T) + payload.size()` bytes are handed to the kernel as one datagram.
     *
     * @throws SocketException
     *   - **Logical (error code = 0):**
     *     - Socket is not open.
     *     - `payload.size()` exceeds `std::numeric_limits<T>::max()` (from `encodeLengthPrefixBE<T>()`).
     *     - No address family can carry the frame size (surfaced by `sendUnconnectedTo()`).
     *   - **System (OS error + `SocketErrorMessage(...)`):**
     *     - Resolution or send failures reported by the OS (e.g., `ENETUNREACH`, `EHOSTUNREACH`, `ENOBUFS`).
     *
     * @note Family-specific size enforcement (IPv4 vs IPv6) and destination caching are handled inside
     *       `sendUnconnectedTo()`. This method does not pre-wait for writability; if you need blocking
     *       behavior, perform an explicit readiness wait on a connected socket or design a higher-level retry.
     *
     * @since 1.0
     *
     * @see writePrefixedTo<T>(std::string_view, Port, std::string_view),
     *      writePrefixed<T>(std::span<const std::byte>),
     *      encodeLengthPrefixBE<T>(std::size_t),
     *      sendUnconnectedTo(std::string_view, Port, const void*, std::size_t),
     *      MaxUdpPayloadIPv4, MaxUdpPayloadIPv6
     *
     * @code
     * // Example: send a length-prefixed binary frame to a host/port using a 16-bit prefix.
     * std::array<std::byte, 3> data{std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}};
     * sock.sendPrefixedUnconnected<std::uint16_t>("239.0.0.1", 5000, std::span<const std::byte>{data});
     * @endcode
     */
    template <typename T>
    void sendPrefixedUnconnected(const std::string_view host, const Port port,
                                 const std::span<const std::byte> payload) const
    {
        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::sendPrefixedUnconnected<T>(): socket is not open.");

        const std::size_t n = payload.size();
        const auto prefix = encodeLengthPrefixBE<T>(n);
        const std::size_t total = sizeof(T) + n;

        // Build once; family skipping & send are handled inside sendUnconnectedTo().
        std::vector<std::byte> datagram(total);
        std::memcpy(datagram.data(), prefix.data(), sizeof(T));
        if (n != 0)
        {
            std::memcpy(datagram.data() + sizeof(T), payload.data(), n);
        }
        sendUnconnectedTo(host, port, datagram.data(), datagram.size());
    }

  private:
    mutable sockaddr_storage
        _remoteAddr{}; ///< Storage for the address of the most recent sender (used in unconnected mode).
    mutable socklen_t _remoteAddrLen =
        0;                         ///< Length of the valid address data in `_remoteAddr` (0 if none received yet).
    sockaddr_storage _localAddr{}; ///< Local address structure.
    mutable socklen_t _localAddrLen = 0; ///< Length of local address.
    std::vector<char> _internalBuffer;   ///< Internal buffer for read operations.
    Port _port;                          ///< Port number the socket is bound to (if applicable).
    bool _isBound = false;               ///< True if the socket is bound to an address
    bool _isConnected = false;           ///< True if the socket is connected to a remote host.
};

} // namespace jsocketpp
