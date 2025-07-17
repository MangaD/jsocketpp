/**
 * @file Socket.hpp
 * @brief TCP client socket abstraction for jsocketpp.
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

#pragma once

#include "SocketException.hpp"
#include "common.hpp"

#include <array>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace jsocketpp
{

/**
 * @brief Represents a raw writable memory region for scatter/gather I/O.
 * @ingroup tcp
 *
 * This struct defines a mutable buffer segment for use with readv-style
 * scatter/gather operations. It holds a pointer to a writable memory region
 * and the number of bytes available at that address.
 */
struct BufferView
{
    void* data;       ///< Pointer to the writable memory region
    std::size_t size; ///< Size in bytes of the writable region
};

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
 * - The socket maintains an internal read buffer (default: 512 bytes).
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
 * - Tune socket options with `enableNoDelay()`, `enableKeepAlive()`, or `setTimeout()`.
 * - Check remote address with `getRemoteSocketAddress()`.
 *
 * ### See Also
 * - @ref ServerSocket "ServerSocket" - for listening to incoming connections
 * - @ref tcp - TCP socket group
 */
class Socket
{
    /**
     * @brief Grants ServerSocket access to private members.
     *
     * ServerSocket needs access to Socket's private members during accept() operations to:
     * - Initialize client Socket objects directly with the accepted socket descriptor
     * - Set address information for the connected client
     * - Configure internal buffers
     *
     * @see ServerSocket::accept() Creates new Socket instances from accepted connections
     * @see Socket(SOCKET,const sockaddr_storage&,socklen_t,std::size_t,std::size_t) Protected constructor used by
     * accept()
     */
    friend class ServerSocket;

  protected:
    /**
     * @brief Protected constructor used internally to create Socket objects for accepted client connections.
     * @ingroup tcp
     *
     * This constructor is specifically designed to be called by ServerSocket::accept() to create Socket
     * objects representing established client connections. It initializes a Socket with an already-connected
     * socket descriptor and the client's address information obtained from a successful accept() call.
     *
     * Usage flow:
     * 1. ServerSocket::accept() receives a new client connection
     * 2. This constructor creates a Socket object for that connection
     * 3. The Socket object takes ownership of the socket descriptor
     * 4. The new Socket is returned to the caller of accept()
     *
     * @param[in] client Socket descriptor obtained from a successful accept() call
     * @param[in] addr Remote peer's address information (sockaddr_storage for IPv4/IPv6 compatibility)
     * @param[in] len Size of the address structure (for sockaddr_in or sockaddr_in6)
     * @param[in] recvBufferSize Used to set the size of the socket's receive buffer (SO_RCVBUF)
     *                           and the internal buffer of this class used in read string operations
     * @param[in] sendBufferSize Used to set the size of the socket's send buffer (SO_SNDBUF)
     *
     * @throws SocketException If buffer allocation fails or socket options cannot be set
     *
     * @note This constructor is protected and only accessible to ServerSocket to ensure proper
     *       initialization of accepted client connections.
     *
     * @see ServerSocket::accept() Creates new Socket instances using this constructor
     */
    Socket(SOCKET client, const sockaddr_storage& addr, socklen_t len, std::size_t recvBufferSize,
           std::size_t sendBufferSize);

  public:
    /**
     * @brief Creates a new Socket object configured to connect to the specified host and port.
     * @ingroup tcp
     *
     * This constructor initializes a Socket object for a TCP connection to a remote host. It performs
     * hostname resolution using getaddrinfo() to support both IPv4 and IPv6 addresses, but does not
     * establish the connection. To establish the connection, call connect() after construction.
     *
     * The constructor performs the following steps:
     * 1. Resolves the hostname using getaddrinfo() (supports both DNS names and IP addresses)
     * 2. Creates a socket with the appropriate address family (AF_INET/AF_INET6)
     * 3. Initializes internal buffers and data structures
     *
     * @param[in] host The hostname or IP address (IPv4/IPv6) to connect to. Can be a DNS name
     *                 (e.g., "example.com") or an IP address (e.g., "192.168.1.1" or "::1").
     * @param[in] port The TCP port number to connect to (1-65535).
     * @param[in] bufferSize Size of the internal read buffer in bytes (default: 512). This buffer
     *                       is used for string-based read operations. Larger values may improve
     *                       performance for applications that receive large messages.
     *
     * @throws SocketException If initialization fails due to:
     *         - Invalid hostname or address
     *         - Memory allocation failure
     *         - System resource limits
     *         - Network subsystem errors
     *
     * @note This constructor only prepares the socket for connection. You must call connect()
     *       to establish the actual network connection.
     *
     * @see connect() To establish the connection after construction
     * @see setInternalBufferSize() To change the buffer size after construction
     */
    explicit Socket(std::string_view host, unsigned short port, std::size_t bufferSize = 512);

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
     * state with no active connection (_sockFd = INVALID_SOCKET).
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
        : _sockFd(rhs._sockFd), _remoteAddr(rhs._remoteAddr), _remoteAddrLen(rhs._remoteAddrLen),
          _cliAddrInfo(rhs._cliAddrInfo), _selectedAddrInfo(rhs._selectedAddrInfo),
          _recvBuffer(std::move(rhs._recvBuffer))
    {
        rhs._sockFd = INVALID_SOCKET;
        rhs._cliAddrInfo = nullptr;
        rhs._selectedAddrInfo = nullptr;
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
            _recvBuffer = std::move(rhs._recvBuffer);
            rhs._sockFd = INVALID_SOCKET;
            rhs._cliAddrInfo = nullptr;
            rhs._selectedAddrInfo = nullptr;
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
    ~Socket() noexcept;

    /**
     * @brief Get the remote peer's address and port as a formatted string.
     * @ingroup tcp
     *
     * Converts the remote peer's address information (stored in sockaddr_storage)
     * to a human-readable string in the format "address:port". This method supports
     * both IPv4 and IPv6 addresses and handles special cases like IPv4-mapped IPv6
     * addresses.
     *
     * The conversion process:
     * 1. Uses getnameinfo() to convert the binary address to text
     * 2. Formats IPv4 addresses as "x.x.x.x:port"
     * 3. Formats IPv6 addresses as "[xxxx:xxxx::xxxx]:port"
     *
     * Example outputs:
     * - IPv4: "192.168.1.1:80"
     * - IPv6: "[2001:db8::1]:80"
     * - IPv4-mapped IPv6: "192.168.1.1:80"
     *
     * @return A string containing the formatted address and port
     * @throws SocketException If address conversion fails or the socket is invalid
     *
     * @see addressToString() Static utility method for general address conversion
     * @see stringToAddress() Convert string address back to binary form
     */
    std::string getRemoteSocketAddress() const;

    /**
     * @brief Establishes a TCP connection to the remote host with optional timeout control.
     * @ingroup tcp
     *
     * Attempts to establish a TCP connection to the remote host specified during Socket construction.
     * The connection can be attempted in either blocking or non-blocking mode, depending on the
     * timeout parameter.
     *
     * ### Connection Modes
     * - **Blocking Mode** (timeoutMillis < 0):
     *   - Blocks until connection is established or fails
     *   - Traditional synchronous behavior
     *   - Suitable for simple clients
     *
     * - **Non-blocking Mode with Timeout** (timeoutMillis >= 0):
     *   - Uses select() to monitor connection progress
     *   - Returns or throws exception after timeout
     *   - Prevents indefinite blocking
     *   - Ideal for responsive applications
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     *
     * // Blocking connect (will wait indefinitely)
     * sock.connect();
     *
     * // Connect with 5-second timeout
     * try {
     *     sock.connect(5000);
     * } catch (const SocketException& ex) {
     *     if (ex.getErrorCode() == ETIMEDOUT) {
     *         std::cerr << "Connection timed out\n";
     *     }
     * }
     * @endcode
     *
     * ### Implementation Details
     * 1. In non-blocking mode:
     *    - Sets socket to non-blocking
     *    - Initiates connection (returns EINPROGRESS/WSAEINPROGRESS)
     *    - Uses select() to wait for completion
     *    - Verifies connection success
     *    - Restores blocking mode if needed
     *
     * 2. In blocking mode:
     *    - Performs traditional blocking connect()
     *    - Returns when connected or throws on error
     *
     * @param[in] timeoutMillis Connection timeout in milliseconds:
     *                          - Negative: blocking mode (no timeout)
     *                          - Zero or positive: maximum wait time
     *
     * @throws SocketException In cases of:
     *         - Connection timeout
     *         - Connection refused
     *         - Network unreachable
     *         - Invalid address
     *         - Permission denied
     *         - Other system-specific errors
     *
     * @note This method is not thread-safe. Do not call connect() on the same Socket
     *       instance from multiple threads simultaneously.
     *
     * @see isConnected() Check if connection is established
     * @see close() Close the connection
     * @see setNonBlocking() Change blocking mode
     * @see waitReady() Low-level connection monitoring
     */
    void connect(int timeoutMillis = -1) const;

    /**
     * @brief Read a trivially copyable type from the socket.
     * @ingroup tcp
     *
     * Reads exactly sizeof(T) bytes from the socket and interprets them as type T.
     * This method is designed for reading binary data structures or primitive types
     * that can be safely copied byte-by-byte (trivially copyable types).
     *
     * ### Usage Example
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * try {
     *     // Read primitive types
     *     int number = sock.read<int>();
     *     double value = sock.read<double>();
     *
     *     // Read a custom struct
     *     struct Message {
     *         uint32_t id;
     *         char data[64];
     *     };
     *     Message msg = sock.read<Message>();
     * } catch (const SocketException& ex) {
     *     std::cerr << "Read failed: " << ex.what() << std::endl;
     * }
     * @endcode
     *
     * ### Implementation Details
     * - Uses recv() to read exactly sizeof(T) bytes
     * - Performs no endianness conversion
     * - Handles partial reads internally by continuing until all data is received
     * - Uses temporary buffer to ensure memory safety
     *
     * ### Supported Types
     * - Built-in types (int, float, double, etc.)
     * - POD structures and classes
     * - Arrays of trivially copyable types
     * - Custom types that satisfy std::is_trivially_copyable
     * - Fixed-size containers of trivially copyable types
     *
     * ### Thread Safety
     * - Not thread-safe: concurrent calls on the same Socket object will cause undefined behavior
     * - Multiple threads should use separate Socket instances
     * - Internal buffer access is not synchronized
     *
     * @tparam T Type to read. Must satisfy std::is_trivially_copyable
     *           to ensure safe binary copying.
     *
     * @returns A value of type T containing the read data. The returned value
     *          is a complete, properly constructed object of type T containing
     *          all bytes read from the socket.
     *
     * @throws SocketException If:
     *         - Connection is closed by peer (errno = ECONNRESET)
     *         - Network error occurs (various platform-specific errno values)
     *         - Read operation fails (errno = EWOULDBLOCK, EAGAIN)
     *         - Memory allocation for internal buffer fails (std::bad_alloc)
     *         - Socket is invalid or not connected (errno = EBADF)
     *         - Timeout occurs during read (errno = ETIMEDOUT)
     *
     * @warning Memory Safety:
     *         - Ensure T's size matches the expected network message size
     *         - Buffer overflows can occur if sender transmits more data than sizeof(T)
     *         - Virtual functions and pointers in T will be meaningless after deserialization
     *         - Avoid types with custom allocators or complex memory management
     *
     * @warning Platform Considerations:
     *         - Byte ordering differences between platforms must be handled by caller
     *         - Structure padding may differ between platforms
     *         - Windows systems may handle partial reads differently
     *         - Alignment requirements may vary across architectures
     *
     * @note The caller must ensure proper byte ordering (endianness) when reading
     *       numeric types across different architectures.
     *
     * @note Internal buffer management:
     *       - A temporary buffer of sizeof(T) bytes is allocated for each read
     *       - Buffer is automatically freed when read completes
     *       - No persistent memory is allocated
     *
     * @see write() For writing data to the socket
     * @see setInternalBufferSize() To modify internal buffer size for string reads
     * @see read<std::string>() For reading string data
     * @see isConnected() To check connection status before reading
     * @see setSoTimeout() To set read timeout
     */
    template <typename T> T read()
    {
        static_assert(std::is_trivially_copyable_v<T>, "Socket::read<T>() only supports trivially copyable types");
        T r;
        std::array<char, sizeof(T)> buffer;
        size_t totalRead = 0;
        size_t remaining = sizeof(T);

        while (remaining > 0)
        {
            const auto len = recv(this->_sockFd, buffer.data() + totalRead,
#ifdef _WIN32
                                  static_cast<int>(remaining),
#else
                                  remaining,
#endif
                                  0);
            if (len == SOCKET_ERROR)
                throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
            if (len == 0)
                throw SocketException(0, "Connection closed by remote host.");

            totalRead += len;
            remaining -= len;
        }
        std::memcpy(&r, buffer.data(), sizeof(T));
        return r;
    }

    /**
     * @brief Read exactly `n` bytes from the socket into a string.
     * @ingroup tcp
     *
     * This method performs a blocking read operation that guarantees to read exactly
     * the requested number of bytes, unless an error occurs. It will continue reading
     * from the socket until all requested bytes are received, handling partial reads
     * transparently.
     *
     * ### Implementation Details
     * - Uses recv() internally for actual socket operations
     * - Handles partial reads by continuing until all data is received
     * - Allocates a buffer of exact size needed
     * - Maintains original byte order and content
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * try {
     *     // Read exactly 1024 bytes
     *     std::string data = sock.readExact(1024);
     *     assert(data.length() == 1024);
     * } catch (const SocketException& ex) {
     *     // Handle read errors
     * }
     * @endcode
     *
     * @param[in] n Number of bytes to read. Must be greater than 0.
     *
     * @return std::string containing exactly `n` bytes of received data.
     *         The returned string's length will always equal `n` on success.
     *
     * @throws SocketException If:
     *         - Connection is closed by peer (errno = ECONNRESET)
     *         - Network error occurs (various platform-specific errno values)
     *         - Read timeout occurs (if configured) (errno = ETIMEDOUT)
     *         - Memory allocation fails (std::bad_alloc)
     *         - Socket is invalid or not connected (errno = EBADF)
     *
     * @note This method blocks until all requested bytes are received or an error occurs.
     *       For non-blocking behavior, use read<std::string>() instead.
     *
     * @see read<std::string>() For single-read best-effort operations
     * @see read<T>() For reading fixed-size binary types
     * @see setSoTimeout() To set read timeout
     * @see setNonBlocking() To control blocking behavior
     */
    std::string readExact(std::size_t n) const;

    /**
     * @brief Reads data from the socket until a specified delimiter character.
     * @ingroup tcp
     *
     * Reads data from the socket until the specified delimiter character is encountered.
     * The method accumulates data efficiently, handling partial reads and buffer resizing
     * as needed. The returned string optionally includes the delimiter character based on
     * the includeDelimiter parameter.
     *
     * ### Implementation Details
     * - Uses internal buffer for efficient reading
     * - Handles partial reads and buffer resizing
     * - Supports arbitrary delimiter characters
     * - Enforces maximum length limit
     * - Optionally includes delimiter in return value
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Read until newline character
     * std::string line = sock.readUntil('\n');
     *
     * // Read with custom delimiter and length limit
     * std::string csv = sock.readUntil(',', 1024);
     *
     * // Read without including the delimiter
     * std::string data = sock.readUntil('\n', 8192, false);
     * @endcode
     *
     * @param[in] delimiter Character that marks the end of data
     * @param[in] maxLen Maximum allowed data length in bytes (default: 8192)
     * @param[in] includeDelimiter Whether to include delimiter in returned string (default: true)
     *
     * @return std::string containing the data up to the delimiter, optionally including the delimiter
     *
     * @throws SocketException If:
     *         - Maximum length is exceeded
     *         - Connection is closed
     *         - Read timeout occurs
     *         - Socket error occurs
     *         - Memory allocation fails
     *
     * @see readLine() For reading newline-terminated data
     * @see read() For general-purpose reading
     * @see readExact() For fixed-length reads
     * @see setSoTimeout() To control read timeout
     */
    std::string readUntil(char delimiter, std::size_t maxLen = 8192, bool includeDelimiter = true) const;

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
     * @param maxLen Maximum number of bytes to read (default: 8192)
     * @param includeDelimiter Whether to include the newline in the returned string (default: true)
     * @return std::string containing the line read
     * @throws SocketException on error or if line exceeds maxLen
     *
     * @see readUntil() The underlying implementation
     * @see read() For general-purpose reading
     * @see readExact() For fixed-length reads
     */
    std::string readLine(const std::size_t maxLen = 8192, const bool includeDelimiter = true) const
    {
        return readUntil('\n', maxLen, includeDelimiter);
    }

    /**
     * @brief Reads up to `n` bytes from the socket into a string.
     * @ingroup tcp
     *
     * Performs a single read operation from the socket, returning immediately with whatever
     * data is available, up to the specified maximum number of bytes. This method provides
     * a "best-effort" read that won't block waiting for more data once some data is available.
     *
     * ### Implementation Details
     * - Uses a single recv() call
     * - Returns immediately when any data is available
     * - Does not wait for full buffer
     * - Handles partial reads gracefully
     * - Thread-safe with respect to other Socket instances
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Read up to 1024 bytes
     * std::string data = sock.readAtMost(1024);
     * std::cout << "Received " << data.length() << " bytes\n";
     *
     * // Use with non-blocking socket
     * sock.setNonBlocking(true);
     * try {
     *     std::string partial = sock.readAtMost(512);
     *     // Process whatever data is available
     * } catch (const SocketException& ex) {
     *     if (ex.getErrorCode() == EWOULDBLOCK) {
     *         // No data currently available
     *     }
     * }
     * @endcode
     *
     * @param[in] n Maximum number of bytes to read in a single operation.
     *              The actual number of bytes read may be less than this value.
     *
     * @return std::string containing between 0 and n bytes of received data.
     *         An empty string may indicate that no data was available.
     *
     * @throws SocketException If:
     *         - Socket is invalid (EBADF)
     *         - Connection is closed by peer (0)
     *         - Network error occurs (platform-specific)
     *         - Memory allocation fails (std::bad_alloc)
     *
     * @note This method differs from readExact() in that it won't attempt to
     *       collect exactly n bytes. It returns as soon as any data is available.
     *
     * @see readExact() For reading exact number of bytes
     * @see read<std::string>() For reading with internal buffer
     * @see setNonBlocking() To control blocking behavior
     * @see setSoTimeout() To set read timeout
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
     * @brief Performs a best-effort read up to `n` bytes, with a maximum timeout.
     * @ingroup tcp
     *
     * Waits up to `timeoutMillis` milliseconds for the socket to become readable,
     * then performs a single `recv()` call for at most `n` bytes.
     *
     * Attempts to read up to `n` bytes from the socket, returning early if:
     * - The requested number of bytes are received
     * - The timeout period expires
     * - The connection is closed by the peer
     * - An error occurs
     *
     * Unlike readExact(), this method returns as soon as any data is available,
     * without waiting for the full requested length. The timeout applies to the
     * initial wait for data availability.
     *
     * ### Implementation Details
     * - Uses waitReady() to implement timeout
     * - Performs single recv() call when data available
     * - Returns immediately with available data
     * - Does not retry or accumulate partial reads
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * try {
     *     // Try to read up to 1024 bytes, waiting max 5 seconds
     *     std::string data = sock.readAtMostWithTimeout(1024, 5000);
     *     std::cout << "Read " << data.length() << " bytes\n";
     * } catch (const SocketException& ex) {
     *     if (ex.getErrorCode() == ETIMEDOUT) {
     *         // No data available within timeout
     *     }
     * }
     * @endcode
     *
     * @param[in] n Maximum number of bytes to read
     * @param[in] timeoutMillis Maximum time to wait for data, in milliseconds:
     *                         - > 0: Maximum wait time
     *                         - 0: Non-blocking check
     *                         - < 0: Invalid (throws exception)
     *
     * @return String containing between 0 and n bytes of received data
     *
     * @throws SocketException If:
     *         - Socket is invalid
     *         - Timeout period expires before any data arrives
     *         - Connection is closed by peer
     *         - Memory allocation fails
     *         - Other network errors occur
     *
     * @see readExact() For reading exact number of bytes
     * @see readAtMost() For immediate best-effort read
     * @see waitReady() For lower-level timeout control
     * @see setSoTimeout() For setting default timeouts
     */
    std::string readAtMostWithTimeout(std::size_t n, int timeoutMillis) const;

    /**
     * @brief Reads a length-prefixed payload using a fixed-size prefix type.
     * @ingroup tcp
     *
     * Reads a message that consists of a length prefix followed by a variable-length payload.
     * The prefix type T determines the format and size of the length field. This method is
     * useful for protocols that encode message length as a fixed-size integer header.
     *
     * ### Implementation Details
     * - First reads sizeof(T) bytes as the length prefix
     * - Then reads exactly that many bytes as the payload
     * - Length prefix must be an integral type (uint8_t, uint32_t, etc.)
     * - No endianness conversion is performed
     * - Handles message sizes up to size_t capacity
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
     * @tparam T The integral type used for the length prefix (e.g., uint32_t).
     *           Must be a trivially copyable integral type.
     *
     * @return The payload as a string, excluding the length prefix.
     *
     * @throws SocketException If:
     *         - Connection is closed while reading
     *         - Length prefix is invalid/corrupt
     *         - Not enough data available
     *         - Memory allocation fails
     *         - Other network errors occur
     *
     * @note The caller must ensure that the prefix type T matches the protocol's
     *       length field size and format. No validation of the length value is
     *       performed beyond available memory constraints.
     *
     * @see read() For reading fixed-size types
     * @see readExact() For reading exact number of bytes
     * @see write() For writing data to socket
     */
    template <typename T> std::string readPrefixed()
    {
        static_assert(std::is_integral_v<T> && std::is_trivially_copyable_v<T>,
                      "Prefix type must be a trivially copyable integral type");

        T length = read<T>(); // already implemented method
        return readExact(static_cast<std::size_t>(length));
    }

    /**
     * @brief Reads a length-prefixed message with an upper bound check.
     * @ingroup tcp
     *
     * This method reads a message consisting of a length prefix followed by a payload, ensuring the payload
     * length does not exceed a specified maximum. The prefix type T determines the format and size of the length field.
     * This is useful for protocols that encode message length as a fixed-size integer header and require validation
     * against corrupted length values.
     *
     * ### Implementation Details
     * - Reads sizeof(T) bytes as the length prefix
     * - Throws exception if length exceeds maxPayloadLen
     * - Reads exactly the specified number of bytes as the payload
     * - Length prefix must be an integral type (e.g., uint8_t, uint32_t)
     * - No endianness conversion is performed
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * try {
     *     std::string msg = sock.readPrefixed<uint32_t>(1024); // Max length 1024
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
     * @tparam T The integral type used for the length prefix (e.g., uint32_t).
     *           Must be a trivially copyable integral type.
     * @param [in] maxPayloadLen Maximum allowed payload length for validation
     * @return The payload as a string, excluding the length prefix.
     *
     * @throws SocketException If:
     *         - Connection is closed while reading
     *         - Length prefix is invalid/corrupt
     *         - Length exceeds maxPayloadLen
     *         - Not enough data available
     *         - Memory allocation fails
     *         - Other network errors occur
     *
     * @note Ensure the prefix type T matches the protocol's length field size and format.
     *       No validation of the length value is performed beyond available memory constraints.
     *
     * @see read() For reading fixed-size types
     * @see readExact() For reading exact number of bytes
     * @see write() For writing data to socket
     */
    template <typename T> std::string readPrefixed(const std::size_t maxPayloadLen)
    {
        static_assert(std::is_integral_v<T> && std::is_trivially_copyable_v<T>,
                      "Prefix type must be a trivially copyable integral type");

        T length = read<T>(); // uses existing fixed-size read

        const auto payloadLen = static_cast<std::size_t>(length);
        if (payloadLen > maxPayloadLen)
        {
            throw SocketException(0, "readPrefixed: Prefix length " + std::to_string(payloadLen) +
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
     * @param buffer Pointer to the memory where received data will be stored.
     * @param bufferSize Maximum number of bytes to store in the buffer.
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
     * @param buffers A span of BufferView objects describing writable regions.
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
     * @param buffers A span of writable buffer views to fill completely.
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
     * This method repeatedly performs vectorized reads into the given sequence
     * of buffers until all bytes are read or the timeout expires. It uses a
     * steady clock to track the total time spent across all retries.
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
     * @param buffers Span of BufferView objects describing writable regions.
     * @param timeoutMillis Maximum allowed time to read all bytes (in milliseconds).
     * @return Total number of bytes read (equal to sum of buffer sizes on success).
     *
     * @throws SocketException If:
     *         - The timeout expires before all data is read
     *         - recv() or readv() fails
     *         - Connection is closed prematurely
     *
     * @note This method is fully blocking but timeout-bounded.
     *       Use `readv()` or `readvAll()` for simpler variants.
     *
     * @see readvAll() For unbounded retry version
     * @see readv() For best-effort single-attempt vectorized read
     */
    std::size_t readvAllWithTotalTimeout(std::span<BufferView> buffers, int timeoutMillis) const;

    /**
     * @brief Attempts a single vectorized read into multiple buffers with a timeout.
     * @ingroup tcp
     *
     * Waits up to `timeoutMillis` for the socket to become readable, then performs
     * a single `readv()` operation. May read less than the total buffer size.
     * This is the timeout-aware version of `readv()`, useful for polling or best-effort I/O.
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
     * std::size_t n = sock.readvAtMostWithTimeout(bufs, 300); // wait up to 300 ms
     * @endcode
     *
     * @param buffers Writable buffer views to receive incoming data.
     * @param timeoutMillis Time to wait for readability before giving up.
     * @return Number of bytes read (can be 0).
     *
     * @throws SocketException If:
     *         - The socket is not readable within timeout
     *         - recv() or readv() fails
     *         - Connection is closed
     *
     * @note This performs exactly one system read. It does not retry.
     *
     * @see readv() For non-timed scatter read
     * @see readvAllWithTotalTimeout() For full delivery
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
     * @param n Maximum number of bytes to peek at.
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
     * @brief Discards `n` bytes of data from the socket's input stream.
     * @ingroup tcp
     *
     * This method reads and silently drops exactly `n` bytes from the socket's input buffer.
     * It is useful when you need to skip over known headers, padding, or unwanted segments
     * in a protocol without storing them.
     *
     * ### Implementation Details
     * - Loops until `n` bytes are discarded
     * - Uses a fixed-size internal buffer (e.g., 1024 bytes)
     * - Performs one or more `recv()` calls to drain the data
     * - Does not store or return any of the read bytes
     *
     * ### Example Usage
     * @code{.cpp}
     * // Discard a 16-byte padding block
     * sock.discard(16);
     *
     * // Skip header and read payload
     * sock.discard(12);
     * std::string body = sock.readExact(64);
     * @endcode
     *
     * @param n Number of bytes to discard.
     *
     * @throws SocketException If:
     *         - Connection is closed before all bytes are discarded
     *         - recv() fails
     *         - Socket is invalid
     *
     * @note This method always reads and blocks until `n` bytes are discarded.
     *       For non-blocking skip logic, use peek() and manual reads.
     *
     * @see peek() To inspect data before discarding
     * @see readExact() If you want to retain skipped bytes
     * @see readAvailable() For draining the socket
     */
    void discard(std::size_t n) const;

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
     * - **ShutdownMode::Both**: Disables both send and receive operations
     *   - Equivalent to calling shutdown() with Read and Write modes
     *   - Most similar to close() but keeps socket descriptor valid
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
     * @brief Writes data to the socket, handling partial writes.
     * @ingroup tcp
     *
     * Attempts to write the provided string data to the connected socket. This method
     * may perform a partial write, meaning not all data may be sent in a single call.
     * The return value indicates exactly how many bytes were successfully written.
     *
     * ### Implementation Details
     * - Uses send() system call internally
     * - Handles platform differences (Windows/POSIX)
     * - May write fewer bytes than requested
     * - Does not retry on partial writes
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     * sock.connect();
     *
     * std::string msg = "Hello, server!";
     * size_t sent = sock.write(msg);
     * if (sent < msg.length()) {
     *     // Handle partial write
     *     std::cout << "Only sent " << sent << " of " << msg.length() << " bytes\n";
     * }
     * @endcode
     *
     * @param[in] message The string data to send. Can contain binary data (including null bytes).
     *
     * @return The number of bytes actually written to the socket.
     *         May be less than message.length() for partial writes.
     *
     * @throws SocketException If:
     *         - Socket is not connected (ENOTCONN)
     *         - Network error occurs (ENETDOWN, EPIPE)
     *         - Buffer is full and socket is blocking (EWOULDBLOCK)
     *         - Other system-specific errors
     *
     * @note [[nodiscard]] is used because ignoring the return value may lead
     *       to data loss if only part of the message was sent. Always check
     *       the return value against message.length().
     *
     * @see writeAll() To ensure complete message transmission
     * @see read() For receiving data
     * @see setSoTimeout() To set write timeout
     * @see setNonBlocking() To control blocking behavior
     */
    [[nodiscard]] size_t write(std::string_view message) const;

    /**
     * @brief Writes the entire contents of a message to the socket, handling partial writes.
     * @ingroup tcp
     *
     * Ensures that the complete message is transmitted by handling partial writes and retrying
     * until all data is sent or an error occurs. This method is particularly useful when
     * dealing with large messages that may require multiple write operations.
     *
     * ### Implementation Details
     * - Uses write() internally for actual socket operations
     * - Tracks remaining data and offset for partial writes
     * - Continues writing until all data is sent
     * - Handles platform differences (Windows/POSIX)
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     * sock.connect();
     *
     * std::string msg = "Hello, server!";
     * try {
     *     size_t sent = sock.writeAll(msg);
     *     std::cout << "Successfully sent all " << sent << " bytes\n";
     * } catch (const SocketException& ex) {
     *     std::cerr << "Failed to send complete message: " << ex.what() << '\n';
     * }
     * @endcode
     *
     * @param[in] message The data to send. Can contain binary data (including null bytes).
     *
     * @return The total number of bytes written (always equal to message.length() on success)
     *
     * @throws SocketException If:
     *         - Socket is not connected (ENOTCONN)
     *         - Network error occurs (ENETDOWN, EPIPE)
     *         - Connection is lost during transmission
     *         - Other system-specific errors
     *
     * @note This method will block until either:
     *       - All data is successfully written
     *       - An error occurs
     *       - The socket timeout is reached (if configured)
     *
     * @see write() For single-attempt writes that may be partial
     * @see setSoTimeout() To set write timeout
     * @see setNonBlocking() To control blocking behavior
     * @see isConnected() To check connection status before writing
     */
    size_t writeAll(std::string_view message) const;

    /**
     * @brief Writes a payload prefixed with its length using a fixed-size integer type.
     * @ingroup tcp
     *
     * Sends a length-prefixed message where the prefix is a fixed-size integral type `T`,
     * followed by the raw payload data. This is useful for protocols that encode the message
     * size before transmitting variable-length payloads, allowing the receiver to determine
     * message boundaries.
     *
     * ### Implementation Details
     * - First converts `payload.size()` into a fixed-size integer of type `T`
     * - Then sends the binary-encoded prefix using `writeAll()`
     * - Finally writes the raw contents of the `payload` string
     * - Validates that the payload fits within the range of type `T`
     * - Uses `writeAll()` to ensure both prefix and payload are fully transmitted
     * - No endianness conversion is applied (caller must convert if needed)
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * std::string msg = "Hello from client!";
     * sock.writePrefixed<uint32_t>(msg); // Sends [4-byte length][data]
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
     * @tparam T The integral type used to represent the length prefix.
     *           Must be trivially copyable (e.g., uint16_t, uint32_t).
     *
     * @param payload The message content to send after the length prefix.
     *
     * @return Total number of bytes written to the socket: sizeof(T) + payload.size().
     *
     * @throws SocketException If:
     *         - The payload size exceeds what can be encoded in T
     *         - Any call to writeAll() fails
     *         - Connection is lost or socket error occurs
     *
     * @note It is the caller’s responsibility to use the same type `T` on the receiving side.
     *       Consider applying endianness conversion if cross-platform compatibility is required.
     *
     * @see readPrefixed() To receive the corresponding length-prefixed message
     * @see writeAll() For writing full data to the socket
     * @see write() For partial/low-level sending
     */
    template <typename T> std::size_t writePrefixed(const std::string& payload) const
    {
        static_assert(std::is_integral_v<T> && std::is_trivially_copyable_v<T>,
                      "Prefix type must be a trivially copyable integral type");

        const std::size_t payloadSize = payload.size();

        // Put parentheses around the `max` function name due to a collision with
        // a macro defined in windows.h. See:
        // https://stackoverflow.com/a/13420838/3049315
        // Another alternative would be to define `NOMINMAX` before including windows.h
        // such as in CMake when compiling the target library
        if (payloadSize > static_cast<std::size_t>((std::numeric_limits<T>::max)()))
        {
            throw SocketException(0, "writePrefixed: Payload size " + std::to_string(payloadSize) +
                                         " exceeds maximum encodable size for prefix type");
        }

        T len = static_cast<T>(payloadSize);

        // Write the length prefix
        const std::size_t prefixSent = writeAll(std::string_view(reinterpret_cast<const char*>(&len), sizeof(T)));

        // Then write the payload
        const std::size_t dataSent = writeAll(payload);

        return prefixSent + dataSent;
    }

    /**
     * @brief Writes a binary payload prefixed with its length using a fixed-size integer type.
     * @ingroup tcp
     *
     * Sends a length-prefixed binary message, where the prefix is a fixed-size integral type `T`,
     * followed by a raw data buffer. This version avoids allocating a temporary `std::string`,
     * making it suitable for binary protocols, zero-copy send paths, or memory-efficient I/O.
     *
     * ### Implementation Details
     * - Validates that `len` can be safely cast to `T`
     * - Sends `sizeof(T)` bytes of the length prefix (no endianness conversion)
     * - Sends `len` bytes of payload immediately afterward
     * - Uses `writeAll()` to guarantee full delivery of both parts
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
     * @tparam T The integral type used for the length prefix (e.g., uint16_t, uint32_t).
     *           Must be trivially copyable.
     *
     * @param data Pointer to the binary payload data.
     * @param len Number of bytes to write from `data`.
     *
     * @return Total number of bytes written (sizeof(T) + len).
     *
     * @throws SocketException If:
     *         - `len` exceeds what can be encoded in `T`
     *         - Connection is closed before completion
     *         - writeAll() fails for prefix or payload
     *
     * @note No endianness conversion is performed.
     *       If network-order is required, the caller must handle it.
     *
     * @see readPrefixed() For corresponding read
     * @see writeAll() For guaranteed single-buffer write
     * @see writePrefixed(std::string) For string-based variant
     */
    template <typename T> std::size_t writePrefixed(const void* data, std::size_t len) const
    {
        static_assert(std::is_integral_v<T> && std::is_trivially_copyable_v<T>,
                      "Prefix type must be a trivially copyable integral type");

        if (!data && len > 0)
            throw SocketException(0, "writePrefixed: Null data pointer with non-zero length");

        if (len > static_cast<std::size_t>((std::numeric_limits<T>::max)()))
        {
            throw SocketException(0, "writePrefixed: Payload length exceeds capacity of prefix type");
        }

        T prefix = static_cast<T>(len);

        // Send prefix
        writeAll(std::string_view(reinterpret_cast<const char*>(&prefix), sizeof(T)));

        // Send payload
        if (len > 0)
        {
            writeAll(std::string_view(static_cast<const char*>(data), len));
        }

        return sizeof(T) + len;
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
     * @param buffers A span of string views to send as a scatter/gather I/O batch.
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
     * @param buffers A span of string fragments to send as a contiguous logical payload.
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
     * Waits up to `timeoutMillis` for the socket to become writable, then attempts
     * a single `send()` of up to `data.size()` bytes. This method does not retry.
     * It is suitable for time-sensitive or polling-based write loops.
     *
     * ### Implementation Details
     * - Uses `waitReady(true, timeoutMillis)` once
     * - Calls `send()` once to write as much as possible
     * - Returns the number of bytes actually written
     *
     * ### Example Usage
     * @code{.cpp}
     * std::string payload = "POST /data";
     * std::size_t sent = sock.writeAtMostWithTimeout(payload, 500);
     * @endcode
     *
     * @param data The data to send.
     * @param timeoutMillis The maximum time to wait before sending.
     *
     * @return The number of bytes written (can be 0).
     *
     * @throws SocketException If:
     *         - Timeout occurs before socket becomes writable
     *         - send() fails
     *         - Connection is closed
     *
     * @note This is a best-effort, low-latency write. Use `writeAll()` or
     *       `writeWithTimeoutRetry()` for full delivery.
     *
     * @see readAtMostWithTimeout()
     * @see writeAll()
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
     * @param data Pointer to the memory to write.
     * @param len Number of bytes to send.
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
     * @param data Pointer to the binary buffer to send.
     * @param len Number of bytes to transmit.
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
     * Repeatedly attempts to write all bytes from `data`, retrying partial writes as needed.
     * The entire operation must complete within `timeoutMillis` milliseconds. If the timeout
     * expires before all data is sent, the method throws a SocketException.
     *
     * ### Implementation Details
     * - Uses a `std::chrono::steady_clock` deadline internally
     * - Each iteration waits for socket writability using remaining time
     * - Behaves like `writeAll()` but bounded by a total wall-clock timeout
     *
     * ### Example Usage
     * @code{.cpp}
     * std::string json = buildPayload();
     * sock.writeWithTotalTimeout(json, 1000); // Must finish in 1 second
     * @endcode
     *
     * @param data The data to send.
     * @param timeoutMillis Maximum total duration (in milliseconds) to complete the operation.
     * @return Total number of bytes written (equals data.size() on success).
     *
     * @throws SocketException If:
     *         - The timeout expires before full delivery
     *         - A network or socket error occurs
     *         - Connection is closed during send
     *
     * @note This is the timeout-aware counterpart to `writeAll()`.
     *       Prefer this when responsiveness or time-bound delivery is required.
     *
     * @see writeAll() For unbounded full delivery
     * @see writeAtMostWithTimeout() For best-effort single attempt
     */
    std::size_t writeWithTotalTimeout(std::string_view data, int timeoutMillis) const;

    /**
     * @brief Writes all buffers fully within a total timeout using vectorized I/O.
     * @ingroup tcp
     *
     * Sends the contents of all buffers in the given span using scatter/gather I/O.
     * Retries on partial sends until either all buffers are sent or the timeout expires.
     * This is the timeout-aware counterpart to `writevAll()`.
     *
     * ### Implementation Details
     * - Uses a `std::chrono::steady_clock` deadline internally
     * - Each iteration waits for socket writability using remaining time
     * - Uses `writev()` to transmit multiple buffers at once
     * - Rebuilds the span as needed to resume after partial sends
     *
     * ### Example Usage
     * @code{.cpp}
     * std::array<std::string_view, 3> parts = {
     *     "Header: ",
     *     "value\r\n\r\n",
     *     "Body content"
     * };
     * sock.writevWithTotalTimeout(parts, 1500);
     * @endcode
     *
     * @param buffers A span of buffers to send in order.
     * @param timeoutMillis Maximum total time allowed for the operation, in milliseconds.
     * @return Total number of bytes sent (equal to the sum of buffer sizes on success).
     *
     * @throws SocketException If:
     *         - The timeout expires before full delivery
     *         - A network or socket error occurs
     *         - The connection is closed prematurely
     *
     * @note This method guarantees atomic full delivery within a strict time budget.
     *       Use `writevAll()` if timeout control is not needed.
     *
     * @see writevAll() For unbounded full delivery
     * @see writeWithTotalTimeout() For single-buffer variant
     */
    std::size_t writevWithTotalTimeout(std::span<const std::string_view> buffers, int timeoutMillis) const;

    /**
     * @brief Sets the socket's receive buffer size (SO_RCVBUF).
     * @ingroup tcp
     *
     * Configures the size of the operating system's receive buffer for this socket.
     * A larger buffer can improve performance for high-bandwidth connections by allowing
     * the system to buffer more incoming data, reducing the risk of packet loss during
     * temporary processing delays.
     *
     * ### Implementation Details
     * - Uses setsockopt() with SO_RCVBUF option to set kernel socket buffer size
     * - The operating system may adjust the requested size:
     *   - Linux typically doubles the value to account for bookkeeping overhead
     *   - Windows may round to system-specific boundaries
     *   - Maximum size is limited by system settings (/proc/sys/net/core/rmem_max on Linux)
     * - Changes affect subsequent receives only, not data already in the buffer
     * - System limits may constrain maximum size based on available memory and OS policy
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     *
     * // Set 64KB receive buffer
     * sock.setReceiveBufferSize(64 * 1024);
     *
     * // Verify actual size (may be different)
     * int actualSize = sock.getReceiveBufferSize();
     * std::cout << "Actual receive buffer size: " << actualSize << " bytes\n";
     * @endcode
     *
     * @param[in] size Desired receive buffer size in bytes. The actual size
     *                 allocated may be adjusted by the operating system.
     *
     * @throws SocketException If:
     *         - Socket is invalid
     *         - System call fails
     *         - Requested size exceeds system limits
     *         - Insufficient permissions
     *
     * @note Some systems may double the specified size internally.
     *       Use getReceiveBufferSize() to verify the actual size.
     *
     * @see getSendBufferSize() Get current send buffer size
     * @see setInternalBufferSize() Set internal buffer size
     * @see setSendBufferSize() Set send buffer size
     */
    void setReceiveBufferSize(std::size_t size) const;

    /**
     * @brief Sets the socket's send buffer size (SO_SNDBUF).
     * @ingroup tcp
     *
     * Configures the size of the operating system's send buffer for this socket.
     * A larger buffer can improve performance for high-bandwidth connections by allowing
     * the system to buffer more outgoing data, reducing the risk of blocking during
     * write operations when the network is temporarily congested.
     *
     * ### Implementation Details
     * - Uses setsockopt() with SO_SNDBUF option to set kernel socket buffer size
     * - The operating system may adjust the requested size:
     *   - Linux typically doubles the value to account for bookkeeping overhead
     *   - Windows may round to system-specific boundaries
     *   - Maximum size is limited by system settings (/proc/sys/net/core/wmem_max on Linux)
     * - Changes affect subsequent sends only, not data already in the buffer
     * - System limits may constrain maximum size based on available memory and OS policy
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 80);
     *
     * // Set 64KB send buffer
     * sock.setSendBufferSize(64 * 1024);
     *
     * // Verify actual size (may be different)
     * int actualSize = sock.getSendBufferSize();
     * std::cout << "Actual send buffer size: " << actualSize << " bytes\n";
     * @endcode
     *
     * @param[in] size Desired send buffer size in bytes. The actual size
     *                 allocated may be adjusted by the operating system.
     *
     * @throws SocketException If:
     *         - Socket is invalid
     *         - System call fails
     *         - Requested size exceeds system limits
     *         - Insufficient permissions
     *
     * @note Some systems may double the specified size internally.
     *       Use getSendBufferSize() to verify the actual size.
     *
     * @see getReceiveBufferSize() Get current receive buffer size
     * @see setReceiveBufferSize() Set receive buffer size
     * @see getSendBufferSize() Get current send buffer size
     */
    void setSendBufferSize(std::size_t size);

    /**
     * @brief Get the socket's receive buffer size (SO_RCVBUF).
     * @ingroup tcp
     *
     * Retrieves the current size of the operating system's receive buffer for this socket.
     * The receive buffer is used by the OS to store incoming data before it is read by
     * the application. A larger buffer can help prevent data loss during high-bandwidth
     * transfers or when the application cannot read data quickly enough.
     *
     * ### Implementation Details
     * - Uses getsockopt() with SO_RCVBUF option
     * - Returns actual buffer size allocated by OS
     * - On Linux, returned value may be twice the requested size due to kernel overhead
     * - Thread-safe with respect to other Socket instances
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Check current receive buffer size
     * int size = sock.getReceiveBufferSize();
     * std::cout << "Current receive buffer: " << size << " bytes\n";
     *
     * // Increase if needed
     * if (size < 64 * 1024) {
     *     sock.setReceiveBufferSize(64 * 1024);
     *     size = sock.getReceiveBufferSize();
     *     std::cout << "New receive buffer: " << size << " bytes\n";
     * }
     * @endcode
     *
     * @return Current receive buffer size in bytes. Note that this may differ
     *         from the size requested via setReceiveBufferSize() due to OS
     *         adjustments and overhead.
     *
     * @throws SocketException If:
     *         - Socket is invalid (EBADF)
     *         - System call fails (EFAULT)
     *         - Insufficient permissions
     *
     * @see setReceiveBufferSize() For setting the receive buffer size
     * @see getSendBufferSize() Get send buffer size
     * @see setInternalBufferSize() Set internal string buffer size
     */
    [[nodiscard]] int getReceiveBufferSize() const;

    /**
     * @brief Get the socket's send buffer size (SO_SNDBUF).
     * @ingroup tcp
     *
     * Retrieves the current size of the operating system's send buffer for this socket.
     * The send buffer is used by the OS to store outgoing data before it is transmitted
     * over the network. A larger buffer can help improve performance during high-bandwidth
     * transfers or when the network is temporarily congested.
     *
     * ### Implementation Details
     * - Uses getsockopt() with SO_SNDBUF option
     * - Returns actual buffer size allocated by OS
     * - On Linux, returned value may be twice the requested size due to kernel overhead
     * - Thread-safe with respect to other Socket instances
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Check current send buffer size
     * int size = sock.getSendBufferSize();
     * std::cout << "Current send buffer: " << size << " bytes\n";
     *
     * // Increase if needed
     * if (size < 64 * 1024) {
     *     sock.setSendBufferSize(64 * 1024);
     *     size = sock.getSendBufferSize();
     *     std::cout << "New send buffer: " << size << " bytes\n";
     * }
     * @endcode
     *
     * @return Current send buffer size in bytes. Note that this may differ
     *         from the size requested via setSendBufferSize() due to OS
     *         adjustments and overhead.
     *
     * @throws SocketException If:
     *         - Socket is invalid (EBADF)
     *         - System call fails (EFAULT)
     *         - Insufficient permissions
     *
     * @see setSendBufferSize() For setting the send buffer size
     * @see getReceiveBufferSize() Get receive buffer size
     * @see setInternalBufferSize() Set internal string buffer size
     */
    [[nodiscard]] int getSendBufferSize() const;

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
     * - Checks if internal socket descriptor (_sockFd) is not INVALID_SOCKET
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
    [[nodiscard]] bool isValid() const noexcept { return this->_sockFd != INVALID_SOCKET; }

    /**
     * @brief Set the socket to non-blocking or blocking mode.
     * @ingroup tcp
     *
     * Controls whether socket operations (connect, read, write) block until completion
     * or return immediately. In non-blocking mode, operations return immediately with
     * an error (usually EWOULDBLOCK/WSAEWOULDBLOCK) if they cannot be completed,
     * allowing the application to perform other tasks while waiting.
     *
     * ### Implementation Details
     * - Uses platform-specific APIs (ioctlsocket on Windows, fcntl on POSIX)
     * - Affects all subsequent socket operations
     * - Thread-safe with respect to other Socket instances
     * - State persists until explicitly changed
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     *
     * // Enable non-blocking mode
     * sock.setNonBlocking(true);
     *
     * try {
     *     // Will return immediately if can't connect
     *     sock.connect();
     * } catch (const SocketException& ex) {
     *     if (ex.getErrorCode() == EWOULDBLOCK) {
     *         // Connection in progress
     *         if (sock.waitReady(true, 5000)) {
     *             // Connected within 5 seconds
     *         }
     *     }
     * }
     * @endcode
     *
     * ### Common Use Cases
     * - Implementing select/poll-based I/O multiplexing
     * - Preventing blocking in UI threads
     * - Managing multiple connections efficiently
     * - Implementing timeouts for operations
     *
     * @param[in] nonBlocking true to enable non-blocking mode,
     *                        false to enable blocking mode (default).
     *
     * @throws SocketException If:
     *         - Socket is invalid (EBADF)
     *         - System call fails (platform-specific)
     *         - Permission denied (EACCES)
     *
     * @see getNonBlocking() Check current blocking mode
     * @see waitReady() Wait for non-blocking operations
     * @see setSoTimeout() Alternative for blocking timeouts
     */
    void setNonBlocking(bool nonBlocking) const;

    /**
     * @brief Check if the socket is currently in non-blocking mode.
     * @ingroup tcp
     *
     * Returns the current blocking mode of the socket. A socket in non-blocking mode
     * will return immediately from operations that would normally block, such as
     * connect(), read(), or write(), with an error code (usually EWOULDBLOCK/WSAEWOULDBLOCK)
     * if the operation cannot be completed immediately.
     *
     * ### Implementation Details
     * - Uses platform-specific APIs (ioctlsocket on Windows, fcntl on POSIX)
     * - Thread-safe with respect to other Socket instances
     * - Performs a system call to check current socket state
     * - O(1) operation complexity
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     *
     * // Check current blocking mode
     * bool isNonBlocking = sock.getNonBlocking();
     * if (!isNonBlocking) {
     *     // Enable non-blocking for async operations
     *     sock.setNonBlocking(true);
     * }
     *
     * // Use with waitReady for timeout control
     * if (sock.getNonBlocking()) {
     *     try {
     *         sock.connect();
     *     } catch (const SocketException& ex) {
     *         if (ex.getErrorCode() == EWOULDBLOCK) {
     *             // Wait up to 5 seconds for connection
     *             if (sock.waitReady(true, 5000)) {
     *                 // Connected successfully
     *             }
     *         }
     *     }
     * }
     * @endcode
     *
     * @return true if the socket is in non-blocking mode,
     *         false if the socket is in blocking mode (default).
     *
     * @throws SocketException If:
     *         - Socket is invalid (EBADF)
     *         - System call fails (platform-specific errors)
     *         - Permission denied (EACCES)
     *
     * @see setNonBlocking() Change blocking mode
     * @see waitReady() Wait for non-blocking operations
     * @see setSoTimeout() Alternative for blocking timeouts
     */
    [[nodiscard]] bool getNonBlocking() const;

    /**
     * @brief Set timeout for socket send and/or receive operations.
     * @ingroup tcp
     *
     * Configures timeouts for socket operations, allowing fine-grained control over how long
     * the socket will wait during blocking send or receive operations before failing. This provides
     * an alternative to non-blocking mode for handling slow or unresponsive peers.
     *
     * ### Timeout Behavior
     * - Read timeout (SO_RCVTIMEO): Maximum time to wait for incoming data
     * - Write timeout (SO_SNDTIMEO): Maximum time to wait when sending data
     * - Zero timeout: Operations return immediately if they would block
     * - Negative timeout: Blocking mode (wait indefinitely)
     *
     * ### Implementation Details
     * - Uses setsockopt() with SO_RCVTIMEO and/or SO_SNDTIMEO
     * - Platform-independent millisecond resolution
     * - Affects all subsequent operations until changed
     * - Thread-safe with respect to other Socket instances
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Set 5-second timeout for both read/write
     * sock.setSoTimeout(5000);
     *
     * try {
     *     // Will fail if no data received within 5 seconds
     *     std::string data = sock.read<std::string>();
     * } catch (const SocketException& ex) {
     *     if (ex.getErrorCode() == ETIMEDOUT) {
     *         // Handle timeout
     *     }
     * }
     * @endcode
     *
     * @param[in] millis Timeout duration in milliseconds:
     *                   - > 0: Maximum wait time
     *                   - 0: Non-blocking operation
     *                   - < 0: Blocking operation (no timeout)
     * @param[in] forRead If true, set receive timeout (default: true)
     * @param[in] forWrite If true, set send timeout (default: true)
     *
     * @throws SocketException If:
     *         - Socket is invalid (EBADF)
     *         - Permission denied (EACCES)
     *         - Invalid timeout value
     *         - System-specific errors
     *
     * @see setNonBlocking() Alternative approach for non-blocking operations
     * @see waitReady() For fine-grained operation timing
     * @see read() Affected by receive timeout
     * @see write() Affected by send timeout
     */
    void setSoTimeout(int millis, bool forRead = true, bool forWrite = true) const;

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
     * This method performs a non-destructive check of the socket's connection status
     * by attempting a zero-byte send (on Windows) or checking socket errors (on POSIX).
     * While not 100% reliable due to the nature of TCP/IP, it provides a best-effort
     * indication of connection status.
     *
     * ### Implementation Details
     * - Windows: Uses send() with zero bytes
     * - POSIX: Checks SO_ERROR socket option
     * - Does not generate network traffic
     * - Non-blocking operation
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Periodically check connection
     * while (sock.isConnected()) {
     *     // Connection is still alive
     *     std::this_thread::sleep_for(std::chrono::seconds(1));
     * }
     * // Connection lost
     * @endcode
     *
     * @return true if the socket appears to be connected, false if disconnected
     *         or in an error state.
     * @note This check is not guaranteed to detect all network failures,
     *       especially temporary outages or routing problems.
     * @see enableKeepAlive() For reliable connection monitoring
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
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Enable TCP_NODELAY for low-latency communication
     * sock.enableNoDelay(true);
     *
     * // Send data immediately without buffering
     * sock.write("time-sensitive-data");
     * @endcode
     *
     * @param enable true to disable Nagle's algorithm (enable TCP_NODELAY, lower latency),
     *               false to enable Nagle's algorithm (disable TCP_NODELAY, higher throughput).
     * @throws SocketException If setting the TCP_NODELAY option fails:
     *         - Invalid socket state (EBADF)
     *         - Permission denied (EACCES)
     *         - Protocol not available (ENOPROTOOPT)
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
     * ### Platform-Specific Details
     * - Linux: Configure via /proc/sys/net/ipv4/tcp_keepalive_*
     * - Windows: Default 2-hour idle time before first probe
     * - macOS: System-wide keepalive settings apply
     *
     * ### Example Usage
     * @code{.cpp}
     * Socket sock("example.com", 8080);
     * sock.connect();
     *
     * // Enable keepalive for long-lived connection
     * sock.enableKeepAlive(true);
     * @endcode
     *
     * @param enable true to enable keepalive probes (SO_KEEPALIVE), false to disable (default).
     * @throws SocketException If setting the SO_KEEPALIVE option fails:
     *         - Invalid socket state (EBADF)
     *         - Permission denied (EACCES)
     *         - Memory allocation error (ENOMEM)
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

    /**
     * @brief Set a socket option at the specified level.
     *
     * This method sets a low-level socket option on the underlying socket.
     * Socket options allow advanced users to customize various aspects of socket behavior,
     * such as timeouts, buffer sizes, and address reuse policies.
     *
     * Example usage to enable address reuse (SO_REUSEADDR):
     * @code
     * socket.setOption(SOL_SOCKET, SO_REUSEADDR, 1);
     * @endcode
     *
     * @param level    The protocol level at which the option resides (e.g., SOL_SOCKET, IPPROTO_TCP).
     * @param optName  The name of the option (e.g., SO_REUSEADDR, SO_RCVBUF).
     * @param value    The integer value to set for the option.
     * @throws SocketException if the operation fails (see the error code/message for details).
     */
    void setOption(int level, int optName, int value) const;

    /**
     * @brief Get the current value of a socket option at the specified level.
     *
     * This method retrieves the value of a low-level socket option from the underlying socket.
     * This can be useful to check the current settings for options like buffer sizes or timeouts.
     *
     * Example usage to read the receive buffer size:
     * @code
     * int recvBuf = socket.getOption(SOL_SOCKET, SO_RCVBUF);
     * @endcode
     *
     * @param level    The protocol level at which the option resides (e.g., SOL_SOCKET, IPPROTO_TCP).
     * @param optName  The name of the option (e.g., SO_RCVBUF, SO_KEEPALIVE).
     * @return         The integer value currently set for the option.
     * @throws SocketException if the operation fails (see the error code/message for details).
     */
    [[nodiscard]] int getOption(int level, int optName) const;

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

  private:
    SOCKET _sockFd = INVALID_SOCKET; ///< Underlying socket file descriptor.
    sockaddr_storage _remoteAddr;    ///< sockaddr_in for IPv4; sockaddr_in6 for IPv6; sockaddr_storage for both
    ///< (portability)
    mutable socklen_t _remoteAddrLen = 0;  ///< Length of remote address (for recvfrom/recvmsg)
    addrinfo* _cliAddrInfo = nullptr;      ///< Address info for connection (from getaddrinfo)
    addrinfo* _selectedAddrInfo = nullptr; ///< Selected address info for connection
    std::vector<char> _recvBuffer;         ///< Internal buffer for read operations
};

/**
 * @brief Template specialization to read a string from the socket.
 * @ingroup tcp
 *
 * Reads incoming data from the socket into the internal buffer and returns it as a string.
 * This specialization provides string-specific functionality that differs from the generic
 * read<T>() implementation.
 *
 * ### Key Features
 * - Uses internal buffer for efficient reading
 * - Handles partial reads gracefully
 * - Supports both text and binary data
 * - Returns actual received data length
 *
 * ### Implementation Details
 * 1. Uses internal buffer (_recvBuffer) sized via setInternalBufferSize()
 * 2. Performs single recv() call up to buffer size
 * 3. Creates string from received data
 * 4. Preserves binary data (including null bytes)
 *
 * ### Example Usage
 * @code{.cpp}
 * Socket sock("example.com", 8080);
 * sock.connect();
 *
 * // Read with default buffer size (512 bytes)
 * std::string data = sock.read<std::string>();
 *
 * // Read with custom buffer size
 * sock.setInternalBufferSize(1024);
 * std::string largeData = sock.read<std::string>();
 * @endcode
 *
 * @return std::string containing the received data. Length may be less than
 *         buffer size depending on available data.
 *
 * @throws SocketException If:
 *         - Socket error occurs during read
 *         - Connection is closed by remote host
 *         - System-specific network errors
 *
 * @note This specialization differs from read<T>() in that it doesn't require
 *       an exact size match and can handle variable-length data.
 *
 * @see setInternalBufferSize() To modify the maximum read length
 * @see write() For sending string data
 * @see read() Generic version for fixed-size types
 */
template <> inline std::string Socket::read()
{
    const auto len = recv(_sockFd, _recvBuffer.data(),
#ifdef _WIN32
                          static_cast<int>(_recvBuffer.size()),
#else
                          _recvBuffer.size(),
#endif
                          0);
    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (len == 0)
        throw SocketException(0, "Connection closed by remote host.");
    return {_recvBuffer.data(), static_cast<size_t>(len)};
}

} // namespace jsocketpp
