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
     * @param port The local UDP port to bind to. Use `0` to auto-select a port.
     * @param bufferSize Size (in bytes) of the receive buffer. Defaults to `DefaultBufferSize` (4096 bytes).
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
     * @param host Hostname or IP address of the intended remote peer (IPv4 or IPv6).
     *             Leave empty for local binding or unconnected use.
     * @param port UDP port number of the peer or target address.
     * @param bufferSize Size of the internal receive buffer in bytes. Defaults to `DefaultBufferSize`.
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
     * @brief Move assignment operator.
     *
     * Releases any owned resources and transfers ownership from another DatagramSocket.
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
     * @param port UDP port number to bind to. Must be in the range [1, 65535] or 0 for ephemeral.
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
     * @param host Local IP address or hostname to bind to (e.g., "127.0.0.1", "::1", "eth0.local").
     *             Use "0.0.0.0" or "::" to bind to all interfaces (same as `bind(port)`).
     * @param port Local UDP port number to bind to. Use 0 for ephemeral port assignment.
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
     * @brief Connects the datagram socket to the remote host and port with optional timeout handling.
     *
     * This function binds the datagram socket to a specific remote destination (host and port) for sending
     * subsequent datagrams. Although UDP is connectionless, calling `connect()` on a datagram socket allows
     * the socket to send datagrams to a specific destination without needing to specify the address each time.
     *
     * The connection process works as follows:
     * - In **non-blocking mode**, the connection attempt proceeds immediately. If the connection cannot be
     *   completed right away, it will return `EINPROGRESS` (or `WSAEINPROGRESS` on Windows), indicating that
     *   the connection is still in progress.
     * - **`select()`** is used to wait for the socket to be ready for communication within the specified timeout.
     * - If no timeout is specified (i.e., `timeoutMillis < 0`), the socket connects in blocking mode, which
     *   will block until either the connection completes or an error occurs.
     * - If the connection fails or times out, an exception is thrown with the relevant error code and message.
     *
     * This function is cross-platform and works on both Windows and Linux systems.
     *
     * @param timeoutMillis The maximum time to wait for the connection to be established, in milliseconds.
     *                      If set to a negative value, the connection will be attempted in blocking mode
     *                      (no timeout). If set to a non-negative value, the connection will be attempted in
     *                      non-blocking mode, and `select()` will be used to wait for the connection to complete
     *                      within the specified timeout.
     *
     * @throws SocketException If the connection fails, times out, or if there is an error during the connection
     * attempt. The exception will contain the error code and message describing the failure.
     */
    void connect(int timeoutMillis = -1);

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
     * @brief Close the datagram socket.
     */
    void close();

    /**
     * @brief Set the socket to non-blocking or blocking mode.
     * @param nonBlocking true for non-blocking, false for blocking.
     * @throws SocketException on error.
     */
    void setNonBlocking(bool nonBlocking) const;

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
     * @brief Get the local socket's address as a string (ip:port).
     * @return String representation of the local address.
     */
    std::string getLocalSocketAddress() const;

    /**
     * @brief Check if the datagram socket is valid (open).
     * @return true if valid, false otherwise.
     */
    [[nodiscard]] bool isValid() const { return getSocketFd() != INVALID_SOCKET; }

    /**
     * @brief Check if the datagram socket is connected to a remote host.
     *
     * For UDP sockets, "connected" means that the socket has been associated with a specific
     * remote address using connect(). This doesn't establish a true connection like TCP,
     * but allows send() and recv() to be used instead of sendto() and recvfrom().
     *
     * @return true if the socket is connected to a remote host, false otherwise.
     */
    [[nodiscard]] bool isConnected() const { return this->_isConnected; }

    /**
     * @brief Enable or disable the ability to send broadcast packets.
     * @param enable Set to true to enable broadcast, false to disable.
     * @throws SocketException on error.
     */
    void enableBroadcast(bool enable) const;

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
