/**
 * @file ServerSocket.hpp
 * @brief Declares the ServerSocket class for TCP server socket operations.
 */

#pragma once

#include "Socket.hpp"
#include "SocketException.hpp"
#include "common.hpp"

#include <exception>
#include <functional>
#include <future>
#include <optional>

namespace jsocketpp
{

/**
 * @class ServerSocket
 * @ingroup tcp
 * @brief TCP server socket abstraction for cross-platform C++ networking.
 *
 * The `ServerSocket` class provides a high-level, Java-like interface to create TCP server sockets in C++17,
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
 *
 * @author MangaD
 * @date 2025
 * @version 1.0
 */
class ServerSocket
{

  public:
    /**
     * @brief Default buffer size (in bytes) for newly accepted sockets.
     *
     * This value determines the size of the internal read buffer allocated for each
     * client socket accepted by the server. A default of 4096 bytes (4 KB) is chosen
     * because it matches the most common memory page size on modern operating systems,
     * resulting in efficient memory usage and reducing the likelihood of buffer overflows
     * for typical application-layer protocols.
     *
     * 4 KB is also large enough to efficiently handle common payloads (such as HTTP headers,
     * small WebSocket frames, or control messages) in a single read, while keeping per-connection
     * memory usage reasonable for servers handling many clients concurrently.
     *
     * This default is suitable for most use cases, but you can override it by specifying a
     * different buffer size when accepting a socket, or by using `setReceiveBufferSize()` to
     * change the per-server default.
     *
     * @note If your application routinely expects larger messages or needs to optimize for
     * very high throughput, you may increase this value. Conversely, for memory-constrained
     * environments or when handling many thousands of connections, reducing the buffer size
     * may be appropriate.
     *
     * @see setReceiveBufferSize()
     */
    static constexpr std::size_t DefaultBufferSize = 4096;

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
     * @param port            The port number to prepare the server socket for (binding will occur according to
     *                        `autoBindListen`).
     * @param localAddress    The local address/interface to bind to (empty for all interfaces).
     * @param autoBindListen  If true (default), automatically binds and listens. If false, user must call them
     *                        manually.
     * @param reuseAddress    If true (default), enables address reuse (see above) before binding.
     * @param soTimeoutMillis Accept timeout in milliseconds for `accept()`; -1 (default) means block indefinitely.
     * @param dualStack       If true (default), enables dual-stack (IPv4+IPv6) for IPv6 sockets. If false, enables
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
    explicit ServerSocket(unsigned short port, std::string_view localAddress = {}, bool autoBindListen = true,
                          bool reuseAddress = true, int soTimeoutMillis = -1, bool dualStack = true);

    /**
     * @brief Get the local IP address to which the server socket is bound.
     *
     * Returns the string representation of the IP address (IPv4 or IPv6) the socket is bound to.
     * Useful for debugging, especially when binding to specific interfaces or when binding to
     * "0.0.0.0" or "::" (any address).
     *
     * @note This method is thread safe.
     * @ingroup tcp
     *
     * @return The local IP address as a string, or an empty string if the socket is not bound.
     * @throws SocketException if there is an error retrieving the address.
     *
     * @code
     * ServerSocket server("127.0.0.1", 8080);
     * server.bind();
     * std::cout << "Server bound to address: " << server.getInetAddress() << std::endl;
     * @endcode
     */
    [[nodiscard]] std::string getInetAddress() const;

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
     * unsigned short port = server.getLocalPort();
     * std::cout << "Server bound to port: " << port << std::endl;
     * @endcode
     */
    [[nodiscard]] unsigned short getLocalPort() const;

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
     * ServerSocket objects cannot be copied because they represent unique system resources.
     * Use move semantics (ServerSocket&&) instead to transfer ownership.
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
     * @param rhs The ServerSocket to copy from (unused since deleted)
     * @return Reference to this ServerSocket (never returns since deleted)
     */
    ServerSocket& operator=(const ServerSocket& rhs) = delete; //-Weffc++

    /**
     * @brief Move constructor.
     *
     * Transfers ownership of socket resources from another ServerSocket object.
     * The moved-from socket is left in a valid but empty state.
     *
     * @param rhs The ServerSocket to move from
     */
    ServerSocket(ServerSocket&& rhs) noexcept
        : _serverSocket(rhs._serverSocket), _srvAddrInfo(rhs._srvAddrInfo), _selectedAddrInfo(rhs._selectedAddrInfo),
          _port(rhs._port), _isBound(rhs._isBound), _isListening(rhs._isListening),
          _soTimeoutMillis(rhs._soTimeoutMillis), _defaultBufferSize(rhs._defaultBufferSize)
    {
        rhs._serverSocket = INVALID_SOCKET;
        rhs._srvAddrInfo = nullptr;
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
     * @param rhs The ServerSocket to move resources from.
     * @return Reference to this ServerSocket (containing the moved resources).
     *
     * @note This operation is thread-safe with respect to the moved-from socket,
     *       but concurrent operations on either socket during the move may cause undefined behavior.
     *
     * @see ServerSocket(ServerSocket&&)
     * @see close()
     */
    ServerSocket& operator=(ServerSocket&& rhs) noexcept
    {
        if (this != &rhs)
        {
            close(); // Clean up existing resources
            _serverSocket = rhs._serverSocket;
            _srvAddrInfo = rhs._srvAddrInfo;
            _selectedAddrInfo = rhs._selectedAddrInfo;
            _port = rhs._port;
            _isBound = rhs._isBound;
            _isListening = rhs._isListening;
            _soTimeoutMillis = rhs._soTimeoutMillis;
            _defaultBufferSize = rhs._defaultBufferSize;

            rhs._serverSocket = INVALID_SOCKET;
            rhs._srvAddrInfo = nullptr;
            rhs._selectedAddrInfo = nullptr;
            rhs._isBound = false;
            rhs._isListening = false;
        }
        return *this;
    }

    /**
     * @brief Destructor. Closes the server socket and frees resources.
     *
     * @note This method is not thread safe. If multiple threads may call close(), external synchronization is required.
     * @ingroup tcp
     */
    ~ServerSocket() noexcept;

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
     * @see setReuseAddress(), listen(), ServerSocket(unsigned short)
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
     * @see bind(), accept(), ServerSocket(unsigned short)
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
     * waiting behavior of this method, since `select()` is used for waiting:
     * - If the socket is in **blocking mode**, after readiness, `accept()` is called; in a rare race condition,
     *   if no connection is available, `accept()` will block again (possibly beyond the timeout) until a client
     * connects.
     * - If the socket is in **non-blocking mode**, in the same race, `accept()` may fail with `EWOULDBLOCK` or `EAGAIN`
     *   and a `SocketException` is thrown.
     *
     * This race condition is inherent to all socket APIs and can only be avoided by synchronizing access
     * to the server socket across threads or processes.
     *
     * @note - Use `setSoTimeout()` and `getSoTimeout()` to configure the timeout.
     *       For manual non-blocking accept in an event loop, see `acceptNonBlocking()` and `waitReady()`.
     *
     * @note - To safely use `accept()` in a multithreaded environment, protect access to the `ServerSocket`
     *       with a mutex. Example:
     *       @code
     *       std::mutex acceptMutex;
     *       jsocketpp::ServerSocket server(8080);
     *       server.bind();
     *       server.listen();
     *       std::vector<std::thread> workers;
     *       for (int i = 0; i < 4; ++i) {
     *           workers.emplace_back([&server, &acceptMutex]() {
     *               while (true) {
     *                   std::unique_lock<std::mutex> lock(acceptMutex);
     *                   auto client = server.accept();
     *                   lock.unlock();
     *                   // Handle client...
     *               }
     *           });
     *       }
     *       @endcode
     *
     * @ingroup tcp
     *
     * @param bufferSize The internal buffer size for the newly accepted client socket.
     *                   If set to `0`, the per-instance default buffer size is used (see `setReceiveBufferSize()` and
     *                   `DefaultBufferSize`). If not specified, defaults to `DefaultBufferSize` (4096 bytes) unless
     *                   changed via `setReceiveBufferSize()`.
     * @return A `Socket` object representing the connected client.
     *
     * @throws SocketException if the server socket is not initialized, closed, or if an internal error occurs.
     * @throws SocketTimeoutException if the timeout expires before a client connects.
     *
     * @see setSoTimeout(int)
     * @see getSoTimeout()
     * @see acceptBlocking()
     * @see acceptNonBlocking()
     * @see waitReady()
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * server.setSoTimeout(5000); // Wait up to 5 seconds for clients
     * try {
     *     jsocketpp::Socket client = server.accept();
     *     // Handle client...
     * } catch (const jsocketpp::SocketTimeoutException&) {
     *     std::cout << "No client connected within timeout." << std::endl;
     * }
     * @endcode
     */
    [[nodiscard]] Socket accept(std::size_t bufferSize = 0) const;

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
     * Internally, this method uses `waitReady(timeoutMillis)` (which uses `select()`) to wait for readiness, then
     * calls `accept()`. If the timeout expires with no client, a `SocketTimeoutException` is thrown.
     *
     * The **blocking or non-blocking mode** of the server socket (via `setNonBlocking()`) does not affect the
     * waiting behavior of this method, since `select()` is used for waiting:
     * - If the socket is in **blocking mode**, after readiness, `accept()` is called; in a rare race condition,
     *   if no connection is available, `accept()` will block again (possibly beyond the timeout) until a client
     * connects.
     * - If the socket is in **non-blocking mode**, in the same race, `accept()` may fail with `EWOULDBLOCK` or `EAGAIN`
     *   and a `SocketException` is thrown.
     *
     * This race condition is inherent to all socket APIs and can only be avoided by synchronizing access
     * to the server socket across threads or processes.
     *
     * @note Use this method to specify a timeout **per call**; to set a default for all accepts, use `setSoTimeout()`.
     *       For manual non-blocking accept in an event loop, see `acceptNonBlocking()` and `waitReady()`.
     *
     * @ingroup tcp
     *
     * @param timeoutMillis The maximum number of milliseconds to wait for a client connection:
     *   - Negative: wait indefinitely.
     *   - Zero: poll (return immediately).
     *   - Positive: wait up to this many milliseconds.
     * @param bufferSize The internal buffer size for the newly accepted client socket.
     *                   If set to `0`, the per-instance default buffer size is used (see `setReceiveBufferSize()` and
     *                   `DefaultBufferSize`). If not specified, defaults to `DefaultBufferSize` (4096 bytes) unless
     *                   changed via `setReceiveBufferSize()`.
     * @return A `Socket` object representing the connected client.
     *
     * @throws SocketException if the server socket is not initialized, closed, or if an internal error occurs.
     * @throws SocketTimeoutException if the timeout expires before a client connects.
     *
     * @see setSoTimeout(int)
     * @see getSoTimeout()
     * @see acceptBlocking()
     * @see acceptNonBlocking()
     * @see waitReady()
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * try {
     *     // Wait up to 2 seconds for a client connection, then timeout.
     *     jsocketpp::Socket client = server.accept(2000);
     *     // Handle client...
     * } catch (const jsocketpp::SocketTimeoutException&) {
     *     std::cout << "No client connected within timeout." << std::endl;
     * }
     * @endcode
     */
    [[nodiscard]] Socket accept(int timeoutMillis, std::size_t bufferSize = 0) const;

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
     * behavior of this method, since `select()` is used for waiting. In rare cases (see below), `accept()` may still
     * fail with `EWOULDBLOCK` or `EAGAIN` if a connection is lost between the readiness check and the actual accept
     * call, in which case a `SocketException` is thrown.
     *
     * @note This method is **not thread safe**. Simultaneous calls from multiple threads or processes may lead to race
     * conditions. See `accept()` for further discussion of these edge cases.
     *
     * @note To specify a timeout for a single call, use `tryAccept(int timeoutMillis, std::size_t bufferSize)`.
     *       For manual non-blocking accept, see `acceptNonBlocking()`.
     *
     * @ingroup tcp
     *
     * @param bufferSize The internal buffer size for the newly accepted client socket.
     *                   If set to `0`, the per-instance default buffer size is used (see `setReceiveBufferSize()` and
     *                   `DefaultBufferSize`). If not specified, defaults to `DefaultBufferSize` (4096 bytes) unless
     *                   changed via `setReceiveBufferSize()`.
     * @return An `std::optional<Socket>` containing the accepted client socket, or `std::nullopt` if no client was
     * available before the timeout.
     *
     * @throws SocketException if the server socket is not initialized, closed, or if an internal error occurs.
     *
     * @see setSoTimeout(int)
     * @see getSoTimeout()
     * @see accept()
     * @see acceptBlocking()
     * @see acceptNonBlocking()
     * @see tryAccept(int, std::size_t)
     * @see waitReady()
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
    [[nodiscard]] std::optional<Socket> tryAccept(std::size_t bufferSize = 0) const;

    /**
     * @brief Attempt to accept an incoming client connection, waiting up to a specified timeout.
     *
     * This method waits for an incoming client connection using the provided `timeoutMillis` value, overriding any
     * global timeout set with `setSoTimeout()`. If `timeoutMillis` is negative, the method blocks indefinitely until a
     * client connects. If `timeoutMillis` is zero, the method polls and returns immediately if no client is waiting.
     *
     * Unlike `accept(int timeoutMillis, ...)`, which throws a `SocketTimeoutException` if the timeout expires,
     * this method returns `std::nullopt` in that case. This is useful for polling loops or event-driven servers.
     *
     * Internally, the method uses `waitReady(timeoutMillis)` (implemented with `select()`) to check for pending
     * connections. If the socket is ready, `acceptBlocking(bufferSize)` is called; otherwise, `std::nullopt` is
     * returned.
     *
     * The blocking or non-blocking mode of the server socket (via `setNonBlocking(true)`) does **not** affect the
     * waiting behavior of this method. Readiness is determined entirely by `select()`. In rare cases, due to race
     * conditions (see `accept()` documentation), `accept()` may still fail with `EWOULDBLOCK` or `EAGAIN` even after
     * readiness was indicated; in this case, a `SocketException` is thrown.
     *
     * @note This method is **not thread safe**. Simultaneous calls from multiple threads or processes may lead to race
     * conditions.
     *
     * @note To use the configured socket timeout instead, call `tryAccept(std::size_t bufferSize)`.
     *       For manual non-blocking accept, see `acceptNonBlocking()`.
     *
     * @ingroup tcp
     *
     * @param timeoutMillis Timeout in milliseconds to wait for a client connection. Negative blocks indefinitely, zero
     * polls.
     * @param bufferSize The internal buffer size for the newly accepted client socket.
     *                   If set to `0`, the per-instance default buffer size is used (see `setReceiveBufferSize()` and
     *                   `DefaultBufferSize`). If not specified, defaults to `DefaultBufferSize` (4096 bytes) unless
     *                   changed via `setReceiveBufferSize()`.
     * @return An `std::optional<Socket>` containing the accepted client socket, or `std::nullopt` if no client
     * connected before the timeout.
     *
     * @throws SocketException if the server socket is not initialized, closed, or if an internal error occurs.
     *
     * @see accept(int, std::size_t)
     * @see tryAccept(std::size_t)
     * @see accept()
     * @see acceptBlocking()
     * @see acceptNonBlocking()
     * @see waitReady()
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * while (true) {
     *     if (auto client = server.tryAccept(100)) {
     *         // Handle client connection
     *     } else {
     *         // No client; perform other tasks
     *     }
     * }
     * @endcode
     */
    [[nodiscard]] std::optional<Socket> tryAccept(int timeoutMillis, std::size_t bufferSize = 0) const;

    /**
     * @brief Accept an incoming client connection, always blocking until a client connects (unless the socket is set to
     * non-blocking).
     *
     * This method invokes the underlying system `accept()` on the listening server socket.
     * - If the socket is in **blocking mode** (default), this method blocks until a client connects, regardless of any
     * timeouts set by `setSoTimeout()`.
     * - If the socket is in **non-blocking mode** (set via `setNonBlocking(true)`), the call returns immediately:
     *   - If a client is ready, returns a connected `Socket`.
     *   - If no client is pending, throws a `SocketException` with `EWOULDBLOCK`/`EAGAIN`.
     *
     * @par Comparison with acceptNonBlocking()
     * - Both methods behave the same: their behavior depends on the blocking mode of the socket.
     * - However, `acceptNonBlocking()` is typically used in event loop code, where you expect the socket to be in
     * non-blocking mode and wish to distinguish "no client yet" (returns `std::nullopt`) from actual errors (throws).
     * - `acceptBlocking()` throws on *all* failures and is idiomatic for classic blocking server loops.
     *
     * @note Both methods require the socket's blocking mode to be managed by the user via `setNonBlocking()`.
     *       There is **no polling or timeout logic** in this method; if you need timeouts, use `accept()` or
     * `tryAccept()` instead.
     *
     * @note Not thread safe: simultaneous accept calls from different threads or processes may result in race
     * conditions or failures.
     *
     * @ingroup tcp
     *
     * @param bufferSize The internal buffer size for the newly accepted client socket.
     *                   If set to `0`, the per-instance default buffer size is used (see `setReceiveBufferSize()` and
     *                   `DefaultBufferSize`). If not specified, defaults to `DefaultBufferSize` (4096 bytes) unless
     *                   changed via `setReceiveBufferSize()`.
     * @return A `Socket` object representing the connected client.
     *
     * @throws SocketException if the server socket is not initialized, closed, or if `accept()` fails (including with
     * `EWOULDBLOCK`/`EAGAIN` in non-blocking mode).
     *
     * @see accept()
     * @see acceptNonBlocking()
     * @see tryAccept()
     * @see setNonBlocking()
     * @see waitReady()
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * // Classic blocking:
     * jsocketpp::Socket client = server.acceptBlocking();
     *
     * // Non-blocking pattern:
     * server.setNonBlocking(true);
     * try {
     *     jsocketpp::Socket client = server.acceptBlocking();
     *     // Handle client
     * } catch (const jsocketpp::SocketException& e) {
     *     if (e.code() == EWOULDBLOCK) { // or platform equivalent
     *         // No client yet, try again later
     *     } else {
     *         throw; // real error
     *     }
     * }
     * @endcode
     */
    [[nodiscard]] Socket acceptBlocking(std::size_t bufferSize = 0) const;

    /**
     * @brief Attempt to accept a client connection in non-blocking fashion.
     *
     * This method attempts to accept an incoming client connection using the underlying system `accept()`.
     * - If the server socket is in **blocking mode** (the default), this method will block until a client is ready,
     *   and will return a connected `Socket` object.
     * - If the server socket is in **non-blocking mode** (set via `setNonBlocking(true)`), this method will
     *   return immediately:
     *     - If a client connection is pending, returns a `Socket` object representing the client.
     *     - If no client is pending, returns `std::nullopt` (no exception is thrown).
     *
     * This method does **not** perform any polling, waiting, or timeout logic. It is designed for use in event loops or
     * with polling mechanisms, where you expect the socket to be non-blocking and want to check for new connections
     * without waiting.
     *
     * @note The blocking or non-blocking behavior of this method depends entirely on the socket's current mode,
     *       which can be set via `setNonBlocking(bool)`.
     * @note For waiting with a timeout or blocking-until-ready semantics, see `accept()`, `tryAccept()`, or
     * `acceptBlocking()`.
     * @note Not thread safe: concurrent accept calls may result in race conditions.
     *
     * @ingroup tcp
     *
     * @param bufferSize The internal buffer size for the newly accepted client socket.
     *                   If set to `0`, the per-instance default buffer size is used (see `setReceiveBufferSize()` and
     *                   `DefaultBufferSize`). If not specified, defaults to `DefaultBufferSize` (4096 bytes) unless
     *                   changed via `setReceiveBufferSize()`.
     * @return A `Socket` object if a client connection is ready; otherwise, `std::nullopt`.
     *
     * @throws SocketException if the server socket is not initialized, closed, or if an unrecoverable error occurs
     *         during accept (other than EWOULDBLOCK/EAGAIN in non-blocking mode).
     *
     * @see setNonBlocking(bool)
     * @see acceptBlocking()
     * @see accept()
     * @see tryAccept()
     * @see waitReady()
     *
     * @code
     * jsocketpp::ServerSocket server(8080);
     * server.bind();
     * server.listen();
     * server.setNonBlocking(true); // Must be in non-blocking mode for true non-blocking behavior
     * while (true) {
     *     auto clientOpt = server.acceptNonBlocking();
     *     if (clientOpt) {
     *         // Handle client connection
     *     } else {
     *         // No client yet, can do other work (e.g. event loop)
     *     }
     * }
     * @endcode
     */
    [[nodiscard]] std::optional<Socket> acceptNonBlocking(std::size_t bufferSize = 0) const;

    /**
     * @brief Asynchronously accept an incoming client connection, returning a future.
     *
     * This method initiates an asynchronous accept operation on the server socket,
     * returning a `std::future<Socket>` that will become ready when a client connects or an error occurs.
     * The returned future allows you to wait for incoming connections in a non-blocking, event-driven way,
     * which is ideal for scalable, modern C++ server architectures.
     *
     * Internally, this implementation launches the accept operation in a background thread
     * using `std::async(std::launch::async, ...)` to ensure that the calling thread is never blocked.
     * When a new client connection is accepted, the returned future becomes ready and yields a fully constructed
     * `jsocketpp::Socket` object for communication with the client. If an error occurs (including timeouts),
     * the future will rethrow a `SocketException` or `SocketTimeoutException` when `.get()` is called.
     *
     * ### Usage Example
     * @code
     * jsocketpp::ServerSocket server(8080);
     * // Start asynchronous accept operation
     * std::future<jsocketpp::Socket> clientFuture = server.acceptAsync();
     *
     * // Do other work while waiting for a client...
     * while (clientFuture.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
     *     // Perform background tasks, handle shutdown signals, or update application state
     * }
     *
     * // When ready, obtain the accepted client socket (may throw)
     * try {
     *     jsocketpp::Socket client = clientFuture.get();
     *     // Use client socket for I/O...
     * } catch (const jsocketpp::SocketException& ex) {
     *     std::cerr << "Accept failed: " << ex.what() << std::endl;
     * }
     * @endcode
     *
     * ### Key Features
     * - Non-blocking: Does not block the calling thread while waiting for connections.
     * - Integrates with modern C++ asynchronous workflows (`std::future`, `std::async`).
     * - Preserves exception semantics: socket errors are rethrown when the future is resolved.
     * - Buffer size for the accepted client socket can be specified (default matches ServerSocket default).
     * - Clean API: future and accepted Socket are managed with RAII and strong exception safety.
     *
     * ### Thread Safety
     * - Not thread-safe: Do not call `accept()`, `acceptAsync()`, or other accept methods concurrently on the same
     * ServerSocket instance. If you need to accept multiple connections in parallel, use one ServerSocket per thread or
     * synchronize calls externally.
     *
     * ### Implementation Notes
     * - This default implementation uses a separate thread for each asynchronous accept operation.
     *   For most server workloads, this is sufficient. For ultra-high concurrency (thousands of clients),
     *   consider integrating an event-driven backend (e.g., Boost.Asio, epoll, IOCP) in a future version.
     *
     * ### Platform Support
     * - Works on all supported platforms (Windows, Linux, macOS). Uses portable C++17 primitives only.
     *
     * ### Error Handling
     * - If an error occurs before or during accept, calling `.get()` on the returned future will rethrow the exception.
     *   - Throws `jsocketpp::SocketException` on fatal socket errors.
     *   - Throws `jsocketpp::SocketTimeoutException` if a timeout occurs (if configured).
     *
     * @note The ServerSocket must outlive the future.
     *
     * @param bufferSize The internal buffer size (in bytes) to use for the newly accepted client socket.
     *                   If set to `0` (the default), the ServerSocket's default buffer size is used.
     *                   Adjust this value if you expect larger or smaller messages.
     *
     * @return std::future<Socket>
     *   A future that resolves to the accepted client socket when a client connects.
     *   If an error occurs, the exception will be rethrown from the future.
     *
     * @see accept(), tryAccept(), acceptBlocking()
     * @see std::future, std::async
     */
    [[nodiscard]] std::future<Socket> acceptAsync(std::size_t bufferSize = 0) const;

    /**
     * @brief Asynchronously accept a client connection and invoke a callback upon completion.
     *
     * This method initiates a non-blocking accept operation on the server socket. When a client connects,
     * or if an error occurs, the provided callback function is invoked. The callback receives either:
     *   - a valid `std::optional<Socket>` containing the accepted client socket (on success), and a `nullptr` exception
     * pointer,
     *   - or an empty `std::optional<Socket>` (on error), and a non-null exception pointer describing the error.
     *
     * The accept operation runs in a detached background thread, so the callback may be executed on a different thread
     * from the caller. This pattern is ideal for event-driven or highly concurrent servers, enabling your application
     * to perform other work while waiting for incoming connections and to react immediately when a client is accepted.
     *
     * ### Usage Example
     * @code
     * server.acceptAsync(
     *     [](std::optional<jsocketpp::Socket> clientOpt, std::exception_ptr eptr) {
     *         if (eptr) {
     *             try { std::rethrow_exception(eptr); }
     *             catch (const jsocketpp::SocketException& ex) {
     *                 std::cerr << "Accept failed: " << ex.what() << std::endl;
     *             }
     *         } else if (clientOpt) {
     *             std::cout << "Accepted client from: " << clientOpt->getRemoteSocketAddress() << std::endl;
     *             // Handle the client socket (e.g., read/write)
     *         }
     *     }
     * );
     * @endcode
     *
     * ### Thread Safety
     * - This method is **not thread-safe** for concurrent calls. Do not call `acceptAsync`, `accept`, or other accept
     *   methods concurrently on the same `ServerSocket` instance without external synchronization.
     *   Use separate `ServerSocket` instances or synchronize access if parallel accepts are needed.
     *
     * ### Callback Details
     * - The callback is always invoked exactly once per call to `acceptAsync`:
     *   - On success: `clientOpt` contains a valid `Socket`, `eptr` is `nullptr`.
     *   - On error: `clientOpt` is empty, `eptr` holds the exception. Use `std::rethrow_exception(eptr)` to rethrow it.
     *
     * ### Implementation Notes
     * - By default, this function launches a detached background thread for each asynchronous accept.
     *   For ultra-high concurrency, consider event-driven or coroutine-based backends in the future.
     *
     * @param callback The function to invoke on completion. Signature:
     *                 `void callback(std::optional<Socket>, std::exception_ptr)`.
     * @param bufferSize Internal buffer size (bytes) for the accepted client socket (default: server default).
     *
     * @see acceptAsync(std::future), accept(), tryAccept()
     * @see std::optional, std::exception_ptr, std::rethrow_exception
     */
    void acceptAsync(std::function<void(std::optional<Socket>, std::exception_ptr)> callback,
                     std::size_t bufferSize = 0) const;

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
     * @see close()
     */
    [[nodiscard]] bool isValid() const noexcept { return this->_serverSocket != INVALID_SOCKET; }

    /**
     * @brief Check if the server socket has been closed.
     *
     * This method returns `true` if the socket has been closed (and is no longer usable), or `false` if it is still
     * open. The logic and naming follow the Java networking API for familiarity.
     *
     * @return `true` if the socket has been closed, `false` if it is still open.
     */
    [[nodiscard]] bool isClosed() const noexcept { return this->_serverSocket == INVALID_SOCKET; }

    /**
     * @brief Set a socket option for the listening server socket.
     *
     * This method allows you to control low-level parameters of the listening (accepting)
     * server socket. Because this socket is used only to accept new client connections,
     * only certain options are meaningful here. Typical uses include:
     *  - Allowing the server to quickly re-bind to a port after restart (SO_REUSEADDR)
     *  - Configuring buffer sizes for incoming connections
     *  - Tuning low-level TCP behaviors (e.g., SO_KEEPALIVE, SO_RCVBUF, SO_SNDBUF)
     *
     * @note
     * - Changing some options on a listening socket (like SO_LINGER or SO_RCVBUF) only affects
     *   the acceptor socket itself, **not** the individual sockets returned by `accept()`.
     *   For per-client tuning, set options on the accepted `Socket` objects.
     * - Attempting to set unsupported or inappropriate options may result in exceptions
     *   or undefined behavior.
     *
     * Example: Enable port reuse (recommended for most servers):
     * @code
     * serverSocket.setOption(SOL_SOCKET, SO_REUSEADDR, 1);
     * @endcode
     *
     * @param level   Protocol level at which the option resides (e.g., SOL_SOCKET, IPPROTO_TCP)
     * @param optName Option name (e.g., SO_REUSEADDR, SO_RCVBUF)
     * @param value   Integer value for the option
     * @throws SocketException if the operation fails
     */
    void setOption(int level, int optName, int value);

    /**
     * @brief Retrieve the current value of a socket option for the listening server socket.
     *
     * This method lets you query the current setting of a socket option on the listening
     * socket. This is most useful for debugging, for monitoring server configuration,
     * or for verifying a platform's default values.
     *
     * @note
     * - Only options relevant to listening sockets will reflect meaningful values here.
     * - To check options for an individual client connection, call `getOption` on the
     *   `Socket` object returned by `accept()`.
     *
     * Example: Get the current size of the receive buffer for new connections:
     * @code
     * int rcvBuf = serverSocket.getOption(SOL_SOCKET, SO_RCVBUF);
     * @endcode
     *
     * @param level   Protocol level (e.g., SOL_SOCKET)
     * @param optName Option name (e.g., SO_RCVBUF)
     * @return        Integer value for the option
     * @throws SocketException if the operation fails
     */
    [[nodiscard]] int getOption(int level, int optName) const;

    /**
     * @brief Returns the correct socket option constant for address reuse, depending on the platform.
     *
     * - On **Unix/Linux** platforms, this returns @c SO_REUSEADDR, which allows a socket to bind to a local
     * address/port that is in the TIME_WAIT state. This is commonly used for servers that need to restart without
     * waiting for TCP connections to fully time out.
     *
     * - On **Windows**, this returns @c SO_EXCLUSIVEADDRUSE, which provides safer server semantics: only one socket can
     *   bind to a given address/port at a time, but it allows quick server restarts. Using @c SO_REUSEADDR on Windows
     * has different (and less safe) behavior, potentially allowing multiple sockets to bind to the same port
     * simultaneously, which is almost never what you want for TCP servers.
     *
     * @return The socket option to use with setsockopt() for configuring address reuse in a cross-platform way.
     *
     * @note This function should be used to select the correct socket option when calling setsockopt() in your server
     * code. Typically, this option must be set before calling bind().
     */
    [[nodiscard]] static int getSocketReuseOption();

    /**
     * @brief Enable or disable address reuse for this server socket.
     *
     * When address reuse is enabled (`enable = true`), the server socket can bind to a local address/port even if
     * a previous socket on that port is still in the TIME_WAIT state. This is useful for restarting servers
     * without waiting for old sockets to time out.
     *
     * On UNIX-like systems, this sets the `SO_REUSEADDR` socket option. On Windows, it sets the equivalent option,
     * `SO_EXCLUSIVEADDRUSE`.
     *
     * @warning This method must be called before calling bind(), and only once the socket has been created
     * (i.e., after construction, before bind).
     *
     * @param enable True to enable address reuse, false to disable (default OS behavior).
     *
     * @throws SocketException if setting the option fails (e.g., socket not open or system error).
     *
     * @note Improper use of address reuse can have security and protocol implications. For most server applications,
     * enabling this is recommended. For advanced load-balancing, see also SO_REUSEPORT (not portable).
     *
     * @see https://man7.org/linux/man-pages/man7/socket.7.html
     */
    void setReuseAddress(bool enable);

    /**
     * @brief Query whether the address reuse option is enabled on this server socket.
     *
     * This function checks whether the socket is currently configured to allow reuse of local addresses.
     *
     * - On UNIX-like systems, this checks the `SO_REUSEADDR` socket option.
     * - On Windows, this checks `SO_EXCLUSIVEADDRUSE`, but note that this is semantically **opposite**:
     *   enabling `SO_EXCLUSIVEADDRUSE` **disables** address reuse.
     *
     * @return `true` if address reuse is enabled (i.e., `SO_REUSEADDR` is set on UNIX-like systems or
     * `SO_EXCLUSIVEADDRUSE` is unset on Windows); `false` otherwise.
     *
     * @throws SocketException if the socket is not valid or the option cannot be retrieved.
     *
     * @note This reflects the current state of the reuse flag, which may have been set manually or inherited
     * from system defaults. Always call this *after* socket creation, and *before* `bind()` for accurate results.
     *
     * @see setReuseAddress()
     */
    [[nodiscard]] bool getReuseAddress() const;

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
     * @param nonBlocking true to enable non-blocking mode, false for blocking (default).
     * @throws SocketException on error.
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
     */
    [[nodiscard]] bool getNonBlocking() const;

    /**
     * @brief Wait for the server socket to become ready to accept a connection.
     *
     * This method uses a specified timeout (in milliseconds) if provided,
     * otherwise it falls back to the timeout set via `setSoTimeout()`.
     *
     * Internally, it uses the `select()` system call to monitor readiness.
     *
     * - If the effective timeout is negative, it blocks indefinitely.
     * - If it is zero, it polls and returns immediately.
     * - If it is positive, it waits up to that duration.
     *
     * @param timeoutMillis Optional timeout in milliseconds. If not provided, uses `getSoTimeout()`.
     * @return true if the socket is ready to accept a connection, false on timeout.
     * @throws SocketException if a system error occurs while waiting.
     *
     * @see setSoTimeout(), getSoTimeout(), accept(), tryAccept()
     */
    [[nodiscard]] bool waitReady(std::optional<int> timeoutMillis = std::nullopt) const;

    /**
     * @brief Set the timeout for accept() operations on this server socket.
     *
     * This timeout controls how long accept() will block while waiting for an incoming connection.
     * It does not affect other socket operations such as read/write.
     *
     * @note Thread-safe if called before concurrent accept() calls.
     *
     * @param millis Timeout in milliseconds:
     *   - If negative (default), blocks indefinitely.
     *   - If zero, polls the socket and returns immediately if no client is waiting.
     *   - If positive, waits up to the specified time for a client to connect.
     */
    void setSoTimeout(const int millis) { _soTimeoutMillis = millis; }

#if defined(IPV6_V6ONLY)
    /**
     * @brief Enable or disable IPv6-only mode for this server socket.
     *
     * By default, an IPv6 socket is configured in dual-stack mode (accepts both IPv6 and IPv4 connections)
     * on most platforms. Enabling IPv6-only mode restricts the socket to **only** IPv6 connections.
     *
     * @param enable True to enable IPv6-only mode, false to allow dual-stack.
     * @throws SocketException if the socket is not IPv6, already bound, or on system error.
     * @note Must be called before bind().
     * @see getIPv6Only()
     */
    void setIPv6Only(bool enable);

    /**
     * @brief Query whether IPv6-only mode is enabled.
     *
     * @return True if the socket is in IPv6-only mode, false if dual-stack.
     * @throws SocketException if the socket is not IPv6, not open, or on system error.
     * @see setIPv6Only()
     */
    [[nodiscard]] bool getIPv6Only() const;
#endif

    /**
     * @brief Get the currently configured timeout for accept() operations.
     *
     * @return Timeout in milliseconds:
     *   - Negative: accept() blocks indefinitely.
     *   - Zero: accept() polls and returns immediately.
     *   - Positive: maximum time to wait for a client connection.
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
     * @param size New buffer size in bytes
     * @see getReceiveBufferSize()
     * @see DefaultBufferSize
     * @see accept()
     */
    void setReceiveBufferSize(const std::size_t size) { _defaultBufferSize = size; }

    /**
     * @brief Get the current default receive buffer size for accepted client sockets.
     *
     * Returns the buffer size that will be used when accepting new client connections.
     * This is the value previously set by setReceiveBufferSize() or the default if not set.
     *
     * @return Current buffer size in bytes
     * @see setReceiveBufferSize()
     * @see DefaultBufferSize
     * @see accept()
     */
    [[nodiscard]] std::size_t getReceiveBufferSize() const noexcept { return _defaultBufferSize; }

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
     * @param enable Set to `true` to enable SO_REUSEPORT, `false` to disable.
     * @throws SocketException if setting the option fails.
     *
     * @see https://man7.org/linux/man-pages/man7/socket.7.html
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
     * @param errorCode The error code to include in the thrown exception
     * @throws SocketException Always throws with the provided error code and corresponding message
     */
    void cleanupAndThrow(int errorCode);

  private:
    /**
     * @brief Get the effective buffer size to use for socket operations.
     *
     * This helper method determines the actual buffer size to use based on the provided
     * buffer size parameter and the default buffer size configured for the server socket.
     * If the provided size is 0, it returns the default buffer size; otherwise, it returns
     * the provided size.
     *
     * @param bufferSize The requested buffer size (0 means use default)
     * @return The effective buffer size to use
     *
     * @see setReceiveBufferSize()
     * @see getReceiveBufferSize()
     * @see DefaultBufferSize
     */
    [[nodiscard]] std::size_t getEffectiveBufferSize(const std::size_t bufferSize) const
    {
        return bufferSize == 0 ? _defaultBufferSize : bufferSize;
    }

    SOCKET _serverSocket = INVALID_SOCKET; ///< Underlying socket file descriptor.
    addrinfo* _srvAddrInfo = nullptr;      ///< Address info for binding (from getaddrinfo)
    addrinfo* _selectedAddrInfo = nullptr; ///< Selected address info for binding
    unsigned short _port;                  ///< Port number the server will listen on
    bool _isBound = false;                 ///< True if the server socket is bound
    bool _isListening = false;             ///< True if the server socket is listening
    int _soTimeoutMillis = -1; ///< Timeout for accept(); -1 = no timeout, 0 = poll, >0 = timeout in milliseconds
    std::size_t _defaultBufferSize =
        DefaultBufferSize; ///< Default buffer size used for accepted client sockets when no specific size is provided
};

} // namespace jsocketpp