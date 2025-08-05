#include "jsocketpp/DatagramSocket.hpp"
#include "jsocketpp/internal/ScopedBlockingMode.hpp"
#include "jsocketpp/SocketException.hpp"
#include "jsocketpp/SocketTimeoutException.hpp"

#include <optional>

using namespace jsocketpp;

DatagramSocket::DatagramSocket(const Port localPort, const std::string_view localAddress,
                               const std::optional<std::size_t> recvBufferSize,
                               const std::optional<std::size_t> sendBufferSize,
                               const std::optional<std::size_t> internalBufferSize, const bool reuseAddress,
                               const int soRecvTimeoutMillis, const int soSendTimeoutMillis, const bool nonBlocking,
                               const bool dualStack, const bool autoBind, const bool autoConnect,
                               const std::string_view remoteAddress, const Port remotePort,
                               const int connectTimeoutMillis)
    : SocketOptions(INVALID_SOCKET), _internalBuffer(internalBufferSize.value_or(DefaultBufferSize)), _port(localPort)
{
    const auto localAddrInfoPtr = internal::resolveAddress(
        localAddress, _port,
        // Specifies the address family: AF_UNSPEC allows both IPv4 (AF_INET) and IPv6 (AF_INET6)
        // addresses to be returned by getaddrinfo, enabling the server to support dual-stack networking
        // (accepting connections over both protocols). This makes the code portable and future-proof,
        // as it can handle whichever protocol is available on the system or preferred by clients.
        AF_UNSPEC,
        // Specifies the desired socket type. Setting it to `SOCK_DGRAM` requests a datagram socket (UDP),
        // which provides connectionless, unreliable, message-oriented communication—this is the standard
        // socket type for UDP. By specifying `SOCK_DGRAM`, the code ensures that only address results suitable
        // for UDP (as opposed to stream-based protocols like TCP, which use `SOCK_STREAM`) are returned by
        // `getaddrinfo`. This guarantees that the created socket will support the semantics required for UDP operation,
        // such as message delivery without connection establishment and no guarantee of delivery order or reliability.
        SOCK_DGRAM,
        // This field ensures that only address results suitable for UDP sockets are returned by
        // `getaddrinfo`. While `SOCK_DGRAM` already implies UDP in most cases, explicitly setting
        // `ai_protocol` to `IPPROTO_UDP` eliminates ambiguity and guarantees that the created socket
        // will use the UDP protocol. This is particularly important on platforms where multiple
        // protocols may be available for a given socket type, or where protocol selection is not
        // implicit. By specifying `IPPROTO_UDP`, the code ensures that the socket will provide
        // connectionless, unreliable, message-oriented communication as required for a UDP socket.
        IPPROTO_UDP,
        // This flag indicates that the returned socket addresses are intended for use in bind operations.
        // When `ai_flags` includes `AI_PASSIVE`, and the `node` argument to `getaddrinfo()` is `nullptr`,
        // the resulting address will be a "wildcard" address:
        // - For IPv4: INADDR_ANY
        // - For IPv6: in6addr_any
        //
        // This allows the socket to accept datagrams on **any local interface**, which is ideal for
        // server-side or bound sockets. If a specific `localAddress` is provided, `AI_PASSIVE` is ignored.
        //
        // Without this flag, `getaddrinfo()` would return addresses suitable for connecting to a peer,
        // not for binding a local endpoint.
        AI_PASSIVE);

    // Sort address candidates: prioritize IPv6 if dualStack is enabled, then IPv4 fallback
    std::vector<addrinfo*> sorted;
    for (addrinfo* p = localAddrInfoPtr.get(); p; p = p->ai_next)
        if (dualStack && p->ai_family == AF_INET6)
            sorted.push_back(p);
    for (addrinfo* p = localAddrInfoPtr.get(); p; p = p->ai_next)
        if (p->ai_family == AF_INET || !dualStack)
            sorted.push_back(p);

    // Attempt to create a socket using the sorted list of preferred addresses
    for (auto* p : sorted)
    {
        setSocketFd(::socket(p->ai_family, p->ai_socktype, p->ai_protocol));
        if (getSocketFd() != INVALID_SOCKET)
        {
            // If this is an IPv6 socket and dualStack is requested,
            // disable the IPV6_V6ONLY flag to allow it to receive IPv4-mapped datagrams as well.
            if (p->ai_family == AF_INET6)
            {
                try
                {
                    setIPv6Only(!dualStack); // false = allow dual-stack
                }
                catch (const SocketException&)
                {
                    cleanupAndRethrow();
                }
            }

            break; // success
        }
    }

    // If no socket could be created, throw an error
    if (getSocketFd() == INVALID_SOCKET)
        cleanupAndThrow(GetSocketError());

    const auto recvResolved = recvBufferSize.value_or(DefaultBufferSize);
    const auto sendResolved = sendBufferSize.value_or(DefaultBufferSize);
    const auto internalResolved = internalBufferSize.value_or(DefaultBufferSize);

    // Apply socket-level configuration
    try
    {
        // Enable or disable SO_REUSEADDR before binding
        setReuseAddress(reuseAddress);

        // Apply the internal buffer size for high-level read<T>()/read<string>() usage
        setInternalBufferSize(internalResolved);

        // Apply OS-level buffer size to the receive buffer (SO_RCVBUF)
        setReceiveBufferSize(recvResolved);

        // Apply OS-level buffer size to the send buffer (SO_SNDBUF)
        setSendBufferSize(sendResolved);

        // Set optional socket timeouts
        if (soRecvTimeoutMillis >= 0)
            setSoRecvTimeout(soRecvTimeoutMillis);
        if (soSendTimeoutMillis >= 0)
            setSoSendTimeout(soSendTimeoutMillis);

        // Set the socket to non-blocking mode if requested
        if (nonBlocking)
            setNonBlocking(true);
    }
    catch (const SocketException&)
    {
        cleanupAndRethrow();
    }

    // If requested, bind immediately to the given address and port
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

    if (autoConnect && !remoteAddress.empty() && remotePort != 0)
    {
        try
        {
            connect(remoteAddress, remotePort, connectTimeoutMillis);
        }
        catch (const SocketException&)
        {
            cleanupAndRethrow();
        }
    }
}

void DatagramSocket::cleanup()
{
    if (getSocketFd() != INVALID_SOCKET)
    {
        CloseSocket(getSocketFd());
        setSocketFd(INVALID_SOCKET);

        // CloseSocket error is ignored.
        // TODO: Consider adding an internal flag, nested exception, or user-configurable error handler
        //       to report errors in future versions.
    }
    _isBound = false;
    _isConnected = false;
}

void DatagramSocket::cleanupAndThrow(const int errorCode)
{
    cleanup();
    throw SocketException(errorCode, SocketErrorMessage(errorCode));
}

void DatagramSocket::cleanupAndRethrow()
{
    cleanup();
    throw;
}

DatagramSocket::~DatagramSocket() noexcept
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

void DatagramSocket::close()
{
    if (getSocketFd() != INVALID_SOCKET)
    {
        if (CloseSocket(getSocketFd()))
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));

        setSocketFd(INVALID_SOCKET);
    }
    _isBound = false;
    _isConnected = false;
}

void DatagramSocket::bind(const std::string_view host, const Port port)
{
    if (_isConnected)
    {
        throw SocketException("DatagramSocket::bind(): socket is already connected");
    }

    if (_isBound)
    {
        throw SocketException("DatagramSocket::bind(): socket is already bound");
    }

    const auto result = internal::resolveAddress(host, port, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, AI_PASSIVE);

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
            return;
        }
    }

    throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));
}

void DatagramSocket::bind(const Port port)
{
    bind("0.0.0.0", port);
}

void DatagramSocket::bind()
{
    bind("0.0.0.0", 0);
}

void DatagramSocket::connect(const std::string_view host, const Port port, const int timeoutMillis)
{
    if (_isConnected)
    {
        throw SocketException("connect() called on an already-connected socket");
    }

    // Resolve the remote address
    const auto remoteInfo = internal::resolveAddress(host, port, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP);
    const addrinfo* target = remoteInfo.get();
    if (!target)
    {
        throw SocketException("connect() failed: could not resolve target address");
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

    // Attempt to connect
    const int res = ::connect(getSocketFd(), target->ai_addr,
#ifdef _WIN32
                              static_cast<int>(target->ai_addrlen)
#else
                              target->ai_addrlen
#endif
    );

    if (res == SOCKET_ERROR)
    {
        auto error = GetSocketError();

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
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));

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

void DatagramSocket::disconnect()
{
    if (!_isConnected)
        return; // Already disconnected — no-op

    // Disconnect using AF_UNSPEC to break the peer association
    sockaddr_storage nullAddr{};
    nullAddr.ss_family = AF_UNSPEC;

    if (const int res = ::connect(getSocketFd(), reinterpret_cast<sockaddr*>(&nullAddr), sizeof(nullAddr));
        res == SOCKET_ERROR)
    {
        throw SocketException(GetSocketError(), "DatagramSocket::disconnect(): disconnect failed");
    }

    _isConnected = false;
}

size_t DatagramSocket::write(std::string_view message) const
{
    if (!_isConnected)
        throw SocketException("DatagramSocket is not connected.");

    if (message.empty())
        return 0; // Nothing to send

    int flags = 0;
#ifndef _WIN32
    flags = MSG_NOSIGNAL; // Don't send SIGPIPE on broken pipe
#endif
    const auto sent = ::send(getSocketFd(), message.data(),
#ifdef _WIN32
                             static_cast<int>(message.size()),
#else
                             message.size(),
#endif
                             flags);
    if (sent == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    return static_cast<size_t>(sent);
}

std::size_t DatagramSocket::write(const DatagramPacket& packet) const
{
    if (packet.buffer.empty())
        return 0;

    int flags = 0;
#ifndef _WIN32
    flags = MSG_NOSIGNAL;
#endif

    if (!packet.address.empty() && packet.port != 0)
    {
        const auto result = internal::resolveAddress(packet.address, packet.port, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP);

        const int sent = sendto(getSocketFd(), packet.buffer.data(),
#ifdef _WIN32
                                static_cast<int>(packet.buffer.size()),
#else
                                packet.buffer.size(),
#endif
                                flags, result->ai_addr,
#ifdef _WIN32
                                static_cast<int>(result->ai_addrlen));
#else
                                result->ai_addrlen);
#endif

        const int error = GetSocketError();
        if (sent < 0)
            throw SocketException(error, SocketErrorMessage(error));
        return static_cast<std::size_t>(sent);
    }

    if (!_isConnected)
        throw SocketException("write(packet): no destination specified and socket is not connected");

    const int sent = send(getSocketFd(), packet.buffer.data(),
#ifdef _WIN32
                          static_cast<int>(packet.buffer.size()),
#else
                          packet.buffer.size(),
#endif
                          flags);
    const int error = GetSocketError();
    if (sent < 0)
        throw SocketException(error, SocketErrorMessage(error));
    return static_cast<std::size_t>(sent);
}

size_t DatagramSocket::write(std::string_view message, const std::string_view host, const Port port) const
{
    if (message.empty())
        return 0; // Nothing to send

    // Prepare destination address with getaddrinfo (supports IPv4 and IPv6)
    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    int flags = 0;
#ifndef _WIN32
    flags = MSG_NOSIGNAL; // Don't send SIGPIPE on broken pipe
#endif

    const std::string portStr = std::to_string(port);
    addrinfo* destInfo = nullptr;

    if (const auto ret = getaddrinfo(host.data(), portStr.c_str(), &hints, &destInfo); ret != 0 || !destInfo)
    {
        throw SocketException(
#ifdef _WIN32
            GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
            ret, SocketErrorMessageWrap(ret, true));
#endif
    }

    const auto sent = sendto(getSocketFd(), message.data(),
#ifdef _WIN32
                             static_cast<int>(message.size()), // Windows: cast to int
#else
                             message.size(), // Linux/Unix: use size_t directly
#endif
                             flags, destInfo->ai_addr,
#ifdef _WIN32
                             static_cast<int>(destInfo->ai_addrlen) // Windows: cast to int
#else
                             destInfo->ai_addrlen // Linux/Unix: use socklen_t directly
#endif
    );
    freeaddrinfo(destInfo); // ignore errors from freeaddrinfo
    if (sent == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    return static_cast<size_t>(sent);
}

size_t DatagramSocket::read(DatagramPacket& packet, const bool resizeBuffer) const
{
    if (packet.buffer.empty())
        throw SocketException("DatagramPacket buffer is empty");

    sockaddr_storage srcAddr{};
    socklen_t addrLen = sizeof(srcAddr);

    // Receive the datagram
    const auto received = recvfrom(getSocketFd(), packet.buffer.data(),
#ifdef _WIN32
                                   static_cast<int>(packet.buffer.size()),
#else
                                   packet.buffer.size(),
#endif
                                   0, reinterpret_cast<sockaddr*>(&srcAddr), &addrLen);

    if (received == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (received == 0)
        throw SocketException("Connection closed by remote host.");

    // Fill in address and port fields
    char hostBuf[NI_MAXHOST], portBuf[NI_MAXSERV];
    if (const auto ret = getnameinfo(reinterpret_cast<const sockaddr*>(&srcAddr), addrLen, hostBuf, sizeof(hostBuf),
                                     portBuf, sizeof(portBuf), NI_NUMERICHOST | NI_NUMERICSERV);
        ret != 0)
    {
        throw SocketException(
#ifdef _WIN32
            GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
            ret, SocketErrorMessageWrap(ret, true));
#endif
    }

    packet.address = std::string(hostBuf);
    packet.port = static_cast<Port>(std::stoul(portBuf));

    if (resizeBuffer)
    {
        packet.buffer.resize(static_cast<size_t>(received));
    }

    return static_cast<size_t>(received);
}

std::string DatagramSocket::getLocalIp(const bool convertIPv4Mapped) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getLocalIp() failed: socket is not open.");

    sockaddr_storage localAddr{};
    socklen_t addrLen = sizeof(localAddr);

    if (::getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&localAddr), &addrLen) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));

    return ipFromSockaddr(reinterpret_cast<const sockaddr*>(&localAddr), convertIPv4Mapped);
}

Port DatagramSocket::getLocalPort() const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getLocalPort() failed: socket is not open.");

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (::getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));

    return portFromSockaddr(reinterpret_cast<const sockaddr*>(&addr));
}

std::string DatagramSocket::getLocalSocketAddress(const bool convertIPv4Mapped) const
{
    return getLocalIp(convertIPv4Mapped) + ":" + std::to_string(getLocalPort());
}

std::string DatagramSocket::getRemoteIp(const bool convertIPv4Mapped) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getRemoteIp() failed: socket is not open.");

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (isConnected())
    {
        if (::getpeername(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));
    }
    else
    {
        if (_remoteAddrLen == 0)
            throw SocketException("getRemoteIp() failed: no datagram received yet (unconnected socket).");

        addr = _remoteAddr;
        addrLen = _remoteAddrLen;
    }

    return ipFromSockaddr(reinterpret_cast<const sockaddr*>(&addr), convertIPv4Mapped);
}

Port DatagramSocket::getRemotePort() const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getRemotePort() failed: socket is not open.");

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (isConnected())
    {
        if (::getpeername(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));
    }
    else
    {
        if (_remoteAddrLen == 0)
            throw SocketException("getRemotePort() failed: no datagram received yet (unconnected socket).");

        addr = _remoteAddr;
        addrLen = _remoteAddrLen;
    }

    return portFromSockaddr(reinterpret_cast<const sockaddr*>(&addr));
}

std::string DatagramSocket::getRemoteSocketAddress(const bool convertIPv4Mapped) const
{
    return getRemoteIp(convertIPv4Mapped) + ":" + std::to_string(getRemotePort());
}

void DatagramSocket::setInternalBufferSize(const std::size_t newLen)
{
    _internalBuffer.resize(newLen);
    _internalBuffer.shrink_to_fit();
}
