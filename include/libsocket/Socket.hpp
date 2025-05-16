#pragma once

#include "SocketException.hpp"
#include "common.hpp"

namespace libsocket
{

/**
 * @brief TCP client socket abstraction (Java-like interface).
 *
 * Provides connect, read, write, close, and address info. Handles both IPv4 and IPv6.
 *
 * @note Not thread-safe. Each socket should only be used from one thread at a time.
 */
class Socket
{
    /**
     * @brief Parent class for ServerSocket to allow friend access.
     *
     * This class requires friendship with ServerSocket to allow it to directly
     * access private members during accept() operations. This enables efficient
     * socket creation without exposing internal implementation details.
     *
     * @see ServerSocket::accept() Creates new Socket instances
     */
    friend class ServerSocket;

  protected:
    /**
     * @brief Protected constructor used internally by ServerSocket::accept().
     *
     * Creates a Socket object from an already-accepted client connection.
     * This constructor is called by ServerSocket when accepting new connections
     * to create Socket objects representing the client connections.
     *
     * @param client Already-connected socket descriptor from accept()
     * @param addr Remote peer's address information
     * @param len Length of the address structure
     * @param bufferSize Size of internal read buffer for this socket
     * @see ServerSocket::accept()
     */
    Socket(SOCKET client, const sockaddr_storage& addr, socklen_t len, std::size_t bufferSize);

  public:
    /**
     * @brief Construct a Socket for a given host and port.
     * @param host Hostname or IP address to connect to.
     * @param port TCP port number.
     * @param bufferSize Size of the internal read buffer (default: 512).
     * @throws SocketException on failure.
     */
    explicit Socket(std::string_view host, unsigned short port, std::size_t bufferSize = 512);

    /**
     * @brief Copy constructor (deleted).
     *
     * Socket objects cannot be copied because they represent unique system resources.
     * Use move semantics (Socket&&) instead to transfer ownership.
     */
    Socket(const Socket& rhs) = delete; //-Weffc++

    /**
     * @brief Move constructor.
     *
     * Transfers ownership of socket resources from another Socket object.
     * The moved-from socket is left in a valid but empty state.
     *
     * @param rhs The Socket to move from
     */
    Socket(Socket&& rhs) noexcept
        : _sockFd(rhs._sockFd), _remoteAddr(rhs._remoteAddr), _remoteAddrLen(rhs._remoteAddrLen),
          _cliAddrInfo(rhs._cliAddrInfo), _selectedAddrInfo(rhs._selectedAddrInfo), _buffer(std::move(rhs._buffer))
    {
        rhs._sockFd = INVALID_SOCKET;
        rhs._cliAddrInfo = nullptr;
        rhs._selectedAddrInfo = nullptr;
    }

    /**
     * @brief Copy assignment operator (deleted).
     *
     * Socket objects cannot be copied because they represent unique system resources.
     * Each socket needs exclusive ownership of its underlying file descriptor and
     * associated resources. Use move semantics (operator=(Socket&&)) instead to
     * transfer ownership between Socket objects.
     *
     * @param rhs The Socket to copy from (unused since deleted)
     * @return Reference to this Socket (never returns since deleted)
     */
    Socket& operator=(const Socket& rhs) = delete; //-Weffc++

    /**
     * @brief Move assignment operator.
     *
     * Transfers ownership of socket resources from another Socket object.
     * Any existing socket is properly closed first.
     * The moved-from socket is left in a valid but empty state.
     *
     * @param rhs The Socket to move from
     * @return Reference to this Socket
     */
    Socket& operator=(Socket&& rhs) noexcept
    {
        if (this != &rhs)
        {
            if (this->_sockFd != INVALID_SOCKET)
            {
                if (CloseSocket(this->_sockFd))
                    std::cerr << "closesocket() failed: " << SocketErrorMessage(GetSocketError()) << ": "
                              << GetSocketError() << std::endl;
            }
            if (_cliAddrInfo)
            {
                freeaddrinfo(_cliAddrInfo);
                _cliAddrInfo = nullptr;
            }
            _sockFd = rhs._sockFd;
            _remoteAddr = rhs._remoteAddr;
            _remoteAddrLen = rhs._remoteAddrLen;
            _cliAddrInfo = rhs._cliAddrInfo;
            _selectedAddrInfo = rhs._selectedAddrInfo;
            _buffer = std::move(rhs._buffer);
            rhs._sockFd = INVALID_SOCKET;
            rhs._cliAddrInfo = nullptr;
            rhs._selectedAddrInfo = nullptr;
        }
        return *this;
    }

    /**
     * @brief Destructor. Closes the socket and frees resources.
     */
    ~Socket() noexcept;

    /**
     * @brief Get the remote socket's address as a string (ip:port).
     * @return String representation of the remote address.
     * @note Uses getnameinfo to convert sockaddr_storage to human-readable format.
     *       Handles both IPv4 and IPv6 addresses, including mapped IPv4 addresses.
     * @throws SocketException on error.
     */
    std::string getRemoteSocketAddress() const;

    /**
     * @brief Connects the socket to the remote host with optional timeout handling.
     *
     * This function attempts to establish a connection to a remote host. It behaves differently depending on whether
     * a timeout is specified. If the timeout is provided (i.e., `timeoutMillis >= 0`), the connection attempt is
     * non-blocking, and the function uses `select()` to monitor the socket for completion within the given timeout.
     * If no timeout is specified (i.e., `timeoutMillis < 0`), it attempts to connect in blocking mode.
     *
     * The connection process works as follows:
     * - In **non-blocking mode**, the `::connect()` call will return immediately. If the connection cannot be
     *   completed right away, it will return `EINPROGRESS` (or `WSAEINPROGRESS` on Windows) indicating that the
     *   connection is still in progress.
     * - **`select()`** is then used to wait for the socket to become writable, indicating that the connection
     *   has been successfully established.
     * - If the connection times out or fails to establish within the specified timeout, an exception is thrown.
     * - In **blocking mode**, `::connect()` will block the execution of the program until the connection is either
     *   established or fails.
     *
     * This function is cross-platform and works on both Windows and Linux systems.
     *
     * @param timeoutMillis The maximum time to wait for the connection to be established, in milliseconds.
     *                      If set to a negative value, the connection will be attempted in blocking mode (no timeout).
     *                      If set to a non-negative value, the connection will be attempted in non-blocking mode,
     *                      and `select()` will be used to wait for the connection to complete within the timeout.
     *
     * @throws SocketException If the connection fails, times out, or if there is an error during the connection
     * attempt. The exception will contain the error code and message describing the failure.
     */
    void connect(int timeoutMillis = -1) const;

    /**
     * @brief Read a trivially copyable type from the socket.
     * @tparam T Type to read (must be trivially copyable).
     * @return Value of type T read from the socket.
     * @throws SocketException on error or disconnect.
     */
    template <typename T> T read()
    {
        static_assert(std::is_trivially_copyable_v<T>, "Socket::read<T>() only supports trivially copyable types");
        T r;
        const auto len = recv(this->_sockFd, reinterpret_cast<char*>(&r),
#ifdef _WIN32
                              static_cast<int>(sizeof(T)),
#else
                              sizeof(T),
#endif
                              0);
        if (len == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
        if (len == 0)
            throw SocketException(0, "Connection closed by remote host.");
        return r;
    }

    /**
     * @brief Close the socket.
     * @throws SocketException on error.
     */
    void close();

    /**
     * @brief Shutdown the socket for reading, writing, or both.
     * @param how Specifies the shutdown type:
     *      - `ShutdownMode::Read` - Shutdown read operations (no more incoming data)
     *      - `ShutdownMode::Write` - Shutdown write operations (no more outgoing data)
     *      - `ShutdownMode::Both` - Shutdown both read and write operations (default)
     * @throws SocketException on error.
     */
    void shutdown(ShutdownMode how) const;

    /**
     * @brief Write a string to the socket.
     * @param message The string to send.
     * @return Number of bytes sent.
     * @throws SocketException on error.
     *
     * @note [[nodiscard]] is appropriate here because ignoring the return value (number of bytes sent)
     *       may lead to subtle bugs, such as assuming the entire message was sent when only part was.
     *       This encourages the caller to check how many bytes were actually written.
     */
    [[nodiscard]] size_t write(std::string_view message) const;

    /**
     * @brief Writes the entire contents of the given message to the socket.
     *
     * Attempts to send all bytes from the provided string view to the underlying socket.
     * This function will continue writing until either all data has been sent or an error occurs.
     *
     * @param message The message to be sent, represented as a std::string_view.
     * @return int The total number of bytes written on success, or a negative value if an error occurs.
     */
    size_t writeAll(std::string_view message) const;

    /**
     * @brief Set the internal buffer size.
     * @param newLen New buffer size in bytes.
     */
    void setBufferSize(std::size_t newLen);

    /**
     * @brief Check if the socket is valid (open).
     * @return true if valid, false otherwise.
     */
    [[nodiscard]] bool isValid() const { return this->_sockFd != INVALID_SOCKET; }

    /**
     * @brief Set the socket to non-blocking or blocking mode.
     *
     * Allows the socket to operate in non-blocking mode, where operations
     * such as connect, read, and write will return immediately if they
     * cannot be completed, or in blocking mode, where these operations
     * will wait until they can be completed.
     *
     * By default, sockets are created in blocking mode.
     *
     * @param nonBlocking true to set non-blocking mode, false for blocking mode.
     * @throws SocketException on error.
     */
    void setNonBlocking(bool nonBlocking) const;

    /**
     * @brief Set a timeout for socket operations.
     *
     * Sets the timeout for send and/or receive operations on the socket.
     * If forRead is true, sets the receive timeout (SO_RCVTIMEO).
     * If forWrite is true, sets the send timeout (SO_SNDTIMEO).
     * If both are true (default), sets both timeouts.
     *
     * @param millis Timeout in milliseconds.
     * @param forRead If true, set receive timeout (default: true).
     * @param forWrite If true, set send timeout (default: true).
     * @throws SocketException on error.
     */
    void setTimeout(int millis, bool forRead = true, bool forWrite = true) const;

    /**
     * @brief Wait for the socket to be ready for reading or writing.
     *
     * @param forWrite true to wait for write, false for read.
     * @param timeoutMillis Timeout in milliseconds.
     * @return true if ready, false if timeout.
     * @throws SocketException on error.
     */
    bool waitReady(bool forWrite, int timeoutMillis) const;

    /**
     * @brief Check if the socket is still connected (TCP only).
     *
     * @return true if connected, false otherwise.
     */
    bool isConnected() const;

    /**
     * @brief Enable or disable TCP_NODELAY (Nagle's algorithm) on the socket.
     *
     * When TCP_NODELAY is enabled (set to true), Nagle's algorithm is disabled. This means that small packets
     * of data are sent immediately over the network, without waiting to accumulate more data. This can reduce
     * latency for applications that require fast, interactive communication (such as games, real-time systems,
     * or protocols where low latency is more important than bandwidth efficiency).
     *
     * When TCP_NODELAY is disabled (set to false), Nagle's algorithm is enabled. This causes the socket to
     * buffer small outgoing packets and send them together, which can reduce network congestion and improve
     * throughput for bulk data transfers, but may introduce slight delays for small messages.
     *
     * By default, TCP_NODELAY is disabled (i.e., Nagle's algorithm is enabled) on new sockets.
     *
     * @param enable true to disable Nagle's algorithm (enable TCP_NODELAY, lower latency),
     *               false to enable Nagle's algorithm (disable TCP_NODELAY, higher throughput).
     */
    void enableNoDelay(bool enable) const;

    /**
     * @brief Enable or disable SO_KEEPALIVE on the socket.
     *
     * SO_KEEPALIVE is a socket option that enables periodic transmission of keepalive probes on an otherwise idle TCP
     * connection. When enabled (set to true), the operating system will periodically send keepalive messages to the
     * remote peer if no data has been exchanged for a certain period. If the peer does not respond, the connection is
     * considered broken and will be closed.
     *
     * This feature is useful for detecting dead peers or broken network links, especially for long-lived connections
     * where silent disconnects would otherwise go unnoticed. It is commonly used in server applications, remote control
     * systems, and protocols that require reliable detection of dropped connections.
     *
     * By default, SO_KEEPALIVE is disabled (i.e., keepalive probes are not sent) on new sockets.
     *
     * @param enable true to enable keepalive probes (SO_KEEPALIVE), false to disable (default).
     */
    void enableKeepAlive(bool enable) const;

    /**
     * @brief Convert an address and port to a string using getnameinfo.
     *
     * Uses getnameinfo to convert a sockaddr_storage structure to a human-readable string (ip:port).
     * Handles both IPv4 and IPv6 addresses.
     *
     * @param addr sockaddr_storage structure.
     * @return String representation (ip:port).
     * @throws SocketException if getnameinfo fails.
     */
    static std::string addressToString(const sockaddr_storage& addr);

    /**
     * @brief Convert a string (ip:port) to sockaddr_storage.
     *
     * @param str Address string.
     * @param addr Output sockaddr_storage.
     */
    static void stringToAddress(const std::string& str, sockaddr_storage& addr);

  private:
    SOCKET _sockFd = INVALID_SOCKET; ///< Underlying socket file descriptor.
    sockaddr_storage _remoteAddr;    ///< sockaddr_in for IPv4; sockaddr_in6 for IPv6; sockaddr_storage for both
    ///< (portability)
    mutable socklen_t _remoteAddrLen = 0;  ///< Length of remote address (for recvfrom/recvmsg)
    addrinfo* _cliAddrInfo = nullptr;      ///< Address info for connection (from getaddrinfo)
    addrinfo* _selectedAddrInfo = nullptr; ///< Selected address info for connection
    std::vector<char> _buffer;             ///< Internal buffer for read operations
};

/**
 * @brief Template specialization to read a string from the socket.
 *
 * Reads data from the socket into the internal buffer and returns it as a string.
 * Uses the socket's internal buffer size (set via setBufferSize) as the maximum read length.
 * The actual returned string length may be shorter depending on how much data was received.
 *
 * @return String containing the received data.
 * @throws SocketException If a socket error occurs or the connection is closed by the remote host.
 * @see setBufferSize() To modify the maximum read length
 */
template <> inline std::string Socket::read()
{
    const auto len = recv(_sockFd, _buffer.data(),
#ifdef _WIN32
                          static_cast<int>(_buffer.size()),
#else
                          _buffer.size(),
#endif
                          0);
    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (len == 0)
        throw SocketException(0, "Connection closed by remote host.");
    return {_buffer.data(), static_cast<size_t>(len)};
}

} // namespace libsocket
