/**
 * @file Socket.hpp
 * @brief TCP client socket abstraction for jsocketpp.
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

#pragma once

#include "BufferView.hpp"
#include "common.hpp"
#include "SocketException.hpp"
#include "SocketOptions.hpp"

#include <array>
#include <bit>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

using jsocketpp::DefaultBufferSize;

namespace jsocketpp
{

/**
 * @class Socket
 * @ingroup tcp
 * @brief TCP client connection abstraction (Java-like interface).
 *
 * The `Socket` class represents a TCP connection between your application and a remote host.
 * It provides a high-level, easy-to-use, and cross-platform API for creating, connecting, sending,
 * and receiving data over TCP sockets. Its interface is inspired by Java's `Socket` class, but uses modern C++20
 * features.
 *
 * ### Key Features
 * - **Connect to remote hosts** using hostnames or IP addresses (IPv4/IPv6).
 * - **Blocking or timeout-enabled connect** for fine-grained control over connection attempts.
 * - **Safe resource management:** sockets are closed automatically when the object is destroyed.
 * - **Read/write interface** for sending and receiving binary data or text.
 * - **Move-only:** socket resources are never accidentally copied.
 * - **Exception-based error handling** via `SocketException`.
 * - **Fine-grained control**: configure timeouts, non-blocking mode, TCP_NODELAY, SO_KEEPALIVE, etc.
 *
 * ### Typical Usage Example
 *
 * @code{.cpp}
 * #include <jsocketpp/Socket.hpp>
 * #include <iostream>
 *
 * int main() {
 *     try {
 *         jsocketpp::Socket sock("example.com", 8080); // Connect to example.com:8080
 *         sock.connect(3000); // Try to connect with 3-second timeout
 *         sock.write("GET / HTTP/1.0\r\n\r\n");
 *         std::string response = sock.read<std::string>();
 *         std::cout << "Received: " << response << std::endl;
 *         sock.close();
 *     } catch (const jsocketpp::SocketException& ex) {
 *         std::cerr << "Socket error: " << ex.what() << std::endl;
 *     }
 * }
 * @endcode
 *
 * ### Internal Buffer
 * - The socket maintains an internal read buffer (default: @ref DefaultBufferSize).
 * - You can resize it with `setInternalBufferSize()` if you expect to receive larger or smaller messages.
 *
 * ### Error Handling
 * - Almost all methods throw `jsocketpp::SocketException` on error (e.g., connect failure, write error, etc).
 * - You should catch exceptions to handle network errors gracefully.
 *
 * ### Thread Safety
 * - Not thread-safe. Use a separate `Socket` object per thread if needed.
 *
 * ### Platform Support
 * - Windows, Linux, macOS. Handles all necessary platform differences internally.
 *
 * ### Advanced Usage
 * - Set non-blocking mode with `setNonBlocking()`.
 * - Tune socket options with `setTcpNoDelay()`, `setKeepAlive()`, and more.
 * - Check remote address with `getRemoteSocketAddress()`.
 *
 * ### See Also
 * - @ref ServerSocket "ServerSocket" - for listening to incoming connections
 * - @ref tcp - TCP socket group
 */
class Socket : public SocketOptions
{
    /**
     * @brief Grants ServerSocket access to private members.
     * @ingroup tcp
     *
     * ServerSocket needs access to Socket's private members during accept() operations to:
     * - Initialize client Socket objects directly with the accepted socket descriptor
     * - Set address information for the connected client
     * - Configure internal buffers
     *
     * @see ServerSocket::accept() Creates new Socket instances from accepted connections
     * @see Socket(SOCKET,const sockaddr_storage&,socklen_t,std::size_t,std::size_t,std::size_t) Protected constructor
     * used by accept()
     */
    friend class ServerSocket;

  protected:
    /**
     * @brief Protected constructor used internally to create Socket objects for accepted client connections.
     * @ingroup tcp
     *
     * This constructor is specifically designed to be called by `ServerSocket::accept()` and related methods
     * to create `Socket` instances representing established client connections. It initializes a `Socket`
     * using an already-connected socket descriptor and the peer's address, and configures internal buffers
     * and socket options for subsequent I/O.
     *
     * ### Usage Flow
     * 1. `ServerSocket::accept()` receives a new client connection from the OS
     * 2. This constructor is invoked to create a `Socket` representing that client
     * 3. The `Socket` takes ownership of the connected socket descriptor and sets buffer sizes
     * 4. The constructed `Socket` is returned to the caller of `accept()`
     *
     * @param[in] client Socket descriptor obtained from a successful `accept()` call.
     * @param[in] addr Remote peer address (`sockaddr_storage` for IPv4/IPv6 compatibility).
     * @param[in] len Size of the peer address structure (`sockaddr_in`, `sockaddr_in6`, etc.).
     * @param[in] recvBufferSize Size in bytes for the internal receive buffer and the socket's `SO_RCVBUF`.
     * @param[in] sendBufferSize Size in bytes for the socket's `SO_SNDBUF` buffer.
     * @param[in] internalBufferSize Size of the internal buffering layer used for string-based and stream reads.
     *
     * @throws SocketException If the socket options cannot be set or internal buffer allocation fails.
     *
     * @note This constructor is `protected` and only accessible to `ServerSocket` to ensure encapsulated
     *       and consistent initialization of accepted client connections.
     *
     * @see ServerSocket::accept() Creates `Socket` instances using this constructor.
     */
    Socket(SOCKET client, const sockaddr_storage& addr, socklen_t len, std::size_t recvBufferSize,
           std::size_t sendBufferSize, std::size_t internalBufferSize);

  public:
    /**
     * @brief Default constructor (deleted) for Socket class.
     * @ingroup tcp
     *
     * The default constructor is explicitly deleted to prevent the creation of
     * uninitialized `Socket` objects. Each socket must be explicitly constructed
     * with a valid host/port combination or from an accepted client connection.
     *
     * ### Rationale
     * - Prevents accidental creation of an invalid socket
     * - Enforces explicit resource ownership and initialization
     * - Avoids ambiguity around object state (e.g., getSocketFd() = INVALID_SOCKET)
     *
     * @code{.cpp}
     * Socket s;               // ❌ Compilation error (deleted constructor)
     * Socket s("host", 1234); // ✅ Correct usage
     * @endcode
     *
     * @see Socket(std::string_view, Port, std::size_t) Primary constructor
     * @see Socket(SOCKET, const sockaddr_storage&, socklen_t, std::size_t, std::size_t) Server-side accept constructor
     */
    Socket() = delete;

    /**
     * @brief Creates a new Socket object configured to connect to the specified host and port.
     * @ingroup tcp
     *
     * This constructor initializes a `Socket` object for a TCP connection to a remote host. It performs
     * hostname resolution using `getaddrinfo()` to support both IPv4 and IPv6 addresses, creates the socket,
     * and configures internal and socket-level buffer sizes.
     *
     * **Note:** This constructor does **not** establish a network connection. You must explicitly call `connect()`
     * after construction to initiate the TCP handshake.
     *
     * ### Initialization Flow
     * 1. Resolves the target host using `getaddrinfo()` (DNS or IP)
     * 2. Creates the socket using the appropriate address family and protocol
     * 3. Applies the provided buffer sizes (or defaults if omitted)
     *
     * @param[in] host The remote hostname or IP address (e.g., "example.com", "127.0.0.1", "::1").
     * @param[in] port The TCP port number to connect to (range: 1–65535).
     * @param[in] recvBufferSize Socket-level receive buffer size (`SO_RCVBUF`). Defaults to @ref DefaultBufferSize
     * (4096 bytes).
     * @param[in] sendBufferSize Socket-level send buffer size (`SO_SNDBUF`). Defaults to @ref DefaultBufferSize (4096
     * bytes).
     * @param[in] internalBufferSize Size of the internal buffer used for high-level `read<T>()` operations. This is
     * distinct from the kernel socket buffers. Defaults to @ref DefaultBufferSize (4096 bytes).
     *
     * @throws SocketException If hostname resolution fails, socket creation fails, or buffer configuration fails.
     *
     * @note This constructor only prepares the socket. Use `connect()` to establish the actual connection.
     *
     * @see connect()
     * @see DefaultBufferSize
     * @see setReceiveBufferSize()
     * @see setSendBufferSize()
     * @see setInternalBufferSize()
     */
    Socket(std::string_view host, Port port, std::optional<std::size_t> recvBufferSize = std::nullopt,
           std::optional<std::size_t> sendBufferSize = std::nullopt,
           std::optional<std::size_t> internalBufferSize = std::nullopt);

    /**
     * @brief Copy constructor (deleted) for Socket class.
     * @ingroup tcp
     *
     * This constructor is explicitly deleted because Socket objects manage unique system
     * resources (socket file descriptors) that cannot be safely duplicated. Each socket
     * must have exclusive ownership of its underlying resources to prevent issues like:
     * - Double-closing of socket descriptors
     * - Race conditions in multi-threaded code
     * - Ambiguous ownership of system resources
     * - Potential resource leaks
     *
     * Instead of copying, use move semantics (Socket&&) to transfer ownership of a Socket
     * from one object to another. For example:
     * @code{.cpp}
     * Socket s1("example.com", 80);
     * Socket s2(std::move(s1)); // OK: moves ownership from s1 to s2
     * Socket s3(s2);            // Error: copying is disabled
     * @endcode
     *
     * @param[in] rhs The Socket object to copy from (unused since deleted)
     *
     * @see Socket(Socket&&) Move constructor for transferring socket ownership
     * @see operator=(Socket&&) Move assignment operator for transferring socket ownership
     */
    Socket(const Socket& rhs) = delete; //-Weffc++

    /**
     * @brief Move constructor that transfers ownership of socket resources.
     * @ingroup tcp
     *
     * Creates a new Socket by taking ownership of another Socket's resources.
     * This constructor is essential for scenarios where Socket objects need
     * to be transferred (e.g., returning from functions, storing in containers)
     * since copying is disabled.
     *
     * The constructor performs the following:
     * 1. Takes ownership of the socket descriptor
     * 2. Transfers remote address information
     * 3. Moves address resolution data
     * 4. Transfers the receive buffer
     * 5. Resets the source object to a valid but empty state
     *
     * The moved-from socket (rhs) remains valid but will be in a default-constructed
     * state with no active connection (getSocketFd() = INVALID_SOCKET).
     *
     * @param[in,out] rhs The Socket object to move from. After the move, rhs will
     *                    be left in a valid but disconnected state.
     *
     * @note This operation is noexcept as it cannot fail - at worst, we'd have a
     *       Socket in an empty state, which is valid.
     *
     * @see Socket(const Socket&) Copy constructor (deleted)
     * @see operator=(Socket&&) Move assignment operator
     */
    Socket(Socket&& rhs) noexcept
        : SocketOptions(rhs.getSocketFd()), _remoteAddr(rhs._remoteAddr), _remoteAddrLen(rhs._remoteAddrLen),
          _cliAddrInfo(std::move(rhs._cliAddrInfo)), _selectedAddrInfo(rhs._selectedAddrInfo),
          _internalBuffer(std::move(rhs._internalBuffer)), _isBound(rhs._isBound), _isConnected(rhs._isConnected),
          _inputShutdown(rhs._inputShutdown), _outputShutdown(rhs._outputShutdown)
    {
        rhs.setSocketFd(INVALID_SOCKET);
        rhs._selectedAddrInfo = nullptr;
        rhs._isBound = false;
        rhs._isConnected = false;
        rhs.resetShutdownFlags();
    }

    /**
     * @brief Copy assignment operator (deleted) for Socket class.
     * @ingroup tcp
     *
     * This operator is explicitly deleted because Socket objects manage unique system
     * resources (socket file descriptors) that cannot be safely duplicated. Each socket
     * must have exclusive ownership of its underlying resources to prevent issues like:
     * - Double-closing of socket descriptors
     * - Race conditions in multi-threaded code
     * - Ambiguous ownership of system resources
     * - Potential resource leaks
     *
     * Instead of copying, use move semantics (operator=(Socket&&)) to transfer ownership
     * of a Socket from one object to another. For example:
     * @code{.cpp}
     * Socket s1("example.com", 80);
     * Socket s2("other.com", 8080);
     * s2 = std::move(s1); // OK: moves ownership from s1 to s2
     * s2 = s1;           // Error: copying is disabled
     * @endcode
     *
     * @param[in] rhs The Socket object to copy from (unused since deleted)
     * @return Reference to this Socket (never returns since deleted)
     *
     * @see Socket(Socket&&) Move constructor for transferring socket ownership
     * @see operator=(Socket&&) Move assignment operator for transferring socket ownership
     */
    Socket& operator=(const Socket& rhs) = delete; //-Weffc++

    /**
     * @brief Move assignment operator that transfers socket ownership safely.
     * @ingroup tcp
     *
     * This operator safely transfers ownership of socket resources from another Socket object
     * while properly managing the current socket's resources. It ensures proper cleanup of
     * existing resources before taking ownership of the new ones.
     *
     * The operator performs these steps in order:
     * 1. Checks for self-assignment to prevent resource corruption
     * 2. Closes any existing socket connection
     * 3. Frees existing address information resources
     * 4. Takes ownership of all resources from the source socket
     * 5. Resets the source socket to a valid but empty state
     *
     * After the move operation:
     * - This socket will own and manage all resources from the source
     * - The source socket (rhs) will be left in a valid but disconnected state
     * - All previous resources of this socket will be properly cleaned up
     *
     * @param[in,out] rhs The source Socket whose resources will be moved.
     *                    After the move, rhs will be left in a valid but disconnected state.
     *
     * @return Reference to this Socket object
     *
     * @note This operation is noexcept as it handles all cleanup internally and
     *       cannot throw exceptions. At worst, socket closure errors will be logged.
     *
     * @see Socket(Socket&&) Move constructor
     * @see Socket(const Socket&) Copy constructor (deleted)
     * @see operator=(const Socket&) Copy assignment operator (deleted)
     */
    Socket& operator=(Socket&& rhs) noexcept
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
            _cliAddrInfo = std::move(rhs._cliAddrInfo);
            _selectedAddrInfo = rhs._selectedAddrInfo;
            _internalBuffer = std::move(rhs._internalBuffer);
            _isBound = rhs._isBound;
            _isConnected = rhs._isConnected;
            _inputShutdown = rhs._inputShutdown;
            _outputShutdown = rhs._outputShutdown;

            // Reset source
            rhs.setSocketFd(INVALID_SOCKET);
            rhs._selectedAddrInfo = nullptr;
            rhs._isBound = false;
            rhs._isConnected = false;
            rhs.resetShutdownFlags();
        }
        return *this;
    }

    /**
     * @brief Destructs the Socket object, closing connections and freeing resources.
     * @ingroup tcp
     *
     * This destructor ensures proper cleanup of all resources owned by the Socket:
     * - Closes the socket file descriptor if still open
     * - Frees any allocated address information structures
     * - Releases internal buffers
     *
     * The destructor is marked noexcept to prevent exception propagation during
     * stack unwinding, as per C++ best practices. Any errors that occur during
     * cleanup (e.g., socket closure failures) are ignored.
     *
     * @note This destructor is thread-safe with respect to other Socket instances
     *       but not with concurrent operations on the same Socket object.
     *
     * @see close() For explicit connection closure before destruction
     * @see shutdown() For controlled shutdown of specific socket operations
     */
    ~Socket() noexcept override;

    /**
     * @brief Binds the client socket to a specific local IP address and/or port.
     * @ingroup tcp
     *
     * This method provides fine-grained control over the local endpoint from which a client socket originates.
     * It supports both IPv4 and IPv6 resolution, dual-stack addressing, and wildcard or specific local interfaces.
     *
     * ### Common Use Cases:
     * - P2P or NAT traversal clients requiring fixed source ports
     * - Multihomed systems selecting a specific local interface
     * - Unit testing with known local port bindings
     * - UDP clients that do not connect but still need a listening endpoint
     *
     * This method must be called after the socket is constructed and before `connect()`.
     * It can only be called once; repeated calls will throw.
     * Internally uses `getaddrinfo()` for resolution and retries all returned addresses until binding succeeds.
     *
     * @param[in] localHost A local IP address or hostname to bind to (e.g., "127.0.0.1" or "::1").
     * @param[in] port      Port number to bind to. Use `0` to allow the OS to auto-assign one.
     *
     * @throws SocketException if resolution or binding fails, or if the socket is already bound.
     *
     * @see connect()
     * @see setReuseAddress()
     * @see getReuseAddress()
     */
    void bind(std::string_view localHost, Port port);

    /**
     * @brief Binds the client socket to all interfaces (INADDR_ANY) using the specified port.
     * @ingroup tcp
     *
     * This overload binds to all available network interfaces using a specific local port.
     * Equivalent to calling `bind("0.0.0.0", port)`.
     *
     * @param[in] port Port number to bind to.
     *
     * @throws SocketException if binding fails or the socket is already bound.
     */
    void bind(Port port);

    /**
     * @brief Binds the client socket to all interfaces using an ephemeral port.
     * @ingroup tcp
     *
     * This overload delegates to the default configuration — bind to any interface, with any available port.
     * Equivalent to calling `bind("0.0.0.0", 0)`.
     *
     * @throws SocketException if binding fails or the socket is already bound.
     */
    void bind();

    /**
     * @brief Indicates whether the socket has been explicitly bound to a local address and/or port.
     * @ingroup tcp
     *
     * Returns `true` if the socket has successfully completed a call to one of the `bind()` overloads.
     * Binding is optional for client sockets, but is often required in advanced scenarios such as:
     * - Assigning a fixed local port for NAT traversal or predictable communication
     * - Selecting a specific local interface in multihomed environments
     * - Participating in multicast, broadcast, or P2P communication setups
     *
     * Binding must occur before the socket is connected. Attempting to bind after `connect()` will throw.
     *
     * ### Example
     * @code
     * Socket socket("example.com", 443);
     * socket.bind("0.0.0.0", 12345); // Use fixed source port
     * socket.connect();
     * @endcode
     *
     * @return `true` if the socket has been bound to a local address/port, `false` otherwise.
     *
     * @see bind()
     */
    [[nodiscard]] bool isBound() const noexcept { return _isBound; }

    /**
     * @brief Indicates whether the socket has been successfully connected to a remote host.
     * @ingroup tcp
     *
     * Returns `true` if the socket has completed a successful connection attempt using `connect()`.
     * This reflects the internal connection state as tracked by the library—it does not actively
     * probe the socket or verify that the connection is still alive at the network layer.
     *
     * Use this method to check whether it is valid to perform send/receive operations.
     * A socket transitions to the "connected" state only after a successful call to `connect()`
     * with no subsequent errors or closures.
     *
     * ### Example
     * @code
     * Socket socket("example.com", 443);
     * socket.connect();
     * if (socket.isConnected()) {
     *     socket.send(...);
     * }
     * @endcode
     *
     * @return `true` if the socket is in the connected state, `false` otherwise.
     *
     * @see connect()
     */
    [[nodiscard]] bool isConnected() const noexcept { return _isConnected; }

    /**
     * @brief Retrieves the IP address of the connected remote peer.
     * @ingroup tcp
     *
     * This method returns the remote peer's IP address in numeric form (IPv4 or IPv6).
     * If the socket is using an IPv4-mapped IPv6 address (e.g., ::ffff:192.168.1.10) and
     * @p convertIPv4Mapped is true, the result is converted to the original IPv4 form.
     *
     * @param[in] convertIPv4Mapped Whether to convert IPv4-mapped IPv6 addresses to pure IPv4.
     * @return Remote IP address as a string (e.g., "203.0.113.42" or "2001:db8::1").
     *
     * @throws SocketException if the socket is not connected or the address cannot be resolved.
     */
    [[nodiscard]] std::string getRemoteIp(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieves the port number of the connected remote peer.
     * @ingroup tcp
     *
     * This method returns the port number of the remote endpoint that this socket is connected to.
     * The result is always returned in host byte order (e.g., 443, 8080).
     *
     * @return Remote port number as a 16-bit unsigned integer.
     *
     * @throws SocketException if the socket is not connected or the port cannot be determined.
     */
    [[nodiscard]] Port getRemotePort() const;

    /**
     * @brief Retrieves the local IP address this socket is bound to.
     * @ingroup tcp
     *
     * This method returns the local interface IP address assigned to the socket,
     * in numeric string format (e.g., "127.0.0.1", "192.168.1.5", or "fe80::1").
     * If the address is an IPv4-mapped IPv6 address (::ffff:a.b.c.d) and
     * @p convertIPv4Mapped is true, it is converted to the original IPv4 form.
     *
     * @param[in] convertIPv4Mapped Whether to convert IPv4-mapped IPv6 addresses to plain IPv4.
     * @return Local IP address as a string.
     *
     * @throws SocketException if the socket is not bound or the address cannot be retrieved.
     */
    [[nodiscard]] std::string getLocalIp(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieves the local port number this socket is bound to.
     * @ingroup tcp
     *
     * This method returns the local port assigned to the socket during binding or connection.
     * The result is in host byte order and suitable for logging, diagnostics, or introspection.
     *
     * @return Local port number as a 16-bit unsigned integer.
     *
     * @throws SocketException if the socket is not bound or the port cannot be determined.
     */
    [[nodiscard]] Port getLocalPort() const;

    /**
     * @brief Retrieves the full local socket address in the form "IP:port".
     * @ingroup tcp
     *
     * This method combines the local IP address and port into a single human-readable string.
     * If the IP address is an IPv4-mapped IPv6 (e.g., ::ffff:192.0.2.1) and @p convertIPv4Mapped is true,
     * the IPv6 form is simplified to plain IPv4.
     *
     * @param[in] convertIPv4Mapped Whether to convert IPv4-mapped IPv6 addresses to pure IPv4.
     * @return A string representing the local socket address (e.g., "127.0.0.1:8080").
     *
     * @throws SocketException if the local address or port cannot be retrieved.
     */
    [[nodiscard]] std::string getLocalSocketAddress(bool convertIPv4Mapped = true) const;

    /**
     * @brief Get the remote peer's address and port as a formatted string.
     * @ingroup tcp
     *
     * Converts the remote peer's address information (stored in sockaddr_storage)
     * to a human-readable string in the format "address:port". This method supports
     * both IPv4 and IPv6 addresses and optionally handles IPv4-mapped IPv6 addresses.
     *
     * The conversion process:
     * 1. Optionally detects and converts IPv4-mapped IPv6 addresses (e.g., "::ffff:192.168.0.1")
     *    to plain IPv4 ("192.168.0.1") if `convertIPv4Mapped` is true.
     * 2. Uses getnameinfo() to convert the binary address to text.
     * 3. Formats IPv4 addresses as "x.x.x.x:port".
     * 4. Formats IPv6 addresses as "[xxxx:xxxx::xxxx]:port".
     *
     * Example outputs:
     * - IPv4: "192.168.1.1:80"
     * - IPv6: "[2001:db8::1]:80"
     * - IPv4-mapped IPv6:
     *   - With `convertIPv4Mapped = true` (default): "192.168.1.1:80"
     *   - With `convertIPv4Mapped = false`: "[::ffff:192.168.1.1]:80"
     *
     * @param[in] convertIPv4Mapped Whether to convert IPv4-mapped IPv6 addresses to pure IPv4 form. Default is true.
     * @return A string containing the formatted address and port.
     *
     * @throws SocketException If address conversion fails or the socket is invalid.
     *
     * @see addressToString() Static utility method for general address conversion.
     * @see stringToAddress() Convert string address back to binary form.
     */
    std::string getRemoteSocketAddress(bool convertIPv4Mapped = true) const;

    /**
     * @brief Establishes a TCP connection to the remote host with optional timeout control.
     * @ingroup tcp
     *
     * Attempts to connect to the remote host and port specified during `Socket` construction.
     * This method supports both blocking and non-blocking connection attempts, depending on
     * the `timeoutMillis` parameter.
     *
     * ### Connection Modes
     * - **Blocking Mode** (`timeoutMillis < 0`):
     *   - Performs a traditional blocking connect().
     *   - The call blocks until the connection succeeds or fails.
     *   - Best suited for synchronous applications.
     *
     * - **Non-blocking Mode with Timeout** (`timeoutMillis >= 0`):
     *   - Temporarily switches the socket to non-blocking mode.
     *   - Initiates a connection and waits for readiness using `select()`.
     *   - Throws an exception if the connection does not complete within the timeout period.
     *   - Restores original blocking mode automatically (RAII).
     *   - Recommended for GUI, async, or responsive applications.
     *
     * ### Implementation Details
     * - Uses `::connect()` followed by `::select()` to monitor write readiness.
     * - Once writable, uses `getsockopt(SO_ERROR)` to determine if the connection succeeded.
     * - On POSIX systems, if the socket descriptor exceeds `FD_SETSIZE`, a `SocketException` is thrown.
     * - The socket's original blocking mode is automatically restored using `ScopedBlockingMode`.
     *
     * ### Platform Notes
     * - On **POSIX**, `select()` monitors the socket using `fd_set`; the file descriptor must be `< FD_SETSIZE`
     * (typically 1024).
     * - On **Windows**, `select()` ignores its first parameter and allows descriptors up to `~2030`, but limits still
     * apply.
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     *
     * // Blocking connect (waits indefinitely)
     * sock.connect();
     *
     * // Connect with a 5-second timeout (non-blocking)
     * try {
     *     sock.connect(5000);
     * } catch (const SocketTimeoutException& timeout) {
     *     std::cerr << "Connection timed out\\n";
     * } catch (const SocketException& ex) {
     *     std::cerr << "Connect failed: " << ex.what() << "\\n";
     * }
     * @endcode
     *
     * @param[in] timeoutMillis Connection timeout in milliseconds:
     *                          - Negative: use blocking mode (no timeout)
     *                          - Zero or positive: maximum wait duration in milliseconds
     *
     * @throws SocketTimeoutException If the connection did not complete before the timeout.
     * @throws SocketException If:
     *         - No address information was resolved
     *         - The socket is invalid or closed
     *         - The connection fails (e.g., refused, unreachable, or timed out)
     *         - The descriptor exceeds platform-specific `select()` limits
     *         - `getsockopt()` reports an error after select returns success
     *
     * @note This method is **not thread-safe**. Do not call `connect()` on the same `Socket`
     *       instance concurrently from multiple threads.
     *
     * @see isConnected()  Check if the connection was established
     * @see close()        Close the socket
     * @see setNonBlocking() Permanently change blocking mode
     * @see waitReady()    Low-level readiness polling with timeout
     */
    void connect(int timeoutMillis = -1);

    /**
     * @brief Reads a fixed-size, trivially copyable object of type `T` from the socket.
     * @ingroup tcp
     *
     * Performs a binary-safe read of exactly `sizeof(T)` bytes from the socket and
     * constructs an instance of `T` from the received data. This method is optimized
     * for reading primitive types and Plain Old Data (POD) structures directly from
     * a binary stream, without any decoding or parsing overhead.
     *
     * ### Implementation Details
     * - Allocates a temporary buffer of `sizeof(T)` bytes
     * - Loops on `recv()` until all bytes are received (handles partial reads)
     * - Copies the buffer into a `std::array<std::byte, sizeof(T)>` and performs a `std::bit_cast`
     * - Enforces type constraint: `std::is_trivially_copyable_v<T>` and `std::is_standard_layout_v<T>`
     * - Performs **no** byte order conversion, alignment, or field normalization
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 9000);
     * sock.connect();
     *
     * int count = sock.read<int>();
     * double ratio = sock.read<double>();
     *
     * struct Header {
     *     uint32_t id;
     *     uint16_t flags;
     *     char tag[16];
     * };
     * Header h = sock.read<Header>();
     * @endcode
     *
     * ### Supported Types
     * - Scalar types: `int`, `float`, `double`, etc.
     * - POD structs and unions
     * - Raw byte arrays or `std::array<T, N>`
     * - Any type that satisfies `std::is_trivially_copyable`
     *
     * ### Limitations
     * - ❌ No pointer-based fields
     * - ❌ No virtual methods or dynamic memory
     * - ❌ No platform-dependent padding or ABI assumptions
     * - ❌ No endianness conversion (use utility functions to normalize fields)
     *
     * ### Thread Safety
     * - ❌ Not thread-safe: do not call concurrently on the same `Socket` instance
     * - ✅ Use separate `Socket` objects per thread, or synchronize externally
     *
     * @tparam T A trivially copyable type to read from the socket. This is enforced at compile time.
     *
     * @return A fully initialized instance of type `T` with raw data populated from the stream.
     *
     * @throws SocketException If:
     *         - The socket is closed before the full `sizeof(T)` bytes are read
     *         - A network error occurs (e.g., `ECONNRESET`, `EPIPE`, `ETIMEDOUT`)
     *         - The socket is invalid, unconnected, or interrupted
     *
     * @warning Byte Order:
     *          No endianness normalization is applied. You must manually convert fields
     *          using utilities like `fromNetwork()` or `ntohl()` if reading cross-platform data.
     *
     * @warning Memory Safety:
     *          Do not use this method with types containing pointers, virtual tables,
     *          custom allocators, or layout-dependent structures. Misuse may result in
     *          undefined behavior, security vulnerabilities, or memory corruption.
     *
     * @note This method allocates a temporary buffer for each call. It does not use
     *       or affect the internal string buffer used by methods like `readUntil()`.
     *
     * @see write()              For writing fixed-size objects to the socket
     * @see readPrefixed()       For reading length-prefixed dynamic types
     * @see setSoRecvTimeout()   To configure read timeout behavior
     * @see isConnected()        To check whether the socket is currently usable
     */
    template <typename T> [[nodiscard]] T read()
    {
        static_assert(std::is_trivially_copyable_v<T>, "Socket::read<T>() requires a trivially copyable type");
        static_assert(std::is_standard_layout_v<T>, "Socket::read<T>() requires a standard layout type");

        std::array<std::byte, sizeof(T)> buffer{};
        std::size_t totalRead = 0;
        std::size_t remaining = sizeof(T);

        while (remaining > 0)
        {
            const auto len = ::recv(getSocketFd(), reinterpret_cast<char*>(buffer.data()) + totalRead,
#ifdef _WIN32
                                    static_cast<int>(remaining),
#else
                                    remaining,
#endif
                                    0);

            if (len == SOCKET_ERROR)
                throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

            if (len == 0)
                throw SocketException("Connection closed by remote host before full object was received.");

            totalRead += static_cast<std::size_t>(len);
            remaining -= static_cast<std::size_t>(len);
        }

        return std::bit_cast<T>(buffer);
    }

    /**
     * @brief Reads exactly `n` bytes from the socket into a `std::string`.
     * @ingroup tcp
     *
     * Performs a fully blocking read that continues reading from the socket
     * until the specified number of bytes (`n`) has been received, or an error
     * or disconnection occurs. This method guarantees exact-length delivery
     * and is suitable for fixed-length binary protocols or framed data.
     *
     * Internally, it repeatedly calls `recv()` as needed to handle partial reads.
     * It allocates a string of size `n` and fills it directly with received data
     * in order, preserving byte order and content integrity.
     *
     * ### Implementation Details
     * - Uses a loop around `recv()` to read remaining bytes
     * - Handles short reads and interruptions transparently
     * - Allocates result string up front (no resizing or reallocation)
     * - Does **not** interpret content — binary-safe and endian-agnostic
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * try {
     *     // Read an exact-length header
     *     std::string header = sock.readExact(16);
     *
     *     // Read an entire frame of known size
     *     std::string frame = sock.readExact(1024);
     *     assert(frame.size() == 1024);
     * } catch (const SocketException& ex) {
     *     std::cerr << "Read failed: " << ex.what() << std::endl;
     * }
     * @endcode
     *
     * @param[in] n The number of bytes to read. Must be greater than zero.
     *
     * @return A `std::string` containing exactly `n` bytes of data received from the socket.
     *
     * @throws SocketException If:
     *         - The peer closes the connection before `n` bytes are received
     *         - A read timeout occurs (e.g. `ETIMEDOUT`)
     *         - A network error or signal interruption occurs
     *         - The socket is invalid or unconnected (e.g. `EBADF`)
     *         - Memory allocation fails (`std::bad_alloc`)
     *
     * @note This method blocks until all `n` bytes are read or an error occurs.
     *       It is not suitable for variable-length or streaming reads.
     * @note This method returns an empty string if n == 0.
     *
     * @see read<std::string>() For best-effort, buffered reads
     * @see read<T>() For fixed-size reads of trivially copyable types
     * @see readUntil() For delimiter-based reads (e.g. lines or tokens)
     * @see setSoRecvTimeout() To configure blocking behavior
     */
    std::string readExact(std::size_t n) const;

    /**
     * @brief Reads data from the socket until a specified delimiter character is encountered.
     * @ingroup tcp
     *
     * Reads and accumulates data from the socket until the given delimiter character is found,
     * using the internal buffer for efficient, chunked `recv()` calls. Unlike byte-at-a-time
     * reads, this method minimizes syscall overhead and improves performance by reading in
     * larger blocks and scanning for the delimiter in memory.
     *
     * If the delimiter is found, the method returns a string containing all bytes up to (and
     * optionally including) the delimiter. If the delimiter is not found within `maxLen` bytes,
     * an exception is thrown to prevent unbounded growth or protocol desynchronization.
     *
     * ### Implementation Details
     * - Uses `_internalBuffer` for buffered reads (size configurable via `setInternalBufferSize()`)
     * - Performs repeated `recv()` calls to fill the buffer and search for the delimiter
     * - Tracks and accumulates received data until the delimiter is located
     * - Supports truncation or inclusion of the delimiter via `includeDelimiter`
     * - Throws on early connection close or delimiter absence beyond `maxLen`
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Read a line terminated by newline
     * std::string line = sock.readUntil('\n');
     *
     * // Read CSV field terminated by comma
     * std::string field = sock.readUntil(',', 1024);
     *
     * // Exclude the delimiter from the result
     * std::string text = sock.readUntil(';', 8192, false);
     * @endcode
     *
     * @param[in] delimiter The character to search for in the incoming data stream.
     * @param[in] maxLen The maximum number of bytes to read before giving up (default: 8192).
     * @param[in] includeDelimiter Whether to include the delimiter in the returned string (default: true).
     *
     * @return A `std::string` containing the data read up to the delimiter.
     *         The result includes the delimiter if `includeDelimiter` is set to `true`.
     *
     * @throws SocketException If:
     *         - The connection is closed before the delimiter is received
     *         - `maxLen` bytes are read without finding the delimiter
     *         - A network or system error occurs during reading
     *         - A configured timeout is exceeded (e.g., `SO_RCVTIMEO`)
     *         - The socket is invalid or in an error state
     *
     * @note This method is fully blocking unless a timeout is configured.
     *       Use `readLine()` as a newline-specific shorthand.
     *
     * @see readLine() For newline-terminated text input
     * @see readExact() For reading fixed-length binary data
     * @see read() For fixed-size or generic reads
     * @see setInternalBufferSize() To configure buffer size for bulk `recv()`
     * @see setSoRecvTimeout() To control read timeout duration
     */
    std::string readUntil(char delimiter, std::size_t maxLen = 8192, bool includeDelimiter = true);

    /**
     * @brief Reads a line terminated by '\n' from the socket.
     * @ingroup tcp
     *
     * This is a convenience method that calls readUntil('\n', maxLen, includeDelimiter).
     * It provides backward compatibility and a more intuitive interface for reading
     * newline-terminated data.
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Read a line terminated by newline
     * std::string line = sock.readLine();
     *
     * // Read without including the newline
     * std::string data = sock.readLine(8192, false);
     * @endcode
     *
     * @param[in] maxLen Maximum number of bytes to read (default: 8192)
     * @param[in] includeDelimiter Whether to include the newline in the returned string (default: true)
     * @return std::string containing the line read
     * @throws SocketException on error or if line exceeds maxLen
     *
     * @see readUntil() The underlying implementation
     * @see read() For general-purpose reading
     * @see readExact() For fixed-length reads
     */
    std::string readLine(const std::size_t maxLen = 8192, const bool includeDelimiter = true)
    {
        return readUntil('\n', maxLen, includeDelimiter);
    }

    /**
     * @brief Performs a single best-effort read of up to `n` bytes from the socket.
     * @ingroup tcp
     *
     * This method attempts to read up to `n` bytes from the socket using a single `recv()` call.
     * It is designed for responsiveness: it returns as soon as any data is available—whether
     * that's the full `n` bytes, fewer bytes, or even zero if the connection was closed.
     *
     * This is a low-overhead, non-looping read ideal for event-driven designs, polling, or
     * non-blocking sockets. It differs from `readExact()` in that it does not retry or wait
     * for the full amount of data.
     *
     * ### Implementation Details
     * - Calls `recv()` exactly once with a preallocated buffer
     * - Returns available data immediately (up to `n` bytes)
     * - Resizes the result to the actual number of bytes read
     * - Throws on socket errors or if the connection is closed
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Try to read up to 512 bytes
     * std::string chunk = sock.readAtMost(512);
     * if (!chunk.empty()) {
     *     process(chunk);
     * }
     * @endcode
     *
     * @param[in] n Maximum number of bytes to read in one call. Must be greater than 0.
     *
     * @return A `std::string` containing 0 to `n` bytes of data. An empty string means
     *         the connection was closed or nothing was available (e.g., non-blocking mode).
     *
     * @throws SocketException If:
     *         - The socket is invalid, disconnected, or closed (`EBADF`, `ENOTCONN`, etc.)
     *         - A network error occurs during the `recv()` operation
     *         - The connection was closed by the peer (`recv()` returned 0)
     *         - Memory allocation fails during buffer allocation
     *
     * @note Unlike `readExact()`, this method does not retry or loop. Use with care in protocols
     *       that depend on strict byte counts.
     *
     * @see readExact() For guaranteed-length blocking reads
     * @see readUntil() For delimiter-based parsing
     * @see setNonBlocking() To configure socket non-blocking behavior
     * @see setSoRecvTimeout() To configure read timeout
     */
    std::string readAtMost(std::size_t n) const;

    /**
     * @brief Reads available data from the socket into the provided buffer.
     * @ingroup tcp
     *
     * Performs a "best-effort" read operation by attempting to read up to len bytes
     * from the socket into the provided buffer. This method makes a single recv() call
     * and returns immediately with whatever data is available, which may be less than
     * the requested length.
     *
     * ### Implementation Details
     * - Uses recv() internally with a single call
     * - Returns immediately with available data
     * - Does not guarantee full buffer will be filled
     * - Non-blocking mode affects behavior
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * char buffer[1024];
     * size_t bytesRead = sock.readInto(buffer, sizeof(buffer));
     * if (bytesRead > 0) {
     *     // Process bytesRead bytes from buffer
     * }
     * @endcode
     *
     * @param[out] buffer Pointer to pre-allocated memory buffer to store read data.
     *                    Must be valid and large enough for len bytes.
     * @param[in] len Maximum number of bytes to read (buffer size)
     *
     * @return Number of bytes actually read (may be less than len)
     *
     * @throws SocketException If:
     *         - Socket is invalid (EBADF)
     *         - Connection closed by peer
     *         - Memory access error (EFAULT)
     *         - Network errors occur
     *
     * @see readIntoExact() For guaranteed full-length reads
     * @see read() Template method for type-safe reads
     * @see readUntil() For delimiter-based reading
     */
    std::size_t readInto(void* buffer, const std::size_t len) const { return readIntoInternal(buffer, len, false); }

    /**
     * @brief Reads exactly `len` bytes into the given buffer (looped recv).
     * @ingroup tcp
     *
     * This method guarantees to read exactly the specified number of bytes from the socket
     * into the provided buffer. It will continue reading until either all requested bytes
     * are received or an error occurs. This is useful when reading fixed-length protocol
     * messages or binary data structures where partial reads are not acceptable.
     *
     * ### Implementation Details
     * - Uses readIntoInternal() with exact=true
     * - Loops until all requested bytes are read
     * - Handles partial reads internally
     * - Throws if connection closes before all bytes received
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Read a fixed-size header
     * struct Header {
     *     uint32_t messageType;
     *     uint32_t payloadLength;
     * };
     * Header header;
     * sock.readIntoExact(&header, sizeof(header));
     *
     * // Read exact payload length
     * std::vector<char> payload(header.payloadLength);
     * sock.readIntoExact(payload.data(), payload.size());
     * @endcode
     *
     * @param[out] buffer Pointer to pre-allocated memory where data should be written.
     *                    Must be valid and large enough for len bytes.
     * @param[in] len Number of bytes to read. Method won't return until
     *                exactly this many bytes are read or an error occurs.
     *
     * @return Number of bytes actually read (always equal to len on success)
     *
     * @throws SocketException If:
     *         - Connection closes before len bytes received
     *         - Socket error occurs during read
     *         - Memory access error (EFAULT)
     *         - Socket is invalid or not connected
     *         - Timeout occurs (if configured)
     *
     * @see readInto() For "best-effort" reads that may return partial data
     * @see read() Template method for type-safe reads
     * @see readIntoInternal() Internal implementation
     */
    std::size_t readIntoExact(void* buffer, const std::size_t len) const { return readIntoInternal(buffer, len, true); }

    /**
     * @brief Attempts a best-effort read of up to `n` bytes with a timeout constraint.
     * @ingroup tcp
     *
     * Waits for the socket to become readable for up to `timeoutMillis` milliseconds,
     * then performs a single `recv()` call to read up to `n` bytes of available data.
     * This method is useful for polling-style or time-sensitive I/O operations where
     * immediate responsiveness is more important than full data delivery.
     *
     * Unlike `readExact()`, this method does not loop or retry to satisfy the full size.
     * It returns as soon as data is available, or throws a `SocketTimeoutException` if
     * no data arrives within the specified timeout window.
     *
     * ### Implementation Details
     * - Calls `waitReady(false)` to wait for data readiness
     * - Performs a single `recv()` once readable
     * - Returns the available data immediately (may be fewer than `n` bytes)
     * - Throws on timeout, disconnection, or error
     * - Returned string is resized to match actual data length
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * try {
     *     // Try to read up to 1024 bytes with a 5-second timeout
     *     std::string data = sock.readAtMostWithTimeout(1024, 5000);
     *     std::cout << "Read " << data.size() << " bytes\n";
     * } catch (const SocketTimeoutException& timeoutEx) {
     *     std::cerr << "Timeout: " << timeoutEx.what() << std::endl;
     * } catch (const SocketException& ex) {
     *     std::cerr << "Socket error: " << ex.what() << std::endl;
     * }
     * @endcode
     *
     * @param[in] n Maximum number of bytes to read in one operation. Must be > 0.
     * @param[in] timeoutMillis Maximum duration to wait for readability, in milliseconds:
     *                          - `> 0`: Wait up to this time
     *                          - `0`: Poll without blocking
     *                          - `< 0`: Invalid; throws `SocketException`
     *
     * @return A `std::string` containing between 1 and `n` bytes of data received.
     *
     * @throws SocketTimeoutException If the socket remains unreadable beyond `timeoutMillis`.
     * @throws SocketException If:
     *         - The socket is invalid or closed
     *         - The peer disconnects (`recv() == 0`)
     *         - A network/system error occurs during `recv()`
     *         - Memory allocation fails
     *
     * @note This method is ideal for polling and timeout-aware reads where partial delivery is acceptable.
     *       For guaranteed reads, use `readExact()`. For immediate best-effort reads without timeout,
     *       use `readAtMost()`.
     *
     * @see readExact() For guaranteed full-length reads
     * @see readAtMost() For best-effort reads without blocking
     * @see waitReady() To wait for readiness explicitly
     * @see setSoRecvTimeout() To set socket-level timeout defaults
     */
    std::string readAtMostWithTimeout(std::size_t n, int timeoutMillis) const;

    /**
     * @brief Reads a length-prefixed payload using a fixed-size prefix type.
     * @ingroup tcp
     *
     * Reads a message that consists of a length prefix followed by a variable-length payload.
     * The prefix type `T` determines the format and size of the length field. This method is
     * useful for protocols that encode message length as a fixed-size integer header.
     *
     * ### Implementation Details
     * - First reads `sizeof(T)` bytes as the length prefix
     * - Converts the prefix from **network byte order** to **host byte order** using `net::fromNetwork()`
     * - Then reads exactly that many bytes as the payload
     * - Length prefix must be a trivially copyable unsigned integral type (e.g., `uint16_t`, `uint32_t`)
     * - Payload is returned as a `std::string` (binary-safe)
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * try {
     *     // Read message with 32-bit length prefix
     *     std::string msg = sock.readPrefixed<uint32_t>();
     *
     *     // Read message with 16-bit length prefix
     *     std::string shortMsg = sock.readPrefixed<uint16_t>();
     *
     *     // Read multiple length-prefixed messages
     *     while (sock.isConnected()) {
     *         std::string data = sock.readPrefixed<uint32_t>();
     *         process(data);
     *     }
     * } catch (const SocketException& ex) {
     *     // Handle read errors
     * }
     * @endcode
     *
     * ### Protocol Format
     * <pre>
     * +----------------+----------------------+
     * | Length (T)     | Payload (n bytes)   |
     * +----------------+----------------------+
     * |<- sizeof(T) ->|<---- length ------->|
     * </pre>
     *
     * @tparam T The unsigned integral type used for the length prefix (e.g., `uint32_t`).
     *           Must be a trivially copyable type.
     *
     * @return The payload as a `std::string`, excluding the length prefix.
     *
     * @throws SocketException If:
     *         - Connection is closed while reading
     *         - Length prefix is invalid/corrupt
     *         - Not enough data is available to complete the read
     *         - Memory allocation fails
     *         - Any network error occurs
     *
     * @note The prefix is assumed to be encoded in **network byte order** and is converted
     *       automatically to host byte order using `jsocketpp::net::fromNetwork()`. You must
     *       ensure that the sender uses the same byte ordering.
     *
     * @see read()          For reading raw fixed-size values
     * @see readExact()     For reading an exact number of bytes
     * @see writePrefixed() To send matching length-prefixed data
     * @see net::fromNetwork() For details on byte order conversion
     */
    template <typename T> std::string readPrefixed()
    {
        static_assert(std::is_integral_v<T> && std::is_unsigned_v<T> && std::is_trivially_copyable_v<T>,
                      "Prefix type must be a trivially copyable unsigned integral type");

        T netLen = read<T>();
        T length = net::fromNetwork(netLen);
        return readExact(static_cast<std::size_t>(length));
    }

    /**
     * @brief Reads a length-prefixed message with an upper bound check.
     * @ingroup tcp
     *
     * Reads a message that consists of a length prefix followed by a variable-length payload.
     * This overload adds protection by validating that the decoded length does not exceed a
     * specified maximum (`maxPayloadLen`), helping prevent corrupted or maliciously large payloads.
     *
     * The prefix type `T` determines the format and size of the length field and is decoded
     * in **network byte order** using `net::fromNetwork()`.
     *
     * ### Implementation Details
     * - Reads `sizeof(T)` bytes as the length prefix
     * - Converts the prefix from **network byte order** to host byte order
     * - Throws an exception if the length exceeds `maxPayloadLen`
     * - Reads exactly `length` bytes as the payload
     * - Returns the payload as a binary-safe `std::string`
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * try {
     *     std::string msg = sock.readPrefixed<uint32_t>(1024); // Rejects messages > 1024 bytes
     *     process(msg);
     * } catch (const SocketException& ex) {
     *     // Handle read errors
     * }
     * @endcode
     *
     * ### Protocol Format
     * <pre>
     * +----------------+----------------------+
     * | Length (T)     | Payload (n bytes)   |
     * +----------------+----------------------+
     * |<- sizeof(T) ->|<---- length ------->|
     * </pre>
     *
     * @tparam T The unsigned integral type used for the length prefix (e.g., `uint32_t`).
     *           Must be a trivially copyable type.
     *
     * @param[in] maxPayloadLen Maximum allowed length of the decoded payload in bytes.
     *                      If the decoded prefix exceeds this, an exception is thrown.
     *
     * @return The payload as a `std::string`, excluding the length prefix.
     *
     * @throws SocketException If:
     *         - The connection is closed while reading
     *         - The length prefix is corrupt or invalid
     *         - The decoded length exceeds `maxPayloadLen`
     *         - Not enough data is available to fulfill the read
     *         - Memory allocation fails
     *         - Any network error occurs
     *
     * @note The prefix is automatically converted from **network byte order** to host byte order
     *       using `jsocketpp::net::fromNetwork()`. The sender must use the corresponding network encoding.
     *
     * @see read()            For reading fixed-size types
     * @see readExact()       For reading a known number of bytes
     * @see writePrefixed()   For sending length-prefixed messages
     * @see net::fromNetwork() For byte order decoding
     */
    template <typename T> std::string readPrefixed(const std::size_t maxPayloadLen)
    {
        static_assert(std::is_integral_v<T> && std::is_trivially_copyable_v<T>,
                      "Prefix type must be a trivially copyable integral type");

        T netLen = read<T>();
        T length = net::fromNetwork(netLen);

        const auto payloadLen = static_cast<std::size_t>(length);
        if (payloadLen > maxPayloadLen)
        {
            throw SocketException("readPrefixed: Prefix length " + std::to_string(payloadLen) +
                                  " exceeds maximum allowed payload length of " + std::to_string(maxPayloadLen));
        }

        return readExact(payloadLen);
    }

    /**
     * @brief Reads all bytes currently available on the socket without blocking.
     * @ingroup tcp
     *
     * Performs a best-effort, non-blocking read of all data that is already available in the
     * socket's input buffer. This method uses platform-specific mechanisms (e.g., FIONREAD or ioctl)
     * to query how many bytes can be read without blocking, then performs a single `recv()` call
     * to consume and return that data.
     *
     * This method is useful in event-driven or polling-based systems where you want to quickly
     * drain the socket buffer without waiting for more data to arrive.
     *
     * ### Implementation Details
     * - Uses ioctl (or ioctlsocket on Windows) with FIONREAD to determine readable bytes
     * - If no data is available, returns an empty string immediately
     * - Performs one recv() call up to the number of available bytes
     * - Avoids dynamic memory allocation if no data is available
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 1234);
     * sock.connect();
     *
     * while (sock.isConnected()) {
     *     std::string data = sock.readAvailable();
     *     if (!data.empty()) {
     *         handleChunk(data);
     *     }
     * }
     * @endcode
     *
     * @return A string containing 0 or more bytes, depending on what was available at the time.
     *
     * @throws SocketException If:
     *         - ioctl/FIONREAD fails
     *         - recv() fails due to network error
     *         - Connection is closed unexpectedly
     *
     * @note This method does not wait or retry. If you want timeout-based behavior,
     *       use readAtMostWithTimeout() instead.
     *
     * @see readAtMostWithTimeout() For time-bounded partial reads
     * @see read() For fixed-size reads
     * @see readExact() For guaranteed-length reads
     * @see waitReady() To wait for socket readiness before calling this
     */
    std::string readAvailable() const;

    /**
     * @brief Reads all currently available bytes into the provided buffer without blocking.
     * @ingroup tcp
     *
     * This method checks how many bytes are available for reading on the socket using
     * platform-specific ioctl/FIONREAD mechanisms, and reads as many bytes as possible
     * (up to the specified buffer size) into the provided memory buffer. It does not block
     * or wait for additional data to arrive.
     *
     * This is the low-level, zero-copy variant of readAvailable(), ideal for high-performance
     * applications and protocol parsers that manage their own memory buffers.
     *
     * ### Implementation Details
     * - Uses FIONREAD (Windows or POSIX) to determine available bytes
     * - Performs one recv() call into `buffer` for min(bufferSize, available)
     * - Returns number of bytes read
     * - Does nothing if no data is available
     *
     * ### Example Usage
     * @code{.cpp}
     * char buf[2048];
     * std::size_t received = sock.readIntoAvailable(buf, sizeof(buf));
     * if (received > 0) {
     *     parse(buf, received);
     * }
     * @endcode
     *
     * @param[out] buffer Pointer to the memory where received data will be stored.
     * @param[in] bufferSize Maximum number of bytes to store in the buffer.
     * @return The number of bytes actually read (may be 0 if no data is available).
     *
     * @throws SocketException If:
     *         - Socket is invalid
     *         - ioctl/FIONREAD fails
     *         - recv() fails
     *         - Connection is closed during read
     *
     * @note This method does not allocate memory and is safe to call frequently.
     *       For a string-returning variant, use readAvailable().
     *
     * @see readAvailable() For dynamic buffer version
     * @see readAtMostWithTimeout() For time-limited reads
     * @see readInto() For controlled-length reading (blocking)
     */
    std::size_t readIntoAvailable(void* buffer, std::size_t bufferSize) const;

    /**
     * @brief Performs a vectorized read into multiple buffers using a single system call.
     * @ingroup tcp
     *
     * Reads data into the specified sequence of buffers using scatter/gather I/O.
     * This method fills each buffer in order and returns the total number of bytes read.
     * It is the counterpart to `writev()` and uses `readv()` or `WSARecv()` internally.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::byte, 4> header;
     * std::array<std::byte, 128> payload;
     * std::array<BufferView, 2> views = {
     *     BufferView{header.data(), header.size()},
     *     BufferView{payload.data(), payload.size()}
     * };
     * std::size_t received = sock.readv(views);
     * @endcode
     *
     * @param[out] buffers A span of BufferView objects describing writable regions.
     * @return The total number of bytes read into the buffer sequence.
     *
     * @throws SocketException If:
     *         - The socket is invalid
     *         - recv() or readv() fails
     *         - Connection is closed prematurely
     *
     * @note This method performs a single recv()-style call. It does not retry.
     *       For full delivery into multiple buffers, use `readvAll()` (future).
     *
     * @see writev() For the write-side equivalent
     * @see readInto() For single-buffer reading
     */
    std::size_t readv(std::span<BufferView> buffers) const;

    /**
     * @brief Reads exactly the full contents of all provided buffers.
     * @ingroup tcp
     *
     * This method performs a reliable scatter read operation. It guarantees that
     * all bytes described by the buffer span are filled by repeatedly calling
     * `readv()` until the entire memory region is received or an error occurs.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::byte, 4> header;
     * std::vector<std::byte> payload(1024);
     *
     * std::array<BufferView, 2> views = {
     *     BufferView{header.data(), header.size()},
     *     BufferView{payload.data(), payload.size()}
     * };
     *
     * sock.readvAll(views);
     * @endcode
     *
     * @param[out] buffers A span of writable buffer views to fill completely.
     * @return Total number of bytes read (equal to sum of buffer sizes).
     *
     * @throws SocketException If:
     *         - The connection is closed before all data is read
     *         - A system or network error occurs
     *
     * @note This is a high-level, blocking call. For partial reads, use `readv()`.
     * @see readv() For non-retrying variant
     */
    std::size_t readvAll(std::span<BufferView> buffers) const;

    /**
     * @brief Reads exactly the full contents of all buffers within a timeout.
     * @ingroup tcp
     *
     * This method performs a series of vectorized `readv()` calls to fill the provided
     * `BufferView` span until all bytes are read or the total timeout expires. A steady
     * clock is used to measure elapsed time across multiple system calls, ensuring the
     * full operation adheres to the timeout limit.
     *
     * If not all data is read before the timeout elapses, a `SocketTimeoutException` is thrown.
     *
     * ### Implementation Details
     * - Performs repeated `readv()` calls into pending buffers.
     * - Tracks time remaining using `std::chrono::steady_clock`.
     * - Waits for readability before each attempt using `waitReady()`.
     * - Automatically handles partially filled buffers.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::byte, 4> header;
     * std::vector<std::byte> payload(1024);
     * std::array<BufferView, 2> views = {
     *     BufferView{header.data(), header.size()},
     *     BufferView{payload.data(), payload.size()}
     * };
     * sock.readvAllWithTotalTimeout(views, 2000); // Must finish in 2 seconds
     * @endcode
     *
     * @param[out] buffers Span of BufferView objects describing writable memory regions.
     * @param[in] timeoutMillis Maximum allowed duration for the entire read operation, in milliseconds.
     *
     * @return The total number of bytes read (must equal the sum of all buffer sizes on success).
     *
     * @throws SocketTimeoutException If:
     *         - Not all data is read before the timeout expires.
     *         - The socket does not become readable in time.
     *
     * @throws SocketException If:
     *         - `readv()` or `recv()` fails due to network error.
     *         - The socket is invalid or disconnected.
     *         - The connection is closed before all bytes are received.
     *
     * @note This method is fully blocking (until timeout or completion).
     *       Use `readv()` for a single read attempt, or `readvAll()` for a non-timed retry loop.
     *
     * @see readvAll() For retrying without a timeout
     * @see readv() For single-attempt best-effort vectorized reads
     * @see SocketTimeoutException For timeout-specific error handling
     */
    std::size_t readvAllWithTotalTimeout(std::span<BufferView> buffers, int timeoutMillis) const;

    /**
     * @brief Attempts a single vectorized read into multiple buffers with a timeout.
     * @ingroup tcp
     *
     * Waits up to `timeoutMillis` milliseconds for the socket to become readable,
     * then performs a single `readv()` operation into the provided buffer views.
     * This method does not retry on partial reads and is suitable for polling-style
     * I/O where responsiveness is more important than completeness.
     *
     * May read fewer bytes than the total available buffer capacity, depending on
     * what the socket delivers in a single system call. This is a timeout-aware variant
     * of `readv()` and should be used when non-blocking responsiveness is desired.
     *
     * ### Implementation Details
     * - Calls `waitReady()` to wait for the socket to become readable.
     * - Performs one `readv()` attempt only.
     * - Reads whatever data is immediately available.
     * - Returns early on connection closure or any received data.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::byte, 8> header;
     * std::array<std::byte, 256> body;
     *
     * std::array<BufferView, 2> bufs = {
     *     BufferView{header.data(), header.size()},
     *     BufferView{body.data(), body.size()}
     * };
     *
     * std::size_t n = sock.readvAtMostWithTimeout(bufs, 300); // Wait up to 300 ms
     * std::cout << "Received " << n << " bytes.\n";
     * @endcode
     *
     * @param[out] buffers Span of writable `BufferView`s to receive incoming data.
     * @param[in] timeoutMillis Time in milliseconds to wait for readability:
     *                      - > 0: Wait up to this duration
     *                      - 0: Non-blocking (poll)
     *                      - < 0: Invalid; throws exception
     *
     * @return Number of bytes read (may be 0 if timeout is 0 or nothing available).
     *
     * @throws SocketTimeoutException If the socket does not become readable before timeout.
     * @throws SocketException If:
     *         - `readv()` fails with a system/network error
     *         - The socket is invalid
     *         - The connection is closed
     *
     * @note This method performs a single system call. It does not retry or block for more data.
     *
     * @see readv() For non-timed scatter read
     * @see readvAllWithTotalTimeout() For complete delivery within a time budget
     * @see SocketTimeoutException For timeout-related error details
     */
    std::size_t readvAtMostWithTimeout(std::span<BufferView> buffers, int timeoutMillis) const;

    /**
     * @brief Peeks at incoming data without consuming it.
     * @ingroup tcp
     *
     * This method performs a non-destructive read of up to `n` bytes using the `MSG_PEEK` flag.
     * It allows inspecting the contents of the socket's receive buffer without removing the
     * data from the queue. This is useful for implementing lookahead parsing, protocol sniffing,
     * or waiting for specific patterns before consuming data.
     *
     * ### Implementation Details
     * - Performs a single `recv()` call with `MSG_PEEK`
     * - Does not remove any bytes from the socket buffer
     * - May return fewer bytes than requested if less data is available
     * - Does not block indefinitely if the socket is non-blocking or has a timeout
     *
     * ### Example Usage
     * @code{.cpp}
     * std::string preview = sock.peek(4);
     * if (preview == "PING") {
     *     std::string full = sock.readExact(4);
     *     handlePing(full);
     * }
     * @endcode
     *
     * @param[in] n Maximum number of bytes to peek at.
     * @return A string containing up to `n` bytes from the receive buffer.
     *
     * @throws SocketException If:
     *         - The socket is invalid
     *         - recv() fails
     *         - Connection is closed unexpectedly
     *
     * @note If you want to discard data after peeking, use `read()` or `discard()` afterward.
     *
     * @see readExact() To consume data after peek
     * @see readAvailable() For full-buffer inspection and consumption
     * @see discard() To remove bytes without copying them (future)
     */
    std::string peek(std::size_t n) const;

    /**
     * @brief Discards exactly `n` bytes from the socket by reading and discarding them.
     * @ingroup tcp
     *
     * This method reads and discards `n` bytes from the socket without returning any data.
     * It is useful in scenarios where part of the stream should be skipped (e.g., headers,
     * fixed-length preambles, corrupted payloads).
     *
     * The discard operation is performed using a temporary buffer of configurable size,
     * which defaults to 1024 bytes. If the chunk size is 0, the method throws an exception.
     *
     * @param[in] n Number of bytes to discard. Must be greater than 0.
     * @param[in] chunkSize Size (in bytes) of the temporary buffer used for reading/discarding. Default is 1024.
     *
     * @throws SocketException If:
     *         - The socket is invalid
     *         - An error occurs during `recv()`
     *         - The connection is closed before all `n` bytes are discarded
     *         - `chunkSize` is 0
     *
     * @note For optimal performance on high-throughput streams or large discards,
     * consider using a larger chunk size (e.g., 4096 or 8192).
     */
    void discard(std::size_t n, std::size_t chunkSize = 1024) const;

    /**
     * @brief Closes the socket connection and releases associated resources.
     * @ingroup tcp
     *
     * This method performs an orderly shutdown and closure of the socket connection:
     * 1. Closes the underlying socket descriptor
     * 2. Releases address information resources
     * 3. Resets internal state
     *
     * The method ensures proper cleanup even if the socket is already closed.
     * After calling close(), the Socket object remains valid but disconnected
     * (isValid() will return false).
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     * sock.connect();
     * // ... use socket ...
     * sock.close();  // Explicitly close when done
     * @endcode
     *
     * ### Resource Management
     * - Socket is automatically closed in destructor if not done explicitly
     * - Safe to call multiple times (subsequent calls have no effect)
     * - Frees system resources immediately rather than waiting for destruction
     *
     * @throws SocketException If the close operation fails due to:
     *         - System resource errors
     *         - Invalid socket state
     *         - Platform-specific network errors
     *
     * @note This method is not thread-safe. Do not call close() while other
     *       threads are performing operations on the same Socket instance.
     *
     * @see shutdown() For finer control over connection termination
     * @see ~Socket() Destructor that automatically closes the socket
     * @see isValid() Check if socket is still valid after closing
     */
    void close();

    /**
     * @brief Shutdown specific communication aspects of the socket.
     * @ingroup tcp
     *
     * This method allows for a controlled shutdown of socket communications in one or both
     * directions without closing the socket itself. Unlike close(), which immediately
     * terminates all communications, shutdown() provides finer control over how the
     * connection is terminated.
     *
     * ### Shutdown Modes
     * - **ShutdownMode::Read**: Disables further receive operations
     *   - Subsequent read() calls will return EOF
     *   - Already received data can still be read
     *   - Send operations remain unaffected
     *
     * - **ShutdownMode::Write**: Disables further send operations
     *   - Sends pending data before shutting down
     *   - Sends FIN packet to peer
     *   - Receive operations remain unaffected
     *
     * - **ShutdownMode::Both**: Disables both send and receive operations (full-duplex shutdown)
     *   - Equivalent to calling shutdown() with Read and Write modes
     *   - Unlike `close()`, this does **not** release the socket descriptor or system resources.
     *   - The socket remains open and can still be queried, reused, or closed later.
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     * sock.connect();
     *
     * // Send final message
     * sock.write("Goodbye!");
     *
     * // Shutdown sending but continue receiving
     * sock.shutdown(ShutdownMode::Write);
     *
     * // Read any remaining responses
     * std::string response = sock.read<std::string>();
     *
     * // Complete shutdown
     * sock.shutdown(ShutdownMode::Both);
     * @endcode
     *
     * ### Implementation Details
     * - Uses platform-specific shutdown() system call
     * - Handles partial shutdowns gracefully
     * - Does not release socket resources (use close() for that)
     * - Socket remains valid after shutdown
     *
     * @param[in] how Specifies which operations to shut down:
     *                - ShutdownMode::Read: Disable receiving
     *                - ShutdownMode::Write: Disable sending
     *                - ShutdownMode::Both: Disable both (default)
     *
     * @throws SocketException If:
     *         - Socket is invalid (EBADF)
     *         - Socket is not connected (ENOTCONN)
     *         - Permission denied (EACCES)
     *         - Memory/resource allocation fails
     *         - Other system-specific errors
     *
     * @note This method is not thread-safe. Do not call shutdown() while other
     *       threads are performing operations on the same Socket instance.
     *
     * @see close() For completely closing the socket
     * @see isConnected() Check connection status
     * @see ShutdownMode Enumeration of available shutdown modes
     */
    void shutdown(ShutdownMode how) const;

    /**
     * @brief Sends data to the socket using a single, best-effort write operation.
     * @ingroup tcp
     *
     * Attempts to write up to `message.size()` bytes from the given `std::string_view` to the
     * connected socket. This method may return before all data is sent, depending on socket
     * state, system buffer availability, and platform behavior. It performs exactly one `send()`
     * call and does **not** retry if the write is partial.
     *
     * This is the fundamental low-level output method. For complete delivery guarantees, use
     * `writeAll()` or `writeWithTotalTimeout()` instead.
     *
     * ### Implementation Details
     * - Performs a single call to `send()` (platform-specific: `send()` or `send_s()` on Windows)
     * - Supports binary data, including embedded null bytes (`\0`)
     * - Returns the number of bytes actually written (may be less than `message.size()`)
     * - This method does not retry; for guaranteed delivery use writeAll().
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     * sock.connect();
     *
     * std::string payload = "POST /data HTTP/1.1\r\n\r\n";
     * std::size_t sent = sock.write(payload);
     *
     * if (sent < payload.size()) {
     *     std::cerr << "Partial write: sent " << sent << " of " << payload.size() << " bytes\n";
     *     // Handle retries or buffering as needed
     * }
     * @endcode
     *
     * @param[in] message The data to send. Can contain binary content, including null characters.
     *
     * @return The number of bytes actually written to the socket.
     *         May be less than `message.size()` for partial writes.
     *
     * @throws SocketException If:
     *         - The socket is invalid, closed, or unconnected (`ENOTCONN`, `EBADF`)
     *         - A network error occurs (`ECONNRESET`, `ENETDOWN`, `EPIPE`)
     *         - The operation would block on a full buffer in non-blocking mode (`EWOULDBLOCK`)
     *         - A system-level error occurs (`EINTR`, `EINVAL`, etc.)
     *
     * @note [[nodiscard]] is applied to enforce that the return value must be checked.
     *       Ignoring the result may lead to silent data loss if only part of the data was sent.
     *       Always compare against `message.size()` to detect partial delivery.
     *
     * @see writeAll() For guaranteed delivery via retries
     * @see writeWithTotalTimeout() For deadline-bounded full writes
     * @see writeFrom() For zero-copy buffer-based sending
     * @see writev() For vectorized scatter/gather writes
     * @see setNonBlocking() To configure non-blocking socket behavior
     * @see setSoSendTimeout() To configure write timeouts
     */
    [[nodiscard]] std::size_t write(std::string_view message) const;

    /**
     * @brief Writes the entire contents of a message to the socket, retrying as needed.
     * @ingroup tcp
     *
     * Ensures that the entire `message` is transmitted by repeatedly invoking the low-level
     * `write()` method to handle partial writes. This method blocks until all data is successfully
     * sent, the connection is lost, or an error occurs. It is suitable for protocols or data formats
     * that require full delivery of fixed-size payloads, headers, or serialized objects.
     *
     * Unlike `write()`, which may send only part of the input in a single call, this method
     * transparently retries until all bytes are written, making it reliable for complete
     * transmission scenarios (e.g., HTTP requests, framed packets, serialized data).
     *
     * ### Implementation Details
     * - Uses `write()` internally for each chunk
     * - Tracks remaining bytes and offset for partial sends
     * - Retries automatically until `message.size()` bytes are sent
     * - Blocks if necessary (unless socket is configured as non-blocking)
     * - Binary-safe: supports null bytes and non-text data
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     * sock.connect();
     *
     * std::string msg = "POST /data HTTP/1.1\r\nContent-Length: 42\r\n\r\n...";
     * try {
     *     std::size_t sent = sock.writeAll(msg);
     *     std::cout << "Successfully sent all " << sent << " bytes\n";
     * } catch (const SocketException& ex) {
     *     std::cerr << "Failed to send full message: " << ex.what() << '\n';
     * }
     * @endcode
     *
     * @param[in] message The message to send in full. May contain binary or textual content.
     *
     * @return The total number of bytes written. Will always equal `message.size()` on success.
     *
     * @throws SocketException If:
     *         - The socket is invalid or not connected (`ENOTCONN`, `EBADF`)
     *         - The connection is closed during transmission
     *         - A network error occurs (`ECONNRESET`, `ENETDOWN`, `EPIPE`, etc.)
     *         - A socket timeout is triggered (`SO_SNDTIMEO`)
     *         - A system-level failure interrupts transmission
     *
     * @note This method will block until all data is sent or an error/timeout occurs.
     *       For non-blocking or time-bounded writes, use `writeWithTotalTimeout()`.
     *
     * @see write() For a single-attempt best-effort write
     * @see writeWithTotalTimeout() For full delivery under time constraints
     * @see setSoSendTimeout() To configure write timeouts
     * @see setNonBlocking() To avoid blocking behavior
     * @see isConnected() To check socket status before writing
     */
    std::size_t writeAll(std::string_view message) const;

    /**
     * @brief Writes a length-prefixed payload using a fixed-size integral prefix.
     * @ingroup tcp
     *
     * Sends a message that consists of a length prefix followed by the actual payload.
     * The prefix is encoded in **network byte order** to ensure cross-platform compatibility.
     *
     * ### Implementation Details
     * - The length of the payload is cast to type `T` (must not exceed its maximum value)
     * - The prefix is converted to **network byte order** using `net::toNetwork()`
     * - The prefix is copied into a properly aligned buffer to avoid undefined behavior
     * - The prefix is written first, followed immediately by the payload
     * - This overload accepts any `std::string_view`, allowing use with string literals,
     *   slices, or raw binary buffers
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * std::string_view msg = "Hello, world!";
     *
     * // Send message with 32-bit length prefix
     * std::size_t totalBytes = sock.writePrefixed<uint32_t>(msg);
     * std::cout << "Sent " << totalBytes << " bytes\\n";
     * @endcode
     *
     * ### Protocol Format
     * <pre>
     * +----------------+----------------------+
     * | Length (T)     | Payload (n bytes)    |
     * +----------------+----------------------+
     * |<- sizeof(T) ->|<---- length ------->|
     * </pre>
     *
     * @tparam T The unsigned integral type used for the length prefix (e.g., `uint32_t`).
     *           Must be trivially copyable.
     *
     * @param[in] payload The payload data to send. The length of this view will be encoded
     *            as the prefix and must not exceed the maximum representable value of type `T`.
     *
     * @return The total number of bytes written (prefix + payload).
     *
     * @throws SocketException If:
     *         - Writing the prefix or payload fails
     *         - Payload size exceeds the maximum encodable value for type `T`
     *         - The connection is closed or interrupted
     *
     * @note The prefix is automatically converted to **network byte order**
     *       using `jsocketpp::net::toNetwork()`. The receiving side must decode it
     *       using `readPrefixed<T>()` or equivalent logic.
     *
     * @see readPrefixed()    To decode the corresponding message
     * @see writeAll()        To write data without a length prefix
     * @see net::toNetwork()  For details on byte order conversion
     */
    template <typename T> std::size_t writePrefixed(const std::string_view payload)
    {
        static_assert(std::is_integral_v<T> && std::is_trivially_copyable_v<T>,
                      "Prefix type must be a trivially copyable integral type");

        const std::size_t payloadSize = payload.size();

        // Guard against overflows: ensure payload fits in the prefix type
        // Note: Put parentheses around the `max` function name due to a collision with
        // a macro defined in windows.h. See:
        // https://stackoverflow.com/a/13420838/3049315
        // Another alternative would be to define `NOMINMAX` before including windows.h
        // such as in CMake when compiling the target library
        if (payloadSize > static_cast<std::size_t>((std::numeric_limits<T>::max)()))
        {
            throw SocketException("writePrefixed: Payload size " + std::to_string(payloadSize) +
                                  " exceeds maximum encodable size for prefix type");
        }

        // Convert the length to network byte order
        T len = net::toNetwork(static_cast<T>(payloadSize));

        // Copy the prefix value into a properly aligned buffer to avoid UB
        std::array<char, sizeof(T)> prefixBuffer{};
        std::memcpy(prefixBuffer.data(), &len, sizeof(T));
        const std::string_view prefixView(prefixBuffer.data(), sizeof(T));

        // Write the length prefix
        const std::size_t prefixSent = writeAll(prefixView);

        // Then write the payload
        const std::size_t dataSent = writeAll(payload);

        return prefixSent + dataSent;
    }

    /**
     * @brief Writes a binary payload prefixed with its length using a fixed-size integer type.
     * @ingroup tcp
     *
     * Sends a length-prefixed binary message, where the prefix is a fixed-size integral type `T`
     * followed by a raw binary buffer. This version avoids constructing a temporary `std::string`,
     * making it efficient for zero-copy binary protocols and high-performance I/O paths.
     *
     * ### Implementation Details
     * - Validates that `len` fits within the range of type `T`
     * - Converts the prefix to **network byte order** using `net::toNetwork()`
     * - Sends `sizeof(T)` bytes of the length prefix
     * - Sends `len` bytes of raw payload data immediately afterward
     * - Uses `writeAll()` to ensure full delivery of both parts
     *
     * ### Example Usage
     * @code{.cpp}
     * std::vector<uint8_t> imageData = loadImage();
     * sock.writePrefixed<uint32_t>(imageData.data(), imageData.size());
     * @endcode
     *
     * ### Protocol Format
     * <pre>
     * +----------------+----------------------+
     * | Length (T)     | Payload (n bytes)   |
     * +----------------+----------------------+
     * |<- sizeof(T) ->|<---- len --------->|
     * </pre>
     *
     * @tparam T The unsigned integral type used for the length prefix (e.g., `uint32_t`).
     *           Must be a trivially copyable type.
     *
     * @param[in] data Pointer to the binary payload data.
     * @param[in] len Number of bytes to write from `data`.
     *
     * @return Total number of bytes written (`sizeof(T) + len`).
     *
     * @throws SocketException If:
     *         - `len` exceeds the maximum value representable by `T`
     *         - If data is null and len > 0
     *         - Writing the prefix or payload fails
     *         - Connection is closed or interrupted
     *
     * @note The prefix is automatically converted to **network byte order**
     *       using `jsocketpp::net::toNetwork()`. Receivers must decode it
     *       accordingly using `readPrefixed<T>()` or equivalent.
     *
     * @see readPrefixed() For the corresponding deserialization method
     * @see writeAll()     For guaranteed single-buffer transmission
     * @see writePrefixed(std::string) For the string-based variant
     * @see net::toNetwork() For details on byte order conversion
     */
    template <typename T> std::size_t writePrefixed(const void* data, std::size_t len) const
    {
        static_assert(std::is_integral_v<T> && std::is_trivially_copyable_v<T>,
                      "Prefix type must be a trivially copyable integral type");

        // Check for null pointer with non-zero length
        if (!data && len > 0)
        {
            throw SocketException("writePrefixed: Null data pointer with non-zero length");
        }

        // Ensure the payload length fits in the prefix type
        if (len > static_cast<std::size_t>((std::numeric_limits<T>::max)()))
        {
            throw SocketException("writePrefixed: Payload length " + std::to_string(len) +
                                  " exceeds capacity of prefix type (" +
                                  std::to_string((std::numeric_limits<T>::max)()) + ")");
        }

        // Encode prefix in network byte order
        T prefix = net::toNetwork(static_cast<T>(len));

        // Use aligned buffer to avoid undefined behavior when casting to char*
        std::array<char, sizeof(T)> prefixBuffer{};
        std::memcpy(prefixBuffer.data(), &prefix, sizeof(T));
        const std::string_view prefixView(prefixBuffer.data(), sizeof(T));

        // Send prefix
        const std::size_t prefixSent = writeAll(prefixView);

        // Send payload if any
        std::size_t payloadSent = 0;
        if (len > 0)
        {
            payloadSent = writeAll(std::string_view(static_cast<const char*>(data), len));
        }

        return prefixSent + payloadSent;
    }

    /**
     * @brief Writes multiple buffers in a single system call using scatter/gather I/O.
     * @ingroup tcp
     *
     * This method efficiently writes multiple non-contiguous buffers to the socket
     * using platform-specific vectorized I/O calls. It is ideal for sending structured
     * packets, headers + body, or other segmented data without concatenating them.
     *
     * ### Implementation Details
     * - On POSIX: uses `writev()` with `struct iovec[]`
     * - On Windows: uses `WSASend()` with `WSABUF[]`
     * - Handles up to IOV_MAX or WSABUF_MAX entries (platform limit)
     * - Ensures the total byte count written is returned
     *
     * ### Example Usage
     * @code{.cpp}
     * std::string_view header = "Content-Length: 12\r\n\r\n";
     * std::string_view body   = "Hello world!";
     *
     * std::array<std::string_view, 2> segments = {header, body};
     * std::size_t sent = sock.writev(segments);
     * @endcode
     *
     * @param[in] buffers A span of string views to send as a scatter/gather I/O batch.
     * @return Total number of bytes sent.
     *
     * @throws SocketException If:
     *         - Socket is not connected
     *         - System I/O call fails
     *         - Connection is broken or interrupted
     *
     * @note This function does not retry partial writes. You must manually check
     *       the return value and retry unsent segments if needed.
     *
     * @see write() For single-buffer sends
     * @see writeAll() To send all bytes in a single string
     */
    std::size_t writev(std::span<const std::string_view> buffers) const;

    /**
     * @brief Writes all buffers fully using vectorized I/O with automatic retry on partial sends.
     * @ingroup tcp
     *
     * Ensures that the entire contents of all buffers in the given span are fully transmitted,
     * retrying as needed. This is the guaranteed-delivery counterpart to `writev()`.
     *
     * ### Implementation Details
     * - Uses `writev()` or `WSASend()` to send as much as possible
     * - Tracks which buffers are partially sent
     * - Rebuilds the buffer list on each retry to resume from the last offset
     * - Stops only when all buffers are fully transmitted or an error occurs
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::string_view, 3> fragments = {
     *     "HTTP/1.1 200 OK\r\n",
     *     "Content-Length: 5\r\n\r\n",
     *     "Hello"
     * };
     * sock.writevAll(fragments);
     * @endcode
     *
     * @param[in] buffers A span of string fragments to send as a contiguous logical payload.
     * @return Total number of bytes written (equal to sum of all buffer sizes).
     *
     * @throws SocketException If:
     *         - A send error occurs
     *         - The connection is closed mid-transmission
     *
     * @note This method guarantees full delivery, unlike writev(), which may send only part.
     *
     * @see writev() For single-shot scatter/gather write
     * @see writeAll() For full single-buffer delivery
     * @see write() For low-level single-buffer writes
     */
    std::size_t writevAll(std::span<const std::string_view> buffers) const;

    /**
     * @brief Performs a best-effort write with a total timeout.
     * @ingroup tcp
     *
     * Waits up to `timeoutMillis` milliseconds for the socket to become writable,
     * then attempts a single `send()` operation to write up to `data.size()` bytes.
     * This method does not retry on partial writes and is designed for time-sensitive,
     * latency-focused applications.
     *
     * This is a timeout-aware counterpart to `write()` that returns early in the
     * event of timeout or connection closure. Use this when you need responsiveness
     * rather than guaranteed delivery.
     *
     * ### Implementation Details
     * - Calls `waitReady(true, timeoutMillis)` to wait for socket writability.
     * - If writable, performs one `send()` call.
     * - Returns the number of bytes successfully written (which may be less than `data.size()`).
     * - Returns 0 if the connection is closed before any data is written.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::string payload = "POST /data";
     *
     * try {
     *     std::size_t sent = sock.writeAtMostWithTimeout(payload, 500); // wait up to 500 ms
     *     std::cout << "Sent " << sent << " bytes.\n";
     * } catch (const SocketTimeoutException& e) {
     *     std::cerr << "Write timed out: " << e.what() << std::endl;
     * } catch (const SocketException& e) {
     *     std::cerr << "Write error: " << e.what() << std::endl;
     * }
     * @endcode
     *
     * @param[in] data The data to send.
     * @param[in] timeoutMillis Maximum time to wait for writability, in milliseconds:
     *                      - > 0: Wait up to the given duration
     *                      - 0: Poll immediately (non-blocking)
     *                      - < 0: Invalid; throws exception
     *
     * @return Number of bytes written (may be 0 if the socket was closed or data not sent).
     *
     * @throws SocketTimeoutException If the socket does not become writable within `timeoutMillis`.
     * @throws SocketException If:
     *         - `send()` fails with a network or system error
     *         - The socket is closed or invalid
     *
     * @note This is a single-attempt, low-latency write. Use `writeAll()` or
     *       `writeWithTotalTimeout()` for guaranteed delivery with retries.
     *
     * @see writeAll() To send full content regardless of time
     * @see writeWithTotalTimeout() To retry until timeout expires
     * @see SocketTimeoutException For timeout-specific error classification
     */
    std::size_t writeAtMostWithTimeout(std::string_view data, int timeoutMillis) const;

    /**
     * @brief Writes up to `len` bytes from a raw memory buffer in a single send call.
     * @ingroup tcp
     *
     * This method sends data directly from a raw memory pointer using a best-effort write.
     * It may write fewer bytes than requested, depending on socket buffer availability.
     * It is the low-level counterpart to `write(std::string_view)` and avoids constructing
     * or copying into strings.
     *
     * ### Example Usage
     * @code{.cpp}
     * const uint8_t buffer[] = { 0x01, 0x02, 0x03 };
     * std::size_t sent = sock.writeFrom(buffer, sizeof(buffer));
     * @endcode
     *
     * @param[in] data Pointer to the memory to write.
     * @param[in] len Number of bytes to send.
     * @return The number of bytes successfully written (can be < len).
     *
     * @throws SocketException If:
     *         - The socket is invalid
     *         - send() fails due to a network error
     *         - Connection is closed
     *
     * @note This method does not guarantee full delivery. Use `writeFromAll()` for that.
     *
     * @see write() For std::string_view interface
     * @see writeFromAll() To guarantee full transmission
     * @see readInto() For symmetric read into raw buffer
     */
    std::size_t writeFrom(const void* data, std::size_t len) const;

    /**
     * @brief Writes all bytes from a raw memory buffer, retrying until complete.
     * @ingroup tcp
     *
     * This method repeatedly calls `send()` until the full `len` bytes of the buffer
     * are successfully transmitted. It guarantees that all bytes are written, or
     * throws an exception on failure. This is the raw-buffer equivalent of `writeAll()`.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::vector<uint8_t> payload = generateMessage();
     * sock.writeFromAll(payload.data(), payload.size());
     * @endcode
     *
     * @param[in] data Pointer to the binary buffer to send.
     * @param[in] len Number of bytes to transmit.
     * @return The total number of bytes written (equal to `len` on success).
     *
     * @throws SocketException If:
     *         - The socket is invalid
     *         - send() fails
     *         - The connection is closed prematurely
     *
     * @note This method blocks until all data is sent or an error occurs.
     *       For partial/best-effort sends, use `writeFrom()` instead.
     *
     * @see writeFrom() For best-effort version
     * @see writeAll() For string-based equivalent
     * @see readIntoExact() For guaranteed binary reads
     */
    std::size_t writeFromAll(const void* data, std::size_t len) const;

    /**
     * @brief Writes the full payload with a total timeout across all retries.
     * @ingroup tcp
     *
     * Repeatedly attempts to send the entire contents of `data`, retrying partial
     * writes as needed. The complete operation must finish within `timeoutMillis`
     * milliseconds or it will throw a `SocketTimeoutException`.
     *
     * This method ensures full delivery like `writeAll()`, but it is time-bounded
     * by wall-clock time. It is suitable for real-time or responsiveness-sensitive
     * applications where you must send everything or fail within a deadline.
     *
     * ### Implementation Details
     * - Uses `std::chrono::steady_clock` to enforce a wall-clock deadline.
     * - Calls `waitReady(true, remainingTime)` before each `send()` attempt.
     * - Retries partial writes until complete or until timeout is reached.
     * - Returns the total number of bytes sent (equal to `data.size()` on success).
     *
     * ### Example Usage
     * @code{.cpp}
     * std::string json = buildPayload();
     *
     * try {
     *     sock.writeWithTotalTimeout(json, 1000); // Must finish in 1 second
     *     std::cout << "Payload sent.\n";
     * } catch (const SocketTimeoutException& e) {
     *     std::cerr << "Timed out: " << e.what() << std::endl;
     * } catch (const SocketException& e) {
     *     std::cerr << "Write error: " << e.what() << std::endl;
     * }
     * @endcode
     *
     * @param[in] data The data to send.
     * @param[in] timeoutMillis Maximum time in milliseconds to send the full payload.
     *
     * @return Total number of bytes written (equals `data.size()` on success).
     *
     * @throws SocketTimeoutException If:
     *         - The socket does not become writable in time
     *         - The full payload could not be sent before the deadline
     *
     * @throws SocketException If:
     *         - `send()` fails due to a network or system error
     *         - The socket is closed or invalid
     *         - The connection is closed during the operation
     *
     * @note This method guarantees full delivery or throws. Use `writeAll()` for unbounded retries.
     *
     * @see writeAll() For guaranteed delivery without time constraint
     * @see writeAtMostWithTimeout() For single-shot, best-effort delivery
     * @see SocketTimeoutException To catch time-limit-specific failures
     */
    std::size_t writeWithTotalTimeout(std::string_view data, int timeoutMillis) const;

    /**
     * @brief Writes all buffers fully within a total timeout using vectorized I/O.
     * @ingroup tcp
     *
     * Sends the contents of all buffers in the provided span using scatter/gather I/O.
     * Retries partial sends as needed, resuming from where the last send left off. The entire
     * operation must complete within `timeoutMillis` milliseconds or a `SocketTimeoutException`
     * will be thrown.
     *
     * This method guarantees that all data across all buffers will be sent in order—or none at all.
     * It is the timeout-aware variant of `writevAll()` and suitable for time-bounded, multi-buffer
     * transmission (e.g., structured headers + body).
     *
     * ### Implementation Details
     * - Uses `std::chrono::steady_clock` to enforce a total wall-clock timeout.
     * - Waits for writability using `waitReady(true, remainingTime)` between sends.
     * - Uses `writev()` to send multiple buffers in one system call.
     * - After partial sends, dynamically rebuilds the buffer span to reflect unsent data.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::string_view, 3> parts = {
     *     "Header: ",
     *     "value\r\n\r\n",
     *     "Body content"
     * };
     *
     * try {
     *     sock.writevWithTotalTimeout(parts, 1500); // Must finish in 1.5 seconds
     *     std::cout << "Message sent.\n";
     * } catch (const SocketTimeoutException& e) {
     *     std::cerr << "Write timeout: " << e.what() << std::endl;
     * } catch (const SocketException& e) {
     *     std::cerr << "Write failure: " << e.what() << std::endl;
     * }
     * @endcode
     *
     * @param[in] buffers Span of string views representing logically distinct memory segments to send in order.
     * @param[in] timeoutMillis Total allowed duration in milliseconds to complete the operation.
     *
     * @return Total number of bytes sent (must equal the sum of all buffer sizes on success).
     *
     * @throws SocketTimeoutException If:
     *         - The socket does not become writable in time
     *         - The full payload is not sent before the timeout expires
     *
     * @throws SocketException If:
     *         - `writev()` fails due to a system or network error
     *         - The socket is closed or invalid
     *         - The connection is interrupted during transmission
     *
     * @note This method guarantees complete delivery of all buffers, bounded by time.
     *       Use `writevAll()` for unbounded retries or `writev()` for a single attempt.
     *
     * @see writevAll() For unlimited retries and guaranteed delivery
     * @see writeWithTotalTimeout() For single-buffer full delivery under timeout
     * @see SocketTimeoutException To differentiate timeout from generic network errors
     */
    std::size_t writevWithTotalTimeout(std::span<const std::string_view> buffers, int timeoutMillis) const;

    /**
     * @brief Writes multiple raw memory regions using vectorized I/O.
     * @ingroup tcp
     *
     * Sends the contents of all buffers in the given span using scatter/gather I/O.
     * This performs a single system call (`writev()` on POSIX, `WSASend()` on Windows),
     * and may transmit fewer bytes than requested. Use `writevFromAll()` for full delivery.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::byte, 4> header = ...;
     * std::vector<std::byte> body = ...;
     *
     * std::array<BufferView, 2> buffers = {
     *     BufferView{header.data(), header.size()},
     *     BufferView{body.data(), body.size()}
     * };
     *
     * std::size_t sent = sock.writevFrom(buffers);
     * @endcode
     *
     * @param[in] buffers A span of BufferView elements (data + size).
     * @return Number of bytes successfully written (can be < total).
     *
     * @throws SocketException If:
     *         - The socket is invalid
     *         - sendv fails (e.g., WSA error, broken pipe)
     *
     * @see writevFromAll() For full-delivery retry logic
     * @see writev() For string-based scatter I/O
     */
    std::size_t writevFrom(std::span<const BufferView> buffers) const;

    /**
     * @brief Writes all raw memory regions fully using scatter/gather I/O.
     * @ingroup tcp
     *
     * This method guarantees full delivery of all bytes across the given buffer span.
     * Internally retries `writevFrom()` until every buffer is fully written.
     * This is the binary-safe, zero-copy equivalent of `writevAll()`.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::byte, 8> header;
     * std::vector<std::byte> body;
     *
     * std::array<BufferView, 2> buffers = {
     *     BufferView{header.data(), header.size()},
     *     BufferView{body.data(), body.size()}
     * };
     *
     * sock.writevFromAll(buffers);
     * @endcode
     *
     * @param[in] buffers A span of raw buffers to send completely.
     * @return Total number of bytes written (equal to sum of buffer sizes).
     *
     * @throws SocketException If:
     *         - A socket error occurs during send
     *         - The connection is closed prematurely
     *
     * @note This method blocks until completion or error.
     *       Use `writevFrom()` for best-effort, single-attempt version.
     *
     * @see writevFrom() For non-retrying variant
     * @see writevAll() For string-view based equivalent
     */
    std::size_t writevFromAll(std::span<BufferView> buffers) const;

    /**
     * @brief Writes all raw memory buffers fully within a timeout using scatter I/O.
     * @ingroup tcp
     *
     * Sends multiple binary buffers using scatter/gather I/O, retrying partial writes
     * as needed until all data is delivered or the specified timeout period elapses.
     * If the timeout expires before completing the transmission, a `SocketTimeoutException`
     * is thrown.
     *
     * This is the binary-buffer equivalent of `writevWithTotalTimeout()`, intended for
     * use with low-level, structured protocols or custom serialization layers.
     *
     * ### Implementation Details
     * - Enforces a wall-clock timeout using `std::chrono::steady_clock`.
     * - Waits for writability using `waitReady()` before each write attempt.
     * - Uses `writevFrom()` to send multiple non-contiguous memory blocks efficiently.
     * - After partial writes, dynamically adjusts buffer offsets to resume transmission.
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::byte, 4> hdr = ...;
     * std::vector<std::byte> body = ...;
     *
     * std::array<BufferView, 2> bufs = {
     *     BufferView{hdr.data(), hdr.size()},
     *     BufferView{body.data(), body.size()}
     * };
     *
     * try {
     *     sock.writevFromWithTotalTimeout(bufs, 2000); // must finish in 2s
     *     std::cout << "Transmission complete.\n";
     * } catch (const SocketTimeoutException& e) {
     *     std::cerr << "Write timed out: " << e.what() << std::endl;
     * } catch (const SocketException& e) {
     *     std::cerr << "Write error: " << e.what() << std::endl;
     * }
     * @endcode
     *
     * @param[in] buffers A span of `BufferView` objects representing raw memory regions to send.
     * @param[in] timeoutMillis Total timeout duration in milliseconds across all write attempts.
     *
     * @return Total number of bytes written (equal to the sum of all buffer sizes on success).
     *
     * @throws SocketTimeoutException If:
     *         - The socket does not become writable within the remaining timeout
     *         - The full payload is not sent before the deadline expires
     *
     * @throws SocketException If:
     *         - `writevFrom()` fails due to a system or network error
     *         - The socket is invalid or disconnected
     *         - The connection is closed during transmission
     *
     * @note This is the low-level, binary-buffer counterpart to `writevWithTotalTimeout()`.
     *       Use `writevFromAll()` for unbounded full delivery.
     *
     * @see writevFromAll() For retrying without a timeout
     * @see writevWithTotalTimeout() For `std::string_view`-based variant
     * @see SocketTimeoutException For timeout-specific error handling
     */
    std::size_t writevFromWithTotalTimeout(std::span<BufferView> buffers, int timeoutMillis) const;

    /**
     * @brief Sets the size of the internal read buffer used for string operations.
     * @ingroup tcp
     *
     * This method controls the size of the internal buffer used by read<std::string>()
     * operations. This is distinct from setReceiveBufferSize(), which controls the
     * operating system's socket buffer (SO_RCVBUF).
     *
     * ### Purpose
     * - Controls maximum size of data readable in one read<std::string>() call
     * - Affects memory usage of the Socket object
     * - Does not affect system socket buffers or network behavior
     *
     * ### Implementation Details
     * - Resizes internal std::vector<char> buffer
     * - Used only for string-based reads
     * - Not used for fixed-size read<T>() operations
     * - Thread-safe with respect to other Socket instances
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Set 8KB internal buffer for string reads
     * sock.setInternalBufferSize(8192);
     *
     * // Read will now use 8KB buffer
     * std::string data = sock.read<std::string>();
     * @endcode
     *
     * @param[in] newLen New size for the internal buffer in bytes
     * @throws std::bad_alloc If memory allocation fails
     *
     * @see setReceiveBufferSize() For setting the OS socket receive buffer
     * @see read<std::string>() Uses this buffer for string operations
     * @see setSendBufferSize() For setting the OS socket send buffer
     */
    void setInternalBufferSize(std::size_t newLen);

    /**
     * @brief Check if the socket is valid and open for communication.
     * @ingroup tcp
     *
     * This method checks if the socket has a valid file descriptor and is ready for
     * communication. A socket is considered valid if it has been successfully created
     * and has not been closed. However, a valid socket is not necessarily connected;
     * use isConnected() to check the connection status.
     *
     * ### Implementation Details
     * - Checks if internal socket descriptor (getSocketFd()) is not INVALID_SOCKET
     * - Fast, constant-time operation (O(1))
     * - Thread-safe (const noexcept)
     * - Does not perform any system calls
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * if (sock.isValid()) {
     *     // Socket is ready for connect()
     *     sock.connect();
     * }
     * sock.close();
     * assert(!sock.isValid()); // Socket is now invalid
     * @endcode
     *
     * @return true if the socket is valid and ready for use,
     *         false if the socket has been closed or failed to initialize.
     *
     * @note This method only checks the socket's validity, not its connection state.
     *       A valid socket may or may not be connected to a remote host.
     *
     * @see isConnected() Check if socket is actually connected
     * @see close() Invalidates the socket
     */
    [[nodiscard]] bool isValid() const noexcept { return getSocketFd() != INVALID_SOCKET; }

    /**
     * @brief Waits for the socket to become ready for reading or writing.
     * @ingroup tcp
     *
     * This method performs a blocking or timed wait until the socket is ready for either
     * reading or writing, depending on the `forWrite` parameter. Internally, it uses the
     * `select()` system call to monitor readiness.
     *
     * The timeout is specified in milliseconds. A negative timeout value (e.g., `-1`)
     * causes the method to wait indefinitely until the socket becomes ready.
     * A timeout of `0` performs a non-blocking poll.
     *
     * ### Implementation Notes
     * - Internally uses `select()` on both POSIX and Windows platforms.
     * - On POSIX, the socket descriptor must be **less than `FD_SETSIZE`** (typically 1024).
     *   If the descriptor exceeds this limit, a `SocketException` is thrown.
     * - On Windows, the first parameter to `select()` is ignored and always set to 0.
     * - This method works for both blocking and non-blocking sockets.
     *
     * ### Example
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     * if (sock.waitReady(false, 5000)) {
     *     std::string msg = sock.read<std::string>();
     *     std::cout << "Received: " << msg << "\\n";
     * }
     * @endcode
     *
     * ### Protocol Behavior
     * - `forWrite == false`: Waits for the socket to be readable (e.g., data available).
     * - `forWrite == true`:  Waits for the socket to be writable (e.g., buffer space available).
     *
     * @param[in] forWrite If true, waits for the socket to become writable; if false, waits for it to be readable.
     * @param[in] timeoutMillis Timeout in milliseconds. Use `-1` for infinite wait, or `0` for non-blocking polling.
     *
     * @retval true  The socket is ready for I/O.
     * @retval false The timeout expired before the socket became ready.
     *
     * @throws SocketException If:
     *         - The socket is invalid (`INVALID_SOCKET`)
     *         - The socket descriptor exceeds `FD_SETSIZE` (POSIX only)
     *         - A system error occurs during polling
     *
     * @note
     * This method uses the legacy `select()` system call, which is portable but has limitations:\n
     * - On POSIX systems, it cannot monitor descriptors ≥ `FD_SETSIZE` (usually 1024)\n
     * - `fd_set` is a static bitset, and overflow causes undefined behavior\n
     * - `select()` scales poorly with large socket sets\n\n
     * **Alternatives:**\n
     * - On POSIX: `poll()` (removes FD_SETSIZE limit, cleaner API)\n
     * - On Linux: `epoll` (scales to thousands of fds)\n
     * - On BSD/macOS: `kqueue` (efficient and feature-rich)\n
     * - On Windows: `WSAPoll()` (requires Windows Vista+), or I/O Completion Ports for async\n\n
     * These alternatives are better suited for server-side multiplexing and event-driven designs.
     * For single-socket or per-connection waiting, `select()` remains simple and widely compatible.
     *
     * @see setBlocking() To configure blocking/non-blocking mode
     * @see read()        For reading data once the socket is ready
     * @see write()       For writing data once the socket is ready
     * @see FD_SETSIZE    System-defined maximum descriptor index for `select()`
     */
    bool waitReady(bool forWrite, int timeoutMillis) const;

    /**
     * @brief Convert an address and port to a string using getnameinfo.
     *
     * Uses getnameinfo to convert a sockaddr_storage structure to a human-readable string (ip:port).
     * Handles both IPv4 and IPv6 addresses.
     *
     * @param[in] addr sockaddr_storage structure.
     * @return String representation (ip:port).
     * @throws SocketException if getnameinfo fails.
     */
    static std::string addressToString(const sockaddr_storage& addr);

    /**
     * @brief Convert a string (ip:port) to sockaddr_storage.
     *
     * @param[in] str Address string.
     * @param[out] addr Output sockaddr_storage.
     */
    static void stringToAddress(const std::string& str, sockaddr_storage& addr);

    /**
     * @brief Reports whether the socket has been closed or invalidated.
     * @ingroup tcp
     *
     * Returns `true` if the socket is no longer usable—either because it was explicitly
     * closed, or because the underlying file descriptor has been released or never initialized.
     *
     * This method does not perform any system-level check or probing. It simply inspects
     * the internal socket descriptor (`getSocketFd()`) and considers the socket closed if it is
     * equal to `INVALID_SOCKET`. This reflects the same mechanism used internally for
     * error detection and safety throughout the library.
     *
     * ### Example
     * @code
     * Socket sock("example.com", 443);
     * assert(!sock.isClosed());
     * sock.connect();
     * sock.close(); // assume you define a close() method
     * assert(sock.isClosed()); // true
     * @endcode
     *
     * @return `true` if the socket is closed or uninitialized, `false` otherwise.
     *
     * @note Once closed, the socket cannot be reused or reopened. Create a new Socket instance instead.
     *
     * @see isConnected()
     * @see isBound()
     */
    [[nodiscard]] bool isClosed() const noexcept { return getSocketFd() == INVALID_SOCKET; }

    /**
     * @brief Checks whether the socket's input stream has been shutdown.
     * @ingroup tcp
     *
     * Returns `true` if the socket has been explicitly shutdown for reading via
     * `shutdown(ShutdownMode::Read)` or `shutdown(ShutdownMode::Both)`. After input shutdown:
     * - Any further read operations will return EOF or throw exceptions.
     * - The socket cannot receive new data from the peer.
     *
     * This method tracks shutdown state internally—it does **not** query the operating system
     * (as no portable API exists for that). It reflects the local state at the time of the last
     * `shutdown()` call on this socket.
     *
     * ### Use Cases
     * - Preventing redundant or invalid read calls after a graceful shutdown.
     * - Verifying application-level protocol state transitions.
     * - Detecting half-closed connections during teardown.
     *
     * @return `true` if the input (receive) side of the socket has been shutdown; `false` otherwise.
     *
     * @see shutdown()
     * @see isOutputShutdown()
     * @see isConnected()
     * @see read(), readExact(), readUntil()
     */
    [[nodiscard]] bool isInputShutdown() const noexcept { return _inputShutdown; }

    /**
     * @brief Checks whether the socket's output stream has been shutdown.
     * @ingroup tcp
     *
     * Returns `true` if the socket has been explicitly shutdown for writing via
     * `shutdown(ShutdownMode::Write)` or `shutdown(ShutdownMode::Both)`. After output shutdown:
     * - All subsequent write operations will fail with a `SocketException`.
     * - The system sends a FIN packet to the peer, indicating no more data will be sent.
     *
     * This method tracks shutdown state internally—it does **not** query the operating system,
     * as no portable `getsockopt()` mechanism exists to detect output shutdown externally.
     *
     * ### Use Cases
     * - Enforcing proper protocol shutdown (e.g. sending EOF before receiving response).
     * - Avoiding invalid writes after intentionally shutting down output.
     * - Debugging and verifying application-level connection state.
     *
     * @return `true` if the output (send) side of the socket has been shutdown; `false` otherwise.
     *
     * @see shutdown()
     * @see isInputShutdown()
     * @see isConnected()
     * @see write(), writeAll(), writePrefixed()
     */
    [[nodiscard]] bool isOutputShutdown() const noexcept { return _outputShutdown; }

  protected:
    /**
     * @brief Reads data from the socket into a user-supplied buffer.
     * @ingroup tcp
     *
     * This method provides direct access to the socket's receive functionality by reading
     * data into a caller-provided buffer. It supports both "best-effort" single reads and
     * exact-length reads that ensure all requested bytes are received.
     *
     * ### Modes of Operation
     * - **Best-effort mode** (exact=false):
     *   - Performs single recv() call
     *   - Returns immediately with available data
     *   - May return fewer bytes than requested
     *   - Suitable for stream processing
     *
     * - **Exact mode** (exact=true):
     *   - Guarantees all requested bytes are read
     *   - Loops until full length received
     *   - Throws if connection closes early
     *   - Good for protocol messages
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Best-effort read
     * char buf[1024];
     * size_t got = sock.readInto(buf, sizeof(buf), false);
     * // got may be less than 1024
     *
     * // Exact read
     * uint32_t length;
     * sock.readInto(&length, sizeof(length), true);
     * // Always reads exactly 4 bytes or throws
     * @endcode
     *
     * ### Implementation Details
     * - Uses recv() internally for actual reads
     * - Handles partial reads in exact mode
     * - Buffer bounds checking
     * - Platform-independent behavior
     *
     * @param[out] buffer Pointer to caller-allocated memory buffer
     *                    Must be valid and large enough for len bytes
     * @param[in] len Maximum number of bytes to read (buffer size)
     * @param[in] exact If true, method won't return until len bytes
     *                  are read or an error occurs
     *
     * @return Number of bytes actually read:
     *         - exact=false: 0 to len bytes
     *         - exact=true: Always len bytes or throws
     *
     * @throws SocketException If:
     *         - Socket is invalid (EBADF)
     *         - Connection closed by peer
     *         - Memory access error (EFAULT)
     *         - Timeout occurs (EAGAIN)
     *         - Other network errors
     *
     * @see read() Template method for type-safe reads
     * @see readUntil() For delimiter-based reading
     * @see readExact() For string-based exact reads
     */
    std::size_t readIntoInternal(void* buffer, std::size_t len, bool exact = false) const;

    /**
     * @brief Cleans up client socket resources and throws a SocketException.
     *
     * This method performs internal cleanup of the client socket's resources, including:
     * - Closing the socket if it is open (`getSocketFd()`)
     * - Releasing any allocated address resolution data (`_cliAddrInfo`)
     * - Resetting `_selectedAddrInfo` to null
     *
     * It is used to centralize error recovery during construction or connection setup,
     * ensuring that all partially initialized resources are properly released before
     * rethrowing an exception.
     *
     * @param[in] errorCode The error code to report in the thrown exception.
     *
     * @throws SocketException Always throws, containing the error code and the corresponding
     * human-readable error message obtained via `SocketErrorMessage(errorCode)`.
     *
     * @note This function is typically called when socket creation, address resolution,
     * or connection setup fails during client socket initialization.
     *
     * @ingroup tcp
     */
    void cleanupAndThrow(int errorCode);

    /**
     * @brief Resets internal shutdown state flags to `false`.
     * @ingroup tcp
     *
     * This method clears the internal flags that track whether the socket's input or output
     * streams have been shutdown via `shutdown(ShutdownMode)`. It does **not** reopen or
     * re-enable socket directions at the system level—it only resets the internal state used
     * by `isInputShutdown()` and `isOutputShutdown()`.
     *
     * This method is useful when reinitializing or repurposing a `Socket` object after calling
     * `close()`, or when transferring ownership during a move assignment where shutdown state
     * is no longer valid.
     *
     * @warning This method does **not** call any OS-level functions like `shutdown()` or `setsockopt()`.
     *          It only resets internal bookkeeping flags.
     *
     * @note This method is intended for internal use or advanced scenarios. Most users should
     *       not need to call this directly.
     *
     * @see shutdown()
     * @see isInputShutdown()
     * @see isOutputShutdown()
     */
    void resetShutdownFlags() noexcept
    {
        _inputShutdown = false;
        _outputShutdown = false;
    }

  private:
    sockaddr_storage _remoteAddr; ///< sockaddr_in for IPv4; sockaddr_in6 for IPv6; sockaddr_storage for both
    ///< (portability)
    mutable socklen_t _remoteAddrLen = 0;         ///< Length of remote address (for recvfrom/recvmsg)
    internal::AddrinfoPtr _cliAddrInfo = nullptr; ///< Address info for connection (from getaddrinfo)
    addrinfo* _selectedAddrInfo = nullptr;        ///< Selected address info for connection
    std::vector<char> _internalBuffer;            ///< Internal buffer for read operations, not thread-safe
    bool _isBound = false;                        ///< True if the socket is bound to an address
    bool _isConnected = false;                    ///< True if the socket is connected to a remote peer
    bool _inputShutdown = false;                  ///< True if input side is shutdown (recv disabled)
    bool _outputShutdown = false;                 ///< True if output side is shutdown (send disabled)
};

/**
 * @brief Specialization of `read<T>()` for reading a single buffer of string data.
 * @ingroup tcp
 *
 * Reads a variable-length block of data from the socket into the internal receive buffer
 * and returns it as a `std::string`. This specialization provides optimized handling
 * for raw or text-based data streams where the amount of available data is not known in advance.
 *
 * Unlike `read<T>()`, which reads a fixed number of bytes based on the type, this method
 * reads up to the size of the internal buffer and returns whatever data is available,
 * making it ideal for use cases where the protocol layer or application logic defines
 * variable-sized records or streams.
 *
 * ### Key Features
 * - Uses `_internalBuffer` for efficient chunked `recv()` operations
 * - Preserves binary data (e.g. embedded null bytes)
 * - Returns actual number of bytes received (may be less than buffer size)
 * - Non-blocking if data is already available (subject to socket timeout)
 *
 * ### Implementation Details
 * 1. Uses `_internalBuffer` sized via `setInternalBufferSize()`
 * 2. Performs a single `recv()` call up to the buffer size
 * 3. Returns a `std::string` containing only the received portion
 * 4. Internal buffer is reused across calls and not resized dynamically
 *
 * ### Example Usage
 * @code{.cpp}
 * Socket sock("example.com", 8080);
 * sock.connect();
 *
 * // Default buffer size (see @ref DefaultBufferSize)
 * std::string data = sock.read<std::string>();
 *
 * // Increase buffer size for larger payloads
 * sock.setInternalBufferSize(4096);
 * std::string chunk = sock.read<std::string>();
 * @endcode
 *
 * @return A `std::string` containing up to `internalBufferSize` bytes of received data.
 *         The length reflects the actual bytes read from the socket.
 *
 * @throws SocketException If:
 *         - The socket is closed (`recv() == 0`)
 *         - A network error occurs (`recv() == SOCKET_ERROR`)
 *         - The socket is in an invalid state or not connected
 *
 * @note This specialization differs from the generic `read<T>()`:
 *       - It supports reading variable-length data
 *       - It does not attempt to construct a specific object type
 *       - It is suitable for stream-based protocols, logging, or framed text
 *
 * @see read()             Generic fixed-size reader for POD types
 * @see setInternalBufferSize() To control the read buffer size
 * @see write()            For sending text or binary data
 * @see readUntil()        For delimiter-based stream parsing
 */
template <> inline std::string Socket::read()
{
    const auto len = recv(getSocketFd(), _internalBuffer.data(),
#ifdef _WIN32
                          static_cast<int>(_internalBuffer.size()),
#else
                          _internalBuffer.size(),
#endif
                          0);
    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (len == 0)
        throw SocketException("Connection closed by remote host.");
    return {_internalBuffer.data(), static_cast<size_t>(len)};
}

} // namespace jsocketpp
