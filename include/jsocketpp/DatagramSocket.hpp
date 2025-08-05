/**
 * @file DatagramSocket.hpp
 * @brief UDP datagram socket abstraction for jsocketpp.
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

#pragma once

#include "common.hpp"
#include "DatagramPacket.hpp"
#include "SocketOptions.hpp"

#include <bit>
#include <optional>
#include <string>

namespace jsocketpp
{

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
     * @brief Default constructor (deleted) for DatagramSocket class.
     * @ingroup udp
     *
     * The default constructor is explicitly deleted to prevent the creation of
     * uninitialized `DatagramSocket` objects. Each datagram socket must be explicitly
     * constructed with a valid port number (or socket descriptor) to participate in
     * sending or receiving UDP packets.
     *
     * ### Rationale
     * - Prevents the creation of sockets without binding context
     * - Enforces correct construction for unicast, broadcast, or multicast use
     * - Avoids silent failures from using an unbound or invalid socket
     *
     * @code{.cpp}
     * DatagramSocket s;         // ❌ Compilation error (deleted constructor)
     * DatagramSocket s(12345);  // ✅ Valid: binds to port 12345
     * @endcode
     *
     * @see DatagramSocket(Port, std::size_t) Constructor for binding to port
     */
    DatagramSocket() = delete;

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
     * @brief Destructor. Closes the socket and releases all associated resources.
     * @ingroup udp
     *
     * Cleans up system resources associated with the UDP socket, including:
     * - Closing the socket file descriptor (via `closesocket()` or `close()`)
     * - Freeing address resolution memory (`freeaddrinfo`)
     *
     * This destructor ensures that sockets are properly closed on destruction,
     * following the RAII (Resource Acquisition Is Initialization) pattern. If
     * any errors occur during cleanup, they are caught and logged to `std::cerr`
     * but will not propagate exceptions, preserving `noexcept` semantics.
     *
     * ### RAII Guarantees
     * - Automatically releases system resources when the object goes out of scope
     * - Prevents socket leaks in exception paths or early returns
     * - Ensures consistent teardown behavior across platforms
     *
     * @note If you require custom error handling for cleanup failures, call
     * `close()` manually before destruction.
     *
     * @warning Do not reuse a `DatagramSocket` after it has been destroyed or moved from.
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
     * This method binds the `DatagramSocket` to an ephemeral (auto-assigned) port
     * on all local network interfaces (`INADDR_ANY` for IPv4 or `in6addr_any` for IPv6),
     * depending on system configuration and address resolution.
     *
     * ### Common Use Cases
     * - UDP client sockets that do not require a specific port
     * - Transient sockets used for fire-and-forget messages or RPC
     * - Systems with NAT traversal where source port is flexible
     *
     * ### Behavior
     * - Uses `getaddrinfo()` to resolve wildcard binding addresses (e.g., "0.0.0.0" or "::")
     * - Binds to the first successfully resolved and compatible address
     * - Updates internal state to reflect bound status
     *
     * @note This method may only be called once per socket instance. Rebinding is not supported.
     * @note If the socket was constructed with an already-resolved address, this will override it.
     *
     * @throws SocketException if address resolution or binding fails.
     *
     * @code{.cpp}
     * DatagramSocket sock;
     * sock.bind(); // Bound to 0.0.0.0:ephemeral
     * @endcode
     *
     * @see bind(Port), bind(std::string_view, Port), getLocalSocketAddress()
     */
    void bind();

    /**
     * @brief Binds the datagram socket to a specific local port on all network interfaces.
     * @ingroup udp
     *
     * This overload binds the socket to the given UDP `port` across all available network
     * interfaces, using a wildcard address (`0.0.0.0` for IPv4 or `::` for IPv6).
     *
     * ### Common Use Cases
     * - Server-side sockets that need to receive packets on a known port
     * - P2P or NAT traversal clients using fixed source ports
     * - Test setups or replay systems where port number must be predictable
     *
     * ### Behavior
     * - Uses `getaddrinfo()` with AI_PASSIVE and a null host to resolve wildcard binding addresses
     * - Attempts all resolved addresses until bind succeeds
     * - If successful, updates internal bound state and allows subsequent `read()`/`recvFrom()` calls
     *
     * @param[in] port UDP port number to bind to. Must be in the range [1, 65535] or 0 for ephemeral.
     *
     * @note This method may only be called once. Rebinding is not supported.
     * @note If the port is already in use, a `SocketException` will be thrown.
     *
     * @throws SocketException if binding fails or the socket is already bound.
     *
     * @code{.cpp}
     * DatagramSocket server;
     * server.bind(12345); // Bind to port 12345 on all interfaces
     * @endcode
     *
     * @see bind(), bind(std::string_view, Port), setReuseAddress()
     */
    void bind(Port port);

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
     * @param[in] host Local IP address or hostname to bind to (e.g., "127.0.0.1", "::1", "eth0.local").
     *             Use "0.0.0.0" or "::" to bind to all interfaces (same as `bind(port)`).
     * @param[in] port Local UDP port number to bind to. Use 0 for ephemeral port assignment.
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
    void bind(std::string_view host, Port port);

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
     * @ingroup socketopts
     *
     * This method queries the system to determine which local IP address has been assigned
     * to the socket, typically as a result of a successful `bind()` or `connect()` call.
     *
     * ---
     *
     * ### 🌍 Applicability
     * - `DatagramSocket`: ✅ Always safe to call after `bind()`
     * - Works for both IPv4 and IPv6 sockets
     * - Also works if the socket was auto-bound to a port by the OS (`bind(0)`)
     *
     * ---
     *
     * @param[in] convertIPv4Mapped If `true`, IPv4-mapped IPv6 addresses (e.g., `::ffff:192.0.2.1`)
     *                              will be returned as plain IPv4 strings (`192.0.2.1`). If `false`,
     *                              the raw mapped form is preserved.
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock(AF_INET);
     * sock.bind("0.0.0.0", 12345);
     *
     * std::string localIP = sock.getLocalIp();
     * std::cout << "Bound to local IP: " << localIP << "\n";
     * @endcode
     *
     * ---
     *
     * @return A string representing the local IP address (e.g., "192.168.1.10", "::1").
     *
     * @throws SocketException if the socket is invalid, unbound, or the system call fails.
     *
     * @see getLocalPort()
     * @see getLocalSocketAddress()
     * @see https://man7.org/linux/man-pages/man2/getsockname.2.html
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
     * ### 🌍 Applicability
     * - Valid after a successful `bind()` or `connect()`
     * - Works for both IPv4 and IPv6 UDP sockets
     * - Safe for sockets that were automatically bound to ephemeral ports (`port = 0`)
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock(AF_INET);
     * sock.bind("0.0.0.0", 0); // Let OS pick an ephemeral port
     *
     * Port port = sock.getLocalPort();
     * std::cout << "Dynamically bound to port: " << port << "\n";
     * @endcode
     *
     * ---
     *
     * @return The local UDP port number (in host byte order).
     *
     * @throws SocketException if the socket is not open or the system call fails.
     *
     * @see getLocalIp() To retrieve the bound IP address
     * @see getLocalSocketAddress() For a formatted "IP:port" string
     * @see https://man7.org/linux/man-pages/man2/getsockname.2.html
     */
    [[nodiscard]] Port getLocalPort() const;

    /**
     * @brief Retrieves the local socket address as a formatted string in the form `"IP:port"`.
     * @ingroup udp
     *
     * This method returns the socket's local IP address and port in a human-readable format,
     * such as `"127.0.0.1:12345"` or `"::1:54321"`. It works for both explicitly bound sockets
     * and sockets where the OS has auto-assigned an ephemeral port after the first `sendTo()`
     * or `connect()` call.
     *
     * ---
     *
     * ### Use Cases
     * - Logging which interface and port the socket is bound to
     * - Debugging multicast or broadcast socket behavior
     * - Confirming OS-assigned ports in dynamic binding scenarios
     * - Introspection during runtime for monitoring tools or metrics
     *
     * ---
     *
     * ### IPv6 Handling
     * If the socket is bound to an IPv4-mapped IPv6 address (e.g., `::ffff:192.0.2.1`) and
     * `@p convertIPv4Mapped` is true (default), the method simplifies it to standard IPv4 format.
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock(AF_INET6);
     * sock.bind(0); // OS assigns an ephemeral port
     *
     * std::cout << "Local socket: " << sock.getLocalSocketAddress() << "\n";
     * // Output: "192.0.2.5:50437"
     * @endcode
     *
     * @param[in] convertIPv4Mapped Whether to convert IPv4-mapped IPv6 addresses to IPv4 (default: true).
     * @return A string in the form `"IP:port"` representing the local address and port.
     *
     * @throws SocketException if the socket is not bound or the local address cannot be retrieved.
     *
     * @see getLocalIp()
     * @see getLocalPort()
     * @see bind()
     */
    [[nodiscard]] std::string getLocalSocketAddress(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieves the IP address of the remote peer for the most recent datagram exchange.
     * @ingroup udp
     *
     * This method returns the remote IP address as a string (e.g., `"192.168.1.42"` or `"::1"`),
     * depending on the current connection state of the socket:
     *
     * ---
     *
     * ### 🔁 Behavior Based on Connection Mode
     * - **Connected DatagramSocket**: Uses `getpeername()` to obtain the address of the fixed remote peer.
     * - **Unconnected DatagramSocket**: Returns the IP address of the sender from the most recent `recvfrom()` call.
     *   If no datagram has yet been received, the method throws a `SocketException`.
     *
     * ---
     *
     * ### IPv6 Handling
     * If the returned IP is an IPv4-mapped IPv6 address (e.g., `::ffff:192.0.2.1`) and
     * `@p convertIPv4Mapped` is true (default), the address is simplified to IPv4 form.
     *
     * ---
     *
     * ### Example (Unconnected Mode)
     * @code
     * DatagramSocket sock(AF_INET);
     * sock.bind(4567);
     *
     * std::vector<std::byte> buf(1024);
     * sock.receiveFrom(buf); // waits for first message
     *
     * std::string peerIp = sock.getRemoteIp();
     * std::cout << "Last datagram was from: " << peerIp << "\n";
     * @endcode
     *
     * ---
     *
     * @param[in] convertIPv4Mapped Whether to simplify IPv4-mapped IPv6 addresses (default: true).
     * @return IP address of the connected peer, or the last datagram sender.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - In connected mode, `getpeername()` fails
     * - In unconnected mode, no datagram has been received yet
     *
     * @see getRemotePort()
     * @see getRemoteSocketAddress()
     * @see isConnected()
     */
    [[nodiscard]] std::string getRemoteIp(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieves the port number of the remote peer for the most recent datagram exchange.
     * @ingroup udp
     *
     * This method returns the remote port number in **host byte order**, depending on the socket’s mode:
     *
     * ---
     *
     * ### 🔁 Behavior by Mode
     * - **Connected DatagramSocket**: Uses `getpeername()` to determine the port of the fixed peer.
     * - **Unconnected DatagramSocket**: Returns the port from the most recent `recvfrom()` call.
     *   If no datagram has been received yet, an exception is thrown.
     *
     * ---
     *
     * ### Use Cases
     * - Debugging or logging the origin of a datagram
     * - Protocol dispatching based on sender’s port
     * - Maintaining peer state or statistics per source
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock(AF_INET);
     * sock.bind(9876);
     *
     * std::array<char, 128> buffer{};
     * sock.receiveFrom(buffer);
     *
     * std::cout << "Sender port: " << sock.getRemotePort() << "\n";
     * @endcode
     *
     * ---
     *
     * @return Port number of the connected peer or last datagram sender, in host byte order.
     *
     * @throws SocketException If:
     * - The socket is invalid or closed
     * - In connected mode, `getpeername()` fails
     * - In unconnected mode, no datagram has been received yet
     *
     * @see getRemoteIp()
     * @see getRemoteSocketAddress()
     * @see isConnected()
     */
    [[nodiscard]] Port getRemotePort() const;

    /**
     * @brief Retrieves the remote peer’s socket address in the form `"IP:port"`.
     * @ingroup udp
     *
     * This method returns the formatted address of the remote peer that this datagram socket
     * is either:
     * - **Connected to** (via `connect()`) —or—
     * - **Most recently received a message from** (via `recvfrom()` in unconnected mode)
     *
     * ---
     *
     * ### Behavior by Connection Mode
     * - **Connected DatagramSocket**: Uses `getpeername()` to query the bound peer address.
     * - **Unconnected DatagramSocket**: Returns the address of the last sender from a successful `recvfrom()` call.
     *   If no datagram has been received yet, an exception is thrown.
     *
     * ---
     *
     * ### IPv6 Handling
     * If the remote IP is an IPv4-mapped IPv6 address (e.g., `::ffff:192.0.2.1`) and
     * `@p convertIPv4Mapped` is `true`, the address will be simplified to standard IPv4 format.
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock(AF_INET);
     * sock.bind(12345);
     *
     * std::array<char, 256> buffer{};
     * sock.receiveFrom(buffer); // wait for any sender
     *
     * std::string remote = sock.getRemoteSocketAddress();
     * std::cout << "Received from: " << remote << "\n"; // "192.168.0.42:56789"
     * @endcode
     *
     * ---
     *
     * @param[in] convertIPv4Mapped Whether to simplify IPv4-mapped IPv6 addresses (default: true).
     * @return Remote socket address as a string in the format `"IP:port"`.
     *
     * @throws SocketException If:
     * - The socket is not open
     * - In unconnected mode, no datagram has been received yet
     * - In connected mode, if `getpeername()` fails
     *
     * @see getRemoteIp()
     * @see getRemotePort()
     * @see isConnected()
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
     * ### Constraints
     * - The socket **must be connected** via `connect()`.
     * - The type `T` must be:
     *   - `std::is_trivially_copyable_v<T> == true`
     *   - `std::is_standard_layout_v<T> == true`
     *
     * ---
     *
     * ### Behavior
     * - Performs a `bit_cast` of the object into a raw byte array
     * - Sends exactly `sizeof(T)` bytes in a single datagram
     * - No padding removal, field conversion, or alignment adjustment is performed
     * - No retries: failure to send will throw
     *
     * ---
     *
     * ### Example
     * @code
     * struct Packet {
     *     uint32_t type;
     *     uint16_t length;
     * };
     *
     * DatagramSocket sock(AF_INET);
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
     * - `send()` fails (e.g. unreachable, interrupted, closed)
     *
     * @warning Byte Order:
     * No endianness normalization is applied. You must convert fields manually.
     * Use the provided `jsocketpp::net::fromNetwork()` and `toNetwork()` helpers
     * for safe and portable conversion of integers between host and network byte order.
     * These replace platform-specific calls like `ntohl()` or `htons()`.
     *
     * @warning Padding bytes will be included in transmission. Avoid structs with padding unless explicitly managed.
     * @warning This method does not fragment — objects larger than the MTU may be dropped by the network.
     *
     * @see read<T>() For receiving structured objects
     * @see writeTo() For unconnected datagram transmission
     * @see connect() To establish peer before writing
     */
    template <typename T> void write(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "DatagramSocket::write<T>() requires trivially copyable type");
        static_assert(std::is_standard_layout_v<T>, "DatagramSocket::write<T>() requires standard layout type");

        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("write<T>() failed: socket is not open.");

        std::array<std::byte, sizeof(T)> buffer = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);

        const auto sent = ::send(getSocketFd(),
#ifdef _WIN32
                                 reinterpret_cast<const char*>(buffer.data()), static_cast<int>(buffer.size()),
#else
                                 buffer.data(), buffer.size(),
#endif
                                 0);

        if (sent == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }

        if (static_cast<std::size_t>(sent) != sizeof(T))
            throw SocketException("write<T>() failed: partial datagram was sent.");
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
     * - No retries or fragmentation — UDP datagrams exceeding MTU may be dropped
     * - The internal socket state is not modified (does not change connection status)
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
     * @see write<T>() For connected send
     * @see connect() To establish a default peer
     * @see read<T>() For receiving structured objects
     * @see receiveFrom() For receiving from arbitrary peers
     */
    template <typename T> void writeTo(const std::string_view host, const Port port, const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "DatagramSocket::writeTo<T>() requires trivially copyable type");
        static_assert(std::is_standard_layout_v<T>, "DatagramSocket::writeTo<T>() requires standard layout type");

        if (getSocketFd() == INVALID_SOCKET)
            throw SocketException("DatagramSocket::writeTo<T>(): socket is not open.");

        const auto addrInfo = internal::resolveAddress(host, port, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP);

        // Bit-cast value into raw byte buffer
        const std::array<std::byte, sizeof(T)> buffer = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);

        int lastError = 0;
        for (const addrinfo* addr = addrInfo.get(); addr != nullptr; addr = addr->ai_next)
        {
            int flags = 0;
#ifndef _WIN32
            flags = MSG_NOSIGNAL; // Don't send SIGPIPE on broken pipe
#endif
            const int sent = ::sendto(getSocketFd(),
#ifdef _WIN32
                                      reinterpret_cast<const char*>(buffer.data()), static_cast<int>(buffer.size()),
#else
                                      buffer.data(), buffer.size(),
#endif
                                      0, addr->ai_addr, static_cast<socklen_t>(addr->ai_addrlen), flags);

            if (sent == static_cast<int>(sizeof(T)))
                return; // success

            lastError = GetSocketError(); // save the error for reporting if none succeed
        }

        throw SocketException(lastError, SocketErrorMessageWrap(lastError));
    }

    /**
     * @brief Sends a UDP datagram using the provided DatagramPacket.
     * @ingroup udp
     *
     * This method sends the contents of a `DatagramPacket` to a specified destination.
     * It supports both connectionless and connected UDP sockets:
     *
     * - **Connectionless Mode:** If `packet.address` is non-empty and `packet.port` is valid,
     *   the packet is sent to the resolved destination using `sendto()`. The destination address
     *   is also stored internally, allowing follow-up calls to `getRemoteIp()` and `getRemotePort()`.
     *
     * - **Connected Mode:** If the socket is connected (via `connect()`), and the packet does
     *   not specify a destination, the message is sent using `send()` to the connected peer.
     *
     * ### Connectionless Behavior
     * - Resolves `packet.address` and `packet.port` via `getaddrinfo()`
     * - Sends the buffer to the resolved `sockaddr` using `sendto()`
     * - Uses `MSG_NOSIGNAL` on POSIX to suppress `SIGPIPE`
     * - Updates the internal remote address fields (`_remoteAddr`, `_remoteAddrLen`)
     *   for use in `getRemoteIp()` and `getRemotePort()`
     *
     * ### Connected Behavior
     * - Uses `send()` to deliver the buffer to the connected peer
     * - Fails if the socket is not connected and no destination is provided
     *
     * ### Error Conditions
     * - Throws if `packet.address` is empty and socket is not connected
     * - Throws if address resolution fails (e.g., invalid hostname)
     * - Throws if `send()` or `sendto()` fails (e.g., network unreachable)
     * - Throws if socket descriptor is invalid
     *
     * @param[in] packet [in] The packet containing destination and payload.
     *                   If `packet.address` is non-empty and `packet.port` is non-zero,
     *                   the destination will override any connected peer.
     *
     * @return Number of bytes successfully sent (may be less than buffer size on rare systems).
     *
     * @throws SocketException on:
     *         - address resolution failure
     *         - socket I/O failure
     *         - missing destination on unconnected socket
     *
     * @note This method updates the socket’s internal remote address metadata if used in connectionless mode.
     * @note UDP datagrams are sent atomically. If the packet exceeds the system MTU, it may be dropped.
     * @note This method does **not** fragment or retransmit. Use application-level framing for large data.
     *
     * @see connect(), isConnected(), write(std::string_view, std::string_view, Port), DatagramPacket,
     *      getRemoteIp(), getRemotePort()
     */
    [[nodiscard]] size_t write(const DatagramPacket& packet);

    /**
     * @brief Write data to the socket (connected UDP) from a string_view.
     *
     * @param message The data to send.
     * @return Number of bytes sent.
     * @throws SocketException on error.
     */
    [[nodiscard]] size_t write(std::string_view message) const;

    /**
     * @brief Send a datagram to a specific host and port (connectionless UDP).
     * @param message The message to send.
     * @param host    The destination hostname or IP.
     * @param port    The destination port.
     * @return Number of bytes sent.
     * @throws SocketException on error.
     */
    [[nodiscard]] size_t write(std::string_view message, std::string_view host, Port port) const;

    /**
     * @brief Receives a UDP datagram and fills the provided DatagramPacket.
     *
     * Receives a datagram into the packet's buffer, and sets the sender's address and port fields.
     * The buffer should be preallocated to the maximum expected datagram size.
     *
     * @param packet DatagramPacket object to receive into (buffer should be pre-sized).
     * @param resizeBuffer If true (default), resizes packet.buffer to the number of bytes received.
     *        If false, leaves buffer size unchanged (only the initial part of the buffer is valid)
     * @return Number of bytes received.
     * @throws SocketException on error.
     */
    [[nodiscard]] size_t read(DatagramPacket& packet, bool resizeBuffer = true) const;

    /**
     * @brief Reads a fixed-size, trivially copyable object of type `T` from the datagram socket.
     * @ingroup udp
     *
     * This method receives a full UDP datagram and deserializes it into a binary-safe
     * object of type `T`. It works seamlessly in both connected and unconnected modes:
     *
     * ---
     *
     * ### 🔁 Connection Modes
     * - If the socket is **connected** (via `connect()`), this method uses `recv()` and only accepts
     *   datagrams from the fixed peer.
     * - If the socket is **unconnected**, it uses `recvfrom()` and captures the sender's address.
     *   The sender’s address will be stored internally and can be accessed using `getRemoteIp()` and `getRemotePort()`.
     *
     * ---
     *
     * ### Implementation Details
     * - Performs a single `recv()` or `recvfrom()` call (no looping)
     * - Requires the received datagram to be at least `sizeof(T)` bytes
     * - Excess bytes (if any) are silently discarded
     * - Enforces type constraints: `std::is_trivially_copyable` and `std::is_standard_layout`
     * - Performs **no** endianness conversion or decoding logic
     *
     * ---
     *
     * ### Use Cases
     * - Receiving fixed-size protocol packets (e.g., headers, commands, binary structs)
     * - Message-based IoT or telemetry systems
     * - High-performance UDP applications requiring raw data deserialization
     *
     * ---
     *
     * ### Example
     * @code
     * struct Packet {
     *     std::uint32_t id;
     *     std::uint16_t flags;
     *     char payload[64];
     * };
     *
     * DatagramSocket sock(AF_INET);
     * sock.bind(12345);
     *
     * // Wait for one UDP packet of exactly sizeof(Packet)
     * Packet p = sock.read<Packet>();
     * std::cout << "Received packet with ID: " << p.id << "\n";
     * @endcode
     *
     * ---
     *
     * @tparam T A trivially copyable POD type to deserialize
     * @return Fully constructed object of type `T` from the received message
     *
     * @throws SocketException If:
     * - The socket is not open or bound
     * - A network or system error occurs (`ECONNRESET`, `EINVAL`, etc.)
     * - The received datagram is shorter than `sizeof(T)`
     *
     * @warning Byte Order:
     * No endianness normalization is applied. You must convert fields manually.
     * Use the provided `jsocketpp::net::fromNetwork()` and `toNetwork()` helpers
     * for safe and portable conversion of integers between host and network byte order.
     * These replace platform-specific calls like `ntohl()` or `htons()`.
     *
     * @warning Do not use this with types containing pointers, virtual methods, padding, or platform-specific layouts.
     *
     * @note If the socket is unconnected, this method also updates the remote peer address
     *       for use with `getRemoteIp()` and `getRemotePort()`.
     *
     * @see write()           For sending POD messages
     * @see receiveFrom()     For full message + peer address access
     * @see isConnected()     To check connection mode
     * @see getRemoteIp(), getRemotePort()
     */
    template <typename T> [[nodiscard]] T read()
    {
        static_assert(std::is_trivially_copyable_v<T>, "DatagramSocket::read<T>() requires a trivially copyable type");
        static_assert(std::is_standard_layout_v<T>, "DatagramSocket::read<T>() requires a standard layout type");

        std::array<std::byte, sizeof(T)> buffer{};

        int bytesReceived = 0;

        if (isConnected())
        {
            bytesReceived = ::recv(getSocketFd(), reinterpret_cast<char*>(buffer.data()),
#ifdef _WIN32
                                   static_cast<int>(buffer.size()),
#else
                                   buffer.size(),
#endif
                                   0);
        }
        else
        {
            sockaddr_storage srcAddr{};
            socklen_t srcLen = sizeof(srcAddr);

            bytesReceived = ::recvfrom(getSocketFd(), reinterpret_cast<char*>(buffer.data()),
#ifdef _WIN32
                                       static_cast<int>(buffer.size()),
#else
                                       buffer.size(),
#endif
                                       0, reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);

            if (bytesReceived >= 0)
            {
                _remoteAddr = srcAddr;
                _remoteAddrLen = srcLen;
            }
        }

        if (bytesReceived == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessageWrap(error));
        }

        if (bytesReceived < static_cast<int>(sizeof(T)))
            throw SocketException("Datagram too short for fixed-size read<T>(); expected full object.");

        return std::bit_cast<T>(buffer);
    }

    /**
     * @brief Receive a datagram into a trivially copyable type.
     * @tparam T The type to receive.
     * @param senderAddr [out] String to store sender's address.
     * @param senderPort [out] Variable to store sender's port.
     * @return The received value of type T.
     * @throws SocketException on error.
     */
    template <typename T> T recvFrom(std::string* senderAddr, Port* senderPort)
    {
        static_assert(std::is_trivially_copyable_v<T>, "recvFrom<T>() only supports trivially copyable types");

        sockaddr_storage srcAddr{};
        socklen_t srcLen = sizeof(srcAddr);
        T value;
        const auto n = ::recvfrom(getSocketFd(), reinterpret_cast<char*>(&value), sizeof(T), 0,
                                  reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
        if (n == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessageWrap(error));
        }
        if (n == 0)
            throw SocketException("Connection closed by remote host.");

        // Get sender's IP:port
        char hostBuf[NI_MAXHOST], portBuf[NI_MAXSERV];
        if (getnameinfo(reinterpret_cast<sockaddr*>(&srcAddr), srcLen, hostBuf, sizeof(hostBuf), portBuf,
                        sizeof(portBuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
        {
            if (senderAddr)
                *senderAddr = hostBuf;
            if (senderPort)
                *senderPort = static_cast<Port>(std::stoul(portBuf));
        }
        else
        {
            if (senderAddr)
                senderAddr->clear();
            if (senderPort)
                *senderPort = 0;
        }

        return value;
    }

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
     * This method shuts down and closes the underlying UDP socket file descriptor or handle.
     * Once closed, the socket can no longer send or receive packets. This is a terminal operation —
     * any further use of the socket (e.g. `sendTo()`, `bind()`, or `setOption()`) will result in exceptions.
     *
     * ---
     *
     * ### 🧱 Effects
     * - The socket becomes invalid (`_sockFd == INVALID_SOCKET`)
     * - Any pending operations are aborted
     * - The operating system reclaims the socket descriptor
     *
     * ---
     *
     * ### 💡 Platform Notes
     * - On **POSIX**, this calls `::close(_sockFd)`
     * - On **Windows**, this calls `::closesocket(_sockFd)`
     * - If the socket was already closed, calling this again has no effect (safe)
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock(AF_INET);
     * sock.bind("0.0.0.0", 9999);
     *
     * // ... use the socket ...
     *
     * sock.close(); // Fully tear down the socket
     * @endcode
     *
     * @throws SocketException only if the system close operation fails unexpectedly.
     *
     * @note After calling `close()`, the socket becomes unusable and must not be reused.
     *       Use `isClosed()` to verify socket state before performing further operations.
     *
     * @see isClosed() To check whether the socket has already been closed
     * @see setOption() To configure socket options before or after bind
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
     * @brief Checks whether the socket has been closed or is otherwise invalid.
     * @ingroup tcp
     *
     * Returns `true` if the socket is no longer usable—either because it was explicitly closed
     * via `close()`, or because it was never successfully initialized (i.e., holds an invalid
     * file descriptor).
     *
     * This method does **not** perform any system-level query (such as `poll()` or `getsockopt()`).
     * Instead, it inspects the internal socket descriptor using `getSocketFd()` and returns `true`
     * if it equals `INVALID_SOCKET`. This is the same invariant used throughout the library for
     * ensuring safe socket operations.
     *
     * ### Common Scenarios
     * - The socket was default-initialized or failed during construction
     * - The socket was explicitly closed via `close()`
     * - The socket was moved-from, leaving the source in a valid but unusable state
     *
     * @note Once a socket is closed, it cannot be reused. You must create a new instance if
     *       you wish to open a new connection.
     *
     * @return `true` if the socket is closed or invalid, `false` if it is open and usable.
     *
     * ### Example
     * @code
     * Socket sock("example.com", 443);
     * assert(!sock.isClosed());
     * sock.connect();
     * sock.close();
     * assert(sock.isClosed()); // ✅
     * @endcode
     *
     * @see close(), isConnected(), isBound(), getSocketFd()
     */
    [[nodiscard]] bool isClosed() const noexcept { return getSocketFd() == INVALID_SOCKET; }

  protected:
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
     * - Releases resolved address information (`_addrInfoPtr`)
     * - Clears `_selectedAddrInfo`
     * - Resets internal flags:
     *   - `_isBound = false`
     *   - `_isConnected = false`
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
     * - Releases address resolution results (`_addrInfoPtr`)
     * - Clears `_selectedAddrInfo`
     * - Resets internal state flags:
     *   - `_isBound = false`
     *   - `_isConnected = false`
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
     * - Frees any resolved address information (`_addrInfoPtr`)
     * - Clears `_selectedAddrInfo`
     * - Resets connection state flags:
     *   - `_isBound = false`
     *   - `_isConnected = false`
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

  private:
    sockaddr_storage _remoteAddr{}; ///< Storage for the address of the most recent sender (used in unconnected mode).
    socklen_t _remoteAddrLen = 0;   ///< Length of the valid address data in `_remoteAddr` (0 if none received yet).
    sockaddr_storage _localAddr{};  ///< Local address structure.
    mutable socklen_t _localAddrLen = 0; ///< Length of local address.
    std::vector<char> _internalBuffer;   ///< Internal buffer for read operations.
    Port _port;                          ///< Port number the socket is bound to (if applicable).
    bool _isBound = false;               ///< True if the socket is bound to an address
    bool _isConnected = false;           ///< True if the socket is connected to a remote host.
};

/**
 * @brief Specialization of `read<T>()` for receiving string messages.
 * @ingroup udp
 *
 * Reads a single UDP datagram and returns its contents as a `std::string`.
 * Works in both connected and unconnected modes:
 *
 * ---
 *
 * ### 🔁 Connection Modes
 * - If the socket is **connected**: Uses `recv()` to receive a datagram from the fixed peer.
 * - If the socket is **unconnected**: Uses `recvfrom()` and updates internal peer tracking.
 *
 * ---
 *
 * ### Behavior & Characteristics
 * - Uses the internal `_buffer` to store the received datagram (sized via constructor).
 * - Reads a **single full datagram** in one call (up to `_buffer.size()` bytes).
 * - Captures sender’s address in unconnected mode, making it available via `getRemoteIp()` etc.
 * - Returns all received bytes (including nulls, if any).
 *
 * ---
 *
 * ### Example
 * @code
 * jsocketpp::DatagramSocket sock(12345); // Bound socket
 * std::string msg = sock.read<std::string>();
 * std::cout << "Received: " << msg << "\n";
 * @endcode
 *
 * ---
 *
 * @return String containing the raw contents of the received datagram (may include nulls).
 *
 * @throws SocketException If:
 * - The socket is not open
 * - `recv` or `recvfrom` fails
 * - The buffer is empty or not initialized
 *
 * @note Unlike `read<T>()`, this version accepts datagrams of **any size up to the internal buffer limit**.
 * @note Truncation may occur if the sender transmits more bytes than the internal buffer can hold.
 *
 * @see write(std::string_view)
 * @see read<T>()
 * @see receiveFrom(DatagramPacket&)
 * @see getRemoteIp(), getRemotePort()
 */
template <> inline std::string DatagramSocket::read<std::string>()
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::read<std::string>(): socket is not open.");

    if (_internalBuffer.empty())
        throw SocketException("DatagramSocket::read<std::string>(): internal buffer is empty.");
#ifdef _WIN32
    int
#else
    ssize_t
#endif
        received = 0;

    if (isConnected())
    {
        received = ::recv(getSocketFd(), reinterpret_cast<char*>(_internalBuffer.data()),
#ifdef _WIN32
                          static_cast<int>(_internalBuffer.size()),
#else
                          _internalBuffer.size(),
#endif
                          0);
    }
    else
    {
        sockaddr_storage srcAddr{};
        socklen_t srcLen = sizeof(srcAddr);

        received = ::recvfrom(getSocketFd(), reinterpret_cast<char*>(_internalBuffer.data()),
#ifdef _WIN32
                              static_cast<int>(_internalBuffer.size()),
#else
                              _internalBuffer.size(),
#endif
                              0, reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);

        if (received >= 0)
        {
            _remoteAddr = srcAddr;
            _remoteAddrLen = srcLen;
        }
    }

    if (received == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    if (received == 0)
        throw SocketException("DatagramSocket::read<std::string>(): connection closed by remote host.");

    return {reinterpret_cast<const char*>(_internalBuffer.data()), static_cast<std::size_t>(received)};
}

/**
 * @brief Receive a datagram as a string (connectionless).
 * @param senderAddr [out] String to store sender's address.
 * @param senderPort [out] Variable to store sender's port.
 * @return The received message as a std::string.
 * @throws SocketException on error.
 */
template <> inline std::string DatagramSocket::recvFrom<std::string>(std::string* senderAddr, Port* senderPort)
{
    sockaddr_storage srcAddr{};
    socklen_t srcLen = sizeof(srcAddr);
    const auto n = ::recvfrom(getSocketFd(), _internalBuffer.data(),
#ifdef _WIN32
                              static_cast<int>(_internalBuffer.size()),
#else
                              _internalBuffer.size(),
#endif
                              0, reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
    if (n == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }
    if (n == 0)
        throw SocketException("Connection closed by remote host.");

    // Get sender's IP:port
    char hostBuf[NI_MAXHOST], portBuf[NI_MAXSERV];
    if (getnameinfo(reinterpret_cast<sockaddr*>(&srcAddr), srcLen, hostBuf, sizeof(hostBuf), portBuf, sizeof(portBuf),
                    NI_NUMERICHOST | NI_NUMERICSERV) == 0)
    {
        if (senderAddr)
            *senderAddr = hostBuf;
        if (senderPort)
            *senderPort = static_cast<Port>(std::stoul(portBuf));
    }
    else
    {
        if (senderAddr)
            senderAddr->clear();
        if (senderPort)
            *senderPort = 0;
    }
    return {_internalBuffer.data(), static_cast<size_t>(n)};
}

/**
 * @brief Specialization of `write<T>()` to send a `std::string` as a UDP datagram.
 * @ingroup udp
 *
 * Sends the contents of a string as a datagram to the connected peer.
 * This overload is intended for sending text-based or binary-safe string data.
 *
 * ---
 *
 * ### Constraints
 * - The socket **must be connected** via `connect()`
 * - The string will be sent **as-is**, with no null termination added
 * - Strings containing embedded nulls (`'\0'`) are fully preserved
 *
 * ---
 *
 * ### Behavior
 * - Calls `send()` to transmit the entire string buffer in one datagram
 * - If the string exceeds the MTU, it may be fragmented or dropped depending on the network
 * - No retries or buffering: failures throw immediately
 *
 * ---
 *
 * ### Example
 * @code
 * DatagramSocket sock(AF_INET);
 * sock.connect("127.0.0.1", 9999);
 *
 * std::string message = "Hello, UDP!";
 * sock.write(message); // sends 11-byte datagram
 * @endcode
 *
 * ---
 *
 * @param[in] value The string to send. Null characters are preserved.
 *
 * @throws SocketException If:
 * - The socket is not open
 * - The socket is not connected
 * - `send()` fails (e.g., ECONNRESET, network unreachable)
 * - A partial datagram is sent (less than string length)
 *
 * @warning This method does not add a trailing `'\0'`. If you need null-terminated data,
 * append it manually.
 *
 * @see read<std::string>() For receiving a string datagram
 * @see write<T>() For sending binary POD data
 * @see sendTo() For unconnected datagram transmission
 * @see connect() To establish remote peer before sending
 */
template <> inline void DatagramSocket::write<std::string>(const std::string& value)
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write<std::string>(): socket is not open.");

    if (!isConnected())
        throw SocketException("DatagramSocket::write<std::string>(): socket is not connected. Use sendTo() instead.");

    const auto sent = ::send(getSocketFd(),
#ifdef _WIN32
                             value.data(), static_cast<int>(value.size()),
#else
                             value.data(), value.size(),
#endif
                             0);

    if (sent == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    if (static_cast<std::size_t>(sent) != value.size())
        throw SocketException("DatagramSocket::write<std::string>(): partial datagram was sent.");
}

/**
 * @brief Specialization of `write<T>()` to send a `std::string_view` as a UDP datagram.
 * @ingroup udp
 *
 * Sends the contents of a `std::string_view` to the connected peer without copying or
 * modifying the underlying data. This method is ideal for zero-copy transmission of
 * constant or non-owning string data, such as protocol commands or log lines.
 *
 * ---
 *
 * ### Constraints
 * - The socket **must be connected** via `connect()`
 * - The string view must reference valid memory that remains alive during the call
 * - Embedded nulls (`'\0'`) are sent as-is
 *
 * ---
 *
 * ### Behavior
 * - Calls `send()` with the raw buffer of the `string_view`
 * - Sends the exact number of bytes as `view.size()`
 * - Does not append null terminators or modify the view
 * - If the view exceeds the MTU, fragmentation may occur or the datagram may be dropped
 *
 * ---
 *
 * ### Example
 * @code
 * DatagramSocket sock(AF_INET);
 * sock.connect("localhost", 9000);
 * sock.write(std::string_view("PING")); // sends 4 bytes
 * @endcode
 *
 * ---
 *
 * @param[in] value The string view to send (raw bytes).
 *
 * @throws SocketException If:
 * - The socket is not open
 * - The socket is not connected
 * - `send()` fails
 * - A partial datagram is sent
 *
 * @warning The caller must ensure that the memory referenced by the `string_view` is valid
 * for the duration of the call. Do not pass views to temporaries.
 *
 * @see write<std::string>(), read<std::string>(), sendTo()
 */
template <> inline void DatagramSocket::write<std::string_view>(const std::string_view& value)
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write<std::string_view>(): socket is not open.");

    if (!isConnected())
        throw SocketException(
            "DatagramSocket::write<std::string_view>(): socket is not connected. Use sendTo() instead.");

    const auto sent = ::send(getSocketFd(),
#ifdef _WIN32
                             value.data(), static_cast<int>(value.size()),
#else
                             value.data(), value.size(),
#endif
                             0);

    if (sent == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    if (static_cast<std::size_t>(sent) != value.size())
        throw SocketException("DatagramSocket::write<std::string_view>(): partial datagram was sent.");
}

} // namespace jsocketpp
