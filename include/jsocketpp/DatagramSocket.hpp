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
 * pkt.resize(4096); // provision capacity
 *
 * // Default (no preflight): fastest path; may truncate if a datagram > 4096 arrives
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
     * sock.bind(); // Binds to all interfaces on an ephemeral port (e.g., 0.0.0.0:49512)
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
     * @brief Writes a trivially copyable object of type `T` to the connected remote peer.
     * @ingroup udp
     *
     * This method serializes a fixed-size object of type `T` into raw binary form and sends it
     * as a datagram to the socket's connected peer. It is intended for use with POD structures,
     * protocol headers, or compact binary messages.
     *
     * ---
     *
     * ### ⚙️ Core Behavior
     * - Uses `std::bit_cast` to convert `T` into raw bytes
     * - Sends exactly `sizeof(T)` bytes in a single datagram
     * - No padding removal, field conversion, or alignment adjustment is performed
     * - No retries: failure to send will throw immediately
     *
     * ---
     *
     * ### 📋 Requirements
     * - The socket must be **connected** via `connect()`
     * - Type `T` must satisfy both:
     *   - `std::is_trivially_copyable_v<T> == true`
     *   - `std::is_standard_layout_v<T> == true`
     *
     * ---
     *
     * ### 🧪 Example
     * @code
     * struct Packet {
     *     uint32_t type;
     *     uint16_t length;
     * };
     *
     * DatagramSocket sock(12345);
     * sock.connect("192.168.1.100", 9000);
     *
     * Packet p{1, 64};
     * sock.write(p); // sends binary packet
     * @endcode
     *
     * ---
     *
     * @tparam T A trivially copyable type with standard layout
     * @param[in] value The object to send. Copied by value.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - The socket is not connected
     * - `send()` fails (e.g., unreachable, interrupted, closed)
     *
     * @warning **Byte Order**: No endianness conversion is performed. Use `jsocketpp::net::fromNetwork()`
     *          and `toNetwork()` helpers to safely convert integers between host and network byte order.
     * @warning **Padding**: All bytes, including padding, are transmitted. Avoid structs with padding
     *          unless explicitly managed.
     * @warning **Size**: This method does not fragment. Objects larger than the MTU may be dropped
     *          by the network.
     *
     * @see read<T>() For receiving structured objects
     * @see writeTo() For unconnected datagram transmission
     * @see connect() To establish peer before writing
     * @since 1.0
     */
    template <typename T> void write(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "DatagramSocket::write<T>() requires trivially copyable type");
        static_assert(std::is_standard_layout_v<T>, "DatagramSocket::write<T>() requires standard layout type");

        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::write<T>(): socket is not open.");

        if (!_isConnected)
            throw SocketException("DatagramSocket::write<T>(): socket is not connected.");

        const auto buffer = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
        internal::sendExact(getSocketFd(), buffer.data(), buffer.size());
    }

    /**
     * @brief Sends a trivially copyable object of type `T` as a UDP datagram to the specified destination.
     * @ingroup udp
     *
     * This method serializes a POD object into raw binary form and transmits it using `sendto()` to a
     * remote IP address and port. It works independently of whether the socket is connected and
     * performs its own address resolution per call.
     *
     * ---
     *
     * ### 📦 Serialization Constraints
     * - The type `T` must be:
     *   - `std::is_trivially_copyable_v<T> == true`
     *   - `std::is_standard_layout_v<T> == true`
     * - Any internal padding in `T` is preserved and transmitted without modification
     * - No endianness conversion is performed — you must normalize fields manually
     *
     * ---
     *
     * ### 🌐 Address Resolution
     * - Performs per-call resolution of the `host` and `port` using `getaddrinfo()`
     * - Iterates through all resolved addresses until one `sendto()` call successfully transmits the full object
     *
     * ---
     *
     * ### ⚙️ Behavior
     * - `value` is bit-cast into a `std::array<std::byte, sizeof(T)>`
     * - The buffer is transmitted via `sendto()` to the first address that accepts the full datagram
     * - If the socket is unconnected, the internal `_remoteAddr` and `_remoteAddrLen` are updated
     *   to reflect the last destination address, enabling `getRemoteIp()` and `getRemotePort()`
     * - The socket’s connected state is not changed by this operation
     *
     * ---
     *
     * ### 🧪 Example
     * @code
     * struct Packet {
     *     std::uint32_t seq;
     *     std::uint16_t flags;
     * };
     *
     * jsocketpp::DatagramSocket udp(0); // unbound UDP socket
     * Packet p{42, 0x01};
     * udp.writeTo("192.168.1.10", 5555, p); // Send to peer
     * std::cout << udp.getRemoteSocketAddress() << std::endl; // "192.168.1.10:5555"
     * @endcode
     *
     * ---
     *
     * @tparam T A trivially copyable, standard layout POD type
     * @param[in] host Remote hostname or IP address to send to
     * @param[in] port Remote UDP port number
     * @param[in] value Object of type `T` to be serialized and transmitted
     *
     * @throws SocketException If:
     * - The socket is not open
     * - Address resolution fails
     * - No destination accepts the datagram
     * - `sendto()` fails with a system error
     *
     * @warning This method does not perform fragmentation or retries. Large objects may exceed MTU.
     * @warning No byte-order normalization is performed. Use `jsocketpp::net::toNetwork()` helpers for integers.
     *
     * @note When used on an unconnected socket, this method updates the internal remote address state for use
     *       with `getRemoteIp()` and `getRemotePort()`.
     *
     * @see write<T>() For connected send
     * @see connect() To establish a default peer
     * @see read<T>() For receiving structured objects
     * @see receiveFrom() For receiving from arbitrary peers
     * @see getRemoteIp(), getRemotePort(), getRemoteSocketAddress()
     */
    template <typename T> void writeTo(const std::string_view host, const Port port, const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "DatagramSocket::writeTo<T>() requires trivially copyable type");
        static_assert(std::is_standard_layout_v<T>, "DatagramSocket::writeTo<T>() requires standard layout type");

        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::writeTo<T>(): socket is not open.");

        const auto addrInfo = internal::resolveAddress(host, port, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP);

        const auto buffer = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);

        int lastError = 0;
        for (const addrinfo* ai = addrInfo.get(); ai != nullptr; ai = ai->ai_next)
        {
            try
            {
                internal::sendExactTo(getSocketFd(), buffer.data(), buffer.size(), ai->ai_addr,
                                      static_cast<socklen_t>(ai->ai_addrlen));

                if (!_isConnected)
                {
                    _remoteAddr = *reinterpret_cast<const sockaddr_storage*>(ai->ai_addr);
                    _remoteAddrLen = static_cast<socklen_t>(ai->ai_addrlen);
                }
                return; // success
            }
            catch (const SocketException&)
            {
                // Preserve the last OS error to report if none of the candidates succeed.
                lastError = GetSocketError();
                // Try next addrinfo candidate.
            }
        }

        // If we got here, all candidates failed.
        throw SocketException(lastError, SocketErrorMessageWrap(lastError));
    }

    /**
     * @brief Sends a UDP datagram using the provided DatagramPacket.
     * @ingroup udp
     *
     * Sends the contents of a `DatagramPacket` either to a specified destination or
     * to the connected peer, depending on the packet's fields:
     *
     * - **Explicit destination:** If `packet.address` is non-empty and `packet.port`
     *   is non-zero, the address is resolved via @ref internal::resolveAddress and
     *   the payload is sent using @ref internal::sendExactTo. If the socket is not
     *   connected, the resolved destination is stored internally for future
     *   `getRemoteIp()` and `getRemotePort()` calls.
     *
     * - **Connected mode:** If the packet has no destination, the socket must be
     *   connected. The payload is sent to the connected peer using
     *   @ref internal::sendExact.
     *
     * ---
     *
     * @param[in] packet The packet containing destination and payload. If the buffer
     *                   is empty, this method does nothing.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - No destination is specified and the socket is not connected
     * - Address resolution fails
     * - The underlying send operation fails or reports a partial datagram
     *
     * @note UDP datagrams are sent atomically. If the payload exceeds the path MTU,
     *       it may be dropped or truncated by the network.
     * @note This method does not fragment or retransmit. Use application-level
     *       framing for large data.
     *
     * @see connect(), isConnected(), write(std::string_view), writeTo(), DatagramPacket,
     *      getRemoteIp(), getRemotePort()
     */
    void write(const DatagramPacket& packet) const;

    /**
     * @brief Sends a string message as a UDP datagram to the connected peer.
     * @ingroup udp
     *
     * Transmits the given `std::string_view` as a single UDP datagram using the
     * socket's connected peer, which must have been previously set via @ref connect().
     * Guarantees that either the full message is sent or an exception is thrown.
     *
     * @param[in] message The message payload to send. May be empty (in which case this
     *                    function does nothing).
     *
     * @throws SocketException If:
     * - The socket is not open
     * - The socket is not connected
     * - A system-level `send()` error occurs
     * - A partial datagram is sent (unexpected)
     *
     * @warning This method does not fragment large payloads. If `message.size() > MTU`,
     * the datagram may be dropped or truncated by the network.
     *
     * @see connect(), writeTo(), write(DatagramPacket), write<T>(), write<std::string>()
     */
    void write(std::string_view message) const;

    /**
     * @brief Sends a message as a UDP datagram to the specified destination address and port.
     * @ingroup udp
     *
     * Transmits the given `std::string_view` as a single UDP datagram to a specific host and port,
     * without requiring the socket to be connected.
     *
     * Internally, this method:
     * - Resolves the destination via @ref internal::resolveAddress (IPv4, IPv6, DNS supported).
     * - Iterates through all resolved addresses, attempting to send the full payload to each
     *   using @ref internal::sendExactTo.
     * - On the first successful send, updates the internal `_remoteAddr` and `_remoteAddrLen`
     *   if the socket is unconnected, enabling later use of `getRemoteIp()` and `getRemotePort()`.
     * - Throws if all address candidates fail.
     *
     * @param[in] message The UDP payload to send. If empty, this method is a no-op.
     * @param[in] host    Destination hostname or IP address (IPv4, IPv6, or DNS).
     * @param[in] port    Destination UDP port number.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - Address resolution fails
     * - All resolved destinations fail to accept the datagram
     * - The underlying send operation fails or reports a partial datagram
     *
     * @note No fragmentation or retries are performed — the payload is sent as a single datagram.
     *       If `message.size()` exceeds the path MTU, it may be dropped or truncated by the network.
     *
     * @warning No byte-order or encoding transformations are applied; the payload is transmitted as raw bytes.
     *
     * @see write(const DatagramPacket&) For sending a datagram object with buffer + destination
     * @see writeTo() For sending binary POD data to a per-call destination
     * @see connect() To establish a persistent default peer
     * @see isConnected(), getRemoteIp(), getRemotePort(), getRemoteSocketAddress()
     */
    void write(std::string_view message, std::string_view host, Port port) const;

    /**
     * @brief Receive a single UDP datagram into a @ref DatagramPacket with full control and telemetry.
     * @ingroup udp
     *
     * @details
     * This high-level method wraps the low-level @ref readIntoBuffer() backbone to provide
     * an easy yet powerful interface for receiving datagrams into a @ref DatagramPacket.
     * It applies the caller-specified policy in @p opts to determine preflight behavior,
     * buffer growth/shrink decisions, additional `recv()` flags, and side effects such as
     * persisting the sender as the "last remote" or resolving numeric host/port fields.
     *
     * **Core behavior**
     * - Always consumes exactly one UDP datagram from the socket's receive queue.
     * - Honors the capacity of @p packet.buffer but may grow/shrink it according to @p opts.
     * - Uses @ref DatagramReceiveMode from @p opts.mode to control whether and how the
     *   next datagram's size is probed before the receive call.
     * - If the socket is unconnected, captures the sender address, optionally persists it
     *   in the socket, and resolves numeric host/port fields in @p packet.
     * - Detects truncation when the datagram is larger than the available capacity.
     * - Returns a @ref DatagramReadResult with full telemetry: actual bytes received,
     *   probed full datagram size (if known), truncation flag, and source address info.
     *
     * **Thread safety:** This method is not inherently thread-safe. External synchronization
     * is required if multiple threads may access the same @ref DatagramSocket concurrently.
     *
     * **Performance note:** Modes that preflight the datagram size may incur an additional
     * syscall compared to `NoPreflight`, trading throughput for accuracy in buffer sizing.
     *
     * @param[in,out] packet Destination datagram container. Its @c buffer provides capacity;
     *        on return, its @c address and @c port fields are updated if requested in @p opts.
     * @param[in] opts Options controlling receive policy, buffer management, and side effects.
     *        Defaults to all-safe behavior with no preflight, growth allowed, and sender
     *        persistence enabled.
     *
     * @return A @ref DatagramReadResult struct containing:
     *         - @c bytes: actual bytes copied into @p packet.buffer
     *         - @c datagramSize: probed full datagram size (0 if unknown)
     *         - @c truncated: true if payload was cut off due to limited capacity
     *         - @c src / @c srcLen: sender address and length (when captured)
     *
     * @throws SocketException
     *         If the socket is not open, @p packet has zero capacity with growth disallowed,
     *         or a non-timeout socket error occurs during the receive.
     * @throws SocketTimeoutException
     *         If the receive times out (SO_RCVTIMEO) or in non-blocking mode when no data is available.
     *
     * @since 1.0
     */
    DatagramReadResult read(DatagramPacket& packet, const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Receive a single UDP datagram into a fixed-size caller-provided buffer with full control and telemetry.
     * @ingroup udp
     *
     * @details
     * This method is the raw-buffer counterpart to @ref read(DatagramPacket&, const DatagramReadOptions&).
     * It reads exactly one UDP datagram from the socket into a memory region provided by the caller,
     * never allocating or resizing storage. All sizing and truncation control is handled by the
     * parameters in @p opts, but unlike @ref DatagramPacket, raw buffers cannot be grown automatically.
     *
     * **Core behavior**
     * - Always consumes exactly one datagram from the socket's receive queue.
     * - Honors @p len as a strict capacity limit; never writes beyond @p buffer.
     * - Uses the preflight policy in @p opts.mode to optionally probe the next datagram's size
     *   before the receive call, clamping the request to `min(probedSize, MaxDatagramPayloadSafe, len)`.
     * - If the socket is unconnected, can capture the sender address, optionally update it as
     *   the socket's "last remote", and return it in the result.
     * - Detects when the datagram payload is larger than @p len and reports truncation in the result.
     * - Returns a @ref DatagramReadResult with telemetry including actual bytes received, probed size,
     *   truncation flag, and sender address (if captured).
     *
     * **Thread safety:** This method is not inherently thread-safe. Use external synchronization
     * if multiple threads access the same @ref DatagramSocket concurrently.
     *
     * **Performance note:** When @ref DatagramReceiveMode in @p opts requests preflight sizing,
     * an extra syscall (e.g., `FIONREAD` or `MSG_PEEK|MSG_TRUNC`) may occur before the actual receive.
     *
     * @param[out] buffer Pointer to writable memory with at least @p len bytes capacity. Must be non-null
     *        if @p len > 0.
     * @param[in] len Capacity of @p buffer in bytes. Acts as a hard cap on how many bytes can be written.
     * @param[in] opts Receive options controlling preflight behavior, recv() flags, and whether to persist
     *        or resolve the sender address. Defaults to no preflight, no extra recv flags, and last-remote updates
     *        enabled.
     *
     * @return A @ref DatagramReadResult struct containing:
     *         - @c bytes: actual bytes copied into @p buffer
     *         - @c datagramSize: probed full datagram size (0 if unknown)
     *         - @c truncated: true if datagram exceeded @p len
     *         - @c src / @c srcLen: sender address and length (when captured for unconnected sockets)
     *
     * @throws SocketException
     *         If the socket is not open, arguments are invalid, or a non-timeout socket error occurs.
     * @throws SocketTimeoutException
     *         If the receive times out (SO_RCVTIMEO) or, in non-blocking mode, no data is available.
     *
     * @note This method never resizes memory; truncation is possible if @p len is smaller than the incoming datagram.
     *
     * @since 1.0
     */
    DatagramReadResult readInto(void* buffer, std::size_t len, const DatagramReadOptions& opts = {}) const;

    /**
     * @brief Receive a single UDP datagram into a caller-provided container type.
     * @ingroup udp
     *
     * @tparam T The container type to receive into.
     *           - **Dynamic containers** (e.g., `std::string`, `std::vector<char>`,
     *             `std::vector<std::byte>`): must satisfy `detail::is_dynamic_buffer_v<T>`.
     *           - **Fixed-size containers** (e.g., `std::array<char, N>`,
     *             `std::array<std::byte, N>`): must satisfy `detail::is_fixed_buffer_v<T>`.
     *
     * @param[in] opts Datagram read options controlling preflight sizing, timeouts, source
     *        address capture, and low-level receive flags.
     * @param[in] minCapacity (Dynamic containers only) Minimum initial capacity to reserve before reading.
     *        Defaults to `DefaultDatagramReceiveSize`.
     *        - If zero, defaults to `1` to allow reception of zero-length datagrams.
     *        - The capacity is clamped to `MaxDatagramPayloadSafe` and may be increased
     *          by a preflight probe when `opts.mode` requests it.
     *
     * @return
     * - For **dynamic containers**: a fully-sized container of type `T` containing exactly
     *   the bytes received. The container is resized down to match the actual datagram length
     *   and is binary-safe (no null terminator is added to `std::string`).
     * - For **fixed-size containers**: a value-initialized container of type `T` containing
     *   up to `T::size()` bytes. Remaining bytes are zero-initialized if the datagram is smaller.
     *
     * @throws SocketException
     *         If the socket is not open or a non-timeout receive error occurs.
     * @throws SocketTimeoutException
     *         If the receive times out (SO_RCVTIMEO) or, in non-blocking mode, no data is available.
     *
     * @details
     * This method is a generic, type-safe wrapper over @ref readInto(void*,size_t,const DatagramReadOptions&),
     * ensuring unified behavior for all receive operations:
     * - All I/O, error handling, timeouts, and truncation detection are handled by the same
     *   underlying backbone, `readIntoBuffer()`.
     * - For dynamic containers, the capacity is chosen from `minCapacity` and optionally
     *   refined using `internal::nextDatagramSize()` when preflight is requested.
     * - For fixed-size containers, the compile-time size is respected; no resizing occurs.
     * - Truncation detection is available through @ref readInto when you need telemetry
     *   such as full datagram size and sender address.
     *
     * ### Example — dynamic container (`std::string`)
     * @code
     * jsocketpp::DatagramSocket sock(12345);
     * sock.bind();
     *
     * std::string msg = sock.read<std::string>(
     *     { .mode = jsocketpp::DatagramReceiveMode::PreflightSize }
     * );
     *
     * std::cout << "Received " << msg.size() << " bytes\n";
     * @endcode
     *
     * ### Example — fixed-size container (`std::array<char, 512>`)
     * @code
     * auto buf = sock.read<std::array<char, 512>>(
     *     { .mode = jsocketpp::DatagramReceiveMode::PreflightMax }
     * );
     * @endcode
     *
     * @note
     * - This overload discards sender address/port information; use
     *   @ref read(DatagramPacket&,const DatagramReadOptions&) or
     *   @ref readInto(void*,size_t,const DatagramReadOptions&) if you need it.
     * - For large datagrams, ensure your container capacity is sufficient to avoid truncation.
     *
     * @since 1.0
     */
    template <typename T, std::enable_if_t<detail::is_dynamic_buffer_v<T>, int> = 0>
    [[nodiscard]] T read(const DatagramReadOptions& opts = {},
                         std::size_t minCapacity = DefaultDatagramReceiveSize) const
    {
        if (minCapacity == 0)
            minCapacity = 1; // ensure we can receive zero-length datagrams cleanly

        std::size_t capacity = (std::min) (minCapacity, MaxDatagramPayloadSafe);

        // Optional preflight probe to choose a better starting capacity
        if (opts.mode != DatagramReceiveMode::NoPreflight)
        {
            try
            {
                if (const std::size_t exact = internal::nextDatagramSize(getSocketFd()); exact > 0)
                {
                    const std::size_t clamped = (std::min) (exact, MaxDatagramPayloadSafe);
                    capacity = (std::max) (capacity, clamped);
                }
            }
            catch (const SocketException&)
            {
                // Graceful degradation: keep current capacity
            }
        }

        T out;
        out.resize(capacity);

        // Delegate to the backbone — ensures consistent error/timeout/truncation handling
        // Shrink to actual bytes received (never null-terminate automatically)
        if (auto res = readInto(static_cast<void*>(out.data()), out.size(), opts); res.bytes < out.size())
            out.resize(res.bytes);

        return out;
    }

    /**
     * @copydoc read(const DatagramReadOptions&,std::size_t) const
     */
    template <typename T, std::enable_if_t<detail::is_fixed_buffer_v<T>, int> = 0>
    [[nodiscard]] T read(const DatagramReadOptions& opts = {}) const
    {
        T out{}; // value-init to ensure predictable padding if datagram is shorter
        (void) readInto(static_cast<void*>(out.data()), out.size(), opts);
        return out;
    }

    /**
     * @brief Receive a single UDP datagram into a container and capture the sender's address/port.
     * @ingroup udp
     *
     * @tparam T The container type to receive into.
     *           - **Dynamic containers**: e.g., `std::string`, `std::vector<char>`,
     *             `std::vector<std::byte>` (must satisfy `detail::is_dynamic_buffer_v<T>`).
     *           - **Fixed-size containers**: e.g., `std::array<char, N>`,
     *             `std::array<std::byte, N>` (must satisfy `detail::is_fixed_buffer_v<T>`).
     *
     * @param[out] buffer Destination container for the received datagram. For dynamic containers,
     *        it is resized to exactly the number of bytes received. For fixed-size containers,
     *        up to `T::size()` bytes are written and any remaining bytes are left value-initialized.
     * @param[out] senderAddr Optional pointer to a `std::string` to receive the sender's numeric IP address.
     *        Only set if provided and the socket is unconnected.
     * @param[out] senderPort Optional pointer to a `Port` to receive the sender's port number.
     *        Only set if provided and the socket is unconnected.
     * @param[in] opts Datagram read options controlling preflight sizing, timeouts,
     *        low-level receive flags, and whether to update the socket's last remote.
     *        Defaults to safe, no-preflight behavior with last-remote updates enabled.
     *
     * @return A @ref DatagramReadResult containing:
     *         - `bytes`: actual bytes received
     *         - `datagramSize`: probed datagram size if available
     *         - `truncated`: whether the payload was cut off
     *         - `src` / `srcLen`: raw sender address info
     *
     * @throws SocketException
     *         If the socket is not open, arguments are invalid, or a non-timeout error occurs.
     * @throws SocketTimeoutException
     *         If the receive times out or, in non-blocking mode, no data is available.
     *
     * @details
     * This is a convenience overload of @ref readInto(void*,size_t,const DatagramReadOptions&)
     * that also captures the sender's numeric address and port without requiring a @ref DatagramPacket.
     *
     * - Delegates all socket I/O to @ref readInto(void*,size_t,const DatagramReadOptions&).
     * - Uses the same preflight/timeout/error handling as other `read*` methods.
     * - Does not allocate for fixed-size containers; dynamic containers are resized once before
     *   and once after the receive.
     *
     * ### Example — dynamic container
     * @code
     * std::string senderIp;
     * Port senderPort;
     * auto payload = sock.readFrom<std::vector<char>>(&senderIp, &senderPort);
     * @endcode
     *
     * ### Example — fixed-size container
     * @code
     * std::array<char, 1024> buf{};
     * std::string sender;
     * Port port;
     * sock.readFrom(buf, &sender, &port);
     * @endcode
     *
     * @note
     * - If the socket is connected, `senderAddr` and `senderPort` are not populated.
     * - If you only need the payload and not the sender, use @ref read instead.
     *
     * @since 1.0
     */
    template <typename T>
    DatagramReadResult readFrom(T& buffer, std::string* senderAddr, Port* senderPort,
                                const DatagramReadOptions& opts) const
    {
        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::readFrom(): socket is not open.");

        DatagramReadResult result{};

        if constexpr (detail::is_dynamic_buffer_v<T>)
        {
            if (buffer.empty())
                buffer.resize((std::min) (DefaultDatagramReceiveSize, MaxDatagramPayloadSafe));

            result = readInto(static_cast<void*>(buffer.data()), buffer.size(), opts);

            // Shrink to actual received size
            if (result.bytes < buffer.size())
                buffer.resize(result.bytes);
        }
        else if constexpr (detail::is_fixed_buffer_v<T>)
        {
            result = readInto(static_cast<void*>(buffer.data()), buffer.size(), opts);
        }
        else
        {
            static_assert(detail::is_dynamic_buffer_v<T> || detail::is_fixed_buffer_v<T>,
                          "T must be a supported dynamic or fixed-size byte container");
        }

        // Populate sender info if requested and available
        if (!_isConnected && (senderAddr || senderPort))
        {
            const sockaddr_storage tmpSrc = result.src;
            const socklen_t tmpLen = result.srcLen;
            std::string addrStr;
            Port portNum;
            internal::resolveNumericHostPort(reinterpret_cast<const sockaddr*>(&tmpSrc), tmpLen, addrStr, portNum);

            if (senderAddr)
                *senderAddr = std::move(addrStr);
            if (senderPort)
                *senderPort = portNum;
        }

        return result;
    }

    /**
     * @brief Receive exactly @p exactLen bytes (per policy) into a caller-provided buffer.
     * @ingroup udp
     *
     * This method provides fine-grained control over datagram size matching and reception behavior.
     * It implements the exact-size policy specified in @ref ReadExactOptions for matching datagram
     * size against the caller's requirements, including zero-padding for undersized datagrams
     * and error handling for oversized ones.
     *
     * ---
     *
     * ### 📦 Core Behaviors
     * - Enforces datagram size requirements based on @p opts.requireExact:
     *   - When true, the datagram **must** be exactly @p exactLen bytes
     *   - When false, size handling is controlled by @p opts.padIfSmaller and @p opts.errorOnTruncate
     * - Optionally zero-pads undersized datagrams (@p opts.padIfSmaller)
     * - Controls error vs truncation for oversized datagrams (@p opts.errorOnTruncate)
     *
     * ---
     *
     * ### 🔍 Size Policy Effects
     * - **Exact match (opts.requireExact = true)**
     *   - Throws if datagram size ≠ exactLen
     * - **Undersized datagram handling**
     *   - If padIfSmaller: fills remaining space with zeros
     *   - If !padIfSmaller: returns actual size
     * - **Oversized datagram handling**
     *   - If errorOnTruncate: throws SocketException
     *   - If !errorOnTruncate: silently truncates to exactLen
     *
     * ---
     *
     * ### 🧪 Example
     * @code
     * DatagramSocket sock(9999);
     * sock.bind();
     *
     * char buf[256];
     * auto result = sock.readExact(buf, 256, {
     *     .requireExact = true,      // must be exactly 256 bytes
     *     .padIfSmaller = false,     // no padding if smaller
     *     .errorOnTruncate = true    // throw if larger
     * });
     * @endcode
     *
     * @param[out] buffer Destination memory (must have at least @p exactLen capacity).
     * @param[in]  exactLen Required byte count to satisfy for this datagram.
     * @param[in]  opts Exact-match policy and base datagram options.
     *
     * @return A @ref DatagramReadResult with telemetry (bytes received, probed size, truncation, sender).
     *
     * @throws SocketTimeoutException On timeout / would-block.
     * @throws SocketException On invalid socket, invalid arguments, or size policy violation
     *                         (when `requireExact == true` and the datagram size differs).
     *
     * @note This method will receive exactly one datagram, regardless of size policy.
     * @note Buffer padding (if enabled) uses secure zero-initialization.
     *
     * @see readExact<T>() Container-based variant
     * @see DatagramReadOptions Base receive options
     * @see ReadExactOptions Size matching policy
     * @since 1.0
     */
    DatagramReadResult readExact(void* buffer, std::size_t exactLen, const ReadExactOptions& opts = {}) const;

    /**
     * @brief Receive exactly @p exactLen bytes (per policy) into a container `T`.
     * @ingroup udp
     *
     * This method provides granular control over datagram size matching and reception into
     * contiguous byte containers. It implements the exact-size policy specified in @ref ReadExactOptions
     * for matching the datagram size against the caller's requirements, including zero-padding for
     * undersized datagrams and error handling for oversized ones.
     *
     * ---
     *
     * ### 📦 Container Requirements
     * @tparam T A contiguous byte container:
     *           - **Dynamic containers** (must satisfy `detail::is_dynamic_buffer_v<T>`):
     *             - `std::string`
     *             - `std::vector<char>`
     *             - `std::vector<std::byte>`
     *           - **Fixed-size containers** (must satisfy `detail::is_fixed_buffer_v<T>`):
     *             - `std::array<char, N>`
     *             - `std::array<std::byte, N>`
     *
     * ---
     *
     * ### ⚙️ Core Behaviors
     * - **Dynamic containers:**
     *   - If `opts.autoResizeDynamic`, resizes to exactly @p exactLen before receive
     *   - Throws if capacity is insufficient and auto-resize is disabled
     *   - Final size matches actual bytes received
     *
     * - **Fixed containers:**
     *   - Must have `T::size() >= exactLen`
     *   - Never resizes
     *   - Zero-fills unused space if datagram is smaller and `opts.padIfSmaller == true`
     *
     * ---
     *
     * ### 🔍 Size Policy Effects
     * - **Exact match** (`opts.requireExact == true`):
     *   - Throws if datagram size ≠ exactLen
     * - **Undersized handling**:
     *   - If padIfSmaller: fills remaining space with zeros
     *   - If !padIfSmaller: returns actual size
     * - **Oversized handling**:
     *   - If errorOnTruncate: throws SocketException
     *   - If !errorOnTruncate: silently truncates to exactLen
     *
     * ---
     *
     * ### 🧪 Examples
     * ```cpp
     * // Dynamic container (std::string)
     * std::string msg;
     * auto result = sock.readExact(msg, 256, {
     *     .requireExact = true,      // must be exactly 256 bytes
     *     .padIfSmaller = false,     // no padding if smaller
     *     .errorOnTruncate = true    // throw if larger
     * });
     *
     * // Fixed container (std::array)
     * std::array<char, 1024> buf;
     * auto result = sock.readExact(buf, 512, {
     *     .requireExact = false,     // allow size mismatch
     *     .padIfSmaller = true,      // zero-pad if smaller
     *     .errorOnTruncate = false   // truncate if larger
     * });
     * ```
     *
     * @param[in,out] buffer Destination container. For dynamic types, capacity must be sufficient
     *                       or `opts.autoResizeDynamic` must be true. For fixed types, `size()`
     *                       must be >= @p exactLen.
     * @param[in] exactLen Required byte count for this datagram.
     * @param[in] opts Exact-match policy and base datagram options controlling size matching,
     *                padding, truncation, and container resizing behavior.
     *
     * @return A @ref DatagramReadResult with telemetry:
     *         - bytes: actual bytes received
     *         - datagramSize: probed size if available (via preflight)
     *         - truncated: whether payload was cut off
     *         - src/srcLen: raw sender info when captured
     *
     * @throws SocketTimeoutException On timeout / would-block.
     * @throws SocketException On invalid socket, invalid arguments, insufficient capacity
     *                        (when dynamic auto-resize is disabled), or size policy violation.
     *
     * @see readExact() Raw buffer variant
     * @see DatagramReadOptions Base receive options
     * @see ReadExactOptions Size matching policy
     * @see detail::is_dynamic_buffer_v
     * @see detail::is_fixed_buffer_v
     * @since 1.0
     */
    template <typename T>
    DatagramReadResult readExact(T& buffer, const std::size_t exactLen, const ReadExactOptions& opts) const
    {
        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::readExact(T&,size_t): socket is not open.");
        if (exactLen == 0)
            throw SocketException("DatagramSocket::readExact(T&,size_t): exactLen must be > 0.");

        DatagramReadOptions base = opts.base;
        if (base.mode == DatagramReceiveMode::NoPreflight)
            base.mode = DatagramReceiveMode::PreflightSize;

        if constexpr (detail::is_dynamic_buffer_v<T>)
        {
            // Ensure capacity matches policy
            if (opts.autoResizeDynamic)
            {
                if (exactLen > MaxDatagramPayloadSafe)
                    throw SocketException(
                        "DatagramSocket::readExact(T&,size_t): exactLen exceeds MaxDatagramPayloadSafe.");
                buffer.resize(exactLen);
            }
            else
            {
                if (buffer.size() < exactLen)
                    throw SocketException(
                        "DatagramSocket::readExact(T&,size_t): buffer too small and autoResizeDynamic=false.");
            }

            auto res = readInto(static_cast<void*>(buffer.data()), (std::min) (buffer.size(), exactLen), base);

            // Apply the same size policy as the raw-buffer version
            const bool knownProbed = (res.datagramSize != 0);
            const std::size_t fullSize = knownProbed ? res.datagramSize : res.bytes;

            if (opts.requireExact)
            {
                if (fullSize < exactLen)
                {
                    if (opts.padIfSmaller)
                    {
                        if (res.bytes < exactLen && buffer.size() >= exactLen)
                        {
                            std::memset(buffer.data() + res.bytes, 0, exactLen - res.bytes);
                        }
                    }
                    else
                    {
                        throwSizeMismatch(exactLen, fullSize, knownProbed);
                    }
                }
                if (fullSize > exactLen)
                {
                    if (opts.errorOnTruncate || !res.truncated)
                        throwSizeMismatch(exactLen, fullSize, knownProbed);
                }
            }
            // Finally, resize dynamic container to exactLen (policyful “exact” surface)
            if (buffer.size() != exactLen)
                buffer.resize(exactLen);

            return res;
        }
        else if constexpr (detail::is_fixed_buffer_v<T>)
        {
            if (buffer.size() < exactLen)
                throw SocketException("DatagramSocket::readExact(T&,size_t): fixed buffer smaller than exactLen.");

            auto res = readInto(static_cast<void*>(buffer.data()), exactLen, base);

            const bool knownProbed = (res.datagramSize != 0);
            const std::size_t fullSize = knownProbed ? res.datagramSize : res.bytes;

            if (opts.requireExact)
            {
                if (fullSize < exactLen)
                {
                    if (!opts.padIfSmaller)
                        throwSizeMismatch(exactLen, fullSize, knownProbed);
                    // else: leave tail zeroed (value-initialized container recommended)
                }
                if (fullSize > exactLen)
                {
                    if (opts.errorOnTruncate || !res.truncated)
                        throwSizeMismatch(exactLen, fullSize, knownProbed);
                }
            }
            else
            {
                if (opts.padIfSmaller && res.bytes < exactLen)
                    std::memset(buffer.data() + res.bytes, 0, exactLen - res.bytes);
            }

            return res;
        }
        else
        {
            static_assert(detail::is_dynamic_buffer_v<T> || detail::is_fixed_buffer_v<T>,
                          "T must be a supported dynamic or fixed-size contiguous byte container");
        }
        return {};
    }

    /**
     * @brief Receive up to the specified number of bytes from the socket.
     * @ingroup udp
     *
     * Reads at most \p n bytes from the socket into an internal buffer and returns
     * them as a `std::string`. This method will return immediately once a single
     * datagram is received—possibly containing fewer than \p n bytes if the datagram
     * is smaller, or exactly \p n bytes if it is larger (truncating the excess).
     *
     * @details
     * - Works in both connected and unconnected modes:
     *   - **Connected:** Data is read from the connected peer only.
     *   - **Unconnected:** Data is read from any sender; the sender's address and port
     *     are stored internally and can be retrieved via `getRemoteIp()` / `getRemotePort()`.
     * - Unlike `readExact()`, this method does not throw if fewer than \p n bytes are received.
     * - If \p n is zero, an empty string is returned immediately.
     *
     * @param[in] n The maximum number of bytes to read from a single datagram.
     *
     * @return A `std::string` containing the received bytes (length ≤ \p n).
     *
     * @throw SocketException If the socket is not open or if a receive error occurs.
     *
     * @note
     * - This method internally calls `read(DatagramPacket&, bool, DatagramReceiveMode)`
     *   with `resizeBuffer=true` and `DatagramReceiveMode::NoPreflight`.
     * - For large datagrams, consider using `MaxDatagramPayloadSafe` as a safe upper bound.
     *
     * @code
     * jsocketpp::DatagramSocket socket;
     * socket.bind(12345);
     * auto data = socket.readAtMost(512);
     * std::cout << "Received: " << data << std::endl;
     * @endcode
     */
    [[nodiscard]] std::string readAtMost(std::size_t n) const;

    /**
     * @brief Receive the next available datagram in its entirety.
     * @ingroup udp
     *
     * Reads the next datagram from the socket and returns it as a `std::string`,
     * automatically sizing the buffer to exactly fit the datagram's payload.
     *
     * @details
     * - Works in both connected and unconnected modes:
     *   - **Connected:** Data is read from the connected peer only.
     *   - **Unconnected:** Data is read from any sender; the sender's address and port
     *     are stored internally and can be retrieved via `getRemoteIp()` / `getRemotePort()`.
     * - This method uses a preflight size check (`DatagramReceiveMode::PreflightSize`)
     *   to determine the exact payload size before performing the actual receive.
     * - If the datagram size exceeds `MaxDatagramPayloadSafe`, the buffer will be
     *   capped at that value.
     *
     * @return A `std::string` containing the full datagram payload.
     *
     * @throw SocketException If the socket is not open or if a receive error occurs.
     *
     * @note
     * - This method is equivalent to calling `read(DatagramPacket&, true, DatagramReceiveMode::PreflightSize)`
     *   followed by constructing a string from the packet's buffer.
     * - For datagrams where the size is not known in advance, this method ensures that
     *   no truncation occurs unless the message exceeds `MaxDatagramPayloadSafe`.
     *
     * @code
     * jsocketpp::DatagramSocket socket;
     * socket.bind(12345);
     * auto data = socket.readAvailable();
     * std::cout << "Received " << data.size() << " bytes" << std::endl;
     * @endcode
     */
    [[nodiscard]] std::string readAvailable() const;

    /**
     * @brief Reads exactly @p len bytes from the next datagram into @p buffer.
     * @ingroup udp
     *
     * Connected-only. Throws unless the datagram payload size is exactly @p len.
     *
     * @param[out] buffer Destination buffer (must hold @p len bytes).
     * @param[in]  len    Required datagram size.
     * @return Number of bytes copied (== len on success).
     *
     * @throws SocketException If not connected, on recv error, or if datagram size != len.
     *
     * @see readExact() String-returning variant
     */
    std::size_t readIntoExact(void* buffer, std::size_t len) const;

    /**
     * @brief Attempts a best-effort single datagram read with a timeout.
     * @ingroup udp
     *
     * Waits up to @p timeoutMillis for readability, then performs one recv() that returns
     * up to @p n bytes (truncating if datagram is larger). Connected-only.
     *
     * @param[in] n             Maximum payload bytes to return (> 0).
     * @param[in] timeoutMillis Time to wait for readability:
     *                          - > 0: wait up to this many ms
     *                          - 0:  poll (non-blocking)
     *                          - < 0: throws SocketException
     *
     * @return 0..n bytes from the datagram (empty only for zero-length datagram).
     *
     * @throws SocketTimeoutException If unreadable within the timeout.
     * @throws SocketException On invalid socket, not connected, or recv error.
     */
    [[nodiscard]] std::string readAtMostWithTimeout(std::size_t n, int timeoutMillis) const;

    /**
     * @brief Reads a length-prefixed payload where the prefix type is @p T (connected-only).
     * @ingroup udp
     *
     * First receives a datagram containing a @p T-sized length prefix followed by the payload.
     * The prefix is interpreted in **network byte order**. If the datagram does not contain
     * exactly sizeof(T) + length bytes, this method throws.
     *
     * @tparam T Unsigned, trivially copyable integral prefix type (e.g., uint16_t/uint32_t).
     * @return The payload bytes as a string (excluding the prefix).
     *
     * @throws SocketException If not connected, on recv error, if size is inconsistent,
     *                         or if prefix decoding fails.
     *
     * @see readPrefixed<T>(std::size_t) Bounded variant
     * @see writePrefixed<T>() Matching sender
     */
    template <typename T> [[nodiscard]] std::string readPrefixed();

    /**
     * @brief Length-prefixed read with a maximum payload bound.
     * @ingroup udp
     *
     * Same as readPrefixed<T>(), but validates that decoded payload length does not
     * exceed @p maxPayloadLen.
     *
     * @tparam T Unsigned integral prefix type (network byte order).
     * @param[in] maxPayloadLen Maximum allowed payload length in bytes.
     * @return The payload as a string.
     *
     * @throws SocketException If size is inconsistent or exceeds @p maxPayloadLen.
     */
    template <typename T> [[nodiscard]] std::string readPrefixed(std::size_t maxPayloadLen);

    /**
     * @brief Discards exactly @p n bytes from the next datagram.
     * @ingroup udp
     *
     * Connected-only. Receives one datagram and throws unless its payload size equals @p n.
     * Useful for skipping fixed-size messages without copying them out.
     *
     * @param[in] n Required datagram size to discard (> 0).
     *
     * @throws SocketException If not connected, on recv error, or if size mismatch.
     */
    void discard(std::size_t n) const;

    /**
     * @brief Vectorized single-datagram read into multiple buffers (scatter read).
     * @ingroup udp
     *
     * Connected-only. Performs a single recv/readv/WSARecv and fills the span of BufferView
     * objects in order with bytes from **one** datagram. Returns total bytes copied (which may
     * be less than the datagram size if buffers are smaller; trailing datagram bytes are dropped).
     *
     * @param[out] buffers Span of writable BufferView regions.
     * @return Total bytes written across buffers.
     *
     * @throws SocketException If invalid socket, not connected, or system I/O fails.
     */
    std::size_t readv(std::span<BufferView> buffers) const;

    /**
     * @brief Guarantees the next datagram fully fits into @p buffers; throws otherwise.
     * @ingroup udp
     *
     * Connected-only. Receives one datagram and requires that its payload fits exactly the
     * total capacity of @p buffers. Throws on size mismatch.
     *
     * @param[out] buffers Span of writable BufferView regions (total size must equal datagram size).
     * @return Total bytes read (sum of buffer sizes).
     *
     * @throws SocketException On mismatch, invalid socket, or recv error.
     */
    std::size_t readvAll(std::span<BufferView> buffers) const;

    /**
     * @brief Single vectorized read with a timeout (connected-only).
     * @ingroup udp
     *
     * Waits for readability for up to @p timeoutMillis, then performs one vectorized recv.
     * Returns whatever fits into @p buffers from one datagram; may be 0..capacity bytes.
     *
     * @param[out] buffers Span of BufferView regions.
     * @param[in]  timeoutMillis >0 wait; 0 poll; <0 throws.
     * @return Bytes copied (may be 0 if zero-length datagram).
     *
     * @throws SocketTimeoutException On timeout.
     * @throws SocketException On socket/I/O errors.
     */
    std::size_t readvAtMostWithTimeout(std::span<BufferView> buffers, int timeoutMillis) const;

    /**
     * @brief Requires the entire datagram be delivered into @p buffers within the timeout.
     * @ingroup udp
     *
     * Connected-only. Waits up to @p timeoutMillis for readiness, then receives exactly one
     * datagram whose size must match the total capacity of @p buffers. Throws on mismatch or timeout.
     *
     * @param[out] buffers Span of BufferView regions (total size == datagram size).
     * @param[in]  timeoutMillis Total deadline in ms.
     * @return Total bytes read.
     *
     * @throws SocketTimeoutException On timeout.
     * @throws SocketException On size mismatch or I/O errors.
     */
    std::size_t readvAllWithTotalTimeout(std::span<BufferView> buffers, int timeoutMillis) const;

    /**
     * @brief Writes the entire message to the connected peer as a single datagram.
     * @ingroup udp
     *
     * Connected-only. Ensures the whole @p message is sent in one send(); throws on partial
     * datagram or error. (Note: the OS may drop/fragment; this only guarantees local send().)
     *
     * @param[in] message Payload to send (may be empty to send a zero-length datagram).
     * @return Number of bytes sent (== message.size() on success).
     *
     * @throws SocketException If not connected or send() fails.
     *
     * @see write(std::string_view) Best-effort single send
     */
    std::size_t writeAll(std::string_view message) const;

    /**
     * @brief Sends a length-prefixed datagram using an integral prefix type @p T.
     * @ingroup udp
     *
     * Builds a datagram as [prefix(T, network byte order)] + [payload], then sends it to
     * the connected peer. Throws if payload size does not fit in @p T.
     *
     * @tparam T Unsigned, trivially copyable integral type (e.g., uint32_t).
     * @param[in] payload The payload bytes to send.
     * @return Total bytes sent (sizeof(T) + payload.size()).
     *
     * @throws SocketException On size overflow, not connected, or send failure.
     *
     * @see readPrefixed<T>() Matching receiver
     */
    template <typename T> std::size_t writePrefixed(std::string_view payload);

    /**
     * @brief Sends a length-prefixed datagram from a raw buffer using prefix type @p T.
     * @ingroup udp
     *
     * @tparam T Unsigned integral type for the prefix (network byte order).
     * @param[in] data Pointer to payload (may be null iff len == 0).
     * @param[in] len  Number of bytes to send from @p data.
     * @return Total bytes sent (sizeof(T) + len).
     *
     * @throws SocketException If len exceeds T max, not connected, or send fails.
     */
    template <typename T> std::size_t writePrefixed(const void* data, std::size_t len) const;

    /**
     * @brief Vectorized single-datagram send (scatter/gather) to the connected peer.
     * @ingroup udp
     *
     * Performs one WSASend()/sendmsg() over the provided buffer views. May send fewer bytes
     * if the platform reports partial datagram (treated as error and throws).
     *
     * @param[in] buffers Span of string views forming one logical datagram.
     * @return Total bytes sent.
     *
     * @throws SocketException If not connected or send fails/partial.
     */
    std::size_t writev(std::span<const std::string_view> buffers) const;

    /**
     * @brief Guarantees full transmission of all buffers as a single datagram.
     * @ingroup udp
     *
     * Connected-only. Retries as needed until the full datagram is accepted by the kernel
     * or throws on error.
     *
     * @param[in] buffers Span of fragments composing the datagram.
     * @return Total bytes sent (sum of sizes).
     *
     * @throws SocketException On send error.
     */
    std::size_t writevAll(std::span<const std::string_view> buffers) const;

    /**
     * @brief Best-effort single send with a timeout (connected-only).
     * @ingroup udp
     *
     * Waits up to @p timeoutMillis for writability, then performs one send() of @p data.
     * Returns the number of bytes the kernel accepted (throws on error). No retries.
     *
     * @param[in] data           Payload to send.
     * @param[in] timeoutMillis  >0 wait; 0 poll; <0 throws.
     * @return Bytes sent (may be 0).
     *
     * @throws SocketTimeoutException On timeout.
     * @throws SocketException On invalid socket or send error.
     *
     * @see writeWithTotalTimeout() Full-delivery under deadline
     */
    std::size_t writeAtMostWithTimeout(std::string_view data, int timeoutMillis) const;

    /**
     * @brief Sends up to @p len bytes from a raw buffer (single send).
     * @ingroup udp
     *
     * Connected-only. Best-effort; returns bytes accepted by the kernel in one call.
     *
     * @param[in] data Pointer to payload (nullable iff len==0).
     * @param[in] len  Number of bytes to send.
     * @return Bytes sent (<= len).
     *
     * @throws SocketException If not connected or send fails.
     *
     * @see writeFromAll() Retry-until-full variant
     */
    std::size_t writeFrom(const void* data, std::size_t len) const;

    /**
     * @brief Sends exactly @p len bytes from a raw buffer in one datagram, retrying as needed.
     * @ingroup udp
     *
     * Connected-only. Ensures kernel accepts the full datagram (throws otherwise).
     *
     * @param[in] data Pointer to payload (nullable iff len==0).
     * @param[in] len  Payload size.
     * @return Bytes sent (== len on success).
     *
     * @throws SocketException On send failure.
     */
    std::size_t writeFromAll(const void* data, std::size_t len) const;

    /**
     * @brief Sends the full @p data as one datagram within a total timeout.
     * @ingroup udp
     *
     * Connected-only. Repeatedly waits for writability and sends until the entire datagram
     * is accepted or the deadline expires.
     *
     * @param[in] data          Payload to send.
     * @param[in] timeoutMillis Total time budget in milliseconds.
     * @return Total bytes sent (== data.size() on success).
     *
     * @throws SocketTimeoutException If not fully sent before the deadline.
     * @throws SocketException On socket/send errors.
     *
     * @see writeAll() Unbounded full-delivery
     */
    std::size_t writeWithTotalTimeout(std::string_view data, int timeoutMillis) const;

    /**
     * @brief Peeks at the next available UDP datagram without removing it from the socket's receive queue.
     * @ingroup udp
     *
     * This method behaves like `read(DatagramPacket&)`, but uses `MSG_PEEK` to inspect the next datagram
     * without consuming it. The datagram remains available for subsequent `read()` calls.
     *
     * ---
     *
     * ### Use Cases
     * - Inspect headers without committing to a full receive
     * - Determine sender identity before deciding to read
     * - Non-destructive receive in protocols that support retries or probing
     *
     * ---
     *
     * ### Behavior
     * - Calls `recvfrom()` with the `MSG_PEEK` flag
     * - Fills `packet.buffer` with the available data and sets the sender’s address and port
     * - If the buffer is empty and `resizeBuffer == true`, it will be resized to match the internal buffer size
     * - The internal remote address state (`_remoteAddr`) is **not** updated
     *
     * ---
     *
     * @param[out] packet The DatagramPacket to fill with peeked data and sender information.
     * @param[in] resizeBuffer If true, resizes the buffer to fit the received datagram size.
     *                         Also enables auto-sizing if the buffer is initially empty.
     *
     * @return Number of bytes peeked.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - The buffer is empty and `resizeBuffer == false`
     * - A system error occurs during `recvfrom()`
     *
     * @note This method does not remove the datagram from the socket buffer.
     * @note Peeking does not update the internal `_remoteAddr` state, so `getRemoteIp()` is unaffected.
     *
     * @see read(), recvFrom(), getRemoteIp(), getRemotePort()
     */
    [[nodiscard]] std::size_t peek(DatagramPacket& packet, bool resizeBuffer = true) const;

    /**
     * @brief Checks whether data is available to be read from the socket within a timeout window.
     * @ingroup udp
     *
     * This method performs a non-blocking poll using `select()` to determine whether the socket has
     * at least one datagram available for reading.
     *
     * ---
     *
     * ### Use Cases
     * - Avoid blocking in `read()` or `recvFrom()` if no data is available
     * - Implement custom polling, timers, or event loops
     * - Integrate into latency-sensitive or real-time applications
     *
     * ---
     *
     * @param[in] timeoutMillis Timeout in milliseconds to wait for data availability.
     *                          Use `0` for an immediate check (non-blocking).
     *                          Use `-1` to wait indefinitely.
     *
     * @return `true` if data is available to read before the timeout, `false` if the timeout expired.
     *
     * @throws SocketException If the socket is invalid or polling fails.
     *
     * @note This method does not consume or modify the receive buffer.
     * @see peek(), read(), recvFrom(), waitReady()
     */
    [[nodiscard]] bool hasPendingData(int timeoutMillis = 0) const;

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
     * @brief Waits for the socket to become ready for reading or writing.
     * @ingroup udp
     *
     * This method uses `select()` to check whether the socket is ready for I/O
     * within the given timeout. It can be used to avoid blocking reads or writes.
     *
     * ---
     *
     * ### Parameters
     * @param[in] forWrite If true, waits for socket to be writable. If false, waits for readability.
     * @param[in] timeoutMillis Timeout in milliseconds to wait. Use 0 for non-blocking poll.
     *
     * @return `true` if the socket is ready for the requested operation before timeout, `false` otherwise.
     *
     * @throws SocketException If the socket is invalid or `select()` fails.
     *
     * @note This method does not perform any I/O. It only checks readiness.
     * @see peek(), hasPendingData(), read(), write()
     */
    [[nodiscard]] bool waitReady(bool forWrite, int timeoutMillis = 0) const;

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
     * @brief Low-level UDP receive helper for single-datagram reads into caller-provided memory.
     * @ingroup udp
     *
     * @details
     * This method is the internal backbone for higher-level `read()` and `readInto()` calls.
     * It performs exactly one datagram receive from the socket into a fixed-size caller buffer,
     * with optional preflight sizing, truncation detection, EINTR handling, and optional
     * capture of the sender's address.
     *
     * **Key features**
     * - Supports all @ref DatagramReceiveMode policies:
     *   - `NoPreflight`: Fastest path; reads directly into @p buf with no size probing.
     *   - `PreflightSize`: Probes the exact pending datagram size (via platform-dependent
     *     mechanisms such as `FIONREAD` or `MSG_PEEK|MSG_TRUNC`) and clamps the request to
     *     `min(probed, MaxDatagramPayloadSafe, len)`.
     *   - `PreflightMax`: Behaves like `PreflightSize` but never requests more than the
     *     caller's provided length; avoids oversizing the read.
     * - Honors @p len as a hard cap; never allocates or grows memory.
     * - Optionally captures the source address for unconnected sockets via @p outSrc/@p outSrcLen.
     * - EINTR-safe: transparently retries the receive if interrupted.
     * - Distinguishes between timeouts, would-block conditions, and other socket errors:
     *   - Throws `SocketTimeoutException` for SO_RCVTIMEO expirations or non-blocking timeouts.
     *   - Throws `SocketException` with full error code and message for other failures.
     * - Can report the probed datagram size and whether truncation occurred via @p outDatagramSz and @p outTruncated.
     *
     * **Thread safety:** This method is not inherently thread-safe; callers must provide their
     * own synchronization if multiple threads may access the same `DatagramSocket`.
     *
     * @param[out] buf
     *        [in,out] Pointer to a writable buffer with at least @p len bytes capacity.
     *        Must be non-null if @p len > 0.
     * @param[in] len
     *        [in] Capacity of @p buf in bytes. Zero is allowed for zero-length datagrams but
     *        still requires a valid pointer on some platforms.
     * @param[in] mode
     *        [in] Datagram receive mode policy controlling preflight behavior.
     * @param[in] recvFlags
     *        [in] Additional flags passed to `recv()`/`recvfrom()` (e.g., `MSG_PEEK`).
     *        Typically `0` for normal reads.
     * @param[out] outSrc
     *        [out] Optional pointer to a `sockaddr_storage` to receive the sender's address
     *        (only used in unconnected mode). If `nullptr`, uses `recv()`.
     * @param[in,out] outSrcLen
     *        [in,out] On input, size of @p outSrc in bytes; on output, size actually written.
     *        Ignored if @p outSrc is `nullptr`.
     * @param[out] outDatagramSz
     *        [out] Optional pointer to receive the total datagram size (from preflight or
     *        actual receive). Set to `0` if unknown.
     * @param[out] outTruncated
     *        [out] Optional pointer set to `true` if the datagram was larger than @p len and
     *        was truncated by the OS; otherwise set to `false`.
     *
     * @return Number of bytes actually stored in @p buf. May be less than @p len if the datagram
     *         is smaller or if truncation occurred.
     *
     * @throws SocketException
     *         If the socket is not open, arguments are invalid, or a non-timeout socket error occurs.
     * @throws SocketTimeoutException
     *         If a receive timeout expires or the socket is non-blocking and no data is available.
     *
     * @note This method does not update `rememberRemote()`; higher-level callers must do so
     *       if they want to persist the last-remote address for subsequent sends.
     *
     * @since 1.0
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
     * @param[in] src [in] Sender address as returned by @c recvfrom().
     * @param[in] len [in] Length of @p src.
     *
     * @note Callers should only invoke this after a successful receive with @c recvfrom().
     * @since 1.0
     */
    void rememberRemote(const sockaddr_storage& src, const socklen_t len) const noexcept
    {
        _remoteAddr = src;
        _remoteAddrLen = len;
    }

    static void throwSizeMismatch(const std::size_t expected, const std::size_t actual, const bool isProbedKnown)
    {
        // Two-arg pattern with wrapped message for consistency across the project
        constexpr int err = 0; // logical (not a system error)
        const std::string msg =
            "UDP datagram size mismatch: expected " + std::to_string(expected) +
            (isProbedKnown ? (", probed " + std::to_string(actual)) : (", received " + std::to_string(actual)));
        throw SocketException(err, msg);
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

/**
 * @brief Specialization of `write<T>()` to send a UDP datagram from a `std::string`.
 * @ingroup udp
 *
 * This method transmits the string's contents as a single datagram to the connected peer.
 * It is optimized for text and binary string data, supporting both text-based protocols and
 * raw binary payloads.
 *
 * ---
 *
 * ### Behavior
 * - Uses a single `send()` call to transmit the entire string atomically
 * - No automatic framing, fragmentation, or retries
 * - Preserves embedded null characters (`\0`) if present
 * - Fails fast on errors or partial sends
 *
 * ---
 *
 * ### Requirements
 * - Socket must be **connected** via `connect()`
 * - For unconnected sends, use `writeTo()` instead
 *
 * ---
 *
 * ### Example
 * ```cpp
 * DatagramSocket sock;
 * sock.connect("example.com", 9999);
 *
 * std::string message = "Hello UDP!";
 * sock.write(message); // Sends 9 bytes
 * ```
 *
 * @param[in] value The string to transmit. May contain nulls. Empty strings result in a no-op.
 *
 * @throws SocketException If:
 * - The socket is not open
 * - The socket is not connected
 * - `send()` fails (e.g., network errors)
 * - Only part of the string was sent
 *
 * @note No null terminator is appended. The exact bytes in the string are sent.
 * @note For large strings exceeding path MTU, the datagram may be fragmented or dropped.
 *
 * @see write<std::string_view>() Zero-copy string sending
 * @see read<std::string>() String-optimized receive
 * @see writeTo() For unconnected sending
 * @see DatagramPacket For more control over destinations
 * @since 1.0
 */
template <> inline void DatagramSocket::write<std::string>(const std::string& value)
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write<std::string>(): socket is not open.");

    if (!isConnected())
        throw SocketException("DatagramSocket::write<std::string>(): socket is not connected. Use writeTo() instead.");

    if (value.empty())
        return; // or optionally throw or return 0

    internal::sendExact(getSocketFd(), value.data(), value.size());
}

/**
 * @brief Specialization of `write<T>()` to send a `std::string_view` as a UDP datagram.
 * @ingroup udp
 *
 * This method transmits the contents of a `std::string_view` as a single datagram to the
 * connected peer. It provides zero-copy optimization for constant or non-owning string data
 * such as protocol messages, fixed command strings, or log lines.
 *
 * ---
 *
 * ### ⚙️ Behavior
 * - Uses a single `send()` syscall to transmit the view's contents atomically
 * - No framing, fragmentation, or auto-retry on failure
 * - Preserves embedded null bytes (`\0`) if present
 * - For empty views, performs a no-op (no syscall)
 *
 * ---
 *
 * ### 📋 Requirements
 * - The socket must be **connected** via `connect()`
 * - For unconnected sends, use `writeTo()` instead
 * - The memory referenced by the view must remain valid during `send()`
 *
 * ---
 *
 * ### 🧪 Example
 * @code
 * DatagramSocket sock;
 * sock.connect("example.com", 9999);
 *
 * const char* cmd = "PING\n";
 * sock.write(std::string_view(cmd, 5)); // Zero-copy send
 * @endcode
 *
 * @param[in] value The string view to transmit. May contain nulls. Empty views result in a no-op.
 *
 * @throws SocketException If:
 * - The socket is not open
 * - The socket is not connected
 * - `send()` fails or reports partial transmission
 *
 * @warning Do not pass views to temporary strings or buffers that may be destroyed before
 *          the send completes.
 *
 * @note No null terminator is appended. The exact bytes in the view are transmitted.
 * @note For views exceeding path MTU, the datagram may be fragmented or dropped.
 *
 * @see write<std::string>() String-owning sending
 * @see writeTo() For unconnected sends
 * @see read<std::string>() For string-optimized receive
 * @see DatagramPacket For more control over destinations
 * @since 1.0
 */
template <> inline void DatagramSocket::write<std::string_view>(const std::string_view& value)
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write<std::string_view>(): socket is not open.");

    if (!isConnected())
        throw SocketException(
            "DatagramSocket::write<std::string_view>(): socket is not connected. Use writeTo() instead.");

    if (value.empty())
        return; // Empty datagram is technically valid, but skip to avoid overhead (optional)

    internal::sendExact(getSocketFd(), value.data(), value.size());
}

} // namespace jsocketpp
