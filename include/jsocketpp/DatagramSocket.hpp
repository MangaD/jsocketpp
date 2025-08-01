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
     * @brief Constructs a UDP socket, optionally bound to the specified local port.
     * @ingroup udp
     *
     * Initializes a new `DatagramSocket` and binds it to the given local port. If `port` is set to `0`,
     * the system will assign an ephemeral port automatically. The internal receive buffer is sized
     * according to the provided `bufferSize` parameter.
     *
     * This constructor is commonly used for both client and server scenarios:
     * - **Server mode**: Bind to a fixed port (e.g., `8888`) so clients know where to send packets.
     * - **Client mode**: Use port `0` to auto-select an available ephemeral port.
     *
     * ### Buffer Size Notes
     * The `bufferSize` parameter defines the size of the internal receive buffer. Choose a size
     * appropriate to your application's expected datagram payload size:
     *
     * - **Maximum UDP payloads**:
     *   - IPv4: up to 65,507 bytes
     *   - IPv6: up to 65,527 bytes
     * - **Typical practical values**: 1024, 2048, 4096, 8192
     *
     * Exceeding the maximum payload size will cause packet truncation.
     *
     * @param[in] port The local UDP port to bind to. Use `0` to auto-select a port.
     * @param[in] bufferSize Size (in bytes) of the receive buffer. Defaults to `DefaultBufferSize` (4096 bytes).
     *
     * @throws SocketException if socket creation or binding fails.
     *
     * @code{.cpp}
     * DatagramSocket server(12345);       // ✅ Server socket on port 12345
     * DatagramSocket client(0, 4096);     // ✅ Client socket with auto-assigned port
     * @endcode
     *
     * @see DefaultBufferSize for tuning guidelines and rationale
     * @see read(), write(), bind() for usage
     */
    explicit DatagramSocket(Port port, std::size_t bufferSize = DefaultBufferSize);

    /**
     * @brief Constructs a datagram socket and resolves the given remote host and port.
     * @ingroup udp
     *
     * This constructor initializes a UDP `DatagramSocket` and resolves the provided remote
     * `host` and `port`, preparing the socket for a later connection via `connect()` or
     * for direct use in connectionless communication (e.g., `write(message, host, port)`).
     *
     * @note This constructor does **not** automatically call `connect()`.
     *       The socket remains unconnected until `connect()` is explicitly invoked.
     *       This allows greater flexibility for connectionless communication or deferred setup.
     *
     * ### Usage Patterns
     * - Call `connect()` manually if you want to establish a default remote peer.
     * - Or use the `write(message, host, port)` overload for connectionless sends.
     * - Or call `bind()` to use it as a UDP server or receiver.
     *
     * ### Buffering
     * The internal read buffer is sized according to `bufferSize`. This buffer is used for
     * reading fixed-size or string messages when the socket is in connected mode.
     *
     * @param[in] host Hostname or IP address of the intended remote peer (IPv4 or IPv6).
     *             Leave empty for local binding or unconnected use.
     * @param[in] port UDP port number of the peer or target address.
     * @param[in] bufferSize Size of the internal receive buffer in bytes. Defaults to `DefaultBufferSize`.
     *
     * @throws SocketException if socket creation or address resolution fails.
     *
     * @code{.cpp}
     * DatagramSocket client("example.com", 9999); // Prepare for later connect()
     * client.connect();                           // ✅ Explicitly connect when ready
     * client.write("Hello");                      // Send to connected peer
     * @endcode
     *
     * @see connect(), bind(), write(), DefaultBufferSize
     */
    DatagramSocket(std::string_view host, Port port, std::size_t bufferSize = DefaultBufferSize);

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
        : SocketOptions(rhs.getSocketFd()), _addrInfoPtr(std::move(rhs._addrInfoPtr)),
          _selectedAddrInfo(rhs._selectedAddrInfo), _localAddr(rhs._localAddr), _localAddrLen(rhs._localAddrLen),
          _buffer(std::move(rhs._buffer)), _port(rhs._port)
    {
        rhs.setSocketFd(INVALID_SOCKET);
        rhs._selectedAddrInfo = nullptr;
        rhs._localAddrLen = 0;
        rhs._port = 0;
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
     * - Resolved address pointers (`_addrInfoPtr`, `_selectedAddrInfo`)
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
            _localAddr = rhs._localAddr;
            _localAddrLen = rhs._localAddrLen;
            _addrInfoPtr = std::move(rhs._addrInfoPtr);
            _selectedAddrInfo = rhs._selectedAddrInfo;
            _buffer = std::move(rhs._buffer);
            _port = rhs._port;

            // Reset source
            rhs.setSocketFd(INVALID_SOCKET);
            rhs._addrInfoPtr = nullptr;
            rhs._selectedAddrInfo = nullptr;
            rhs._localAddrLen = 0;
            rhs._port = 0;
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
     * @brief Connects the datagram socket to a remote peer with optional timeout handling.
     * @ingroup udp
     *
     * Although UDP is a connectionless protocol, calling `connect()` on a datagram socket binds it
     * to a specific remote address and port. This simplifies socket usage by:
     *
     * - Filtering inbound datagrams to only accept packets from the connected peer
     * - Allowing `write()`, `read()`, and `recv()` without needing to specify a destination
     * - Enabling use of `send()`/`recv()` semantics while retaining the performance of UDP
     *
     * This method supports both blocking and non-blocking connection attempts based on the
     * `timeoutMillis` parameter. Internally, it uses `::connect()` and may wait for the socket
     * to become writable via `::select()` if timeout-based non-blocking mode is used.
     *
     * ### Connection Modes
     * - **Blocking Mode** (`timeoutMillis < 0`):
     *   - Performs a traditional blocking `connect()` system call.
     *   - Returns only after success or failure.
     *
     * - **Non-blocking Mode with Timeout** (`timeoutMillis >= 0`):
     *   - Temporarily switches the socket to non-blocking mode (RAII-managed).
     *   - Initiates the connection and waits for socket write readiness using `select()`.
     *   - After readiness, confirms success using `getsockopt(SO_ERROR)`.
     *   - Restores the original blocking mode automatically after completion.
     *
     * ### Implementation Details
     * - Uses `ScopedBlockingMode` to safely revert socket mode.
     * - Retries `select()` on POSIX if interrupted by signals (`EINTR`).
     * - Checks `FD_SETSIZE` to avoid platform limits on select()'able descriptors.
     * - Properly verifies socket status using `getsockopt(SO_ERROR)` even after select success.
     * - Updates `_isConnected` only after a successful connection.
     *
     * @note Unlike TCP, this method does **not establish a session** or guarantee delivery.
     *       It simply associates the socket with a default remote endpoint.
     *
     * @note This method throws if the socket is already connected or if no resolved address is available.
     *
     * ### Platform Notes
     * - On **POSIX**, `select()` only supports descriptors `< FD_SETSIZE` (typically 1024).
     * - On **Windows**, `select()` accepts higher descriptors, but practical limits still apply.
     *
     * ### Example Usage
     * @code{.cpp}
     * DatagramSocket client("example.com", 9999);
     *
     * // Blocking connect
     * client.connect();
     *
     * // Non-blocking connect with 3-second timeout
     * try {
     *     client.connect(3000);
     *     client.write("ping");
     * } catch (const SocketTimeoutException& timeout) {
     *     std::cerr << "Connection timed out\\n";
     * } catch (const SocketException& ex) {
     *     std::cerr << "Connect failed: " << ex.what() << std::endl;
     * }
     * @endcode
     *
     * @param[in] timeoutMillis Timeout in milliseconds:
     *                          - `< 0`: Blocking connect (default behavior)
     *                          - `>= 0`: Non-blocking connect with timeout
     *
     * @throws SocketTimeoutException If the connection does not complete before the timeout.
     * @throws SocketException If:
     *         - The socket is already connected
     *         - No address was resolved during construction
     *         - The descriptor exceeds `select()` limits
     *         - The connection fails or is refused
     *         - Socket readiness check via `getsockopt(SO_ERROR)` fails
     *
     * @see isConnected(), write(), read<T>(), disconnect(), setNonBlocking()
     */
    void connect(int timeoutMillis = -1);

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
    [[nodiscard]] std::string DatagramSocket::getLocalIp(bool convertIPv4Mapped = true) const;

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
    [[nodiscard]] Port DatagramSocket::getLocalPort() const;

    /**
     * @brief Retrieves the full local socket address in the form "IP:port".
     * @ingroup udp
     *
     * This method combines the results of `getLocalIp()` and `getLocalPort()` into a
     * single formatted string representing the socket's current local binding.
     *
     * ---
     *
     * ### 🔍 Format
     * - For IPv4: `"192.168.0.42:12345"`
     * - For IPv6: `"[fe80::1]:12345"` (wrapped in square brackets for URI compatibility)
     *
     * ---
     *
     * ### 🌍 Applicability
     * - Works for both IPv4 and IPv6 UDP sockets
     * - Safe after `bind()` or `connect()` to examine the bound interface and port
     * - Useful for logging, debugging, metrics, and connection tracking
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock(AF_INET6);
     * sock.bind("::", 9999);
     *
     * std::string addr = sock.getLocalSocketAddress();
     * std::cout << "Listening on: " << addr << "\n";
     * @endcode
     *
     * ---
     *
     * @param[in] convertIPv4Mapped If `true`, IPv4-mapped IPv6 addresses will be returned in plain IPv4 format.
     *                              If `false`, the raw mapped form is retained.
     *
     * @return A string in the format `"IP:port"` or `"[IPv6]:port"`.
     *
     * @throws SocketException if the socket is invalid or address retrieval fails.
     *
     * @see getLocalIp()
     * @see getLocalPort()
     * @see getRemoteSocketAddress() For peer address
     */
    [[nodiscard]] std::string getLocalSocketAddress(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieves the IP address of the remote peer this datagram socket is connected to.
     * @ingroup udp
     *
     * This method returns the IP address of the remote endpoint to which this UDP socket is
     * logically connected using `connect()`. If the socket is not connected, this method throws.
     *
     * This is primarily useful in client-side applications or UDP-based protocols that require
     * a fixed communication peer and benefit from `connect()`-based filtering and error detection.
     *
     * ---
     *
     * ### IPv4 vs IPv6
     * - Supports both IPv4 and IPv6 sockets
     * - If `convertIPv4Mapped == true`, IPv4-mapped IPv6 addresses will be returned as plain IPv4
     * - Otherwise, IPv6-mapped IPv4 addresses will remain in their mapped form (`::ffff:192.0.2.1`)
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock(AF_INET);
     * sock.connect("192.168.1.100", 9000);
     *
     * std::string peerIp = sock.getRemoteIp();
     * std::cout << "Connected to: " << peerIp << "\n";
     * @endcode
     *
     * ---
     *
     * @param[in] convertIPv4Mapped Whether to normalize IPv4-mapped IPv6 addresses
     * @return The IP address of the connected peer in string format
     *
     * @throws SocketException if the socket is not connected or the address cannot be retrieved
     *
     * @see connect()
     * @see getRemotePort()
     * @see getRemoteSocketAddress()
     */
    [[nodiscard]] std::string getRemoteIp(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieves the port number of the remote peer this datagram socket is connected to.
     * @ingroup udp
     *
     * This method returns the UDP port number of the peer to which the socket is currently
     * connected via `connect()`. If the socket is not connected, an exception is thrown.
     *
     * ---
     *
     * ### 📡 Use Cases
     * - Useful in client-side or semi-connected UDP scenarios to track the destination port
     * - Enables diagnostic output (e.g. `"Sending to IP:port"`)
     * - Allows verification after `connect()` to check actual remote binding
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock("fe80::1", 7000); // Connect to remote IPv6 peer
     *
     * Port port = sock.getRemotePort();
     * std::cout << "Remote port: " << port << "\n";
     * @endcode
     *
     * ---
     *
     * @return The 16-bit UDP port of the connected peer (in host byte order).
     *
     * @throws SocketException if the socket is not connected or the port cannot be determined.
     *
     * @see getRemoteIp()
     * @see getRemoteSocketAddress()
     * @see connect()
     */
    [[nodiscard]] Port getRemotePort() const;

    /**
     * @brief Get the remote peer's address and port as a formatted string.
     * @ingroup udp
     *
     * This method returns the IP address and port of the connected remote endpoint,
     * formatted as a single string. It is only valid if the socket is connected via `connect()`.
     *
     * The output is of the form:
     * - IPv4: `"192.168.1.100:9000"`
     * - IPv6: `"[fe80::1]:9000"`
     *
     * ---
     *
     * ### Use Cases
     * - Logging or displaying connection targets
     * - Diagnostics or telemetry for connected clients
     * - Simplified UI or monitoring output
     *
     * ---
     *
     * ### Example
     * @code
     * DatagramSocket sock("192.168.1.100", 9000);
     * std::cout << "Connected to: " << sock.getRemoteSocketAddress() << "\n";
     * @endcode
     *
     * ---
     *
     * @param[in] convertIPv4Mapped If true, IPv4-mapped IPv6 addresses will be normalized to IPv4 form.
     * @return A formatted string in the form "IP:port" or "[IPv6]:port"
     *
     * @throws SocketException if the socket is not connected or address retrieval fails.
     *
     * @see getRemoteIp()
     * @see getRemotePort()
     * @see getLocalSocketAddress()
     */
    [[nodiscard]] std::string getRemoteSocketAddress(bool convertIPv4Mapped = true) const;

    /**
     * @brief Write data to a remote host using a DatagramPacket.
     *
     * Sends the data contained in the packet's buffer to the address and port
     * specified in the packet. Can be used in both connected and connectionless modes.
     *
     * @param packet DatagramPacket containing the data to send and destination info.
     * @return Number of bytes sent.
     * @throws SocketException on error.
     */
    [[nodiscard]] size_t write(const DatagramPacket& packet) const;

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
     * @brief Read a trivially copyable type from the socket (connected UDP).
     * @tparam T Type to read (must be trivially copyable).
     * @return Value of type T received from the socket.
     * @throws SocketException on error or disconnect.
     */
    template <typename T> T read()
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "DatagramSocket::read<T>() only supports trivially copyable types");

        if (!_isConnected)
            throw SocketException("DatagramSocket is not connected.");

        T value;
        const int n = ::recv(getSocketFd(), reinterpret_cast<char*>(&value), sizeof(T), 0);

        if (n == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
        if (n == 0)
            throw SocketException("Connection closed by remote host.");
        if (n != sizeof(T))
            throw SocketException("Partial read: did not receive a complete T object.");
        return value;
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
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
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
     * @brief Sets the socket's receive timeout.
     *
     * @param millis Timeout in milliseconds:
     *                 - 0: No timeout (blocking mode)
     *                 - >0: Wait up to specified milliseconds
     *               Common values:
     *                 - 1000 (1 second): Quick response time
     *                 - 5000 (5 seconds): Balance between responsiveness and reliability
     *                 - 30000 (30 seconds): Long-running operations
     * @throws SocketException if setting timeout fails
     */
    void setTimeout(int millis) const;

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
     * @brief Cleans up datagram socket resources and throws a SocketException.
     *
     * This method performs cleanup of the address information structures (_addrInfoPtr)
     * and throws a SocketException with the provided error code. It's typically called
     * when an error occurs during socket initialization or configuration.
     *
     * @param errorCode The error code to include in the thrown exception
     * @throws SocketException Always throws with the provided error code and corresponding message
     */
    void cleanupAndThrow(int errorCode);

    // Also required for MulticastSocket, hence protected.
    internal::AddrinfoPtr _addrInfoPtr = nullptr; ///< Address info pointer for bind/connect.
    addrinfo* _selectedAddrInfo = nullptr;        ///< Selected address info for connection

  private:
    sockaddr_storage _localAddr{};       ///< Local address structure.
    mutable socklen_t _localAddrLen = 0; ///< Length of local address.
    std::vector<char> _buffer;           ///< Internal buffer for read operations.
    Port _port;                          ///< Port number the socket is bound to (if applicable).
    bool _isBound = false;               ///< True if the socket is bound to an address
    bool _isConnected = false;           ///< True if the socket is connected to a remote host.
};

/**
 * @brief Read a string from the socket (connected UDP).
 * @return The received string.
 * @throws SocketException on error.
 */
template <> inline std::string DatagramSocket::read<std::string>()
{
    if (!_isConnected)
        throw SocketException("DatagramSocket is not connected.");

    _buffer.resize(_buffer.size()); // Ensure buffer is sized
    const auto n = ::recv(getSocketFd(), _buffer.data(),
#ifdef _WIN32
                          static_cast<int>(_buffer.size()),
#else
                          _buffer.size(),
#endif
                          0);
    if (n == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (n == 0)
        throw SocketException("Connection closed by remote host.");
    return {_buffer.data(), static_cast<size_t>(n)};
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
    const auto n = ::recvfrom(getSocketFd(), _buffer.data(),
#ifdef _WIN32
                              static_cast<int>(_buffer.size()),
#else
                              _buffer.size(),
#endif
                              0, reinterpret_cast<sockaddr*>(&srcAddr), &srcLen);
    if (n == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
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
    return {_buffer.data(), static_cast<size_t>(n)};
}

} // namespace jsocketpp
