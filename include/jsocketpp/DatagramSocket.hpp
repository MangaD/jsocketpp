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
     * @brief Constructs a UDP socket optionally bound to a local port.
     * @param port The local UDP port to bind to (0 means any available port).
     *            Common scenarios:
     *            - Server: Use specific port (e.g., 8888) that clients know
     *            - Client: Use 0 to get any available port
     * @param bufferSize Size of the receive buffer in bytes (default: 2048).
     *                   Choose based on your expected maximum datagram size:
     *                   - IPv4: Max 65,507 bytes
     *                   - IPv6: Max 65,527 bytes
     *                   - Typical values: 1024-8192 bytes
     * @throws SocketException if socket creation or binding fails
     */
    explicit DatagramSocket(Port port, std::size_t bufferSize = 2048);

    /**
     * @brief Construct a datagram socket to a remote host and port.
     * @param host Hostname or IP address to connect to.
     * @param port UDP port number.
     * @param bufferSize Size of the internal receive buffer (default: 2048).
     * @throws SocketException on failure.
     */
    DatagramSocket(std::string_view host, Port port, std::size_t bufferSize = 2048);

    /**
     * @brief Destructor. Closes the socket and frees resources.
     */
    ~DatagramSocket() noexcept override;

    /**
     * @brief Copy constructor (deleted).
     */
    DatagramSocket(const DatagramSocket&) = delete;

    /**
     * @brief Copy assignment operator (deleted).
     */
    DatagramSocket& operator=(const DatagramSocket&) = delete;

    /**
     * @brief Move constructor.
     *
     * Transfers ownership of the socket and resources from another DatagramSocket.
     * The moved-from object is left in a valid but unspecified state.
     */
    DatagramSocket(DatagramSocket&& rhs) noexcept
        : SocketOptions(rhs._sockFd), _sockFd(rhs._sockFd), _addrInfoPtr(rhs._addrInfoPtr),
          _selectedAddrInfo(rhs._selectedAddrInfo), _localAddr(rhs._localAddr), _localAddrLen(rhs._localAddrLen),
          _buffer(std::move(rhs._buffer)), _port(rhs._port)
    {
        rhs._sockFd = INVALID_SOCKET;
        rhs.setSocketFd(INVALID_SOCKET);
        rhs._addrInfoPtr = nullptr;
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
            close();
            _sockFd = rhs._sockFd;
            setSocketFd(_sockFd);
            _localAddr = rhs._localAddr;
            _localAddrLen = rhs._localAddrLen;
            _addrInfoPtr = rhs._addrInfoPtr;
            _selectedAddrInfo = rhs._selectedAddrInfo;
            _buffer = std::move(rhs._buffer);
            _port = rhs._port;

            rhs._sockFd = INVALID_SOCKET;
            rhs.setSocketFd(INVALID_SOCKET);
            rhs._addrInfoPtr = nullptr;
            rhs._selectedAddrInfo = nullptr;
            rhs._localAddrLen = 0;
            rhs._port = 0;
        }
        return *this;
    }

    /**
     * @brief Bind the datagram socket to the configured port set in the constructor.
     * @throws SocketException on error.
     */
    void bind() const;

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
        const int n = ::recv(_sockFd, reinterpret_cast<char*>(&value), sizeof(T), 0);

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
        const auto n = ::recvfrom(_sockFd, reinterpret_cast<char*>(&value), sizeof(T), 0,
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
    [[nodiscard]] bool isValid() const { return _sockFd != INVALID_SOCKET; }

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
    SOCKET _sockFd = INVALID_SOCKET;       ///< Underlying socket file descriptor.
    addrinfo* _addrInfoPtr = nullptr;      ///< Address info pointer for bind/connect.
    addrinfo* _selectedAddrInfo = nullptr; ///< Selected address info for connection

  private:
    sockaddr_storage _localAddr{};       ///< Local address structure.
    mutable socklen_t _localAddrLen = 0; ///< Length of local address.
    std::vector<char> _buffer;           ///< Internal buffer for read operations.
    Port _port;                          ///< Port number the socket is bound to (if applicable).
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
    const auto n = ::recv(_sockFd, _buffer.data(),
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
    const auto n = ::recvfrom(_sockFd, _buffer.data(),
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
