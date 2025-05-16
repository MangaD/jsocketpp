#pragma once

#include "common.hpp"

// Enable AF_UNIX support on Windows 10+ (version 1803, build 17134) only
#if defined(_MSC_VER) && (_WIN32_WINNT >= _WIN32_WINNT_WIN10)
// https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows/
#include <afunix.h> // Windows 10+ AF_UNIX support
#define AF_UNIX 1
#endif

#include "SocketException.hpp"
#include "common.hpp"

namespace libsocket
{

#if defined(_WIN32) && defined(AF_UNIX) || defined(__unix__) || defined(__APPLE__)

/**
 * @class UnixSocket
 * @brief A cross-platform wrapper for Unix domain sockets.
 *
 * On POSIX, uses native AF_UNIX sockets. On Windows, only available on Windows 10 (version 1803, build 17134) and
 * later. Uses AF_UNIX support in Winsock2.
 *
 * This class provides an interface for creating, binding, listening, accepting,
 * connecting, reading from, writing to, and closing Unix domain sockets.
 * It abstracts away platform-specific details for both Unix-like systems and Windows.
 *
 * @note Not thread-safe. Each UnixSocket should only be used from one thread at a time.
 */
class UnixSocket
{
  public:
    /**
     * @brief Constructs a UnixSocket and connects or binds to the specified path.
     * @param path The filesystem path for the Unix domain socket.
     * @param bufferSize Size of the internal read buffer (default: 512).
     */
    explicit UnixSocket(const std::string& path, std::size_t bufferSize = 512);

    /**
     * @brief Destructor.
     *
     * Closes the socket if it is open.
     */
    ~UnixSocket() noexcept;

    /**
     * @brief Binds the socket.
     * @throws std::socket_exception if binding fails.
     */
    void bind();

    /**
     * @brief Marks the socket as a passive socket to accept incoming connections.
     * @param backlog The maximum length to which the queue of pending connections may grow.
     * @throws std::socket_exception if listen fails.
     *
     * The backlog parameter defines the maximum number of pending connections
     * that can be queued up before connections are refused.
     */
    void listen(int backlog = SOMAXCONN) const;

    /**
     * @brief Accepts an incoming connection.
     * @return A new UnixSocket representing the accepted connection.
     * @throws std::socket_exception if accept fails.
     */
    UnixSocket accept() const;

    /**
     * @brief Connects the socket.
     * @throws std::socket_exception if connection fails.
     */
    void connect();

    /**
     * @brief Writes data to the socket.
     * @param data The data to write.
     * @return The number of bytes written.
     * @throws std::socket_exception if writing fails.
     */
    int write(std::string_view data) const;

    /**
     * @brief Reads data from the socket into a buffer.
     * @param buffer Pointer to the buffer to read data into.
     * @param len Maximum number of bytes to read.
     * @return The number of bytes read.
     * @throws std::socket_exception if reading fails.
     */
    int read(char* buffer, std::size_t len) const;

    /**
     * @brief Reads a trivially copyable type from the socket.
     * @tparam T Type to read (must be trivially copyable).
     * @return Value of type T read from the socket.
     * @throws SocketException on error or disconnect.
     */
    template <typename T> T read()
    {
        static_assert(std::is_trivially_copyable_v<T>, "UnixSocket::read<T>() only supports trivially copyable types");
        T value;
        const int len = ::recv(_sockfd, reinterpret_cast<char*>(&value),
#ifdef _WIN32
                               static_cast<int>(sizeof(T)),
#else
                               sizeof(T),
#endif
                               0);
        if (len == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
        if (len == 0)
            throw SocketException(0, "Connection closed by remote socket.");
        return value;
    }

    /**
     * @brief Closes the socket.
     */
    void close();

    /**
     * @brief Checks if the socket is valid (open).
     * @return true if the socket is valid, false otherwise.
     */
    [[nodiscard]] bool isValid() const { return this->_sockfd != INVALID_SOCKET; }

    /**
     * @brief Sets the socket to non-blocking or blocking mode.
     *
     * This function configures the socket to operate in either non-blocking or blocking mode,
     * depending on the value of the `nonBlocking` parameter. In non-blocking mode, socket operations
     * will return immediately if they cannot be completed, rather than blocking the calling thread.
     *
     * @param nonBlocking If true, sets the socket to non-blocking mode; if false, sets it to blocking mode.
     */
    void setNonBlocking(bool nonBlocking) const;

    /**
     * @brief Sets a timeout for socket operations.
     * @param millis Timeout in milliseconds.
     */
    void setTimeout(int millis) const;

  protected:
    /**
     * @brief Default constructor for internal use (e.g., accept()).
     */
    UnixSocket() : _sockfd(INVALID_SOCKET), _socketPath(), _addr(), _buffer(512) {}

  private:
    SOCKET _sockfd = INVALID_SOCKET; ///< Underlying socket file descriptor.
    std::string _socketPath;         ///< Path for the Unix domain socket.
    SOCKADDR_UN _addr;               ///< Address structure for Unix domain sockets.
    std::vector<char> _buffer;       ///< Internal buffer for read operations.
};

/**
 * @brief Template specialization to read a string from the Unix domain socket.
 *
 * Reads data from the socket into the internal buffer and returns it as a string.
 * Uses the socket's internal buffer size (set via constructor) as the maximum read length.
 * The actual returned string length may be shorter depending on how much data was received.
 *
 * @return String containing the received data.
 * @throws SocketException If a socket error occurs or the connection is closed by the remote peer.
 * @see UnixSocket() To modify the maximum read length via bufferSize parameter.
 */
template <> inline std::string UnixSocket::read()
{
    const auto len = static_cast<int>(recv(_sockfd, _buffer.data(),
#ifdef _WIN32
                                           static_cast<int>(_buffer.size()),
#else
                                           _buffer.size(),
#endif
                                           0));
    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (len == 0)
        throw SocketException(0, "Connection closed by remote socket.");
    return {_buffer.data(), static_cast<size_t>(len)};
}

#endif

} // namespace libsocket
