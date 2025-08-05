#include "jsocketpp/Socket.hpp"

#include "jsocketpp/internal/ScopedBlockingMode.hpp"
#include "jsocketpp/SocketTimeoutException.hpp"

#include <chrono>
#include <cstring> // std::memcpy
#include <span>

using namespace jsocketpp;

Socket::Socket(const SOCKET client, const sockaddr_storage& addr, const socklen_t len, const std::size_t recvBufferSize,
               const std::size_t sendBufferSize, const std::size_t internalBufferSize, const int soRecvTimeoutMillis,
               const int soSendTimeoutMillis, const bool tcpNoDelay, const bool keepAlive, const bool nonBlocking)
    : SocketOptions(client), _remoteAddr(addr), _remoteAddrLen(len), _internalBuffer(internalBufferSize)
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("Socket(SOCKET): invalid socket descriptor.");

    try
    {
        setReceiveBufferSize(recvBufferSize);
        setSendBufferSize(sendBufferSize);
        setInternalBufferSize(internalBufferSize);
        setTcpNoDelay(tcpNoDelay);
        setKeepAlive(keepAlive);
        setNonBlocking(nonBlocking);

        if (soRecvTimeoutMillis >= 0)
            setSoRecvTimeout(soRecvTimeoutMillis);
        if (soSendTimeoutMillis >= 0)
            setSoSendTimeout(soSendTimeoutMillis);
    }
    catch (const SocketException&)
    {
        cleanupAndRethrow();
    }
}

Socket::Socket(const std::string_view host, const Port port, const std::optional<std::size_t> recvBufferSize,
               const std::optional<std::size_t> sendBufferSize, const std::optional<std::size_t> internalBufferSize,
               const bool reuseAddress, const int soRecvTimeoutMillis, const int soSendTimeoutMillis,
               const bool dualStack, const bool tcpNoDelay, const bool keepAlive, const bool nonBlocking,
               const bool autoConnect, const bool autoBind, const std::string_view localAddress, const Port localPort)
    : SocketOptions(INVALID_SOCKET), _remoteAddr{}, _remoteAddrLen(0),
      _internalBuffer(internalBufferSize.value_or(DefaultBufferSize))
{
    _cliAddrInfo = internal::resolveAddress(host, port, dualStack ? AF_UNSPEC : AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Try each candidate until socket creation succeeds
    for (addrinfo* p = _cliAddrInfo.get(); p != nullptr; p = p->ai_next)
    {
        setSocketFd(::socket(p->ai_family, p->ai_socktype, p->ai_protocol));
        if (getSocketFd() != INVALID_SOCKET)
        {
            _selectedAddrInfo = p;
            break;
        }
    }

    if (getSocketFd() == INVALID_SOCKET)
        cleanupAndThrow(GetSocketError());

    // --- Configure socket options before connect ---
    setReuseAddress(reuseAddress);
    setInternalBufferSize(_internalBuffer.size());
    setReceiveBufferSize(recvBufferSize.value_or(DefaultBufferSize));
    setSendBufferSize(sendBufferSize.value_or(DefaultBufferSize));
    setTcpNoDelay(tcpNoDelay);
    setKeepAlive(keepAlive);
    setNonBlocking(nonBlocking);

    if (soRecvTimeoutMillis >= 0)
        setSoRecvTimeout(soRecvTimeoutMillis);

    if (soSendTimeoutMillis >= 0)
        setSoSendTimeout(soSendTimeoutMillis);

    if (autoBind)
    {
        try
        {
            bind(localAddress, localPort);
        }
        catch (const SocketException&)
        {
            cleanupAndRethrow();
        }
    }

    if (autoConnect)
    {
        // Blocking connect; user may later call non-blocking connect with timeout explicitly
        connect();
    }
}

void Socket::cleanup()
{
    if (getSocketFd() != INVALID_SOCKET)
    {
        CloseSocket(getSocketFd());
        setSocketFd(INVALID_SOCKET);
        // CloseSocket error is ignored.
        // TODO: Consider adding an internal flag, nested exception, or user-configurable error handler
        //       to report errors in future versions.
    }

    _cliAddrInfo.reset();
    _selectedAddrInfo = nullptr;
    _isBound = false;
    _isConnected = false;
    resetShutdownFlags();
}

void Socket::cleanupAndThrow(const int errorCode)
{
    cleanup();
    throw SocketException(errorCode, SocketErrorMessage(errorCode));
}

void Socket::cleanupAndRethrow()
{
    cleanup();
    throw; // Preserve original exception
}

void Socket::bind(const std::string_view localHost, const Port port)
{
    if (_isConnected)
    {
        throw SocketException("Socket::bind(): socket is already connected");
    }

    if (_isBound)
    {
        throw SocketException("Socket::bind(): socket is already bound");
    }

    const internal::AddrinfoPtr result =
        internal::resolveAddress(localHost, port, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, AI_PASSIVE);

    for (const addrinfo* p = result.get(); p != nullptr; p = p->ai_next)
    {
        if (::bind(getSocketFd(), p->ai_addr,
#ifdef _WIN32
                   static_cast<int>(p->ai_addrlen)
#else
                   p->ai_addrlen
#endif
                       ) == 0)
        {
            _isBound = true;
            return; // success
        }
    }

    const int error = GetSocketError();
    throw SocketException(error, SocketErrorMessageWrap(error));
}

void Socket::bind(const Port port)
{
    bind("0.0.0.0", port);
}

void Socket::bind()
{
    bind("0.0.0.0", 0);
}

void Socket::connect(const int timeoutMillis)
{
    if (_isConnected)
    {
        throw SocketException("connect() called on an already-connected socket");
    }

    // Ensure that we have already selected an address during construction
    if (_selectedAddrInfo == nullptr)
    {
        throw SocketException("connect() failed: no valid addrinfo found");
    }

    // Determine if we should use non-blocking connect logic
    const bool useNonBlocking = (timeoutMillis >= 0);

    // Automatically switch to non-blocking if needed, and restore original mode later
    // NOLINTNEXTLINE - Temporarily switch to non-blocking mode (RAII-reverted)
    std::optional<internal::ScopedBlockingMode> blockingGuard;
    if (useNonBlocking)
    {
        blockingGuard.emplace(getSocketFd(), true); // Set non-blocking temporarily
    }

    // Attempt to initiate the connection
    const auto res = ::connect(getSocketFd(), _selectedAddrInfo->ai_addr,
#ifdef _WIN32
                               static_cast<int>(_selectedAddrInfo->ai_addrlen)
#else
                               _selectedAddrInfo->ai_addrlen
#endif
    );

    if (res == SOCKET_ERROR)
    {
        const int error = GetSocketError();

        // On most platforms, these errors indicate a non-blocking connection in progress
#ifdef _WIN32
        const bool wouldBlock = (error == WSAEINPROGRESS || error == WSAEWOULDBLOCK);
#else
        const bool wouldBlock = (error == EINPROGRESS || error == EWOULDBLOCK);
#endif

        if (!useNonBlocking || !wouldBlock)
        {
            throw SocketException(error, SocketErrorMessage(error));
        }

        // Check FD_SETSIZE limit before using select()
        if (getSocketFd() >= FD_SETSIZE)
        {
            throw SocketException("connect(): socket descriptor exceeds FD_SETSIZE (" + std::to_string(FD_SETSIZE) +
                                  "), select() cannot be used");
        }

        // Wait until socket becomes writable (connection ready or failed)
        timeval tv{};
        tv.tv_sec = timeoutMillis / 1000;
        tv.tv_usec = (timeoutMillis % 1000) * 1000;

        fd_set writeFds;
        FD_ZERO(&writeFds);
        FD_SET(getSocketFd(), &writeFds);

#ifdef _WIN32
        const int selectResult = ::select(0, nullptr, &writeFds, nullptr, &tv);
#else
        int selectResult;
        do
        {
            selectResult = ::select(getSocketFd() + 1, nullptr, &writeFds, nullptr, &tv);
        } while (selectResult < 0 && errno == EINTR);
#endif

        if (selectResult == 0)
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE,
                                         "Connection timed out after " + std::to_string(timeoutMillis) + " ms");
        if (selectResult < 0)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessageWrap(error));
        }

        // Even if select() reports writable, we must check if the connection actually succeeded
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        // SO_ERROR is always retrieved as int (POSIX & Windows agree on semantics)
        if (::getsockopt(getSocketFd(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len) < 0 ||
            so_error != 0)
        {
            throw SocketException(so_error, SocketErrorMessage(so_error));
        }
    }

    _isConnected = true;
    // Socket mode will be restored automatically via ScopedBlockingMode destructor
}

Socket::~Socket() noexcept
{
    try
    {
        close();
    }
    catch (...)
    {
        // Suppress all exceptions to maintain noexcept guarantee.
        // TODO: Consider adding an internal flag or user-configurable error handler
        //       to report destructor-time errors in future versions.
    }
}

/**
 * @brief Close the socket.
 * @throws SocketException on error.
 */
void Socket::close()
{
    if (getSocketFd() != INVALID_SOCKET)
    {
        if (CloseSocket(getSocketFd()))
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessageWrap(error));
        }

        setSocketFd(INVALID_SOCKET);
    }
    _cliAddrInfo.reset();
    _selectedAddrInfo = nullptr;
    _isBound = false;
    _isConnected = false;
    resetShutdownFlags();
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
    if (getSocketFd() != INVALID_SOCKET)
    {
        if (::shutdown(getSocketFd(), shutdownType))
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessageWrap(error));
        }
    }
}

std::string Socket::getLocalIp(const bool convertIPv4Mapped) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getLocalIp() failed: socket is not open.");

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (::getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    return ipFromSockaddr(reinterpret_cast<const sockaddr*>(&addr), convertIPv4Mapped);
}

Port Socket::getLocalPort() const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getLocalPort() failed: socket is not open.");

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (::getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    return portFromSockaddr(reinterpret_cast<const sockaddr*>(&addr));
}

std::string Socket::getLocalSocketAddress(const bool convertIPv4Mapped) const
{
    return getLocalIp(convertIPv4Mapped) + ":" + std::to_string(getLocalPort());
}

std::string Socket::getRemoteIp(const bool convertIPv4Mapped) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getRemoteIp() failed: socket is not open.");

    sockaddr_storage remoteAddr{};
    socklen_t addrLen = sizeof(remoteAddr);

    if (::getpeername(getSocketFd(), reinterpret_cast<sockaddr*>(&remoteAddr), &addrLen) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    return ipFromSockaddr(reinterpret_cast<const sockaddr*>(&remoteAddr), convertIPv4Mapped);
}

Port Socket::getRemotePort() const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getRemotePort() failed: socket is not open.");

    sockaddr_storage remoteAddr{};
    socklen_t addrLen = sizeof(remoteAddr);

    if (::getpeername(getSocketFd(), reinterpret_cast<sockaddr*>(&remoteAddr), &addrLen) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    return portFromSockaddr(reinterpret_cast<const sockaddr*>(&remoteAddr));
}

std::string Socket::getRemoteSocketAddress(bool convertIPv4Mapped) const
{
    return getRemoteIp(convertIPv4Mapped) + ":" + std::to_string(getRemotePort());
}

size_t Socket::write(std::string_view message) const
{
    int flags = 0;
#ifndef _WIN32
    flags = MSG_NOSIGNAL; // Prevent SIGPIPE on write to a closed socket (POSIX)
#endif
    const auto len = send(getSocketFd(), message.data(),
#ifdef _WIN32
                          static_cast<int>(message.size()), // Windows: cast to int
#else
                          static_cast<size_t>(message.size()), // Linux/Unix: use size_t directly
#endif
                          flags);
    if (len == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
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
        if (sent == 0)
            throw SocketException("Connection closed during writeAll()");
        totalSent += static_cast<std::size_t>(sent);
    }
    return totalSent;
}

void Socket::setInternalBufferSize(const std::size_t newLen)
{
    _internalBuffer.resize(newLen);
    _internalBuffer.shrink_to_fit();
}

bool Socket::waitReady(bool forWrite, const int timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("Invalid socket");

    // Guard against file descriptors exceeding FD_SETSIZE, which causes UB in FD_SET()
    if (getSocketFd() >= FD_SETSIZE)
    {
        throw SocketException("Socket descriptor exceeds FD_SETSIZE (" + std::to_string(FD_SETSIZE) +
                              "), cannot use select()");
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(getSocketFd(), &fds);

    // Default to zero-timeout for non-blocking poll
    timeval tv{0, 0};
    if (timeoutMillis >= 0)
    {
        tv.tv_sec = timeoutMillis / 1000;
        tv.tv_usec = (timeoutMillis % 1000) * 1000;
    }

    int result;

#ifdef _WIN32
    // On Windows, the first argument to select() is ignored but must be >= 0.
    result = select(0, forWrite ? nullptr : &fds, forWrite ? &fds : nullptr, nullptr, &tv);
#else
    // On POSIX, first argument must be the highest fd + 1
    result =
        select(static_cast<int>(getSocketFd()) + 1, forWrite ? nullptr : &fds, forWrite ? &fds : nullptr, nullptr, &tv);
#endif

    if (result < 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    return result > 0;
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
        throw SocketException("Invalid address format: " + str);

    const std::string host = str.substr(0, pos);
    const Port port = static_cast<Port>(std::stoi(str.substr(pos + 1)));

    const internal::AddrinfoPtr res =
        internal::resolveAddress(host, port, AF_UNSPEC, SOCK_STREAM, 0, AI_NUMERICHOST | AI_NUMERICSERV);

    std::memcpy(&addr, res->ai_addr, res->ai_addrlen);
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

        const auto len = recv(getSocketFd(), result.data() + totalRead,
#ifdef _WIN32
                              static_cast<int>(remaining),
#else
                              remaining,
#endif
                              0);

        if (len == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }

        if (len == 0)
            throw SocketException("Connection closed before reading all data.");

        totalRead += static_cast<std::size_t>(len);
    }

    return result;
}

std::string Socket::readUntil(const char delimiter, const std::size_t maxLen, const bool includeDelimiter)
{
    if (maxLen == 0)
    {
        throw SocketException("readUntil: maxLen must be greater than 0.");
    }

    std::string result;
    result.reserve(std::min<std::size_t>(128, maxLen)); // Preallocate small buffer

    std::size_t totalRead = 0;

    while (totalRead < maxLen)
    {
        const std::size_t toRead = (std::min) (_internalBuffer.size(), maxLen - totalRead);
        const auto len = recv(getSocketFd(), _internalBuffer.data(),
#ifdef _WIN32
                              static_cast<int>(toRead),
#else
                              toRead,
#endif
                              0);

        if (len == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }

        if (len == 0)
            throw SocketException("readUntil: connection closed before delimiter was found.");

        for (std::size_t i = 0; i < static_cast<std::size_t>(len); ++i)
        {
            const char ch = _internalBuffer[i];
            if (totalRead >= maxLen)
            {
                throw SocketException("readUntil: exceeded maximum read limit without finding delimiter.");
            }

            result.push_back(ch);
            ++totalRead;

            if (ch == delimiter)
            {
                if (!includeDelimiter)
                {
                    result.pop_back();
                }
                return result;
            }
        }
    }

    throw SocketException("readUntil: maximum length reached without finding delimiter.");
}

std::string Socket::readAtMost(std::size_t n) const
{
    if (n == 0)
    {
        // Nothing to read, return empty string immediately
        return {};
    }

    std::string result(n, '\0'); // Preallocate n bytes initialized to null

    const auto len = recv(getSocketFd(), result.data(),
#ifdef _WIN32
                          static_cast<int>(n),
#else
                          n,
#endif
                          0);

    if (len == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (len == 0)
    {
        throw SocketException("readAtMost: connection closed by remote host.");
    }

    result.resize(static_cast<std::size_t>(len)); // Trim to actual number of bytes read
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
        const auto bytesRead = recv(getSocketFd(), out + totalRead,
#ifdef _WIN32
                                    static_cast<int>(len - totalRead),
#else
                                    len - totalRead,
#endif
                                    0);

        if (bytesRead == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }

        if (bytesRead == 0)
        {
            if (exact)
                throw SocketException("Connection closed before full read completed.");
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
        throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE,
                                     "Read timed out after waiting " + std::to_string(timeoutMillis) + " ms");

    std::string result;
    result.resize(n); // max allocation

    const auto len = recv(getSocketFd(), result.data(),
#ifdef _WIN32
                          static_cast<int>(n),
#else
                          n,
#endif
                          0);

    if (len == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (len == 0)
        throw SocketException("Connection closed before data could be read.");

    result.resize(static_cast<std::size_t>(len)); // shrink to actual
    return result;
}

std::string Socket::readAvailable() const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("readAvailable() called on invalid socket");

#ifdef _WIN32
    u_long bytesAvailable = 0;
    if (ioctlsocket(getSocketFd(), FIONREAD, &bytesAvailable) != 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
#else
    int bytesAvailable = 0;
    if (ioctl(getSocketFd(), FIONREAD, &bytesAvailable) < 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
#endif

    if (bytesAvailable <= 0)
        return {};

    std::string result;
    result.resize(static_cast<std::size_t>(bytesAvailable));

    const auto len = recv(getSocketFd(), result.data(),
#ifdef _WIN32
                          static_cast<int>(bytesAvailable),
#else
                          static_cast<std::size_t>(bytesAvailable),
#endif
                          0);

    if (len == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (len == 0)
        throw SocketException("Connection closed while attempting to read available data.");

    result.resize(static_cast<std::size_t>(len)); // shrink to actual read
    return result;
}

std::size_t Socket::readIntoAvailable(void* buffer, const std::size_t bufferSize) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("readIntoAvailable() called on invalid socket");

    if (buffer == nullptr || bufferSize == 0)
        return 0;

#ifdef _WIN32
    u_long bytesAvailable = 0;
    if (ioctlsocket(getSocketFd(), FIONREAD, &bytesAvailable) != 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
#else
    int bytesAvailable = 0;
    if (ioctl(getSocketFd(), FIONREAD, &bytesAvailable) < 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
#endif

    if (bytesAvailable <= 0)
        return 0;

    const std::size_t toRead = std::min<std::size_t>(static_cast<std::size_t>(bytesAvailable), bufferSize);

    const auto len = recv(getSocketFd(),
#ifdef _WIN32
                          static_cast<char*>(buffer), static_cast<int>(toRead),
#else
                          buffer, toRead,
#endif
                          0);

    if (len == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (len == 0)
        throw SocketException("Connection closed while attempting to read available data.");

    return static_cast<std::size_t>(len);
}

std::string Socket::peek(std::size_t n) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("peek() called on invalid socket");

    if (n == 0)
        return {};

    std::string result;
    result.resize(n);

    const auto len = recv(getSocketFd(), result.data(),
#ifdef _WIN32
                          static_cast<int>(n),
#else
                          n,
#endif
                          MSG_PEEK);

    if (len == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (len == 0)
        throw SocketException("Connection closed during peek operation.");

    result.resize(static_cast<std::size_t>(len)); // trim to actual
    return result;
}

void Socket::discard(const std::size_t n, const std::size_t chunkSize /* = 1024 */) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("discard(): attempted on invalid socket.");

    if (n == 0)
        return;

    if (chunkSize == 0)
        throw SocketException("discard(): chunkSize must be greater than zero.");

    std::vector<char> tempBuffer(chunkSize); // Heap-allocated scratch buffer
    std::size_t totalDiscarded = 0;

    while (totalDiscarded < n)
    {
        const std::size_t toRead = (std::min) (chunkSize, n - totalDiscarded);

        const auto len = recv(getSocketFd(), tempBuffer.data(),
#ifdef _WIN32
                              static_cast<int>(toRead),
#else
                              toRead,
#endif
                              0);

        if (len == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }

        if (len == 0)
        {
            throw SocketException("discard(): connection closed before all bytes were discarded.");
        }

        totalDiscarded += static_cast<std::size_t>(len);
    }
}

std::size_t Socket::writev(std::span<const std::string_view> buffers) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("writev() called on invalid socket");

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
            WSASend(getSocketFd(), wsabufs.data(), static_cast<DWORD>(wsabufs.size()), &bytesSent, 0, nullptr, nullptr);
        result == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

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

    ssize_t bytesSent = ::writev(getSocketFd(), iovecs.data(), static_cast<int>(iovecs.size()));
    if (bytesSent == -1)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

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
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("writeAtMostWithTimeout() called on invalid socket");

    if (data.empty())
        return 0;

    if (!waitReady(true /* forWrite */, timeoutMillis))
        throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE,
                                     "Write timed out after " + std::to_string(timeoutMillis) + " ms");

    const auto len = send(getSocketFd(),
#ifdef _WIN32
                          data.data(), static_cast<int>(data.size()),
#else
                          data.data(), data.size(),
#endif
                          0);

    if (len == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
    if (len == 0)
        throw SocketException("Connection closed while writing.");

    return static_cast<std::size_t>(len);
}

std::size_t Socket::writeFrom(const void* data, std::size_t len) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("writeFrom() called on invalid socket");

    if (!data || len == 0)
        return 0;

    const auto* ptr = static_cast<const char*>(data);

    const auto sent = send(getSocketFd(),
#ifdef _WIN32
                           ptr, static_cast<int>(len),
#else
                           ptr, len,
#endif
                           0);

    if (sent == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (sent == 0)
        throw SocketException("Connection closed while writing to socket.");

    return static_cast<std::size_t>(sent);
}

std::size_t Socket::writeFromAll(const void* data, const std::size_t len) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("writeFromAll() called on invalid socket");

    if (!data || len == 0)
        return 0;

    auto ptr = static_cast<const char*>(data);
    std::size_t totalSent = 0;

    while (totalSent < len)
    {
        const std::size_t remaining = len - totalSent;

        const auto sent = send(getSocketFd(),
#ifdef _WIN32
                               ptr + totalSent, static_cast<int>(remaining),
#else
                               ptr + totalSent, remaining,
#endif
                               0);

        if (sent == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }
        if (sent == 0)
            throw SocketException("Connection closed during writeFromAll().");

        totalSent += static_cast<std::size_t>(sent);
    }

    return totalSent;
}

std::size_t Socket::writeWithTotalTimeout(const std::string_view data, const int timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("writeWithTotalTimeout() called on invalid socket");

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
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "Write operation timed out before completing");

        if (const auto remainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            !waitReady(true /* forWrite */, static_cast<int>(remainingTime)))
        {
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "Socket not writable within remaining timeout window");
        }

        const auto sent = send(getSocketFd(),
#ifdef _WIN32
                               ptr + totalSent, static_cast<int>(remaining),
#else
                               ptr + totalSent, remaining,
#endif
                               0);

        if (sent == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }
        if (sent == 0)
            throw SocketException("Connection closed during timed write.");

        totalSent += static_cast<std::size_t>(sent);
        remaining -= static_cast<std::size_t>(sent);
    }

    return totalSent;
}

std::size_t Socket::writevWithTotalTimeout(std::span<const std::string_view> buffers, const int timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("writevWithTotalTimeout() called on invalid socket");

    if (buffers.empty())
        return 0;

    std::vector pending(buffers.begin(), buffers.end());
    std::size_t totalSent = 0;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMillis);

    while (!pending.empty())
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "Timeout while writing vectorized buffers");

        if (const auto remainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            !waitReady(true /* forWrite */, static_cast<int>(remainingTime)))
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "Socket not writable within remaining timeout");

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
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("readv() called on invalid socket");

    if (buffers.empty())
        return 0;

#ifdef _WIN32
    auto wsaBufs = internal::toWSABUF(buffers);

    DWORD bytesReceived = 0;
    DWORD flags = 0;
    const int result = WSARecv(getSocketFd(), wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()), &bytesReceived,
                               &flags, nullptr, nullptr);
    if (result == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
    if (bytesReceived == 0)
        throw SocketException("Connection closed during readv().");

    return static_cast<std::size_t>(bytesReceived);

#else
    const auto ioVecs = internal::toIOVec(buffers);
    const ssize_t bytes = ::readv(getSocketFd(), ioVecs.data(), static_cast<int>(ioVecs.size()));
    if (bytes < 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (bytes == 0)
        throw SocketException("Connection closed during readv().");

    return static_cast<std::size_t>(bytes);
#endif
}

std::size_t Socket::readvAll(std::span<BufferView> buffers) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("readvAll() called on invalid socket");

    std::vector pending(buffers.begin(), buffers.end());
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
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("readvAllWithTotalTimeout() called on invalid socket");

    if (buffers.empty())
        return 0;

    std::vector pending(buffers.begin(), buffers.end());
    std::size_t totalRead = 0;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMillis);

    while (!pending.empty())
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "Timeout while reading into vector buffers");

        if (const auto timeLeft = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            !waitReady(false /* forRead */, static_cast<int>(timeLeft)))
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "Socket not readable within timeout");

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
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("readvAtMostWithTimeout() called on invalid socket");

    if (buffers.empty())
        return 0;

    if (!waitReady(false /* forRead */, timeoutMillis))
        throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "Socket not readable within timeout");

    return readv(buffers);
}

std::size_t Socket::writevFrom(std::span<const BufferView> buffers) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("writevFrom() called on invalid socket");

    if (buffers.empty())
        return 0;

#ifdef _WIN32
    auto wsaBufs = internal::toWSABUF(buffers);

    DWORD bytesSent = 0;
    if (const int result =
            WSASend(getSocketFd(), wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()), &bytesSent, 0, nullptr, nullptr);
        result == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    return static_cast<std::size_t>(bytesSent);

#else
    const auto ioVecs = internal::toIOVec(buffers);

    const ssize_t written = ::writev(getSocketFd(), ioVecs.data(), static_cast<int>(ioVecs.size()));
    if (written < 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    return static_cast<std::size_t>(written);
#endif
}

std::size_t Socket::writevFromAll(std::span<BufferView> buffers) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("writevFromAll() called on invalid socket");

    std::vector pending(buffers.begin(), buffers.end());
    std::size_t totalSent = 0;

    while (!pending.empty())
    {
        const std::size_t bytesSent = writevFrom(pending);
        totalSent += bytesSent;

        std::size_t advanced = 0;
        std::size_t remaining = bytesSent;

        for (const auto& [data, size] : pending)
        {
            if (remaining >= size)
            {
                remaining -= size;
                ++advanced;
            }
            else
            {
                break;
            }
        }

        pending.erase(pending.begin(),
                      pending.begin() + static_cast<std::vector<BufferView>::difference_type>(advanced));

        // Adjust first partially sent buffer
        if (!pending.empty() && remaining > 0)
        {
            pending[0].data = static_cast<std::byte*>(pending[0].data) + remaining;
            pending[0].size -= remaining;
        }
    }

    return totalSent;
}

std::size_t Socket::writevFromWithTotalTimeout(std::span<BufferView> buffers, const int timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("writevFromWithTotalTimeout() called on invalid socket");

    if (buffers.empty())
        return 0;

    std::vector pending(buffers.begin(), buffers.end());
    std::size_t totalSent = 0;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMillis);

    while (!pending.empty())
    {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline)
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "Timeout while writing binary buffers");

        if (const auto remainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            !waitReady(true /* forWrite */, static_cast<int>(remainingTime)))
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "Socket not writable within remaining timeout");

        const std::size_t bytesSent = writevFrom(pending);
        totalSent += bytesSent;

        std::size_t advanced = 0;
        std::size_t left = bytesSent;

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

        if (!pending.empty() && left > 0)
        {
            pending[0].data = static_cast<std::byte*>(pending[0].data) + left;
            pending[0].size -= left;
        }
    }

    return totalSent;
}
