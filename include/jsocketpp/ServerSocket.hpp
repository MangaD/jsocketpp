#pragma once

#include "Socket.hpp"
#include "common.hpp"

namespace jsocketpp
{

/**
 * @brief TCP server socket abstraction (Java-like interface).
 *
 * Listens for incoming connections and accepts them as Socket objects.
 *
 * @note Not thread-safe. Each server socket should only be used from one thread at a time.
 */
class ServerSocket
{
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

    /**
     * @brief Get the platform-specific socket address reuse option.
     *
     * This method returns the appropriate socket option for controlling address reuse behavior,
     * which varies between Windows and Unix-like systems:
     *
     * - On Windows: Returns SO_EXCLUSIVEADDRUSE, which prevents other sockets from binding
     *   to the same address/port combination, even if SO_REUSEADDR is set. This provides
     *   better security against port hijacking attacks.
     *
     * - On Unix/Linux: Returns SO_REUSEADDR, which allows the socket to bind to an address
     *   that is already in use. This is particularly useful for server applications that
     *   need to restart and rebind to their previous address immediately.
     *
     * @return int The platform-specific socket option constant (SO_EXCLUSIVEADDRUSE on Windows,
     *             SO_REUSEADDR on Unix/Linux)
     */
    [[nodiscard]] static int getSocketReuseOption();

  public:
    /**
     * @brief Constructs a ServerSocket object and prepares it to listen for incoming TCP connections on the specified
     * port.
     *
     * This constructor initializes a server socket that can accept both IPv4 and IPv6 connections.
     * It performs the following steps:
     *   - Sets up address resolution hints for TCP sockets, supporting both IPv4 and IPv6.
     *   - Uses getaddrinfo to resolve the local address and port for binding.
     *   - Iterates through the resolved addresses, attempting to create a socket for each until successful.
     *   - For IPv6 sockets, configures the socket to accept both IPv6 and IPv4 connections by disabling IPV6_V6ONLY.
     *   - Sets socket options to control address reuse behavior:
     *       - On Windows, uses SO_EXCLUSIVEADDRUSE to prevent port hijacking.
     *       - On other platforms, uses SO_REUSEADDR to allow quick reuse of the port after closure.
     *   - Throws socket_exception on failure at any step, providing error details.
     *
     * @param port The port number on which the server will listen for incoming connections.
     *
     * @throws SocketException If address resolution, socket creation, or socket option configuration fails.
     */
    explicit ServerSocket(unsigned short port);

    /**
     * @brief Copy constructor (deleted).
     *
     * ServerSocket objects cannot be copied because they represent unique system resources.
     * Use move semantics (ServerSocket&&) instead to transfer ownership.
     */
    ServerSocket(const ServerSocket& rhs) = delete; //-Weffc++

    /**
     * @brief Move constructor.
     *
     * Transfers ownership of socket resources from another ServerSocket object.
     * The moved-from socket is left in a valid but empty state.
     *
     * @param rhs The ServerSocket to move from
     */
    ServerSocket(ServerSocket&& rhs) noexcept
        : _serverSocket(rhs._serverSocket), _srvAddrInfo(rhs._srvAddrInfo), _port(rhs._port)
    {
        rhs._serverSocket = INVALID_SOCKET;
        rhs._srvAddrInfo = nullptr;
    }

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
     * @brief Destructor. Closes the server socket and frees resources.
     */
    ~ServerSocket() noexcept;

    /**
     * @brief Bind the server socket to the configured port.
     * @throws SocketException on error.
     */
    void bind() const;

    /**
     * @brief Start listening for incoming connections.
     * @param backlog Maximum length of the pending connections queue (default: SOMAXCONN).
     * @throws SocketException on error.
     */
    void listen(int backlog = SOMAXCONN) const;

    /**
     * @brief Accept an incoming connection.
     * @param bufferSize Size of the internal read buffer for the accepted socket (default: 512).
     * @return A Socket object for the client.
     * @throws SocketException on error.
     */
    [[nodiscard]] Socket accept(std::size_t bufferSize = 512) const;

    /**
     * @brief Close the server socket.
     * @throws SocketException on error.
     */
    void close();

    /**
     * @brief Shutdown the server socket for both send and receive.
     * @throws SocketException on error.
     */
    void shutdown() const;

    /**
     * @brief Check if the server socket is valid (open).
     * @return true if valid, false otherwise.
     */
    [[nodiscard]] bool isValid() const { return this->_serverSocket != INVALID_SOCKET; }

  private:
    SOCKET _serverSocket = INVALID_SOCKET; ///< Underlying socket file descriptor.
    addrinfo* _srvAddrInfo = nullptr;      ///< Address info for binding (from getaddrinfo)
    addrinfo* _selectedAddrInfo = nullptr; ///< Selected address info for binding
    unsigned short _port;                  ///< Port number the server will listen on
};

} // namespace jsocketpp