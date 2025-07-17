#include "jsocketpp/Socket.hpp"

#include <array>
#include <chrono>

using namespace jsocketpp;

Socket::Socket(const SOCKET client, const sockaddr_storage& addr, const socklen_t len, const std::size_t recvBufferSize,
               const std::size_t sendBufferSize)
    : _sockFd(client), _remoteAddr(addr), _remoteAddrLen(len), _recvBuffer(recvBufferSize)
{
    setInternalBufferSize(recvBufferSize);
    setReceiveBufferSize(recvBufferSize);
    setSendBufferSize(sendBufferSize);
}

Socket::Socket(const std::string_view host, const unsigned short port, const std::size_t bufferSize)
    : _remoteAddr{}, _recvBuffer(bufferSize)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the local address and port to be used by the server
    const std::string portStr = std::to_string(port);

    if (auto ret = getaddrinfo(host.data(), portStr.c_str(), &hints, &_cliAddrInfo); ret != 0)
    {
        throw SocketException(
#ifdef _WIN32
            GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
            ret, SocketErrorMessageWrap(ret, true));
#endif
    }

    // Try all available addresses (IPv6/IPv4) to create a SOCKET for
    _sockFd = INVALID_SOCKET;
    for (addrinfo* p = _cliAddrInfo; p != nullptr; p = p->ai_next)
    {
        _sockFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (_sockFd != INVALID_SOCKET)
        {
            // Store the address that worked
            _selectedAddrInfo = p;
            break;
        }
    }
    if (_sockFd == INVALID_SOCKET)
    {
        const auto error = GetSocketError();
        freeaddrinfo(_cliAddrInfo); // ignore errors from freeaddrinfo
        _cliAddrInfo = nullptr;
        throw SocketException(error, SocketErrorMessage(error));
    }
}

void Socket::connect(const int timeoutMillis) const
{
    // Ensure that we have already selected an address during construction
    if (_selectedAddrInfo == nullptr)
    {
        throw SocketException(0, "connect() failed: no valid addrinfo found");
    }

    // Flag to track whether we're in non-blocking mode
    const bool isNonBlocking = (timeoutMillis >= 0);

    // Set the socket to non-blocking mode if a timeout is specified
    if (isNonBlocking)
    {
        setNonBlocking(true); // This sets the socket to non-blocking mode
    }

    // Attempt to connect to the remote host
    const auto res = ::connect(_sockFd, _selectedAddrInfo->ai_addr,
#ifdef _WIN32
                               static_cast<int>(_selectedAddrInfo->ai_addrlen) // Windows: cast to int
#else
                               _selectedAddrInfo->ai_addrlen // Linux/Unix: use socklen_t directly
#endif
    );

    if (res == SOCKET_ERROR)
    {
        auto error = GetSocketError();

        // If the error is EINPROGRESS or EWOULDBLOCK, it means the connection is in progress
        // which is expected in non-blocking mode
#ifdef _WIN32
        // Windows: Check if WSAEINPROGRESS or WSAEWOULDBLOCK is returned
        if (error != WSAEINPROGRESS && error != WSAEWOULDBLOCK)
#else
        // Linux/Unix: Check if EINPROGRESS or EWOULDBLOCK is returned
        if (error != EINPROGRESS && error != EWOULDBLOCK)
#endif
        {
            // Reset the socket to blocking mode before throwing the exception
            if (isNonBlocking)
                setNonBlocking(false);
            throw SocketException(error, SocketErrorMessage(error));
        }

        // If we're in non-blocking mode, use select to wait for the connection to complete
        if (isNonBlocking)
        {
            timeval tv{};
            tv.tv_sec = timeoutMillis / 1000;
            tv.tv_usec = (timeoutMillis % 1000) * 1000;

            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(_sockFd, &fds);

#ifdef _WIN32
            // On Windows, the first parameter to select is ignored, but must be set to 0
            auto result = select(0, nullptr, &fds, nullptr, &tv);
#else
            // On Linux/Unix, the first parameter to select should be the highest file descriptor + 1
            auto result = select(_sockFd + 1, nullptr, &fds, nullptr, &tv);
#endif

            // If select() timed out or failed, reset the socket to blocking mode
            if (result <= 0)
            {
                // Reset the socket to blocking mode before throwing the timeout exception
                setNonBlocking(false);
                throw SocketException(0, "Connection timed out or failed.");
            }
        }
    }

    // After the connection attempt, if we used non-blocking mode, reset to blocking mode
    if (isNonBlocking)
    {
        setNonBlocking(false); // Reset the socket back to blocking mode
    }
}

/**
 * @brief Destructor. Closes the socket and frees resources.
 */
Socket::~Socket() noexcept
{
    try
    {
        close();
    }
    catch (std::exception&)
    {
    }
}

/**
 * @brief Close the socket.
 * @throws SocketException on error.
 */
void Socket::close()
{
    if (this->_sockFd != INVALID_SOCKET)
    {
        if (CloseSocket(this->_sockFd))
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));

        this->_sockFd = INVALID_SOCKET;
    }
    if (_cliAddrInfo)
    {
        freeaddrinfo(_cliAddrInfo); // ignore errors from freeaddrinfo
        _cliAddrInfo = nullptr;
    }
    _selectedAddrInfo = nullptr;
}

void Socket::shutdown(ShutdownMode how) const
{
    // Convert ShutdownMode to platform-specific shutdown constants
    int shutdownType;
#ifdef _WIN32
    switch (how)
    {
    case ShutdownMode::Read:
        shutdownType = SD_RECEIVE;
        break;
    case ShutdownMode::Write:
        shutdownType = SD_SEND;
        break;
    case ShutdownMode::Both:
        [[fallthrough]]; // SD_BOTH is equivalent to SD_SEND | SD_RECEIVE
    default:
        shutdownType = SD_BOTH;
        break;
    }
#else
    switch (how)
    {
    case ShutdownMode::Read:
        shutdownType = SHUT_RD;
        break;
    case ShutdownMode::Write:
        shutdownType = SHUT_WR;
        break;
    case ShutdownMode::Both:
        [[fallthrough]]; // SHUT_RDWR is equivalent to SHUT_WR | SHUT_RD
    default:
        shutdownType = SHUT_RDWR;
        break;
    }
#endif

    // Ensure the socket is valid before attempting to shutdown
    if (this->_sockFd != INVALID_SOCKET)
    {
        if (::shutdown(this->_sockFd, shutdownType))
        {
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));
        }
    }
}

// http://www.microhowto.info/howto/convert_an_ip_address_to_a_human_readable_string_in_c.html
std::string Socket::getRemoteSocketAddress() const
{
    if (_remoteAddrLen == 0)
        return ""; // Or throw, depending on API contract

    sockaddr_storage tempAddr = _remoteAddr;
    socklen_t tempLen = _remoteAddrLen;

    // Normalize IPv4-mapped IPv6 to IPv4 (make a local copy)
    if (tempAddr.ss_family == AF_INET6)
    {
        const auto* addr6 = reinterpret_cast<const sockaddr_in6*>(&tempAddr);
        if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr))
        {
            sockaddr_in addr4{};
            addr4.sin_family = AF_INET;
            addr4.sin_port = addr6->sin6_port;
            memcpy(&addr4.sin_addr.s_addr, addr6->sin6_addr.s6_addr + 12, sizeof(addr4.sin_addr.s_addr));
            // Overwrite tempAddr with IPv4
            memset(&tempAddr, 0, sizeof(tempAddr));
            memcpy(&tempAddr, &addr4, sizeof(addr4));
            tempLen = sizeof(addr4);
        }
    }

    char host[NI_MAXHOST] = {};
    char serv[NI_MAXSERV] = {};
    int ret = getnameinfo(reinterpret_cast<const sockaddr*>(&tempAddr), tempLen, host, sizeof(host), serv, sizeof(serv),
                          NI_NUMERICHOST | NI_NUMERICSERV);
    if (ret != 0)
    {
#ifdef _WIN32
        throw SocketException(WSAGetLastError(), SocketErrorMessageWrap(WSAGetLastError(), true));
#else
        throw SocketException(ret, SocketErrorMessageWrap(ret, true));
#endif
    }
    return std::string(host) + ":" + serv;
}

size_t Socket::write(std::string_view message) const
{
    int flags = 0;
#ifndef _WIN32
    flags = MSG_NOSIGNAL; // Prevent SIGPIPE on write to a closed socket (POSIX)
#endif
    const auto len = send(_sockFd, message.data(),
#ifdef _WIN32
                          static_cast<int>(message.size()), // Windows: cast to int
#else
                          static_cast<size_t>(message.size()), // Linux/Unix: use size_t directly
#endif
                          flags);
    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    // send() may send fewer bytes than requested (partial write), especially on non-blocking sockets.
    // It is the caller's responsibility to check the return value and handle partial sends if needed.
    return static_cast<size_t>(len);
}

// Write all data, retrying as needed until all bytes are sent or an error occurs.
// Returns the total number of bytes sent (should be message.size() on success).
size_t Socket::writeAll(const std::string_view message) const
{
    std::size_t totalSent = 0;
    while (totalSent < message.size())
    {
        const auto sent = write(message.substr(totalSent));
        if (sent <= 0)
            break; // Error or connection closed
        totalSent += static_cast<std::size_t>(sent);
    }
    return totalSent;
}

void Socket::setReceiveBufferSize(const std::size_t size) const
{
    setOption(SOL_SOCKET, SO_RCVBUF, static_cast<int>(size));
}

// NOLINTNEXTLINE(readability-make-member-function-const) - changes socket state
void Socket::setSendBufferSize(const std::size_t size)
{
    setOption(SOL_SOCKET, SO_SNDBUF, static_cast<int>(size));
}

int Socket::getReceiveBufferSize() const
{
    return getOption(SOL_SOCKET, SO_RCVBUF);
}

int Socket::getSendBufferSize() const
{
    return getOption(SOL_SOCKET, SO_SNDBUF);
}

void Socket::setInternalBufferSize(const std::size_t newLen)
{
    _recvBuffer.resize(newLen);
    _recvBuffer.shrink_to_fit();
}

void Socket::setNonBlocking(bool nonBlocking) const
{
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(_sockFd, FIONBIO, &mode) != 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    int flags = fcntl(_sockFd, F_GETFL, 0);
    if (flags == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (nonBlocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    if (fcntl(_sockFd, F_SETFL, flags) == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif
}

bool Socket::getNonBlocking() const
{
#ifdef _WIN32
    u_long mode = 0;
    if (ioctlsocket(_sockFd, FIONBIO, &mode) != 0)
    {
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
    return mode != 0;
#else
    int flags = fcntl(_sockFd, F_GETFL, 0);
    if (flags == -1)
    {
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
    return (flags & O_NONBLOCK) != 0;
#endif
}

void Socket::setSoTimeout(int millis, bool forRead, bool forWrite) const
{
#ifdef _WIN32
    const int timeout = millis;

    // Set the timeout for receiving if specified
    if (forRead && setsockopt(_sockFd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                              sizeof(timeout)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    // Set the timeout for sending if specified
    if (forWrite && setsockopt(_sockFd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout),
                               sizeof(timeout)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    timeval tv{0, 0};
    if (millis >= 0)
    {
        tv.tv_sec = millis / 1000;
        tv.tv_usec = (millis % 1000) * 1000;
    }

    // Set the timeout for receiving if specified
    if (forRead && setsockopt(_sockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    // Set the timeout for sending if specified
    if (forWrite && setsockopt(_sockFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif
}

bool Socket::waitReady(bool forWrite, const int timeoutMillis) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "Invalid socket");

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(_sockFd, &fds);

    timeval tv{0, 0};
    if (timeoutMillis >= 0)
    {
        tv.tv_sec = timeoutMillis / 1000;
        tv.tv_usec = (timeoutMillis % 1000) * 1000;
    }

    int result;
#ifdef _WIN32
    // On Windows, the first parameter to select is ignored, but must be set to 0.
    if (forWrite)
        result = select(0, nullptr, &fds, nullptr, &tv);
    else
        result = select(0, &fds, nullptr, nullptr, &tv);
#else
    if (forWrite)
        result = select(static_cast<int>(_sockFd) + 1, nullptr, &fds, nullptr, &tv);
    else
        result = select(static_cast<int>(_sockFd) + 1, &fds, nullptr, nullptr, &tv);
#endif

    if (result < 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    return result > 0;
}

bool Socket::isConnected() const
{
    if (_sockFd == INVALID_SOCKET)
        return false;

    char buf;
#ifdef _WIN32
    u_long bytesAvailable = 0;
    if (ioctlsocket(_sockFd, FIONREAD, &bytesAvailable) == SOCKET_ERROR)
        return false; // Error, assume socket is closed.
    if (bytesAvailable > 0)
        return true; // Data available, socket is alive.

    // Try non-blocking recv with MSG_PEEK
    u_long mode = 1;
    ioctlsocket(_sockFd, FIONBIO, &mode); // Set socket to non-blocking mode
    auto ret = recv(_sockFd, &buf, 1, MSG_PEEK);
    mode = 0;
    ioctlsocket(_sockFd, FIONBIO, &mode); // Restore socket to blocking mode
    if (ret == 0)
        return false; // Connection closed
    if (ret < 0)
    {
        const auto err = WSAGetLastError();
        return (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS); // Connection still in progress or non-blocking
    }
    return true;
#else
    const auto flags = fcntl(_sockFd, F_GETFL, 0);
    const bool wasNonBlocking = flags & O_NONBLOCK;
    if (!wasNonBlocking)
        fcntl(_sockFd, F_SETFL, flags | O_NONBLOCK);

    auto ret = recv(_sockFd, &buf, 1, MSG_PEEK);
    if (!wasNonBlocking)
        fcntl(_sockFd, F_SETFL, flags); // Restore socket to original mode

    if (ret == 0)
        return false; // Connection closed
    if (ret < 0)
        return (errno == EWOULDBLOCK || errno == EAGAIN); // Connection still in progress or non-blocking
    return true;
#endif
}

void Socket::enableNoDelay(const bool enable) const
{
    const int flag = enable ? 1 : 0;
    if (setsockopt(_sockFd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag)) ==
        SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
}

void Socket::enableKeepAlive(const bool enable) const
{
    const int flag = enable ? 1 : 0;
    if (setsockopt(_sockFd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&flag), sizeof(flag)) ==
        SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
}

std::string Socket::addressToString(const sockaddr_storage& addr)
{
    char ip[INET6_ADDRSTRLEN] = {0};
    char port[6] = {0};

    if (addr.ss_family == AF_INET)
    {
        const auto* addr4 = reinterpret_cast<const sockaddr_in*>(&addr);
        if (const auto ret = getnameinfo(reinterpret_cast<const sockaddr*>(addr4), sizeof(sockaddr_in), ip, sizeof(ip),
                                         port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
            ret != 0)
        {
            throw SocketException(
#ifdef _WIN32
                GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
                ret, SocketErrorMessageWrap(ret, true));
#endif
        }
    }
    else if (addr.ss_family == AF_INET6)
    {
        const auto* addr6 = reinterpret_cast<const sockaddr_in6*>(&addr);
        auto ret = getnameinfo(reinterpret_cast<const sockaddr*>(addr6), sizeof(sockaddr_in6), ip, sizeof(ip), port,
                               sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
        if (ret != 0)
        {
            throw SocketException(
#ifdef _WIN32
                GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
                ret, SocketErrorMessageWrap(ret, true));
#endif
        }
    }
    else
    {
        return "unknown";
    }
    return std::string(ip) + ":" + port;
}

void Socket::stringToAddress(const std::string& str, sockaddr_storage& addr)
{
    std::memset(&addr, 0, sizeof(addr));
    // Find last ':' (to allow IPv6 addresses with ':')
    const auto pos = str.rfind(':');
    if (pos == std::string::npos)
        throw SocketException(0, "Invalid address format: " + str);

    const std::string host = str.substr(0, pos);
    const std::string port = str.substr(pos + 1);

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

    addrinfo* res = nullptr;
    if (const auto ret = getaddrinfo(host.c_str(), port.c_str(), &hints, &res); ret != 0)
    {
        throw SocketException(
#ifdef _WIN32
            GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
            ret, SocketErrorMessageWrap(ret, true));
#endif
    }

    std::memcpy(&addr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res); // ignore errors from freeaddrinfo
}

void Socket::setOption(const int level, const int optName, int value) const
{
    if (setsockopt(_sockFd, level, optName,
#ifdef _WIN32
                   reinterpret_cast<const char*>(&value),
#else
                   &value,
#endif
                   sizeof(value)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
}

int Socket::getOption(const int level, const int optName) const
{
    int value = 0;
#ifdef _WIN32
    int len = sizeof(value);
#else
    socklen_t len = sizeof(value);
#endif
    if (getsockopt(_sockFd, level, optName,
#ifdef _WIN32
                   reinterpret_cast<char*>(&value),
#else
                   &value,
#endif
                   &len) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    return value;
}

std::string Socket::readExact(const std::size_t n) const
{
    if (n == 0)
        return {};

    std::string result;
    result.resize(n); // pre-allocate for performance

    std::size_t totalRead = 0;
    while (totalRead < n)
    {
        const auto remaining = n - totalRead;

        const auto len = recv(_sockFd, result.data() + totalRead,
#ifdef _WIN32
                              static_cast<int>(remaining),
#else
                              remaining,
#endif
                              0);

        if (len == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

        if (len == 0)
            throw SocketException(0, "Connection closed before reading all data.");

        totalRead += static_cast<std::size_t>(len);
    }

    return result;
}

std::string Socket::readUntil(const char delimiter, const std::size_t maxLen, const bool includeDelimiter) const
{
    std::string result;
    result.reserve(128); // optimistic allocation

    char ch = 0;
    std::size_t totalRead = 0;

    while (totalRead < maxLen)
    {
        const auto len = recv(_sockFd, &ch,
#ifdef _WIN32
                              1,
#else
                              1,
#endif
                              0);
        if (len == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
        if (len == 0)
            throw SocketException(0, "Connection closed before delimiter was found.");

        result.push_back(ch);
        ++totalRead;

        if (ch == delimiter)
        {
            if (!includeDelimiter)
                result.pop_back(); // Remove delimiter before returning
            break;
        }
    }

    if (totalRead >= maxLen && (includeDelimiter || (!includeDelimiter && result.size() == maxLen)))
    {
        throw SocketException(0, "Maximum line length exceeded before delimiter was found.");
    }

    return result;
}

std::string Socket::readAtMost(std::size_t n) const
{
    if (n == 0)
        return {};

    std::string result;
    result.resize(n); // preallocate n bytes

    const auto len = recv(_sockFd, result.data(),
#ifdef _WIN32
                          static_cast<int>(n),
#else
                          n,
#endif
                          0);

    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (len == 0)
        throw SocketException(0, "Connection closed by remote host.");

    result.resize(static_cast<std::size_t>(len)); // trim to actual length
    return result;
}

std::size_t Socket::readIntoInternal(void* buffer, std::size_t len, const bool exact) const
{
    if (buffer == nullptr || len == 0)
        return 0;

    const auto out = static_cast<char*>(buffer);
    std::size_t totalRead = 0;

    do
    {
        const auto bytesRead = recv(_sockFd, out + totalRead,
#ifdef _WIN32
                                    static_cast<int>(len - totalRead),
#else
                                    len - totalRead,
#endif
                                    0);

        if (bytesRead == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

        if (bytesRead == 0)
        {
            if (exact)
                throw SocketException(0, "Connection closed before full read completed.");
            break; // return what we got so far
        }

        totalRead += static_cast<std::size_t>(bytesRead);
    } while (exact && totalRead < len);

    return totalRead;
}

std::string Socket::readAtMostWithTimeout(std::size_t n, const int timeoutMillis) const
{
    if (n == 0)
        return {};

    if (!waitReady(false /* forRead */, timeoutMillis))
        throw SocketException(0, "Read timed out after waiting " + std::to_string(timeoutMillis) + " ms");

    std::string result;
    result.resize(n); // max allocation

    const auto len = recv(_sockFd, result.data(),
#ifdef _WIN32
                          static_cast<int>(n),
#else
                          n,
#endif
                          0);

    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (len == 0)
        throw SocketException(0, "Connection closed before data could be read.");

    result.resize(static_cast<std::size_t>(len)); // shrink to actual
    return result;
}

std::string Socket::readAvailable() const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "readAvailable() called on invalid socket");

#ifdef _WIN32
    u_long bytesAvailable = 0;
    if (ioctlsocket(_sockFd, FIONREAD, &bytesAvailable) != 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    int bytesAvailable = 0;
    if (ioctl(_sockFd, FIONREAD, &bytesAvailable) < 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif

    if (bytesAvailable <= 0)
        return {};

    std::string result;
    result.resize(static_cast<std::size_t>(bytesAvailable));

    const auto len = recv(_sockFd, result.data(),
#ifdef _WIN32
                          static_cast<int>(bytesAvailable),
#else
                          static_cast<std::size_t>(bytesAvailable),
#endif
                          0);

    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (len == 0)
        throw SocketException(0, "Connection closed while attempting to read available data.");

    result.resize(static_cast<std::size_t>(len)); // shrink to actual read
    return result;
}

std::size_t Socket::readIntoAvailable(void* buffer, const std::size_t bufferSize) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "readIntoAvailable() called on invalid socket");

    if (buffer == nullptr || bufferSize == 0)
        return 0;

#ifdef _WIN32
    u_long bytesAvailable = 0;
    if (ioctlsocket(_sockFd, FIONREAD, &bytesAvailable) != 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    int bytesAvailable = 0;
    if (ioctl(_sockFd, FIONREAD, &bytesAvailable) < 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif

    if (bytesAvailable <= 0)
        return 0;

    const std::size_t toRead = std::min<std::size_t>(static_cast<std::size_t>(bytesAvailable), bufferSize);

    const auto len = recv(_sockFd,
#ifdef _WIN32
                          static_cast<char*>(buffer), static_cast<int>(toRead),
#else
                          buffer, toRead,
#endif
                          0);

    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (len == 0)
        throw SocketException(0, "Connection closed while attempting to read available data.");

    return static_cast<std::size_t>(len);
}

std::string Socket::peek(std::size_t n) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "peek() called on invalid socket");

    if (n == 0)
        return {};

    std::string result;
    result.resize(n);

    const auto len = recv(_sockFd, result.data(),
#ifdef _WIN32
                          static_cast<int>(n),
#else
                          n,
#endif
                          MSG_PEEK);

    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (len == 0)
        throw SocketException(0, "Connection closed during peek operation.");

    result.resize(static_cast<std::size_t>(len)); // trim to actual
    return result;
}

void Socket::discard(std::size_t n) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "discard() called on invalid socket");

    if (n == 0)
        return;

    constexpr std::size_t chunkSize = 1024;
    std::array<char, chunkSize> tempBuffer{};

    std::size_t totalDiscarded = 0;
    while (totalDiscarded < n)
    {
        std::size_t toRead = (std::min)(chunkSize, n - totalDiscarded);
        const auto len = recv(_sockFd, tempBuffer.data(),
#ifdef _WIN32
                              static_cast<int>(toRead),
#else
                              toRead,
#endif
                              0);

        if (len == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

        if (len == 0)
            throw SocketException(0, "Connection closed before all bytes were discarded.");

        totalDiscarded += static_cast<std::size_t>(len);
    }
}

std::size_t Socket::writev(std::span<const std::string_view> buffers) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "writev() called on invalid socket");

#ifdef _WIN32
    // Convert to WSABUF
    std::vector<WSABUF> wsabufs;
    wsabufs.reserve(buffers.size());

    for (const auto& buf : buffers)
    {
        WSABUF w;
        w.buf = const_cast<char*>(buf.data()); // WSABUF is not const-correct
        w.len = static_cast<ULONG>(buf.size());
        wsabufs.push_back(w);
    }

    DWORD bytesSent = 0;
    if (const int result =
            WSASend(_sockFd, wsabufs.data(), static_cast<DWORD>(wsabufs.size()), &bytesSent, 0, nullptr, nullptr);
        result == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    return static_cast<std::size_t>(bytesSent);

#else
    // POSIX: use writev
    std::vector<iovec> iovecs;
    iovecs.reserve(buffers.size());

    for (const auto& buf : buffers)
    {
        iovec io{};
        io.iov_base = const_cast<char*>(buf.data()); // iovec is not const-correct either
        io.iov_len = buf.size();
        iovecs.push_back(io);
    }

    ssize_t bytesSent = ::writev(_sockFd, iovecs.data(), static_cast<int>(iovecs.size()));
    if (bytesSent == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    return static_cast<std::size_t>(bytesSent);
#endif
}

std::size_t Socket::writevAll(std::span<const std::string_view> buffers) const
{
    std::vector remainingBuffers(buffers.begin(), buffers.end());
    std::size_t totalSent = 0;

    while (!remainingBuffers.empty())
    {
        const std::size_t bytesSent = writev(remainingBuffers);
        totalSent += bytesSent;

        std::size_t advanced = 0;
        std::size_t remaining = bytesSent;

        // Count how many buffers were fully sent
        for (const auto& buf : remainingBuffers)
        {
            if (remaining >= buf.size())
            {
                remaining -= buf.size();
                ++advanced;
            }
            else
            {
                break;
            }
        }

        // Erase fully sent buffers
        remainingBuffers.erase(remainingBuffers.begin(),
                               remainingBuffers.begin() +
                                   static_cast<std::vector<std::string_view>::difference_type>(advanced));

        // Adjust the partially sent first buffer
        if (!remainingBuffers.empty() && remaining > 0)
        {
            remainingBuffers[0] = remainingBuffers[0].substr(remaining);
        }
    }

    return totalSent;
}

std::size_t Socket::writeAtMostWithTimeout(std::string_view data, const int timeoutMillis) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "writeAtMostWithTimeout() called on invalid socket");

    if (data.empty())
        return 0;

    if (!waitReady(true /* forWrite */, timeoutMillis))
        throw SocketException(0, "Write timed out after " + std::to_string(timeoutMillis) + " ms");

    const auto len = send(_sockFd,
#ifdef _WIN32
                          data.data(), static_cast<int>(data.size()),
#else
                          data.data(), data.size(),
#endif
                          0);

    if (len == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (len == 0)
        throw SocketException(0, "Connection closed while writing.");

    return static_cast<std::size_t>(len);
}

std::size_t Socket::writeFrom(const void* data, std::size_t len) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "writeFrom() called on invalid socket");

    if (!data || len == 0)
        return 0;

    const auto* ptr = static_cast<const char*>(data);

    const auto sent = send(_sockFd,
#ifdef _WIN32
                           ptr, static_cast<int>(len),
#else
                           ptr, len,
#endif
                           0);

    if (sent == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (sent == 0)
        throw SocketException(0, "Connection closed while writing to socket.");

    return static_cast<std::size_t>(sent);
}

std::size_t Socket::writeFromAll(const void* data, const std::size_t len) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "writeFromAll() called on invalid socket");

    if (!data || len == 0)
        return 0;

    auto ptr = static_cast<const char*>(data);
    std::size_t totalSent = 0;

    while (totalSent < len)
    {
        const std::size_t remaining = len - totalSent;

        const auto sent = send(_sockFd,
#ifdef _WIN32
                               ptr + totalSent, static_cast<int>(remaining),
#else
                               ptr + totalSent, remaining,
#endif
                               0);

        if (sent == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

        if (sent == 0)
            throw SocketException(0, "Connection closed during writeFromAll().");

        totalSent += static_cast<std::size_t>(sent);
    }

    return totalSent;
}

std::size_t Socket::writeWithTotalTimeout(const std::string_view data, const int timeoutMillis) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "writeWithTotalTimeout() called on invalid socket");

    if (data.empty())
        return 0;

    const char* ptr = data.data();
    std::size_t totalSent = 0;
    std::size_t remaining = data.size();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMillis);

    while (remaining > 0)
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            throw SocketException(0, "Write operation timed out before completing");

        if (const auto remainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            !waitReady(true /* forWrite */, static_cast<int>(remainingTime)))
        {
            throw SocketException(0, "Socket not writable within remaining timeout window");
        }

        const auto sent = send(_sockFd,
#ifdef _WIN32
                               ptr + totalSent, static_cast<int>(remaining),
#else
                               ptr + totalSent, remaining,
#endif
                               0);

        if (sent == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

        if (sent == 0)
            throw SocketException(0, "Connection closed during timed write.");

        totalSent += static_cast<std::size_t>(sent);
        remaining -= static_cast<std::size_t>(sent);
    }

    return totalSent;
}

std::size_t Socket::writevWithTotalTimeout(std::span<const std::string_view> buffers, const int timeoutMillis) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "writevWithTotalTimeout() called on invalid socket");

    if (buffers.empty())
        return 0;

    std::vector pending(buffers.begin(), buffers.end());
    std::size_t totalSent = 0;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMillis);

    while (!pending.empty())
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            throw SocketException(0, "Timeout while writing vectorized buffers");

        if (const auto remainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            !waitReady(true /* forWrite */, static_cast<int>(remainingTime)))
            throw SocketException(0, "Socket not writable within remaining timeout");

        const std::size_t bytesSent = writev(pending);
        totalSent += bytesSent;

        // Determine how many buffers were fully sent
        std::size_t advanced = 0;
        std::size_t left = bytesSent;

        for (const auto& buf : pending)
        {
            if (left >= buf.size())
            {
                left -= buf.size();
                ++advanced;
            }
            else
            {
                break;
            }
        }

        pending.erase(pending.begin(),
                      pending.begin() + static_cast<std::vector<std::string_view>::difference_type>(advanced));

        // Adjust the partially sent buffer
        if (!pending.empty() && left > 0)
        {
            pending[0] = pending[0].substr(left);
        }
    }

    return totalSent;
}

std::size_t Socket::readv(std::span<BufferView> buffers) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "readv() called on invalid socket");

    if (buffers.empty())
        return 0;

#ifdef _WIN32
    std::vector<WSABUF> wsaBufs;
    wsaBufs.reserve(buffers.size());

    for (const auto& [data, size] : buffers)
    {
        WSABUF w;
        w.buf = static_cast<char*>(data);
        w.len = static_cast<ULONG>(size);
        wsaBufs.push_back(w);
    }

    DWORD bytesReceived = 0;
    DWORD flags = 0;
    const int result =
        WSARecv(_sockFd, wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()), &bytesReceived, &flags, nullptr, nullptr);
    if (result == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (bytesReceived == 0)
        throw SocketException(0, "Connection closed during readv().");

    return static_cast<std::size_t>(bytesReceived);

#else
    std::vector<iovec> ioVecs;
    ioVecs.reserve(buffers.size());

    for (const auto& [data, size] : buffers)
    {
        iovec io{};
        io.iov_base = data;
        io.iov_len = size;
        ioVecs.push_back(io);
    }

    const ssize_t bytes = ::readv(_sockFd, ioVecs.data(), static_cast<int>(ioVecs.size()));
    if (bytes < 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (bytes == 0)
        throw SocketException(0, "Connection closed during readv().");

    return static_cast<std::size_t>(bytes);
#endif
}

std::size_t Socket::readvAll(std::span<BufferView> buffers) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "readvAll() called on invalid socket");

    std::vector<BufferView> pending(buffers.begin(), buffers.end());
    std::size_t totalRead = 0;

    while (!pending.empty())
    {
        const std::size_t bytesRead = readv(pending);
        totalRead += bytesRead;

        // Advance through fully read buffers
        std::size_t advanced = 0;
        std::size_t left = bytesRead;

        for (const auto& [data, size] : pending)
        {
            if (left >= size)
            {
                left -= size;
                ++advanced;
            }
            else
            {
                break;
            }
        }

        pending.erase(pending.begin(),
                      pending.begin() + static_cast<std::vector<BufferView>::difference_type>(advanced));

        // Adjust partially read buffer
        if (!pending.empty() && left > 0)
        {
            auto& [data, size] = pending[0];
            data = static_cast<std::byte*>(data) + left;
            size -= left;
        }
    }

    return totalRead;
}

std::size_t Socket::readvAllWithTotalTimeout(std::span<BufferView> buffers, const int timeoutMillis) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "readvAllWithTotalTimeout() called on invalid socket");

    if (buffers.empty())
        return 0;

    std::vector pending(buffers.begin(), buffers.end());
    std::size_t totalRead = 0;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMillis);

    while (!pending.empty())
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            throw SocketException(0, "Timeout while reading into vector buffers");

        if (auto timeLeft = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            !waitReady(false /* forRead */, static_cast<int>(timeLeft)))
            throw SocketException(0, "Socket not readable within timeout");

        const std::size_t bytesRead = readv(pending);
        totalRead += bytesRead;

        // Advance through fully read buffers
        std::size_t advanced = 0;
        std::size_t left = bytesRead;

        for (const auto& [data, size] : pending)
        {
            if (left >= size)
            {
                left -= size;
                ++advanced;
            }
            else
            {
                break;
            }
        }

        pending.erase(pending.begin(),
                      pending.begin() + static_cast<std::vector<BufferView>::difference_type>(advanced));

        // Adjust the partially filled buffer
        if (!pending.empty() && left > 0)
        {
            pending[0].data = static_cast<std::byte*>(pending[0].data) + left;
            pending[0].size -= left;
        }
    }

    return totalRead;
}

std::size_t Socket::readvAtMostWithTimeout(const std::span<BufferView> buffers, const int timeoutMillis) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(0, "readvAtMostWithTimeout() called on invalid socket");

    if (buffers.empty())
        return 0;

    if (!waitReady(false /* forRead */, timeoutMillis))
        throw SocketException(0, "Socket not readable within timeout");

    return readv(buffers);
}
