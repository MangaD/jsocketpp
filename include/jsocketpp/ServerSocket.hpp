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
     * @brief Default constructor (deleted) for ServerSocket class.
     * @ingroup tcp
     *
     * The default constructor is explicitly deleted to prevent the creation of
     * uninitialized `ServerSocket` objects. Each server socket must be explicitly
     * constructed with a valid port and configuration parameters to ensure correct
     * binding and listening behavior.
     *
     * ### Rationale
     * - Prevents creation of non-functional server sockets
     * - Enforces proper initialization with port, backlog, and reuse options
     * - Ensures RAII ownership over system-level resources
     *
     * @code{.cpp}
     * ServerSocket s;          // ❌ Compilation error (deleted constructor)
     * ServerSocket s(8080);    // ✅ Valid usage: bind to port 8080
     * @endcode
     *
     * @see ServerSocket(Port, std::string_view, bool, bool, int, bool) Primary constructor
     */
    ServerSocket() = delete;

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
        : SocketOptions(rhs.getSocketFd()), _srvAddrInfo(std::move(rhs._srvAddrInfo)),
          _selectedAddrInfo(rhs._selectedAddrInfo), _port(rhs._port), _isBound(rhs._isBound),
          _isListening(rhs._isListening), _soTimeoutMillis(rhs._soTimeoutMillis),
          _defaultReceiveBufferSize(rhs._defaultReceiveBufferSize)
    {
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
            setSocketFd(rhs.getSocketFd());
            _srvAddrInfo = std::move(rhs._srvAddrInfo);
            _selectedAddrInfo = rhs._selectedAddrInfo;
            _port = rhs._port;
            _isBound = rhs._isBound;
            _isListening = rhs._isListening;
            _soTimeoutMillis = rhs._soTimeoutMillis;
            _defaultReceiveBufferSize = rhs._defaultReceiveBufferSize;

            // Reset source;
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
     * @brief Accept an incoming client connection, respecting the configured socket timeout and applying tuning
     * options.
     * @ingroup tcp
     *
     * This method waits for an incoming connection using the timeout set via `setSoTimeout()` or `getSoTimeout()`.
     * Once a connection is ready, it calls the underlying system `accept()` and returns a fully configured `Socket`
     * object with the specified buffer sizes, timeouts, and TCP socket options.
     *
     * ---
     *
     * ### ⏱ Timeout Behavior
     * - If the timeout is **negative** (default), the method blocks indefinitely.
     * - If the timeout is **zero**, it polls once and returns immediately if no client is waiting.
     * - If the timeout is **positive**, it waits for the given number of milliseconds before throwing
     *   a `SocketTimeoutException` if no connection occurs.
     *
     * Internally, this method uses `select()` via `waitReady()` to wait for readiness, then invokes `accept()`.
     * The socket’s blocking mode (via `setNonBlocking()`) does **not** affect this method.
     *
     * ---
     *
     * ### ⚙️ Configuration of Accepted Socket
     * The returned `Socket` is configured immediately using the provided tuning parameters:
     * - `recvBufferSize`, `sendBufferSize`: OS-level socket buffers (`SO_RCVBUF`, `SO_SNDBUF`)
     * - `internalBufferSize`: Internal buffer used by `read<T>()` and `read<std::string>()`
     * - `soRecvTimeoutMillis`: Read timeout in milliseconds (`SO_RCVTIMEO`); `-1` disables
     * - `soSendTimeoutMillis`: Send timeout in milliseconds (`SO_SNDTIMEO`); `-1` disables
     * - `tcpNoDelay`: Disables Nagle’s algorithm for low-latency communication (default: `true`)
     * - `keepAlive`: Enables TCP keep-alive probes
     * - `nonBlocking`: If `true`, the accepted socket is set to non-blocking mode
     *
     * ---
     *
     * ### ⚠️ Thread Safety
     * This method is **not thread-safe**. Concurrent calls to `accept()` or `acceptNonBlocking()` from multiple threads
     * or processes may cause race conditions or undefined behavior. Protect the `ServerSocket` instance with a mutex if
     * needed.
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * server.setSoTimeout(5000); // 5-second accept timeout
     *
     * try {
     *     Socket client = server.accept(
     *         8192, 8192, 8192,
     *         3000, 1000,
     *         true, true, false);
     *     handleClient(client);
     * } catch (const SocketTimeoutException&) {
     *     // No client connected within 5 seconds
     * }
     * @endcode
     *
     * ---
     *
     * @param[in] recvBufferSize Optional socket receive buffer size (`SO_RCVBUF`)
     * @param[in] sendBufferSize Optional socket send buffer size (`SO_SNDBUF`)
     * @param[in] internalBufferSize Optional internal buffer size for high-level reads
     * @param[in] soRecvTimeoutMillis Read timeout (`SO_RCVTIMEO`) in milliseconds; `-1` disables
     * @param[in] soSendTimeoutMillis Send timeout (`SO_SNDTIMEO`) in milliseconds; `-1` disables
     * @param[in] tcpNoDelay Whether to disable Nagle's algorithm (`TCP_NODELAY`); default: `true`
     * @param[in] keepAlive Whether to enable TCP keep-alive probes (`SO_KEEPALIVE`)
     * @param[in] nonBlocking Whether to set the accepted socket to non-blocking mode
     *
     * @return A fully configured `Socket` representing the connected client
     *
     * @throws SocketException If:
     * - The server socket is invalid, closed, or in an incorrect state
     * - `accept()` fails due to a system error
     * - Socket configuration fails after acceptance
     *
     * @throws SocketTimeoutException If the timeout expires with no connection
     *
     * @pre The server socket must be valid, bound, and listening
     * @post A connected and configured `Socket` is returned on success
     *
     * @see setSoTimeout(), getSoTimeout(), waitReady(), acceptBlocking(), acceptNonBlocking()
     */
    [[nodiscard]] Socket accept(std::optional<std::size_t> recvBufferSize = std::nullopt,
                                std::optional<std::size_t> sendBufferSize = std::nullopt,
                                std::optional<std::size_t> internalBufferSize = std::nullopt,
                                int soRecvTimeoutMillis = -1, int soSendTimeoutMillis = -1, bool tcpNoDelay = true,
                                bool keepAlive = false, bool nonBlocking = false) const;

    /**
     * @brief Accept an incoming client connection, waiting up to the specified timeout and applying socket tuning
     * options.
     * @ingroup tcp
     *
     * This method waits for a client connection using the specified timeout (in milliseconds), then calls the
     * underlying system `accept()` and returns a fully configured `Socket` object.
     *
     * ---
     *
     * ### ⏱ Timeout Behavior
     * - If `timeoutMillis` is **negative**, the method blocks indefinitely.
     * - If `timeoutMillis` is **zero**, the method polls once and returns immediately if no client is waiting.
     * - If `timeoutMillis` is **positive**, the method waits up to that many milliseconds before throwing a
     *   `SocketTimeoutException` if no connection is made.
     *
     * Internally, this method uses `waitReady(timeoutMillis)` (based on `select()`) to wait for readiness and
     * then calls `acceptBlocking()` with the resolved parameters.
     *
     * ---
     *
     * ### ⚙️ Configuration of Accepted Socket
     * The returned `Socket` will be configured with the following tuning options:
     * - `recvBufferSize`, `sendBufferSize`: OS-level buffer sizes (`SO_RCVBUF`, `SO_SNDBUF`)
     * - `internalBufferSize`: Internal buffer for high-level `read()` operations
     * - `soRecvTimeoutMillis`: Socket-level receive timeout (`SO_RCVTIMEO`) in milliseconds
     * - `soSendTimeoutMillis`: Socket-level send timeout (`SO_SNDTIMEO`) in milliseconds
     * - `tcpNoDelay`: Disables Nagle’s algorithm for latency-sensitive connections (default: `true`)
     * - `keepAlive`: Enables TCP keep-alive (`SO_KEEPALIVE`)
     * - `nonBlocking`: Immediately sets the accepted socket to non-blocking mode
     *
     * ---
     *
     * ### ⚠️ Thread Safety
     * This method is **not thread-safe**. Concurrent calls to `accept()`, `acceptBlocking()`, or `acceptNonBlocking()`
     * from multiple threads or processes may lead to race conditions. Synchronize access to the `ServerSocket` if
     * needed.
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * ServerSocket server(8080);
     * server.bind();
     * server.listen();
     *
     * try {
     *     Socket client = server.accept(2000,              // Wait up to 2 seconds
     *                                    8192, 8192, 8192,  // Buffer sizes
     *                                    3000, 1000,        // Timeouts
     *                                    true, true, false  // TCP_NODELAY, keepAlive, nonBlocking
     *     );
     *     handleClient(client);
     * } catch (const SocketTimeoutException&) {
     *     std::cout << "No client connected within timeout." << std::endl;
     * }
     * @endcode
     *
     * ---
     *
     * @param[in] timeoutMillis Timeout in milliseconds to wait for a connection:
     * - Negative: block indefinitely
     * - Zero: poll once and return immediately
     * - Positive: wait up to this many milliseconds
     *
     * @param[in] recvBufferSize Optional socket receive buffer size (`SO_RCVBUF`)
     * @param[in] sendBufferSize Optional socket send buffer size (`SO_SNDBUF`)
     * @param[in] internalBufferSize Internal buffer used by `read()` and `read<T>()`
     * @param[in] soRecvTimeoutMillis Receive timeout in milliseconds (`SO_RCVTIMEO`); `-1` disables
     * @param[in] soSendTimeoutMillis Send timeout in milliseconds (`SO_SNDTIMEO`); `-1` disables
     * @param[in] tcpNoDelay Whether to disable Nagle’s algorithm (`TCP_NODELAY`); default: `true`
     * @param[in] keepAlive Whether to enable TCP keep-alive (`SO_KEEPALIVE`)
     * @param[in] nonBlocking Whether to set the accepted socket to non-blocking mode
     *
     * @return A configured `Socket` representing the connected client
     *
     * @throws SocketException If:
     * - The server socket is invalid or closed
     * - `waitReady()` or `accept()` fails
     * - Socket configuration fails after acceptance
     *
     * @throws SocketTimeoutException If no client connects before the timeout expires
     *
     * @pre Server socket must be valid, bound, and listening
     * @post Returns a connected `Socket` or throws an exception on failure
     *
     * @see accept(), tryAccept(), acceptBlocking(), setSoTimeout(), waitReady()
     */
    [[nodiscard]] Socket accept(int timeoutMillis, std::optional<std::size_t> recvBufferSize = std::nullopt,
                                std::optional<std::size_t> sendBufferSize = std::nullopt,
                                std::optional<std::size_t> internalBufferSize = std::nullopt,
                                int soRecvTimeoutMillis = -1, int soSendTimeoutMillis = -1, bool tcpNoDelay = true,
                                bool keepAlive = false, bool nonBlocking = false) const;

    /**
     * @brief Attempt to accept an incoming client connection, returning `std::nullopt` on timeout instead of throwing.
     * @ingroup tcp
     *
     * This method waits for an incoming client connection using the timeout configured via `setSoTimeout()`.
     * If a client is available before the timeout expires, it returns a fully configured `Socket` object.
     * If no client connects in time, it returns `std::nullopt` rather than throwing.
     *
     * ---
     *
     * ### ⏱ Timeout Behavior
     * - If the socket timeout is **negative** (default), the method blocks indefinitely.
     * - If the timeout is **zero**, it polls and returns immediately if no client is waiting.
     * - If the timeout is **positive**, the method waits up to that many milliseconds using `select()`.
     *   - If no connection arrives within that time, it returns `std::nullopt` (unlike `accept()` which throws).
     *
     * Internally, `waitReady()` is used for readiness detection, followed by `acceptBlocking()` for the actual accept.
     *
     * ---
     *
     * ### ⚙️ Configuration of Accepted Socket
     * The returned `Socket` (if any) is configured using the following tuning options:
     * - `recvBufferSize`, `sendBufferSize`: OS-level buffer sizes (`SO_RCVBUF`, `SO_SNDBUF`)
     * - `internalBufferSize`: Internal buffer used by `read<T>()` and `read<std::string>()`
     * - `soRecvTimeoutMillis`: Receive timeout for the socket (`SO_RCVTIMEO`) in milliseconds
     * - `soSendTimeoutMillis`: Send timeout for the socket (`SO_SNDTIMEO`) in milliseconds
     * - `tcpNoDelay`: Disables Nagle’s algorithm for lower latency (default: `true`)
     * - `keepAlive`: Enables TCP keep-alive probes
     * - `nonBlocking`: Immediately sets the accepted socket into non-blocking mode
     *
     * ---
     *
     * ### ⚠️ Thread Safety
     * This method is **not thread-safe**. Concurrent calls from multiple threads or processes may cause
     * race conditions or spurious failures. Use a mutex to guard access to `accept()` methods if needed.
     *
     * ---
     *
     * ### 💡 Notes
     * - This method is ideal for polling servers and event-driven architectures.
     * - For per-call timeouts instead of using `setSoTimeout()`, use `tryAccept(int timeoutMillis, ...)`.
     * - Unlike `accept()`, this method never throws `SocketTimeoutException`.
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * server.setSoTimeout(100); // Poll every 100 ms
     *
     * while (true) {
     *     if (auto client = server.tryAccept(
     *             8192, 8192, 8192,
     *             3000, 1000,
     *             true, true, false)) {
     *         handleClient(*client);
     *     } else {
     *         // No client ready; do other work
     *     }
     * }
     * @endcode
     *
     * ---
     *
     * @param[in] recvBufferSize Optional socket receive buffer size (`SO_RCVBUF`)
     * @param[in] sendBufferSize Optional socket send buffer size (`SO_SNDBUF`)
     * @param[in] internalBufferSize Optional internal buffer for high-level reads
     * @param[in] soRecvTimeoutMillis Receive timeout (`SO_RCVTIMEO`) in milliseconds; `-1` disables
     * @param[in] soSendTimeoutMillis Send timeout (`SO_SNDTIMEO`) in milliseconds; `-1` disables
     * @param[in] tcpNoDelay Whether to disable Nagle’s algorithm (`TCP_NODELAY`); default: `true`
     * @param[in] keepAlive Whether to enable TCP keep-alive (`SO_KEEPALIVE`)
     * @param[in] nonBlocking Whether to set the accepted socket to non-blocking mode
     *
     * @return `std::optional<Socket>` — a connected and configured `Socket` on success, or `std::nullopt` on timeout
     *
     * @throws SocketException if the server socket is invalid or `accept()` fails for other reasons
     *
     * @pre Server socket must be valid, bound, and listening
     * @post Returns `std::nullopt` if no client connects in time, or a valid `Socket` if one does
     *
     * @see setSoTimeout(), getSoTimeout(), tryAccept(int), accept(), acceptBlocking(), waitReady()
     */
    [[nodiscard]] std::optional<Socket> tryAccept(std::optional<std::size_t> recvBufferSize = std::nullopt,
                                                  std::optional<std::size_t> sendBufferSize = std::nullopt,
                                                  std::optional<std::size_t> internalBufferSize = std::nullopt,
                                                  int soRecvTimeoutMillis = -1, int soSendTimeoutMillis = -1,
                                                  bool tcpNoDelay = true, bool keepAlive = false,
                                                  bool nonBlocking = false) const;

    /**
     * @brief Attempt to accept an incoming client connection, waiting up to a specified timeout and returning
     * `std::nullopt` on timeout.
     * @ingroup tcp
     *
     * This method waits for an incoming client connection using the explicitly provided `timeoutMillis` value,
     * overriding any timeout configured via `setSoTimeout()`. If a client connects before the timeout expires,
     * a fully configured `Socket` is returned. Otherwise, `std::nullopt` is returned instead of throwing.
     *
     * ---
     *
     * ### ⏱ Timeout Behavior
     * - **Negative** `timeoutMillis`: Block indefinitely until a client connects
     * - **Zero**: Poll immediately; return `std::nullopt` if no client is waiting
     * - **Positive**: Wait up to `timeoutMillis` milliseconds; return `std::nullopt` if no client connects in time
     *
     * Readiness is detected using `select()` via `waitReady(timeoutMillis)`, followed by a call to
     * `acceptBlocking(...)` if the socket is ready.
     *
     * ---
     *
     * ### ⚙️ Configuration of Accepted Socket
     * If a client is accepted, the resulting `Socket` is configured using the following options:
     * - `recvBufferSize`, `sendBufferSize`: OS-level socket buffer sizes (`SO_RCVBUF`, `SO_SNDBUF`)
     * - `internalBufferSize`: High-level internal buffer used by `read<T>()` and `read<std::string>()`
     * - `soRecvTimeoutMillis`: Socket receive timeout (`SO_RCVTIMEO`) in milliseconds
     * - `soSendTimeoutMillis`: Socket send timeout (`SO_SNDTIMEO`) in milliseconds
     * - `tcpNoDelay`: Disables Nagle’s algorithm (`TCP_NODELAY`) for low-latency operation (default: `true`)
     * - `keepAlive`: Enables TCP keep-alive probes (`SO_KEEPALIVE`)
     * - `nonBlocking`: If `true`, sets the accepted socket to non-blocking mode
     *
     * ---
     *
     * ### ⚠️ Race Condition Warning
     * Even after readiness is reported, `accept()` may still fail with `EWOULDBLOCK` or `EAGAIN` if the client
     * disconnects in the small window before `accept()` is invoked. In that case, a `SocketException` is thrown.
     *
     * ---
     *
     * ### ⚠️ Thread Safety
     * This method is **not thread-safe**. If used from multiple threads, you must externally synchronize access
     * to the server socket to prevent race conditions.
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * ServerSocket server(8080);
     * server.bind();
     * server.listen();
     *
     * while (true) {
     *     auto client = server.tryAccept(100,            // Wait 100 ms per poll
     *                                     8192, 8192, 8192, // Buffer sizes
     *                                     3000, 1000,     // Timeouts
     *                                     true, true, false); // Tuning flags
     *
     *     if (client) {
     *         handleClient(*client);
     *     } else {
     *         // No client yet — continue polling
     *     }
     * }
     * @endcode
     *
     * ---
     *
     * @param[in] timeoutMillis Timeout in milliseconds for this call:
     * - Negative: block indefinitely
     * - Zero: poll once
     * - Positive: wait up to `timeoutMillis` for a connection
     *
     * @param[in] recvBufferSize Optional receive buffer size (`SO_RCVBUF`)
     * @param[in] sendBufferSize Optional send buffer size (`SO_SNDBUF`)
     * @param[in] internalBufferSize Optional internal buffer for `read<T>()`-style calls
     * @param[in] soRecvTimeoutMillis Receive timeout for the accepted socket (`SO_RCVTIMEO`); `-1` disables
     * @param[in] soSendTimeoutMillis Send timeout for the accepted socket (`SO_SNDTIMEO`); `-1` disables
     * @param[in] tcpNoDelay Whether to disable Nagle’s algorithm (`TCP_NODELAY`); default: `true`
     * @param[in] keepAlive Whether to enable TCP keep-alive (`SO_KEEPALIVE`)
     * @param[in] nonBlocking Whether to set the accepted socket to non-blocking mode
     *
     * @return `std::optional<Socket>` — configured client socket on success, or `std::nullopt` on timeout
     *
     * @throws SocketException If the server socket is invalid or `accept()` fails due to system error
     *
     * @pre The server socket must be open, bound, and listening
     * @post Returns a valid `Socket` or `std::nullopt` if the timeout expires
     *
     * @see tryAccept(), accept(), acceptBlocking(), acceptNonBlocking(), waitReady(), setSoTimeout()
     */
    [[nodiscard]] std::optional<Socket> tryAccept(int timeoutMillis,
                                                  std::optional<std::size_t> recvBufferSize = std::nullopt,
                                                  std::optional<std::size_t> sendBufferSize = std::nullopt,
                                                  std::optional<std::size_t> internalBufferSize = std::nullopt,
                                                  int soRecvTimeoutMillis = -1, int soSendTimeoutMillis = -1,
                                                  bool tcpNoDelay = true, bool keepAlive = false,
                                                  bool nonBlocking = false) const;

    /**
     * @brief Accepts a TCP client connection, configures the socket, and returns a high-level `Socket` object.
     * @ingroup tcp
     *
     * This method calls the underlying system `accept()` on the listening server socket.
     * - If the socket is in **blocking mode** (default), this method blocks until a client connects.
     * - If the socket is in **non-blocking mode**, it returns immediately:
     *   - If a client is pending, it returns a configured `Socket`.
     *   - If no client is waiting, it throws `SocketException` with `EWOULDBLOCK` or `EAGAIN`.
     *
     * ---
     *
     * ### ⚙️ Behavior
     * - Applies OS-level buffer sizes (`SO_RCVBUF`, `SO_SNDBUF`) to the accepted socket
     * - Applies high-level `internalBufferSize` for `read<T>()`
     * - Configures optional timeouts for send and receive operations
     * - Enables/disables TCP features like Nagle’s algorithm and keep-alive
     * - Sets blocking mode immediately after accept if requested
     *
     * ---
     *
     * ### 🧪 Comparison with `acceptNonBlocking()`
     * - Both methods rely on the socket’s current blocking mode
     * - `acceptBlocking()` always throws on failure, including `EWOULDBLOCK`
     * - `acceptNonBlocking()` is used in polling loops where no connection is not an error
     *
     * ---
     *
     * ### ⚠️ Thread Safety
     * This method is **not thread-safe**. Simultaneous `accept()` calls on the same `ServerSocket`
     * from multiple threads may result in race conditions or inconsistent results.
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * ServerSocket server(8080);
     * server.bind();
     * server.listen();
     *
     * // Blocking pattern
     * Socket client = server.acceptBlocking(8192, 8192, 8192,
     *                                       3000, 1000, // timeouts
     *                                       true, true, false);
     *
     * // Non-blocking pattern
     * server.setNonBlocking(true);
     * try {
     *     Socket client = server.acceptBlocking(); // may throw EWOULDBLOCK
     *     // handle client
     * } catch (const SocketException& ex) {
     *     if (ex.code() == EWOULDBLOCK) {
     *         // no connection available yet
     *     } else {
     *         throw;
     *     }
     * }
     * @endcode
     *
     * ---
     *
     * @param[in] recvBufferSize Optional socket receive buffer size (`SO_RCVBUF`)
     * @param[in] sendBufferSize Optional socket send buffer size (`SO_SNDBUF`)
     * @param[in] internalBufferSize Internal buffer used by `read<T>()` and `read<std::string>()`
     * @param[in] soRecvTimeoutMillis Receive timeout (`SO_RCVTIMEO`) in milliseconds; `-1` disables
     * @param[in] soSendTimeoutMillis Send timeout (`SO_SNDTIMEO`) in milliseconds; `-1` disables
     * @param[in] tcpNoDelay Whether to disable Nagle’s algorithm (`TCP_NODELAY`)
     * @param[in] keepAlive Whether to enable TCP keep-alive (`SO_KEEPALIVE`)
     * @param[in] nonBlocking Whether to immediately make the accepted socket non-blocking
     *
     * @return A configured `Socket` object representing the connected client
     *
     * @throws SocketException If:
     * - The server socket is not bound or listening
     * - `accept()` fails
     * - No client is waiting in non-blocking mode (`EWOULDBLOCK`, `EAGAIN`)
     * - Socket configuration (timeouts, buffers, etc.) fails after `accept()`
     *
     * @pre Server socket must be open, bound, and in listening state.
     * @post A new `Socket` is returned on success, or an exception is thrown on failure.
     *
     * @see acceptNonBlocking(), tryAccept(), setNonBlocking(), setSoRecvTimeout(), setSoSendTimeout()
     */
    [[nodiscard]] Socket acceptBlocking(std::optional<std::size_t> recvBufferSize = std::nullopt,
                                        std::optional<std::size_t> sendBufferSize = std::nullopt,
                                        std::optional<std::size_t> internalBufferSize = std::nullopt,
                                        int soRecvTimeoutMillis = -1, int soSendTimeoutMillis = -1,
                                        bool tcpNoDelay = true, bool keepAlive = false, bool nonBlocking = false) const;

    /**
     * @brief Attempts to accept a client connection in non-blocking mode and returns a fully configured `Socket`.
     * @ingroup tcp
     *
     * This method calls the system `accept()` in non-blocking mode to check for pending client connections:
     *
     * - If a client is ready, it wraps the accepted connection into a configured `Socket` object.
     * - If no client is waiting, it returns `std::nullopt` without throwing.
     *
     * ---
     *
     * ### 🔁 Blocking Behavior
     * - If the server socket is in **blocking mode** (default), this call **blocks** until a client connects.
     * - If the socket is in **non-blocking mode** (`setNonBlocking(true)`), it returns immediately:
     *   - If a client is ready, returns a configured `Socket`.
     *   - If no client is pending, returns `std::nullopt`.
     *
     * ---
     *
     * ### ⚙️ Configuration
     * If a client is accepted, the returned `Socket` will be configured using the following options:
     *
     * - `recvBufferSize` / `sendBufferSize`: OS-level socket buffers (`SO_RCVBUF`, `SO_SNDBUF`)
     * - `internalBufferSize`: Internal buffer used by `read()` and `read<T>()`
     * - `soRecvTimeoutMillis`: Read timeout in milliseconds (`SO_RCVTIMEO`)
     * - `soSendTimeoutMillis`: Send timeout in milliseconds (`SO_SNDTIMEO`)
     * - `tcpNoDelay`: Disables Nagle’s algorithm for lower latency (`TCP_NODELAY`)
     * - `keepAlive`: Enables TCP keep-alive probes (`SO_KEEPALIVE`)
     * - `nonBlocking`: If `true`, the accepted socket is made non-blocking immediately
     *
     * ---
     *
     * ### ⚠️ Notes
     * - This method does **not** use `select()`, `poll()`, or `epoll()` internally.
     * - It is ideal for event-loop and polling-based architectures where you explicitly check for readiness.
     * - Socket behavior is determined entirely by whether `setNonBlocking(true)` was called on the server socket.
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * server.setNonBlocking(true);
     *
     * while (true) {
     *     auto client = server.acceptNonBlocking(
     *         8192, 8192, 8192,
     *         5000, 1000,
     *         true, true, true);
     *
     *     if (client) {
     *         handleClient(*client);
     *     } else {
     *         // No connection yet; do other work
     *     }
     * }
     * @endcode
     *
     * ---
     *
     * @param[in] recvBufferSize Optional socket receive buffer size (`SO_RCVBUF`)
     * @param[in] sendBufferSize Optional socket send buffer size (`SO_SNDBUF`)
     * @param[in] internalBufferSize Internal buffer size for `read<T>()`
     * @param[in] soRecvTimeoutMillis Read timeout in milliseconds (`SO_RCVTIMEO`); `-1` disables
     * @param[in] soSendTimeoutMillis Send timeout in milliseconds (`SO_SNDTIMEO`); `-1` disables
     * @param[in] tcpNoDelay Disables Nagle's algorithm (`TCP_NODELAY`) if `true` (default: true)
     * @param[in] keepAlive Enables TCP keep-alive probes (`SO_KEEPALIVE`) if `true`
     * @param[in] nonBlocking Sets the accepted socket to non-blocking mode if `true`
     *
     * @return A configured `Socket` if a client is pending, or `std::nullopt` if no connection is ready
     *
     * @throws SocketException if:
     * - The server socket is invalid or closed
     * - `accept()` fails due to a system error (excluding `EWOULDBLOCK` or `EAGAIN`)
     *
     * @pre The server socket must be bound and listening
     * @post Returns a connected `Socket` or `std::nullopt` if no client is available
     *
     * @see accept(), acceptBlocking(), tryAccept(), setNonBlocking(), setSoRecvTimeout(), setTcpNoDelay()
     */
    [[nodiscard]] std::optional<Socket> acceptNonBlocking(std::optional<std::size_t> recvBufferSize = std::nullopt,
                                                          std::optional<std::size_t> sendBufferSize = std::nullopt,
                                                          std::optional<std::size_t> internalBufferSize = std::nullopt,
                                                          int soRecvTimeoutMillis = -1, int soSendTimeoutMillis = -1,
                                                          bool tcpNoDelay = true, bool keepAlive = false,
                                                          bool nonBlocking = false) const;

    /**
     * @brief Asynchronously accept an incoming client connection, returning a `std::future` that resolves to a
     * configured `Socket`.
     * @ingroup tcp
     *
     * This method initiates an asynchronous accept operation on the server socket using
     * `std::async(std::launch::async, ...)`. It returns a `std::future<Socket>` that will
     * become ready once a client is accepted or an error/timeout occurs.
     *
     * ---
     *
     * ### ⚙️ Behavior
     * - The calling thread is **never blocked**
     * - A background thread calls `accept(...)` with the specified tuning options
     * - When `.get()` is called on the future:
     *   - If successful, a fully configured `Socket` is returned
     *   - If an error occurred, the exception is rethrown (`SocketException`, `SocketTimeoutException`, etc.)
     *
     * This function is ideal for use in thread-based architectures where the server must
     * continue operating while waiting for new clients in parallel.
     *
     * ---
     *
     * ### ⚠️ Thread Safety
     * This method is **not thread-safe**.
     * You must externally synchronize calls to `acceptAsync()`, `accept()`, `tryAccept()`, and related methods
     * on the same `ServerSocket` instance.
     *
     * ---
     *
     * ### ⚠️ Lifetime Warning
     * The `ServerSocket` object must remain valid and alive **at least until** the returned `std::future` has resolved
     * and `.get()` has been called. Using this method in contexts where the `ServerSocket` may be destroyed early
     * can lead to undefined behavior or dangling references.
     *
     * ---
     *
     * ### ⚙️ Configuration of Accepted Socket
     * The returned `Socket` (from `.get()`) will be initialized with:
     * - `recvBufferSize`, `sendBufferSize`: OS-level buffer sizes (`SO_RCVBUF`, `SO_SNDBUF`)
     * - `internalBufferSize`: High-level internal buffer used for `read()` / `read<T>()`
     * - `soRecvTimeoutMillis`: Socket read timeout in milliseconds (`SO_RCVTIMEO`)
     * - `soSendTimeoutMillis`: Socket send timeout in milliseconds (`SO_SNDTIMEO`)
     * - `tcpNoDelay`: Disables Nagle’s algorithm for lower latency (default: `true`)
     * - `keepAlive`: Enables TCP keep-alive (`SO_KEEPALIVE`)
     * - `nonBlocking`: If `true`, sets the accepted socket to non-blocking mode
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * ServerSocket server(8080);
     * server.bind();
     * server.listen();
     *
     * std::future<Socket> future = server.acceptAsync(
     *     8192, 8192, 8192,     // buffer sizes
     *     3000, 1000,           // socket timeouts
     *     true, true, false     // TCP_NODELAY, keep-alive, non-blocking
     * );
     *
     * // Do other work...
     * if (future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
     *     Socket client = future.get(); // May throw
     *     handleClient(client);
     * } else {
     *     std::cout << "Still waiting or timed out." << std::endl;
     * }
     * @endcode
     *
     * ---
     *
     * @param[in] recvBufferSize Optional socket receive buffer size (`SO_RCVBUF`)
     * @param[in] sendBufferSize Optional socket send buffer size (`SO_SNDBUF`)
     * @param[in] internalBufferSize Optional internal buffer size for `read<T>()` operations
     * @param[in] soRecvTimeoutMillis Socket receive timeout (`SO_RCVTIMEO`) in milliseconds; `-1` disables
     * @param[in] soSendTimeoutMillis Socket send timeout (`SO_SNDTIMEO`) in milliseconds; `-1` disables
     * @param[in] tcpNoDelay Whether to disable Nagle’s algorithm (`TCP_NODELAY`); default: `true`
     * @param[in] keepAlive Whether to enable TCP keep-alive (`SO_KEEPALIVE`)
     * @param[in] nonBlocking Whether to immediately set the accepted socket to non-blocking mode
     *
     * @return A `std::future<Socket>` that resolves to a connected and configured client socket, or throws on error
     *
     * @throws SocketException If a fatal socket error occurs during background accept
     * @throws SocketTimeoutException If no client connects and the configured timeout expires
     *
     * @pre Server socket must be valid, bound, and listening
     * @post Future resolves to a connected `Socket`, or throws from `.get()`
     *
     * @see accept(), acceptBlocking(), tryAccept(), acceptNonBlocking(), acceptAsync(callback), std::future, std::async
     */
    [[nodiscard]] std::future<Socket> acceptAsync(std::optional<std::size_t> recvBufferSize = std::nullopt,
                                                  std::optional<std::size_t> sendBufferSize = std::nullopt,
                                                  std::optional<std::size_t> internalBufferSize = std::nullopt,
                                                  int soRecvTimeoutMillis = -1, int soSendTimeoutMillis = -1,
                                                  bool tcpNoDelay = true, bool keepAlive = false,
                                                  bool nonBlocking = false) const;

    /**
     * @brief Asynchronously accept a client connection and invoke a callback upon completion or error.
     * @ingroup tcp
     *
     * This method starts a detached background thread that performs a socket `accept()` and
     * invokes a user-provided callback upon completion. It is designed for event-driven or
     * callback-oriented architectures where blocking or polling is not desirable.
     *
     * ---
     *
     * ### ⚙️ Behavior
     * - Accepts one client connection in a background thread
     * - Applies the same tuning options available to synchronous `accept()` methods
     * - If a client connects, the callback receives a fully constructed `Socket` and `nullptr` exception
     * - If an error occurs, the callback receives `std::nullopt` and a `std::exception_ptr`
     * - You may rethrow the exception in the callback using `std::rethrow_exception()`
     *
     * ---
     *
     * ### 🔁 Callback Signature
     * ```cpp
     * void callback(std::optional<Socket>, std::exception_ptr);
     * ```
     * - On success: the `Socket` contains a valid connection, and the `exception_ptr` is `nullptr`
     * - On error: the `Socket` is empty, and the `exception_ptr` holds a `SocketException`, `SocketTimeoutException`,
     * etc.
     *
     * ---
     *
     * ### ⚠️ Thread Safety
     * This method is **not thread-safe**. Do not call `accept()`, `acceptAsync()`, or related methods
     * concurrently on the same `ServerSocket` unless access is externally synchronized.
     *
     * ---
     *
     * ### ⚠️ Lifetime Warning
     * The `ServerSocket` must outlive the background thread and callback execution.
     * Destroying the `ServerSocket` before the callback is invoked results in **undefined behavior**.
     *
     * ---
     *
     * ### ⚙️ Configuration of Accepted Socket
     * The `Socket` passed to the callback (if any) will be configured using:
     * - `recvBufferSize`, `sendBufferSize`: OS-level buffer sizes (`SO_RCVBUF`, `SO_SNDBUF`)
     * - `internalBufferSize`: Internal buffer used by `read<T>()` and `read<std::string>()`
     * - `soRecvTimeoutMillis`: Socket receive timeout (`SO_RCVTIMEO`) in milliseconds
     * - `soSendTimeoutMillis`: Socket send timeout (`SO_SNDTIMEO`) in milliseconds
     * - `tcpNoDelay`: Disables Nagle’s algorithm (`TCP_NODELAY`) (default: `true`)
     * - `keepAlive`: Enables TCP keep-alive (`SO_KEEPALIVE`)
     * - `nonBlocking`: Immediately sets the accepted socket to non-blocking mode
     *
     * ---
     *
     * ### 🧠 Example
     * @code
     * server.acceptAsync(
     *     [](std::optional<Socket> clientOpt, std::exception_ptr eptr) {
     *         if (eptr) {
     *             try { std::rethrow_exception(eptr); }
     *             catch (const SocketException& ex) {
     *                 std::cerr << "Accept failed: " << ex.what() << std::endl;
     *             }
     *         } else if (clientOpt) {
     *             std::cout << "Accepted client from: "
     *                       << clientOpt->getRemoteSocketAddress() << std::endl;
     *             // Handle client...
     *         }
     *     },
     *     8192, 4096, 8192, 3000, 1000, true, true, false
     * );
     * @endcode
     *
     * ---
     *
     * @param[in] callback Completion handler that receives the accepted socket or an exception
     * @param[in] recvBufferSize Optional receive buffer size (`SO_RCVBUF`)
     * @param[in] sendBufferSize Optional send buffer size (`SO_SNDBUF`)
     * @param[in] internalBufferSize Optional internal buffer for `read<T>()`
     * @param[in] soRecvTimeoutMillis Receive timeout (`SO_RCVTIMEO`) in milliseconds; `-1` disables
     * @param[in] soSendTimeoutMillis Send timeout (`SO_SNDTIMEO`) in milliseconds; `-1` disables
     * @param[in] tcpNoDelay Whether to disable Nagle’s algorithm (`TCP_NODELAY`); default: `true`
     * @param[in] keepAlive Whether to enable TCP keep-alive (`SO_KEEPALIVE`)
     * @param[in] nonBlocking Whether to immediately make the accepted socket non-blocking
     *
     * @pre The server socket must be valid, bound, and listening
     * @post The callback is invoked exactly once, either with a valid `Socket` or an error
     *
     * @see accept(), tryAccept(), acceptBlocking(), acceptAsync(std::future)
     * @see std::optional, std::exception_ptr, std::rethrow_exception
     */
    void acceptAsync(std::function<void(std::optional<Socket>, std::exception_ptr)> callback,
                     std::optional<std::size_t> recvBufferSize = std::nullopt,
                     std::optional<std::size_t> sendBufferSize = std::nullopt,
                     std::optional<std::size_t> internalBufferSize = std::nullopt, int soRecvTimeoutMillis = -1,
                     int soSendTimeoutMillis = -1, bool tcpNoDelay = true, bool keepAlive = false,
                     bool nonBlocking = false) const;

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
    [[nodiscard]] bool isValid() const noexcept { return getSocketFd() != INVALID_SOCKET; }

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
    [[nodiscard]] bool isClosed() const noexcept { return getSocketFd() == INVALID_SOCKET; }

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
