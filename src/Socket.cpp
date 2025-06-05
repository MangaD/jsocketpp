
#include "jsocketpp/Socket.hpp"

using namespace jsocketpp;

Socket::Socket(const SOCKET client, const sockaddr_storage& addr, const socklen_t len, const std::size_t bufferSize)
    : _sockFd(client), _remoteAddr(addr), _remoteAddrLen(len), _buffer(bufferSize)
{
}

Socket::Socket(const std::string_view host, const unsigned short port, const std::size_t bufferSize)
    : _remoteAddr{}, _buffer(bufferSize)
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
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
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

void Socket::setBufferSize(std::size_t newLen)
{
    _buffer.resize(newLen);
    _buffer.shrink_to_fit();
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

void Socket::setTimeout(int millis, bool forRead, bool forWrite) const
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
