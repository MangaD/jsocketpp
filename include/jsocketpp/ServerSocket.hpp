/**
 * @file ServerSocket.hpp
 * @brief TCP server socket abstraction for jsocketpp.
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

#pragma once

#include "common.hpp"
#include "Socket.hpp"

#include <exception>
#include <functional>
#include <future>
#include <optional>

using jsocketpp::DefaultBufferSize;

namespace jsocketpp
{

/**
 * @class ServerSocket
 * @ingroup tcp
 * @brief TCP server socket abstraction for cross-platform C++ networking.
 *
 * The `ServerSocket` class provides a high-level, Java-like interface to create TCP server sockets in C++20,
 * supporting both IPv4 and IPv6, and working on both Windows and Unix-like systems.
 *
 * ## Overview
 * `ServerSocket` is designed to simplify the creation of network server applications. It allows you to:
 * - Bind to a specified port (optionally on a specific address/interface)
 * - Listen for incoming connections
 * - Accept client connections as new `Socket` objects
 * - Clean up resources automatically (RAII)
 *
 * This class handles platform differences (such as Winsock vs BSD Sockets) internally, so you can write portable code.
 *
 * ## Typical Usage
 * Here’s how you can use `ServerSocket` to create a simple TCP echo server:
 *
 * @code{.cpp}
 * #include <jsocketpp/ServerSocket.hpp>
 * #include <jsocketpp/Socket.hpp>
 * #include <iostream>
 *
 * int main() {
 *     try {
 *         // Create a server socket listening on port 12345 (all interfaces, dual-stack IPv4/IPv6)
 *         ServerSocket server(12345);
 *         server.bind();
 *         server.listen();
 *         std::cout << "Server is listening on port 12345..." << std::endl;
 *
 *         while (true) {
 *             Socket client = server.accept();
 *             std::string remoteAddr = client.getRemoteSocketAddress();
 *             std::cout << "Accepted connection from " << remoteAddr << std::endl;
 *
 *             // Echo loop: read a string, send it back
 *             std::string message = client.read<std::string>();
 *             client.write(message);
 *         }
 *     } catch (const socket_exception& e) {
 *         std::cerr << "Server error: " << e.what() << std::endl;
 *     }
 *     return 0;
 * }
 * @endcode
 *
 * ## Key Features
 * - **Cross-platform**: Windows and Linux/Unix support
 * - **IPv4 & IPv6**: Automatic dual-stack support if available
 * - **Resource management**: RAII ensures sockets are closed automatically
 * - **Error handling**: Throws exceptions on error for robust applications
 * - **Customizable**: Control backlog, address reuse, blocking/non-blocking modes, etc.
 *
 * ## Basic API
 * - `ServerSocket(port)`: Construct with a port to listen on.
 * - `bind()`: Bind the server to the selected address and port.
 * - `listen()`: Start listening for connections.
 * - `accept()`: Accept a new client and return a `Socket`.
 * - `close()`: Close the server socket (also called automatically in destructor).
 *
 * @note
 * - Not thread-safe. Each `ServerSocket` instance should be used from a single thread at a time, unless
 *       external synchronization is applied. Concurrent calls to methods like `accept()` from multiple threads
 *       may result in undefined behavior.
 * - After calling `accept()`, you should use the returned `Socket` object to communicate with the client.
 * - The server socket only accepts TCP connections. Use `DatagramSocket` for UDP.
 * - Exceptions are thrown as `SocketException` for error conditions.
 *
 * ### See Also
 * - @ref Socket "Socket" - for connecting to a remote host
 * - @ref tcp - TCP socket group
 */
class ServerSocket : public SocketOptions
{
  public:
    /**
     * @brief Constructs a ServerSocket for listening to incoming TCP connections with full configuration control.
     *
     * This constructor creates a TCP server socket that supports both IPv4 and IPv6, with flexible options
     * for binding, listening, address selection, address reuse, accept timeouts, and dual-stack (IPv4+IPv6) control.
     *
     * The constructor performs the following steps:
     *   - Prepares address resolution hints for dual-stack TCP sockets (IPv4 and IPv6).
     *   - Uses `getaddrinfo()` to resolve the provided `localAddress` (IP address or hostname) and the given `port`.
     *     - If `localAddress` is empty (`{}`), the socket will accept connections on **ALL local interfaces**.
     *     - If non-empty, binds only to the specified address/interface (e.g., "127.0.0.1", "::1",
     *       "192.168.1.10").
     *   - Iterates through the address results, attempting to create a socket for each until one succeeds.
     *   - For IPv6 sockets, configures dual-stack or IPv6-only mode according to the `dualStack` parameter:
     *       - If `dualStack` is true (default), disables `IPV6_V6ONLY` for dual-stack support (accepts both IPv4 and
     * IPv6).
     *       - If `dualStack` is false, enables `IPV6_V6ONLY` for IPv6-only operation (no IPv4-mapped addresses).
     *   - Sets the address reuse option (`reuseAddress`) **before** binding:
     *       - On Windows, uses `SO_EXCLUSIVEADDRUSE` (for exclusive binding).
     *       - On Unix-like systems, uses `SO_REUSEADDR` (for fast port reuse).
     *   - If `autoBindListen` is true, automatically calls `bind()` and `listen()` (with default backlog).
     *     Otherwise, you must call them manually after construction.
     *   - Sets the accept timeout (`soTimeoutMillis`) for all subsequent `accept()` operations.
     *
     * @note
     *   - If you want to fine-tune socket options (e.g., reuse, timeouts) or bind on demand, use `autoBindListen =
     *     false` and set options before calling `bind()` and `listen()`.
     *   - The final reuse address setting is determined by the last value set before `bind()` (either by parameter or
     *     `setReuseAddress()`).
     *   - Once bound, further changes to address reuse have no effect.
     *   - The timeout applies to all `accept()` and `tryAccept()` calls unless a per-call timeout is provided.
     *   - For maximum compatibility with both IPv4 and IPv6 clients, use an empty `localAddress` and default settings.
     *   - Dual-stack mode is only relevant for IPv6 sockets. On platforms or addresses that do not support dual-stack,
     *     the `dualStack` parameter may be ignored.
     *
     * @note This constructor is not thread safe. Do not share a ServerSocket instance between threads during
     * construction.
     *
     * @param[in] port            The port number to prepare the server socket for (binding will occur according to
     *                        `autoBindListen`).
     * @param[in] localAddress    The local address/interface to bind to (empty for all interfaces).
     * @param[in] autoBindListen  If true (default), automatically binds and listens. If false, user must call them
     *                        manually.
     * @param[in] reuseAddress    If true (default), enables address reuse (see above) before binding.
     * @param[in] soTimeoutMillis Accept timeout in milliseconds for `accept()`; -1 (default) means block indefinitely.
     * @param[in] dualStack       If true (default), enables dual-stack (IPv4+IPv6) for IPv6 sockets. If false, enables
     *                        IPv6-only mode (no IPv4-mapped addresses). Has no effect for IPv4 sockets.
     *
     * @throws SocketException If address resolution, socket creation, binding, or socket option configuration fails.
     *
     * @see setReuseAddress(), setSoTimeout(), bind(), listen(), accept()
     *
     * @ingroup tcp
     *
     * @code
     * // Example: Minimal usage—listen on all interfaces, default options
     * jsocketpp::ServerSocket server(8080);
     *
     * // Example: Listen on all interfaces, enable address reuse, block up to 5 seconds for accept()
     * jsocketpp::ServerSocket server(8080, {}, true, true, 5000);
     *
     * // Example: Manual control—bind and listen later
     * jsocketpp::ServerSocket server(8080, "127.0.0.1", false, false);
     * server.setReuseAddress(true); // Change before bind
     * server.bind();
     * server.listen();
     *
     * // Example: IPv6-only server (no IPv4-mapped addresses)
     * jsocketpp::ServerSocket server(8080, "::1", true, true, -1, false);
     * @endcode
     */
    explicit ServerSocket(Port port, std::string_view localAddress = {}, bool autoBindListen = true,
                          bool reuseAddress = true, int soTimeoutMillis = -1, bool dualStack = true);

    /**
     * @brief Returns the local IP address this server socket is bound to.
     *
     * Retrieves the numeric IP address (IPv4 or IPv6) that the server socket is bound to.
     * If the socket is bound to a wildcard interface (e.g., "0.0.0.0" or "::"), the wildcard address
     * is returned as-is.
     *
     * If the address is an IPv4-mapped IPv6 (::ffff:a.b.c.d) and @p convertIPv4Mapped is true,
     * it is converted to its original IPv4 representation (e.g., "192.168.0.10").
     *
     * @param convertIPv4Mapped Whether to convert IPv4-mapped IPv6 addresses to plain IPv4. Default is true.
     * @return Local IP address as a string (e.g., "127.0.0.1", "::1", "0.0.0.0").
     *
     * @throws SocketException if the socket is invalid or the address cannot be retrieved.
     */
    [[nodiscard]] std::string getLocalIp(bool convertIPv4Mapped = true) const;

    /**
     * @brief Retrieve the local port number to which the server socket is bound.
     *
     * This method returns the port number that the server socket is currently bound to.
     * This is particularly useful when the socket is bound to port `0`, which tells the operating
     * system to automatically assign an available port. You can use this method after binding to discover the actual
     * port being used.
     *
     * @note This method is thread safe.
     * @ingroup tcp
     *
     * @return The local port number, or `0` if the socket is not bound.
     * @throws SocketException if there is an error retrieving the port number.
     *
     * @code
     * ServerSocket server(0); // Bind to any available port
     * server.bind();
     * Port port = server.getLocalPort();
     * std::cout << "Server bound to port: " << port << std::endl;
     * @endcode
     */
    [[nodiscard]] Port getLocalPort() const;

    /**
     * @brief Get the local socket address (IP and port) to which the server socket is bound.
     *
     * Returns a string with the IP address and port in the format "ip:port" (e.g., "127.0.0.1:8080").
     * Useful for debugging, logging, and displaying server status.
     *
     * @note This method is thread safe.
     * @ingroup tcp
     *
     * @return The local socket address as a string, or an empty string if the socket is not bound.
     * @throws SocketException if there is an error retrieving the address.
     *
     * @code
     * ServerSocket server(8080);
     * server.bind();
     * std::cout << "Server bound to: " << server.getLocalSocketAddress() << std::endl;
     * @endcode
     */
    [[nodiscard]] std::string getLocalSocketAddress() const;

    /**
     * @brief Copy constructor (deleted).
     *
     * This constructor is explicitly deleted because ServerSocket instances manage unique system resources (socket
     * descriptors) that cannot be safely shared or duplicated. Each socket must have exclusive ownership of its
     * underlying system resources to ensure proper cleanup and avoid resource leaks.
     *
     * Instead of copying, use move semantics (ServerSocket&&) to transfer ownership of a socket between objects.
     *
     * @param[in] rhs The ServerSocket to copy from (unused since deleted).
     *
     * @note This deletion helps prevent accidental copying of socket resources and enforces RAII principles.
     *
     * @see ServerSocket(ServerSocket&&)
     * @see operator=(ServerSocket&&)
     *
     * @ingroup tcp
     */
    ServerSocket(const ServerSocket& rhs) = delete; //-Weffc++

    /**
     * @brief Copy assignment operator (deleted).
     *
     * ServerSocket objects cannot be copied because they represent unique system resources.
     * Each server socket needs exclusive ownership of its underlying file descriptor and
     * associated resources. Use move semantics (operator=(ServerSocket&&)) instead to
     * transfer ownership between ServerSocket objects.
     *
     * @param[in] rhs The ServerSocket to copy from (unused since deleted)
     * @return Reference to this ServerSocket (never returns since deleted)
     *
     * @note This deletion enforces RAII principles and prevents resource leaks.
     *
     * @see operator=(ServerSocket&&)
     * @see ServerSocket(ServerSocket&&)
     *
     * @ingroup tcp
     */
    ServerSocket& operator=(const ServerSocket& rhs) = delete; //-Weffc++

    /**
     * @brief Move constructor that transfers ownership of server socket resources.
     *
     * This constructor implements move semantics to efficiently transfer ownership of
     * socket resources from one ServerSocket object to another. It's particularly useful
     * when you need to transfer socket ownership (e.g., returning from functions or
     * storing in containers) without copying the underlying system resources.
     *
     * The operation provides the strong exception guarantee and is marked noexcept:
     * - All resources are transferred from `rhs` to the new object.
     * - The moved-from object (`rhs`) is left in a valid but empty state.
     * - No system resources are duplicated or leaked during the transfer.
     *
     * After the move:
     * - The new object takes full ownership of the socket and its state.
     * - The moved-from object becomes closed (invalid socket, null pointers).
     * - All socket options and state flags are transferred.
     *
     * @param[in] rhs The ServerSocket to move resources from.
     *
     * @note This operation is thread-safe with respect to the moved-from socket,
     *       but concurrent operations on either socket during the move may cause
     *       undefined behavior.
     *
     * @see operator=(ServerSocket&&)
     * @see close()
     *
     * @ingroup tcp
     */
    ServerSocket(ServerSocket&& rhs) noexcept
        : SocketOptions(rhs._serverSocket), _serverSocket(rhs._serverSocket), _srvAddrInfo(std::move(rhs._srvAddrInfo)),
          _selectedAddrInfo(rhs._selectedAddrInfo), _port(rhs._port), _isBound(rhs._isBound),
          _isListening(rhs._isListening), _soTimeoutMillis(rhs._soTimeoutMillis),
          _defaultReceiveBufferSize(rhs._defaultReceiveBufferSize)
    {
        rhs._serverSocket = INVALID_SOCKET;
        rhs.setSocketFd(INVALID_SOCKET);
        rhs._selectedAddrInfo = nullptr;
        rhs._isBound = false;
        rhs._isListening = false;
    }

    /**
     * @brief Move assignment operator for ServerSocket.
     *
     * Transfers ownership of socket resources from another ServerSocket object to this one.
     * If this socket already owns resources, they are properly cleaned up before the transfer.
     *
     * The operation is **noexcept** and provides the strong exception guarantee:
     * - If this socket already owns resources, they are closed before the transfer.
     * - The moved-from socket is left in a valid but empty state (closed socket, null pointers).
     * - No system resources are leaked during the transfer.
     *
     * After the move:
     * - This socket takes ownership of all resources from `rhs`.
     * - The moved-from socket (`rhs`) becomes closed and can be safely destroyed.
     * - All socket options, state flags, and configurations are transferred.
     *
     * @param[in] rhs The ServerSocket to move resources from.
     * @return Reference to this ServerSocket (containing the moved resources).
     *
     * @note This operation is thread-safe with respect to the moved-from socket,
     *       but concurrent operations on either socket during the move may cause undefined behavior.
     *
     * @see ServerSocket(ServerSocket&&)
     * @see close()
     *
     * @ingroup tcp
     */
    ServerSocket& operator=(ServerSocket&& rhs) noexcept
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
            _serverSocket = rhs._serverSocket;
            setSocketFd(_serverSocket);
            _srvAddrInfo = std::move(rhs._srvAddrInfo);
            _selectedAddrInfo = rhs._selectedAddrInfo;
            _port = rhs._port;
            _isBound = rhs._isBound;
            _isListening = rhs._isListening;
            _soTimeoutMillis = rhs._soTimeoutMillis;
            _defaultReceiveBufferSize = rhs._defaultReceiveBufferSize;

            // Reset source
            rhs._serverSocket = INVALID_SOCKET;
            rhs.setSocketFd(INVALID_SOCKET);
            rhs._selectedAddrInfo = nullptr;
            rhs._isBound = false;
            rhs._isListening = false;
        }
        return *this;
    }

    /**
     * @brief Destructor that automatically closes the server socket and releases all associated resources.
     *
     * This destructor ensures proper cleanup of system resources when a ServerSocket object is destroyed:
     * - Closes the underlying socket handle/descriptor if still open
     * - Frees any allocated memory for address structures
     * - Releases system socket resources
     * - Sets internal state to closed/invalid
     *
     * The destructor is marked `noexcept` to prevent exception propagation during stack unwinding,
     * as per C++ best practices. Any errors that occur during cleanup are logged but not thrown.
     *
     * ### Resource Cleanup
     * - Socket handle is closed via close()
     * - Address info structures (_srvAddrInfo) are freed
     * - Internal buffers and state are reset
     *
     * ### Thread Safety
     * - This destructor is **not thread-safe**
     * - The ServerSocket must not be used by other threads during destruction
     * - If multiple threads might access the socket during shutdown, external synchronization is required
     *
     * @warning
     * - Do not destroy a ServerSocket while other threads are using it
     * - Ensure all client operations are complete before destruction
     * - Use proper synchronization if the socket might be accessed during shutdown
     *
     * @see close()
     * @see Socket
     *
     * @ingroup tcp
     */
    ~ServerSocket() noexcept override;

    /**
     * @brief Binds the server socket to the configured port and network interface.
     *
     * This method assigns a local address and port number to the socket, making it ready to accept incoming TCP
     * connections.
     *
     * - **Preconditions:** The socket must have been created successfully (via the constructor) but must not be already
     * bound or listening.
     * - **Typical usage:** Call `bind()` after configuring any desired socket options (such as address reuse) and
     * before calling `listen()`.
     * - **Effect:** Once `bind()` succeeds, the server socket is associated with the local port specified during
     * construction, and is ready to transition to listening mode with `listen()`.
     *
     * @note
     * - If you want to override the default address reuse behavior or other options, you must call the corresponding
     * setter methods *before* calling `bind()`. After `bind()` is called, changes to those options will have no effect.
     * - On error, this function throws a `SocketException` containing the specific error code and message.
     * - This function is cross-platform and works on both Windows and Unix-like systems.
     *
     * @throws SocketException if the bind operation fails (for example, if the port is already in use or insufficient
     * permissions).
     *
     * @pre `_selectedAddrInfo` must not be null.
     * @pre The socket must not be already bound.
     * @post The socket is bound to the specified address and port.
     *
     * @see setReuseAddress(), listen(), ServerSocket(Port)
     *
     * @ingroup tcp
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.setReuseAddress(true); // Optional: customize before binding
     * server.bind();
     * server.listen();
     * @endcode
     */
    void bind();

    /**
     * @brief Check if the server socket is bound to a local address.
     *
     * Returns `true` if the socket has been successfully bound to a local address and port using the `bind()` method,
     * or `false` otherwise. This means the socket has reserved a port but is not necessarily accepting connections yet.
     *
     * Follows the naming and semantics of Java's `ServerSocket::isBound()`.
     *
     * @return `true` if the socket is bound, `false` otherwise.
     *
     * @ingroup tcp
     */
    [[nodiscard]] bool isBound() const noexcept { return _isBound; }

    /**
     * @brief Marks the socket as a passive (listening) socket, ready to accept incoming TCP connection requests.
     *
     * This method puts the server socket into listening mode, so it can accept incoming client connections using
     * `accept()`.
     *
     * - **How it works:** After binding the socket to a local address and port (via `bind()`), call `listen()` to have
     * the operating system start queueing incoming connection requests. Only after calling `listen()` can the server
     * accept connections.
     * - **Backlog parameter:** The `backlog` argument specifies the maximum number of pending client connections that
     * can be queued before connections are refused. If not specified, the system default (`SOMAXCONN`) is used.
     * - **Usage sequence:** Typical usage is: `ServerSocket sock(port); sock.bind(); sock.listen();`
     *
     * @note
     * - You must call `bind()` before calling `listen()`. If the socket is not bound, this call will fail.
     * - The backlog parameter is a hint to the operating system. The actual queue length may be capped by system
     *   configuration (e.g., `/proc/sys/net/core/somaxconn` on Linux). On Windows, `SOMAXCONN` may be very large, so a
     *   smaller value (e.g., 128) is recommended for most applications.
     * - On success, the socket is ready for calls to `accept()`.
     * - On error (e.g., socket not bound, invalid arguments, system resource exhaustion), a `SocketException` is thrown
     * with details.
     *
     * @throws SocketException if the listen operation fails.
     *
     * @pre `bind()` must be called successfully before this.
     * @post The server socket is now listening for incoming connections.
     *
     * @see bind(), accept(), ServerSocket(Port)
     *
     * @ingroup tcp
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.setReuseAddress(true); // Optional: set before bind
     * server.bind();
     * server.listen();
     * while (true) {
     *     jsocketpp::Socket client = server.accept();
     *     // Handle client connection
     * }
     * @endcode
     */
    void listen(int backlog = 128);

    /**
     * @brief Check if the server socket is currently listening for incoming connections.
     *
     * Returns `true` if the socket has successfully entered the listening state by calling the `listen()` method,
     * and is ready to accept new client connections. Returns `false` otherwise.
     *
     * This complements `isBound()`, which only tells you if the socket has been bound to a local address.
     *
     * @return `true` if the socket is listening for connections, `false` otherwise.
     *
     * @ingroup tcp
     */
    [[nodiscard]] bool isListening() const noexcept { return _isListening; }

    /**
     * @brief Accept an incoming client connection, respecting the configured socket timeout.
     *
     * @note This method is **not thread safe**. Simultaneous calls to `accept()` or `acceptNonBlocking()` from
     * multiple threads (or processes) may result in race conditions or unexpected exceptions.
     *
     * Waits for an incoming client connection using the timeout value configured by `setSoTimeout()`.
     * - If the timeout is **negative** (default), the method blocks indefinitely until a client connects.
     * - If the timeout is **zero**, the method polls and returns immediately if no client is waiting.
     * - If the timeout is **positive**, the method waits up to that many milliseconds for a connection, then throws a
     *   `SocketTimeoutException` if none arrives.
     *
     * Internally, this method uses `waitReady()` (which internally uses `select()`) to wait for readiness and
     * only then calls `accept()`. If the timeout expires with no client, a `SocketTimeoutException` is thrown.
     *
     * The **blocking or non-blocking mode** of the server socket (via `setNonBlocking()`) does not affect the
     * waiting behavior of this method, since `select()` is used for waiting.
     *
     * @note - Use `setSoTimeout()` and `getSoTimeout()` to configure the timeout.
     *       For manual non-blocking accept in an event loop, see `acceptNonBlocking()` and `waitReady()`.
     *
     * @note - To safely use `accept()` in a multithreaded environment, protect access to the `ServerSocket`
     *       with a mutex.
     *
     * @ingroup tcp
     *
     * @param[in] recvBufferSize The receive buffer size (`SO_RCVBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultReceiveBufferSize()`.
     * @param[in] sendBufferSize The send buffer size (`SO_SNDBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultSendBufferSize()`.
     * @param[in] internalBufferSize The internal buffer size used by the accepted socket for `read<T>()` operations.
     * Defaults to `DefaultBufferSize`. Can also be set via `setDefaultInternalBufferSize()`.
     *
     * @return A `Socket` object representing the connected client.
     *
     * @throws SocketException if the server socket is not initialized, closed, or if an internal error occurs.
     * @throws SocketTimeoutException if the timeout expires before a client connects.
     *
     * @pre Server socket must be valid, bound, and listening.
     * @post A new connected `Socket` is returned on success.
     *
     * @see setSoTimeout(int)
     * @see getSoTimeout()
     * @see acceptBlocking()
     * @see acceptNonBlocking()
     * @see waitReady()
     */
    [[nodiscard]] Socket accept(std::optional<std::size_t> recvBufferSize = std::nullopt,
                                std::optional<std::size_t> sendBufferSize = std::nullopt,
                                std::optional<std::size_t> internalBufferSize = std::nullopt) const;

    /**
     * @brief Accept an incoming client connection, waiting up to the specified timeout.
     *
     * @note This method is **not thread safe**. Simultaneous calls to `accept()` or `acceptNonBlocking()` from
     * multiple threads (or processes) may result in race conditions or unexpected exceptions.
     *
     * Waits for an incoming client connection using the timeout specified by the `timeoutMillis` parameter:
     * - If `timeoutMillis` is **negative**, the method blocks indefinitely until a client connects.
     * - If `timeoutMillis` is **zero**, the method polls and returns immediately if no client is waiting.
     * - If `timeoutMillis` is **positive**, the method waits up to that many milliseconds for a connection,
     *   then throws a `SocketTimeoutException` if none arrives.
     *
     * Internally, this method uses `waitReady(timeoutMillis)` (which uses `select()`) to wait for readiness,
     * then calls `acceptBlocking()` with the resolved buffer sizes. If the timeout expires with no client,
     * a `SocketTimeoutException` is thrown.
     *
     * The **blocking or non-blocking mode** of the server socket (via `setNonBlocking()`) does not affect the
     * waiting behavior of this method, since `select()` is used for readiness detection.
     *
     * @note Use this method to specify a timeout **per call**. To configure a default for all accepts,
     *       use `setSoTimeout()`. For non-blocking polling, use `tryAccept()` or `acceptNonBlocking()`.
     *
     * @param[in] timeoutMillis Timeout in milliseconds to wait for a client connection:
     *   - Negative: block indefinitely
     *   - Zero: poll once
     *   - Positive: wait up to this many milliseconds
     * @param[in] recvBufferSize The receive buffer size (`SO_RCVBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultReceiveBufferSize()`.
     * @param[in] sendBufferSize The send buffer size (`SO_SNDBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultSendBufferSize()`.
     * @param[in] internalBufferSize The internal buffer size used by the accepted socket for `read<T>()` operations.
     * Defaults to `DefaultBufferSize`. Can also be set via `setDefaultInternalBufferSize()`.
     *
     * @return A `Socket` object representing the connected client.
     *
     * @throws SocketException if the server socket is invalid or closed, or if an internal error occurs.
     * @throws SocketTimeoutException if no client connects before the timeout expires.
     *
     * @pre Server socket must be valid, bound, and listening.
     * @post A new connected `Socket` is returned on success.
     *
     * @see accept()
     * @see tryAccept()
     * @see acceptBlocking()
     * @see setSoTimeout()
     * @see waitReady()
     *
     * @ingroup tcp
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * try {
     *     jsocketpp::Socket client = server.accept(2000); // Wait up to 2 seconds
     *     // Handle client...
     * } catch (const jsocketpp::SocketTimeoutException&) {
     *     std::cout << "No client connected within timeout." << std::endl;
     * }
     * @endcode
     */
    [[nodiscard]] Socket accept(int timeoutMillis, std::optional<std::size_t> recvBufferSize = std::nullopt,
                                std::optional<std::size_t> sendBufferSize = std::nullopt,
                                std::optional<std::size_t> internalBufferSize = std::nullopt) const;

    /**
     * @brief Attempt to accept an incoming client connection, returning immediately or after the configured timeout.
     *
     * This method waits for an incoming client connection using the timeout set by `setSoTimeout()`. If no timeout
     * is configured (i.e., negative value), the method blocks indefinitely until a client connects. If the timeout is
     * zero, the method polls and returns immediately if no client is waiting.
     *
     * Unlike `accept()`, this method does **not** throw a `SocketTimeoutException` if no client is available.
     * Instead, it returns `std::nullopt`. This makes it suitable for event loops or non-blocking server designs.
     *
     * Internally, this method uses `waitReady()` (which uses `select()`) to check for pending connections. If a client
     * is ready, `acceptBlocking()` is called to retrieve the connection. If not, `std::nullopt` is returned.
     *
     * The **blocking or non-blocking mode** of the server socket (via `setNonBlocking()`) does not affect the waiting
     * behavior of this method, since `select()` is used for readiness. In rare cases, `accept()` may still fail with
     * `EWOULDBLOCK` or `EAGAIN` if the connection is lost between readiness check and accept call, in which case a
     * `SocketException` is thrown.
     *
     * @note This method is **not thread safe**. Simultaneous calls from multiple threads or processes may lead to race
     * conditions. See `accept()` for a discussion of those edge cases.
     *
     * @note To specify a timeout for a single call, use `tryAccept(int timeoutMillis, ...)`.
     *
     * @note This method uses the logical timeout configured via `setSoTimeout()` and does not rely on
     *       kernel-level socket timeouts. To control socket-level I/O timeout (for client sockets), see
     *       `Socket::setSoTimeout()`.
     *
     * @param[in] recvBufferSize The receive buffer size (`SO_RCVBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultReceiveBufferSize()`.
     * @param[in] sendBufferSize The send buffer size (`SO_SNDBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultSendBufferSize()`.
     * @param[in] internalBufferSize The internal buffer size used by the accepted socket for `read<T>()` operations.
     * Defaults to `DefaultBufferSize`. Can also be set via `setDefaultInternalBufferSize()`.
     *
     * @return An `std::optional<Socket>` containing the accepted client socket, or `std::nullopt` if no client was
     *         available before the timeout.
     *
     * @throws SocketException if the server socket is not initialized, closed, or if an internal error occurs.
     *
     * @pre Server socket must be valid, bound, and listening.
     * @post Returns `std::nullopt` if timeout expires, or a connected `Socket` on success.
     *
     * @see setSoTimeout(int)
     * @see getSoTimeout()
     * @see accept()
     * @see tryAccept(int, std::optional<std::size_t>, std::optional<std::size_t>)
     * @see acceptBlocking()
     * @see waitReady()
     *
     * @ingroup tcp
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * server.setSoTimeout(100); // Poll every 100 ms for new clients
     * while (true) {
     *     if (auto client = server.tryAccept()) {
     *         // Handle client...
     *     } else {
     *         // No client ready yet; perform other tasks or continue loop
     *     }
     * }
     * @endcode
     */
    [[nodiscard]] std::optional<Socket> tryAccept(std::optional<std::size_t> recvBufferSize = std::nullopt,
                                                  std::optional<std::size_t> sendBufferSize = std::nullopt,
                                                  std::optional<std::size_t> internalBufferSize = std::nullopt) const;

    /**
     * @brief Attempt to accept an incoming client connection, waiting up to a specified timeout.
     *
     * This method waits for an incoming client connection using the provided `timeoutMillis` value, overriding any
     * global timeout set with `setSoTimeout()`. If `timeoutMillis` is negative, the method blocks indefinitely until a
     * client connects. If `timeoutMillis` is zero, the method polls and returns immediately if no client is waiting.
     *
     * Unlike `accept(int timeoutMillis, ...)`, which throws a `SocketTimeoutException` if the timeout expires,
     * this method returns `std::nullopt` in that case. This makes it ideal for polling loops or event-driven servers.
     *
     * Internally, this method uses `waitReady(timeoutMillis)` (via `select()`) to check for connection readiness.
     * If the socket is ready, `acceptBlocking(...)` is called to retrieve the connection. Otherwise, `std::nullopt` is
     * returned.
     *
     * The **blocking or non-blocking mode** of the server socket (via `setNonBlocking(true)`) does **not** affect
     * the behavior of this method. Readiness is determined exclusively via `select()`. However, due to inherent race
     * conditions in all socket APIs, it’s still possible for `accept()` to fail with `EWOULDBLOCK` or `EAGAIN` after
     * readiness has been reported. In such cases, a `SocketException` is thrown.
     *
     * @note This method is **not thread safe**. Protect server socket access externally if used from multiple threads.
     *
     * @param[in] timeoutMillis Timeout in milliseconds to wait for a client connection:
     *   - Negative: block indefinitely
     *   - Zero: poll once
     *   - Positive: wait up to this many milliseconds
     * @param[in] recvBufferSize The receive buffer size (`SO_RCVBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultReceiveBufferSize()`.
     * @param[in] sendBufferSize The send buffer size (`SO_SNDBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultSendBufferSize()`.
     * @param[in] internalBufferSize The internal buffer size used by the accepted socket for `read<T>()` operations.
     * Defaults to `DefaultBufferSize`. Can also be set via `setDefaultInternalBufferSize()`.
     *
     * @return `std::optional<Socket>` containing a client connection if accepted, or `std::nullopt` on timeout.
     *
     * @throws SocketException if the server socket is invalid, closed, or an internal error occurs.
     *
     * @pre Server socket must be valid, bound, and listening.
     * @post Returns `std::nullopt` if timeout expires; otherwise a connected `Socket`.
     *
     * @see accept()
     * @see tryAccept()
     * @see acceptBlocking()
     * @see acceptNonBlocking()
     * @see waitReady()
     *
     * @ingroup tcp
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * while (true) {
     *     if (auto client = server.tryAccept(100)) {
     *         // Handle client connection
     *     } else {
     *         // No client yet; continue polling
     *     }
     * }
     * @endcode
     */
    [[nodiscard]] std::optional<Socket> tryAccept(int timeoutMillis,
                                                  std::optional<std::size_t> recvBufferSize = std::nullopt,
                                                  std::optional<std::size_t> sendBufferSize = std::nullopt,
                                                  std::optional<std::size_t> internalBufferSize = std::nullopt) const;

    /**
     * @brief Accept an incoming client connection, always blocking until a client connects (unless the socket is set to
     * non-blocking).
     *
     * This method directly invokes the underlying system `accept()` on the listening socket.
     * - If the socket is in **blocking mode** (default), this method blocks until a client connects. It ignores any
     * timeout set by `setSoTimeout()`.
     * - If the socket is in **non-blocking mode**, the call returns immediately:
     *   - If a client is pending, returns a connected `Socket`.
     *   - If no client is waiting, throws `SocketException` with `EWOULDBLOCK` or `EAGAIN`.
     *
     * @par Comparison with acceptNonBlocking()
     * - Both methods behave the same, based on the socket's blocking mode.
     * - `acceptBlocking()` throws on *all* failures, and is idiomatic for classic server loops.
     * - `acceptNonBlocking()` is used in polling/event-driven code where you want to return `std::nullopt` when no
     * client is available.
     *
     * @note This method does not perform any timeout polling. Use `accept()` or `tryAccept()` if you need timeouts.
     *
     * @note This method is **not thread safe**. Simultaneous `accept` calls on the same socket from multiple threads or
     * processes may result in race conditions or failures.
     *
     * @param[in] recvBufferSize The receive buffer size (`SO_RCVBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultReceiveBufferSize()`.
     * @param[in] sendBufferSize The send buffer size (`SO_SNDBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultSendBufferSize()`.
     * @param[in] internalBufferSize The internal buffer size used by the accepted socket for `read<T>()` operations.
     * Defaults to `DefaultBufferSize`. Can also be set via `setDefaultInternalBufferSize()`.
     *
     * @return A `Socket` object representing the connected client.
     *
     * @throws SocketException if the server socket is invalid or `accept()` fails (including with `EWOULDBLOCK` or
     *         `EAGAIN` in non-blocking mode).
     *
     * @pre Server socket must be valid, bound, and listening.
     * @post A new connected `Socket` is returned on success.
     *
     * @see accept()
     * @see tryAccept()
     * @see acceptNonBlocking()
     * @see setNonBlocking()
     * @see waitReady()
     *
     * @ingroup tcp
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     *
     * // Blocking example
     * jsocketpp::Socket client = server.acceptBlocking();
     *
     * // Non-blocking pattern
     * server.setNonBlocking(true);
     * try {
     *     jsocketpp::Socket client = server.acceptBlocking();
     *     // Handle client
     * } catch (const jsocketpp::SocketException& e) {
     *     if (e.code() == EWOULDBLOCK) {
     *         // No client yet
     *     } else {
     *         throw;
     *     }
     * }
     * @endcode
     */
    [[nodiscard]] Socket acceptBlocking(std::optional<std::size_t> recvBufferSize = std::nullopt,
                                        std::optional<std::size_t> sendBufferSize = std::nullopt,
                                        std::optional<std::size_t> internalBufferSize = std::nullopt) const;

    /**
     * @brief Attempt to accept a client connection in non-blocking fashion.
     *
     * This method attempts to accept an incoming client connection using the system `accept()` call:
     * - If the server socket is in **blocking mode** (default), the call will block until a client connects.
     * - If the socket is in **non-blocking mode** (`setNonBlocking(true)`), the call will return immediately:
     *   - If a client is ready, returns a `Socket` object.
     *   - If no client is waiting, returns `std::nullopt`.
     *
     * This method does **not** use `select()`, `poll()`, or any timeout logic. It is ideal for event loops and
     * polling-based architectures where you explicitly check for readiness before accepting.
     *
     * @note The blocking behavior depends entirely on the socket mode, which is controlled by `setNonBlocking(bool)`.
     * @note For timeout-aware or readiness-checked accept patterns, use `accept()`, `tryAccept()`, or
     * `acceptBlocking()`.
     *
     * @param[in] recvBufferSize The receive buffer size (`SO_RCVBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultReceiveBufferSize()`.
     * @param[in] sendBufferSize The send buffer size (`SO_SNDBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultSendBufferSize()`.
     * @param[in] internalBufferSize The internal buffer size used by the accepted socket for `read<T>()` operations.
     * Defaults to `DefaultBufferSize`. Can also be set via `setDefaultInternalBufferSize()`.
     *
     * @return A `Socket` object if a client is connected, or `std::nullopt` if no connection was ready.
     *
     * @throws SocketException if the socket is invalid or `accept()` fails due to a system error
     *         (excluding `EWOULDBLOCK` / `EAGAIN` on non-blocking sockets).
     *
     * @pre Server socket must be valid, bound, and listening.
     * @post Returns a connected `Socket` or `std::nullopt` if no client is pending.
     *
     * @see accept()
     * @see tryAccept()
     * @see acceptBlocking()
     * @see setNonBlocking()
     * @see waitReady()
     *
     * @ingroup tcp
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * server.setNonBlocking(true);
     *
     * while (true) {
     *     auto client = server.acceptNonBlocking();
     *     if (client) {
     *         // Handle client...
     *     } else {
     *         // No client yet; do other work
     *     }
     * }
     * @endcode
     */
    [[nodiscard]] std::optional<Socket>
    acceptNonBlocking(std::optional<std::size_t> recvBufferSize = std::nullopt,
                      std::optional<std::size_t> sendBufferSize = std::nullopt,
                      std::optional<std::size_t> internalBufferSize = std::nullopt) const;

    /**
     * @brief Asynchronously accept an incoming client connection, returning a future.
     *
     * This method launches an asynchronous accept operation on the server socket,
     * returning a `std::future<Socket>` that resolves once a client connects or an error occurs.
     *
     * Internally, this uses `std::async(std::launch::async, ...)` to spawn a background thread
     * that calls `accept(...)`. The calling thread is never blocked.
     *
     * When a client is accepted, the future becomes ready and yields a fully constructed `Socket` object.
     * If an error or timeout occurs, the exception is rethrown when `.get()` is called on the future.
     *
     * @note This is **not thread safe**: do not call `acceptAsync()`, `accept()`, or related methods
     * concurrently on the same `ServerSocket` instance unless externally synchronized.
     *
     * @note The `ServerSocket` object must outlive the returned future. Use caution when capturing `this`.
     *
     * @param[in] recvBufferSize The receive buffer size (`SO_RCVBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultReceiveBufferSize()`.
     * @param[in] sendBufferSize The send buffer size (`SO_SNDBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultSendBufferSize()`.
     * @param[in] internalBufferSize The internal buffer size used by the accepted socket for `read<T>()` operations.
     * Defaults to `DefaultBufferSize`. Can also be set via `setDefaultInternalBufferSize()`.
     *
     * @return A `std::future<Socket>` that resolves to a connected client socket or throws on error.
     *
     * @throws SocketException if a fatal socket error occurs.
     * @throws SocketTimeoutException if a timeout is configured and no client connects.
     *
     * @pre Server socket must be valid, bound, and listening.
     * @post Future resolves to a connected `Socket`, or throws from `.get()`.
     *
     * @see accept()
     * @see tryAccept()
     * @see acceptBlocking()
     * @see std::future, std::async
     *
     * @ingroup tcp
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * auto future = server.acceptAsync();
     *
     * // Do other work while waiting...
     * if (future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
     *     jsocketpp::Socket client = future.get();
     *     // Handle client...
     * } else {
     *     // Timeout or still waiting
     * }
     * @endcode
     */
    [[nodiscard]] std::future<Socket> acceptAsync(std::optional<std::size_t> recvBufferSize = std::nullopt,
                                                  std::optional<std::size_t> sendBufferSize = std::nullopt,
                                                  std::optional<std::size_t> internalBufferSize = std::nullopt) const;

    /**
     * @brief Asynchronously accept a client connection and invoke a callback upon completion.
     *
     * Launches a background thread to perform an `accept()` operation and calls the user-provided callback
     * when a client connects or an error occurs. This is useful for event-driven servers that require
     * non-blocking, callback-based acceptance of new clients.
     *
     * The callback is invoked exactly once with:
     * - a valid `Socket` and `nullptr` exception on success, or
     * - an empty `std::optional<Socket>` and a non-null `std::exception_ptr` on error.
     *
     * Internally, this uses a detached `std::thread` that invokes `accept(...)`, wrapped in a try-catch block.
     * If the server socket is not valid or an error occurs (including timeouts), the exception is captured
     * and passed to the callback for inspection or rethrowing via `std::rethrow_exception()`.
     *
     * @note This method is **not thread safe**. Avoid concurrent calls to `accept`, `acceptAsync`, or related
     * methods from multiple threads unless externally synchronized.
     *
     * @note The `ServerSocket` object must remain alive until the callback is invoked. Destroying the object
     * before the background thread finishes is undefined behavior.
     *
     * @param[in] callback Completion handler with signature:
     *   `void callback(std::optional<Socket>, std::exception_ptr)`
     * @param[in] recvBufferSize The receive buffer size (`SO_RCVBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultReceiveBufferSize()`.
     * @param[in] sendBufferSize The send buffer size (`SO_SNDBUF`) to apply to the accepted socket. Defaults to
     * `DefaultBufferSize`. Can also be set via `setDefaultSendBufferSize()`.
     * @param[in] internalBufferSize The internal buffer size used by the accepted socket for `read<T>()` operations.
     * Defaults to `DefaultBufferSize`. Can also be set via `setDefaultInternalBufferSize()`.
     *
     * @pre Server socket must be valid, bound, and listening.
     * @post Callback is invoked exactly once with either a valid `Socket` or an exception.
     *
     * @see accept()
     * @see tryAccept()
     * @see acceptAsync(std::future)
     * @see std::optional
     * @see std::exception_ptr, std::rethrow_exception
     *
     * @ingroup tcp
     *
     * @code
     * server.acceptAsync(
     *     [](std::optional<jsocketpp::Socket> clientOpt, std::exception_ptr eptr) {
     *         if (eptr) {
     *             try { std::rethrow_exception(eptr); }
     *             catch (const jsocketpp::SocketException& ex) {
     *                 std::cerr << "Accept failed: " << ex.what() << std::endl;
     *             }
     *         } else if (clientOpt) {
     *             std::cout << "Accepted client from: "
     *                       << clientOpt->getRemoteSocketAddress() << std::endl;
     *             // Handle client...
     *         }
     *     },
     *     8192, // receive buffer size
     *     4096  // send buffer size
     * );
     * @endcode
     */
    void acceptAsync(std::function<void(std::optional<Socket>, std::exception_ptr)> callback,
                     std::optional<std::size_t> recvBufferSize = std::nullopt,
                     std::optional<std::size_t> sendBufferSize = std::nullopt,
                     std::optional<std::size_t> internalBufferSize = std::nullopt) const;

    /**
     * @brief Closes the server socket and releases its associated system resources.
     *
     * This method closes the underlying server socket, making it no longer able to accept new client connections.
     * After calling `close()`, the server socket enters the **CLOSED** state, and any further operations such as
     * `accept()`, `bind()`, or `listen()` will fail with an exception.
     *
     * Key details:
     * - All file descriptors or handles associated with the server socket are released.
     * - This operation is **idempotent**: calling `close()` multiple times on the same socket is safe, but only the
     * first call has effect.
     * - Existing client sockets returned by `accept()` are unaffected; you must close them individually.
     * - On many systems, closing a socket that is actively being used may result in a `SocketException` if a system
     * error occurs.
     * - After closing, you should not use this `ServerSocket` object for further network operations.
     *
     * Example usage:
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * // ... handle clients ...
     * server.close(); // Clean up when done
     * @endcode
     *
     * @throws SocketException If an error occurs while closing the socket (for example, if the underlying system call
     * fails).
     *
     * @note On some systems, closing a socket with active client connections does not forcibly disconnect clients,
     *       but simply prevents new connections from being accepted.
     * @note Always close your sockets when finished to prevent resource leaks!
     *
     * @ingroup tcp
     */
    void close();

    /**
     * @brief Check whether the server socket is currently open and valid.
     *
     * This method determines if the server socket has been successfully created and is ready for binding, listening,
     * or accepting connections. It checks whether the underlying socket handle is valid on the current platform.
     *
     * @note
     * - Returns `true` if the server socket has been created and not yet closed.
     * - Returns `false` if the socket has not been created, or has already been closed (and resources released).
     *
     * @return `true` if the server socket is open and valid; `false` otherwise.
     *
     * @see close(), isClosed()
     *
     * @ingroup tcp
     */
    [[nodiscard]] bool isValid() const noexcept { return this->_serverSocket != INVALID_SOCKET; }

    /**
     * @brief Check if the server socket has been closed.
     *
     * This method returns `true` if the socket has been closed (and is no longer usable), or `false` if it is still
     * open. The logic and naming follow the Java networking API for familiarity.
     *
     * @return `true` if the socket has been closed, `false` if it is still open.
     *
     * @see isValid(), close()
     *
     * @ingroup tcp
     */
    [[nodiscard]] bool isClosed() const noexcept { return this->_serverSocket == INVALID_SOCKET; }

    /**
     * @brief Set the server socket to non-blocking or blocking mode.
     *
     * When in non-blocking mode, the `accept()` call will return immediately if no connections are pending,
     * instead of blocking until a client connects.
     *
     * This is useful for integrating the server socket into event loops or custom I/O polling systems.
     *
     * @note This only affects the listening server socket. The accepted sockets returned by `accept()`
     * remain in blocking mode by default and must be configured separately.
     *
     * @param[in] nonBlocking true to enable non-blocking mode, false for blocking (default).
     * @throws SocketException on error.
     *
     * @see acceptBlocking(), acceptNonBlocking()
     *
     * @ingroup socketopts
     */
    void setNonBlocking(bool nonBlocking);

    /**
     * @brief Check if the server socket is in non-blocking mode.
     *
     * This function queries the socket's current blocking mode.
     * In non-blocking mode, operations like accept() return immediately
     * if no connection is available, instead of blocking.
     *
     * @return true if the socket is non-blocking, false if it is blocking.
     * @throws SocketException if the socket flags cannot be retrieved.
     *
     * @ingroup socketopts
     */
    [[nodiscard]] bool getNonBlocking() const;

    /**
     * @brief Waits for the server socket to become ready to accept an incoming connection.
     * @ingroup tcp
     *
     * This method blocks or polls until the server socket becomes readable, indicating that a
     * new client is attempting to connect. It is used internally by `accept()` and `tryAccept()`,
     * but can also be used directly in custom event loops or multiplexed servers.
     *
     * ### Platform Behavior
     * - **POSIX:** Uses `poll()` to monitor readiness, avoiding the `FD_SETSIZE` constraint imposed by `select()`.
     * - **Windows:** Uses `select()` to ensure compatibility with all socket types.
     *   - The socket descriptor must be less than `FD_SETSIZE` (typically 64 on Windows).
     *   - If the descriptor exceeds `FD_SETSIZE`, a `SocketException` is thrown to prevent undefined behavior.
     *
     * ### Timeout Semantics
     * - `timeoutMillis < 0`: Blocks indefinitely until a connection attempt is ready.
     * - `timeoutMillis == 0`: Performs a non-blocking poll and returns immediately.
     * - `timeoutMillis > 0`: Waits up to the specified number of milliseconds.
     * - If `timeoutMillis` is not provided, the socket's logical timeout (`_soTimeoutMillis`) is used.
     *
     * @note This method does **not** rely on or modify kernel-level timeouts (e.g., `SO_RCVTIMEO`).
     *       It uses event polling (`poll()` or `select()`) for logical timeout behavior.
     *
     * ### Thread Safety
     * This method is thread-safe **as long as the server socket is not concurrently closed or reconfigured.**
     *
     * @param[in] timeoutMillis Optional timeout in milliseconds. If omitted, the socket's `SO_TIMEOUT` is used.
     * @return `true` if the socket is ready to accept a connection, `false` if the timeout expired.
     *
     * @throws SocketException if:
     *         - The server socket is not initialized (`INVALID_SOCKET`)
     *         - A system-level polling error occurs
     *         - The socket descriptor exceeds platform limits (`FD_SETSIZE`) on Windows
     *
     * @see accept()      Accepts a new incoming connection
     * @see tryAccept()   Non-blocking variant of `accept()`
     * @see setSoTimeout() Sets the default timeout used by this method
     * @see getSoTimeout() Retrieves the current logical timeout value
     */
    [[nodiscard]] bool waitReady(std::optional<int> timeoutMillis = std::nullopt) const;

    /**
     * @brief Set the logical timeout (in milliseconds) for accepting client connections.
     *
     * This timeout applies to methods like `accept()` and `tryAccept()`, and determines how long
     * the server socket will wait for a client connection before timing out.
     *
     * Unlike `Socket::setSoTimeout()`, this method does **not** call `setsockopt()` and does **not**
     * affect the underlying socket descriptor. Instead, it is used internally to control the behavior
     * of `select()` during accept operations.
     *
     * @note Use:
     * - Negative value: wait indefinitely (blocking behavior)
     * - Zero: poll mode (non-blocking)
     * - Positive value: wait up to the specified milliseconds
     *
     * @param timeoutMillis Timeout value in milliseconds
     *
     * @see getSoTimeout()
     * @see accept()
     * @see tryAccept()
     * @see waitReady()
     *
     * @ingroup socketopts
     */
    void setSoTimeout(const int timeoutMillis) { _soTimeoutMillis = timeoutMillis; }

    /**
     * @brief Get the logical timeout (in milliseconds) for accept operations.
     *
     * This value determines how long methods like `accept()` and `tryAccept()` will wait for an incoming
     * client connection before timing out. It is used internally by `select()` or similar readiness mechanisms.
     *
     * @note This timeout is a logical userland timeout and does **not** affect the socket descriptor
     *       via `setsockopt()` (unlike `Socket::getSoTimeout()`).
     *
     * @return The configured accept timeout in milliseconds.
     *
     * @see setSoTimeout()
     * @see accept()
     * @see tryAccept()
     * @ingroup socketopts
     */
    [[nodiscard]] int getSoTimeout() const noexcept { return _soTimeoutMillis; }

#if defined(IPV6_V6ONLY)
    /**
     * @brief Enable or disable IPv6-only mode for this server socket.
     *
     * By default, an IPv6 socket is configured in dual-stack mode (accepts both IPv6 and IPv4 connections)
     * on most platforms. Enabling IPv6-only mode restricts the socket to **only** IPv6 connections.
     *
     * @param[in] enable True to enable IPv6-only mode, false to allow dual-stack.
     * @throws SocketException if the socket is not IPv6, already bound, or on system error.
     * @note Must be called before bind().
     * @see getIPv6Only()
     *
     * @ingroup tcp
     */
    void setIPv6Only(bool enable);

    /**
     * @brief Query whether IPv6-only mode is enabled.
     *
     * @return True if the socket is in IPv6-only mode, false if dual-stack.
     * @throws SocketException if the socket is not IPv6, not open, or on system error.
     * @see setIPv6Only()
     *
     * @ingroup tcp
     */
    [[nodiscard]] bool getIPv6Only() const;
#endif

    /**
     * @brief Set the default receive buffer size for accepted client sockets.
     *
     * This sets the initial buffer size used when accepting new client connections.
     * The buffer size determines how much data can be buffered for reading from
     * the socket before the underlying receive buffer overflows.
     *
     * @note Thread-safe if called before concurrent accept() calls.
     *
     * @param[in] size New buffer size in bytes
     * @see getDefaultReceiveBufferSize()
     * @see DefaultBufferSize
     * @see accept(), acceptBlocking(), tryAccept()
     *
     * @ingroup tcp
     */
    void setDefaultReceiveBufferSize(const std::size_t size) { _defaultReceiveBufferSize = size; }

    /**
     * @brief Get the current default receive buffer size for accepted client sockets.
     *
     * Returns the buffer size that will be used when accepting new client connections.
     * This is the value previously set by setDefaultReceiveBufferSize() or the default if not set.
     *
     * @return Current buffer size in bytes
     * @see setDefaultReceiveBufferSize()
     * @see DefaultBufferSize
     * @see accept()
     *
     * @ingroup tcp
     */
    [[nodiscard]] std::size_t getDefaultReceiveBufferSize() const noexcept { return _defaultReceiveBufferSize; }

    /**
     * @brief Set the default send buffer size for accepted client sockets.
     *
     * This sets the initial send buffer size used when accepting new client connections.
     * The buffer size determines how much data can be buffered for writing to
     * the socket before the underlying send buffer overflows.
     *
     * @note Thread-safe if called before concurrent accept() calls.
     *
     * @param[in] size New send buffer size in bytes
     * @see getDefaultSendBufferSize()
     * @see DefaultBufferSize
     * @see accept()
     *
     * @ingroup tcp
     */
    void setDefaultSendBufferSize(const std::size_t size) { _defaultSendBufferSize = size; }

    /**
     * @brief Get the current default send buffer size for accepted client sockets.
     *
     * Returns the send buffer size that will be used when accepting new client connections.
     * This is the value previously set by setDefaultSendBufferSize() or the default if not set.
     *
     * @return Current send buffer size in bytes
     * @see setDefaultSendBufferSize()
     * @see DefaultBufferSize
     * @see accept()
     *
     * @ingroup tcp
     */
    [[nodiscard]] std::size_t getDefaultSendBufferSize() const noexcept { return _defaultSendBufferSize; }

    /**
     * @brief Set the per-instance default internal buffer size used for buffered read operations.
     *
     * This method updates the `_defaultInternalBufferSize` value, which determines the fallback buffer size
     * used by methods like `accept()` or `readLine()` when no explicit `internalBufferSize` is provided.
     *
     * This does **not** affect the kernel-level `SO_RCVBUF`. It applies only to the internal buffering layer
     * used in higher-level stream-oriented reads (e.g., reading lines or strings).
     *
     * @param size New default buffer size in bytes for this socket/server instance.
     *
     * @see getDefaultInternalBufferSize()
     * @see getEffectiveInternalBufferSize()
     * @see DefaultBufferSize
     *
     * @ingroup tcp
     */
    void setDefaultInternalBufferSize(const std::size_t size) { _defaultInternalBufferSize = size; }

    /**
     * @brief Get the per-instance default internal buffer size used for buffered read operations.
     *
     * Returns the current value of `_defaultInternalBufferSize`, which is used as the fallback size
     * for internal buffering in stream-oriented socket reads when no explicit buffer size is provided.
     *
     * This value is typically initialized to `DefaultBufferSize` (4096 bytes), but can be modified
     * using `setDefaultInternalBufferSize()`.
     *
     * @return The current default internal buffer size in bytes.
     *
     * @see setDefaultInternalBufferSize()
     * @see getEffectiveInternalBufferSize()
     * @see DefaultBufferSize
     *
     * @ingroup tcp
     */
    [[nodiscard]] std::size_t getDefaultInternalBufferSize() const noexcept { return _defaultInternalBufferSize; }

#if defined(SO_REUSEPORT)
    /**
     * @brief Enable or disable the SO_REUSEPORT socket option.
     *
     * The SO_REUSEPORT option allows multiple sockets on the same host to bind to the same port number,
     * enabling load balancing of incoming connections across multiple processes or threads.
     * This can dramatically improve scalability for high-performance servers (such as web servers or proxies).
     *
     * @warning SO_REUSEPORT is **not supported on all platforms**. It is available on:
     *   - Linux kernel 3.9 and later
     *   - Many BSD systems (FreeBSD 10+, OpenBSD, macOS 10.9+)
     *   - **Not supported on Windows**
     *
     * If you compile or run on a platform where SO_REUSEPORT is unavailable, this method will **not be present**.
     * Use conditional compilation (`#ifdef SO_REUSEPORT`) if you require portability.
     *
     * Improper use of SO_REUSEPORT may result in complex behavior and should only be used if you fully understand
     * its implications (e.g., distributing incoming connections evenly across processes).
     *
     * @param[in] enable Set to `true` to enable SO_REUSEPORT, `false` to disable.
     * @throws SocketException if setting the option fails.
     *
     * @see https://man7.org/linux/man-pages/man7/socket.7.html
     *
     * @ingroup tcp
     */
    void setReusePort(bool enable);

    /**
     * @brief Query whether SO_REUSEPORT is enabled for this socket.
     *
     * This method returns the current status of the SO_REUSEPORT option on this socket.
     * Like `setReusePort()`, this method is only available on platforms that define `SO_REUSEPORT`.
     *
     * @return `true` if SO_REUSEPORT is enabled, `false` otherwise.
     * @throws SocketException if querying the option fails.
     *
     * @see setReusePort(bool)
     * @see https://man7.org/linux/man-pages/man7/socket.7.html
     *
     * @ingroup tcp
     */
    [[nodiscard]] bool getReusePort() const;
#endif

    /**
     * @brief Get the underlying native socket handle/descriptor.
     *
     * This method provides low-level access to the native socket handle (file descriptor on Unix-like systems,
     * SOCKET handle on Windows) for advanced usage scenarios such as:
     * - Integration with external event loops (select, poll, epoll)
     * - Platform-specific socket operations not exposed by the jsocketpp API
     * - Custom socket monitoring or diagnostics
     *
     * @warning HANDLE WITH CARE:
     * - DO NOT close or shutdown the socket using this handle directly
     * - DO NOT modify socket options or state without careful consideration
     * - DO NOT store the handle beyond the lifetime of the ServerSocket object
     * - DO NOT share the handle between threads without proper synchronization
     *
     * Improper use of the raw socket handle can lead to:
     * - Resource leaks
     * - Double-close scenarios
     * - Undefined behavior
     * - Thread safety violations
     * - Broken socket state
     *
     * @note The handle remains owned and managed by the ServerSocket object.
     *       It will be automatically closed when the ServerSocket is destroyed.
     *
     * @return The native socket handle/descriptor
     *
     * @see acceptBlocking(), setOption()
     *
     * @ingroup tcp
     */
    [[nodiscard]] SOCKET getHandle() const { return _serverSocket; }

  protected:
    /**
     * @brief Cleans up server socket resources and throws a SocketException.
     *
     * This method performs cleanup of the address information structures (_srvAddrInfo)

     * and throws a SocketException with the provided error code. It's typically called
     * when an error occurs during socket initialization or configuration.
     *
     * @param[in] errorCode The error code to include in the thrown exception
     * @throws SocketException Always throws with the provided error code and corresponding message
     *
     * @ingroup tcp
     */
    void cleanupAndThrow(int errorCode);

    /**
     * @brief Identifies this socket as a passive (listening) socket.
     * @ingroup socketopts
     *
     * Overrides the base `SocketOptions::isPassiveSocket()` to return `true`, indicating that
     * this `ServerSocket` instance is intended to accept incoming connections rather than
     * initiate outbound ones.
     *
     * This designation is important for platform-specific behaviors — particularly on Windows,
     * where passive sockets use `SO_EXCLUSIVEADDRUSE` instead of `SO_REUSEADDR` to control
     * address reuse semantics. The `SocketOptions` interface uses this signal to select the
     * correct option and logic in `setReuseAddress()` and `getReuseAddress()`.
     *
     * ### When `true` matters
     * - Enables use of `SO_EXCLUSIVEADDRUSE` on Windows
     * - Prevents unsafe or misleading reuse logic on listening sockets
     * - Helps unify reuse handling logic across all socket types in a central place
     *
     * @return Always returns `true` for `ServerSocket` to indicate passive socket behavior.
     *
     * @see SocketOptions::isPassiveSocket()
     * @see SocketOptions::setReuseAddress()
     * @see SocketOptions::getReuseAddress()
     */
    [[nodiscard]] bool isPassiveSocket() const noexcept override { return true; }

  private:
    /**
     * @brief Get the effective receive buffer size to use for socket read operations.
     *
     * This helper determines the actual buffer size based on an optional user-provided value.
     * If the parameter is `std::nullopt`, it returns the per-instance default receive buffer size;
     * otherwise, it returns the explicitly provided value (even if it's 0).
     *
     * @param[in] recvBufferSize Optional size. If unset, defaults to `_defaultReceiveBufferSize`.
     * @return The effective buffer size to use.
     *
     * @see setDefaultReceiveBufferSize()
     * @see getDefaultReceiveBufferSize()
     * @see DefaultBufferSize
     *
     * @ingroup tcp
     */
    [[nodiscard]] std::size_t getEffectiveReceiveBufferSize(const std::optional<std::size_t> recvBufferSize) const
    {
        return recvBufferSize.value_or(_defaultReceiveBufferSize);
    }

    /**
     * @brief Get the effective send buffer size to use for socket write operations.
     *
     * This helper determines the actual send buffer size based on an optional user-specified value.
     * If the parameter is `std::nullopt`, it returns the per-instance default send buffer size.
     * Otherwise, it returns the explicitly specified size (including zero if the caller intends it).
     *
     * @param[in] sendBufferSize Optional size. If unset, defaults to `_defaultSendBufferSize`.
     * @return The effective buffer size to use for send operations.
     *
     * @see setSendBufferSize()
     * @see getSendBufferSize()
     * @see DefaultBufferSize
     *
     * @ingroup tcp
     */
    [[nodiscard]] std::size_t getEffectiveSendBufferSize(std::optional<std::size_t> sendBufferSize) const
    {
        return sendBufferSize.value_or(_defaultSendBufferSize);
    }

    /**
     * @brief Get the effective internal buffer size to use for buffered socket read operations.
     *
     * This helper determines the internal buffer size for read-related operations based on the provided
     * optional value. If `internalBufferSize` is set, its value is returned directly. If not, the method
     * returns the per-instance `_defaultInternalBufferSize`, which defaults to `DefaultBufferSize` (4096 bytes)
     * unless explicitly overridden via `setInternalBufferSize()`.
     *
     * This internal buffer is used for stream-based operations (e.g. reading strings or protocol lines) and is
     * distinct from the kernel-level socket buffers (`SO_RCVBUF`, `SO_SNDBUF`).
     *
     * @param[in] internalBufferSize Optional buffer size override for a single operation.
     * @return The effective internal buffer size to use.
     *
     * @see DefaultBufferSize
     * @see setInternalBufferSize()
     * @see Socket::readLine()
     * @see Socket::getInternalBufferSize()
     *
     * @ingroup tcp
     */
    [[nodiscard]] std::size_t getEffectiveInternalBufferSize(std::optional<std::size_t> internalBufferSize) const
    {
        return internalBufferSize.value_or(_defaultInternalBufferSize);
    }

    /**
     * @brief Resolves effective receive and send buffer sizes from optional user inputs.
     *
     * If either buffer size is not provided, this method falls back to the
     * per-instance `_defaultReceiveBufferSize` or `_defaultSendBufferSize`.
     *
     * @param recv Optional receive buffer size.
     * @param send Optional send buffer size.
     * @param internal Optional internal buffer size.
     * @return A tuple: {resolvedRecvBufferSize, resolvedSendBufferSize, resolvedInternalBufferSize}
     *
     * @see getEffectiveReceiveBufferSize()
     * @see getEffectiveSendBufferSize()
     *
     * @ingroup tcp
     */
    [[nodiscard]] std::tuple<std::size_t, std::size_t, std::size_t>
    resolveBuffers(const std::optional<std::size_t> recv, const std::optional<std::size_t> send,
                   const std::optional<std::size_t> internal) const
    {
        return {getEffectiveReceiveBufferSize(recv), getEffectiveSendBufferSize(send),
                getEffectiveInternalBufferSize(internal)};
    }

    SOCKET _serverSocket = INVALID_SOCKET;        ///< Underlying socket file descriptor.
    internal::AddrinfoPtr _srvAddrInfo = nullptr; ///< Address info for binding (from getaddrinfo)
    addrinfo* _selectedAddrInfo = nullptr;        ///< Selected address info for binding
    Port _port;                                   ///< Port number the server will listen on
    bool _isBound = false;                        ///< True if the server socket is bound
    bool _isListening = false;                    ///< True if the server socket is listening
    int _soTimeoutMillis = -1; ///< Timeout for accept(); -1 = no timeout, 0 = poll, >0 = timeout in milliseconds
    std::size_t _defaultReceiveBufferSize =
        DefaultBufferSize; ///< Default buffer size used for accepted client sockets when no specific size is provided
    std::size_t _defaultSendBufferSize = DefaultBufferSize; ///< Default send buffer size for accepted client sockets
    std::size_t _defaultInternalBufferSize =
        DefaultBufferSize; ///< Default internal buffer size for accepted client sockets, used by some read() methods
};

} // namespace jsocketpp
