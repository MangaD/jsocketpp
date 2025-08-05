#include "jsocketpp/ServerSocket.hpp"
#include "jsocketpp/SocketTimeoutException.hpp"

#include <thread>

using namespace jsocketpp;

ServerSocket::ServerSocket(const Port port, const std::string_view localAddress, const bool autoBindListen,
                           const bool reuseAddress, const int soTimeoutMillis, const bool dualStack)
    : SocketOptions(INVALID_SOCKET), _port(port)
{
    _srvAddrInfo = internal::resolveAddress(
        localAddress, _port,
        // Specifies the address family: AF_UNSPEC allows both IPv4 (AF_INET) and IPv6 (AF_INET6)
        // addresses to be returned by getaddrinfo, enabling the server to support dual-stack networking
        // (accepting connections over both protocols). This makes the code portable and future-proof,
        // as it can handle whichever protocol is available on the system or preferred by clients.
        AF_UNSPEC,
        // Specifies the desired socket type. Setting it to `SOCK_STREAM` requests a stream socket (TCP),
        // which provides reliable, connection-oriented, byte-stream communication—this is the standard
        // socket type for TCP. By specifying `SOCK_STREAM`, the code ensures that only address results suitable
        // for TCP (as opposed to datagram-based protocols like UDP, which use `SOCK_DGRAM`) are returned by
        // `getaddrinfo`. This guarantees that the created socket will support the semantics required for TCP server
        // operation, such as connection establishment, reliable data transfer, and orderly connection termination.
        SOCK_STREAM,
        // This field ensures that only address results suitable for TCP sockets are returned by
        // `getaddrinfo`. While `SOCK_STREAM` already implies TCP in most cases, explicitly setting
        // `ai_protocol` to `IPPROTO_TCP` eliminates ambiguity and guarantees that the created socket
        // will use the TCP protocol. This is particularly important on platforms where multiple
        // protocols may be available for a given socket type, or where protocol selection is not
        // implicit. By specifying `IPPROTO_TCP`, the code ensures that the socket will provide
        // reliable, connection-oriented, byte-stream communication as required for a TCP server.
        IPPROTO_TCP,
        // Setting `hints.ai_flags = AI_PASSIVE` indicates that the returned socket addresses
        // should be suitable for binding a server socket that will accept incoming connections.
        // When `AI_PASSIVE` is set and the `node` parameter to `getaddrinfo` is `nullptr`,
        // the resolved address will be a "wildcard address" (INADDR_ANY for IPv4 or IN6ADDR_ANY_INIT for IPv6),
        // allowing the server to accept connections on any local network interface.
        //
        // This is essential for server applications, as it enables them to listen for client
        // connections regardless of which network interface the client uses to connect.
        // If `AI_PASSIVE` were not set, the resolved address would be suitable for a client
        // socket (i.e., for connecting to a specific remote host), not for a server socket.
        // If the `node` parameter to `getaddrinfo` is not NULL, then the AI_PASSIVE flag is ignored.
        AI_PASSIVE);

    // First, try to create a socket with IPv6 (prioritized)
    for (addrinfo* p = _srvAddrInfo.get(); p != nullptr; p = p->ai_next)
    {
        if (p->ai_family == AF_INET6)
        {
            // Attempt to create a socket using the current address
            setSocketFd(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
            if (getSocketFd() == INVALID_SOCKET)
                continue; // If socket creation failed, try the next address

            // Store the address that worked
            _selectedAddrInfo = p;

            // Configure the socket for dual-stack (IPv4 + IPv6) or IPv6-only as requested
            try
            {
                setIPv6Only(!dualStack);
            }
            catch (const SocketException&)
            {
                cleanupAndRethrow();
            }

            break; // Exit after successfully creating the IPv6 socket
        }
    }

    // If no IPv6 address worked, fallback to IPv4
    if (getSocketFd() == INVALID_SOCKET)
    {
        for (addrinfo* p = _srvAddrInfo.get(); p != nullptr; p = p->ai_next)
        {
            // Only attempt IPv4 if no valid IPv6 address has been used
            if (p->ai_family == AF_INET)
            {
                // Attempt to create a socket using the current address (IPv4)
                setSocketFd(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
                if (getSocketFd() == INVALID_SOCKET)
                    continue; // If socket creation failed, try the next address

                // Store the address that worked
                _selectedAddrInfo = p;
                break; // Exit after successfully creating the IPv4 socket
            }
        }
    }

    if (getSocketFd() == INVALID_SOCKET)
    {
        cleanupAndThrow(GetSocketError());
    }

    // Set socket options for address reuse
    try
    {
        setReuseAddress(reuseAddress);
    }
    catch (const SocketException&)
    {
        cleanupAndRethrow();
    }

    // Set accept timeout as requested
    setSoTimeout(soTimeoutMillis);

    if (autoBindListen)
    {
        bind();
        listen();
    }
}

void ServerSocket::cleanup()
{
    if (getSocketFd() != INVALID_SOCKET)
    {
        CloseSocket(getSocketFd());
        setSocketFd(INVALID_SOCKET);

        // CloseSocket error is ignored.
        // TODO: Consider adding an internal flag, nested exception, or user-configurable error handler
        //       to report errors in future versions.
    }

    _srvAddrInfo.reset();
    _selectedAddrInfo = nullptr;
    _isBound = false;
    _isListening = false;
}

void ServerSocket::cleanupAndThrow(const int errorCode)
{
    cleanup();
    throw SocketException(errorCode, SocketErrorMessage(errorCode));
}

void ServerSocket::cleanupAndRethrow()
{
    cleanup();
    throw;
}

std::string ServerSocket::getLocalIp(const bool convertIPv4Mapped) const
{
    if (getSocketFd() == INVALID_SOCKET)
        return {};

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (::getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    return ipFromSockaddr(reinterpret_cast<const sockaddr*>(&addr), convertIPv4Mapped);
}

Port ServerSocket::getLocalPort() const
{
    if (getSocketFd() == INVALID_SOCKET)
        return 0;

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (::getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    return portFromSockaddr(reinterpret_cast<const sockaddr*>(&addr));
}

std::string ServerSocket::getLocalSocketAddress() const
{
    if (getSocketFd() == INVALID_SOCKET)
        return "";

    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);
    if (::getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &len) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    char host[INET6_ADDRSTRLEN]{};
    char serv[6]{};
    if (::getnameinfo(reinterpret_cast<sockaddr*>(&addr), len, host, sizeof(host), serv, sizeof(serv),
                      NI_NUMERICHOST | NI_NUMERICSERV) != 0)
        return "";

    return std::string(host) + ":" + serv;
}

ServerSocket::~ServerSocket() noexcept
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

void ServerSocket::close()
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
    _srvAddrInfo.reset();
    _selectedAddrInfo = nullptr;
    _isBound = false;
    _isListening = false;
}

void ServerSocket::bind()
{
    // Ensure that we have already selected an address during construction
    if (_selectedAddrInfo == nullptr)
        throw SocketException("bind() failed: no valid addrinfo found");

    const auto res = ::bind(
        getSocketFd(),              // The socket file descriptor to bind.
        _selectedAddrInfo->ai_addr, // Pointer to the sockaddr structure (address and port) to bind to.
#ifdef _WIN32
        static_cast<int>(_selectedAddrInfo->ai_addrlen) // Size of the sockaddr structure (cast to int for Windows).
#else
        _selectedAddrInfo->ai_addrlen // Size of the sockaddr structure (for Linux/Unix).
#endif
    );

    if (res == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    _isBound = true;
}

void ServerSocket::listen(const int backlog /* = 128 */)
{
    if (::listen(getSocketFd(), backlog) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    _isListening = true;
}

Socket ServerSocket::accept(const std::optional<std::size_t> recvBufferSize,
                            const std::optional<std::size_t> sendBufferSize,
                            const std::optional<std::size_t> internalBufferSize, const int soRecvTimeoutMillis,
                            const int soSendTimeoutMillis, const bool tcpNoDelay, const bool keepAlive,
                            const bool nonBlocking) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("Server socket is not initialized or already closed.");

    const auto [resolvedRecvBuf, resolvedSendBuf, resolvedInternalBuf] =
        resolveBuffers(recvBufferSize, sendBufferSize, internalBufferSize);

    if (!waitReady())
        throw SocketTimeoutException{};

    return acceptBlocking(resolvedRecvBuf, resolvedSendBuf, resolvedInternalBuf, soRecvTimeoutMillis,
                          soSendTimeoutMillis, tcpNoDelay, keepAlive, nonBlocking);
}

Socket ServerSocket::accept(int timeoutMillis, const std::optional<std::size_t> recvBufferSize,
                            const std::optional<std::size_t> sendBufferSize,
                            const std::optional<std::size_t> internalBufferSize, const int soRecvTimeoutMillis,
                            const int soSendTimeoutMillis, const bool tcpNoDelay, const bool keepAlive,
                            const bool nonBlocking) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("Server socket is not initialized or already closed.");

    const auto [resolvedRecvBuf, resolvedSendBuf, resolvedInternalBuf] =
        resolveBuffers(recvBufferSize, sendBufferSize, internalBufferSize);

    if (!waitReady(timeoutMillis))
        throw SocketTimeoutException{};

    return acceptBlocking(resolvedRecvBuf, resolvedSendBuf, resolvedInternalBuf, soRecvTimeoutMillis,
                          soSendTimeoutMillis, tcpNoDelay, keepAlive, nonBlocking);
}

std::optional<Socket> ServerSocket::tryAccept(const std::optional<std::size_t> recvBufferSize,
                                              const std::optional<std::size_t> sendBufferSize,
                                              const std::optional<std::size_t> internalBufferSize,
                                              const int soRecvTimeoutMillis, const int soSendTimeoutMillis,
                                              const bool tcpNoDelay, const bool keepAlive, const bool nonBlocking) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("Server socket is not initialized or already closed.");

    const auto [resolvedRecvBuf, resolvedSendBuf, resolvedInternalBuf] =
        resolveBuffers(recvBufferSize, sendBufferSize, internalBufferSize);

    if (!waitReady())
        return std::nullopt;

    return acceptBlocking(resolvedRecvBuf, resolvedSendBuf, resolvedInternalBuf, soRecvTimeoutMillis,
                          soSendTimeoutMillis, tcpNoDelay, keepAlive, nonBlocking);
}

std::optional<Socket> ServerSocket::tryAccept(int timeoutMillis, const std::optional<std::size_t> recvBufferSize,
                                              const std::optional<std::size_t> sendBufferSize,
                                              const std::optional<std::size_t> internalBufferSize,
                                              const int soRecvTimeoutMillis, const int soSendTimeoutMillis,
                                              const bool tcpNoDelay, const bool keepAlive, const bool nonBlocking) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("Server socket is not initialized or already closed.");

    const auto [resolvedRecvBuf, resolvedSendBuf, resolvedInternalBuf] =
        resolveBuffers(recvBufferSize, sendBufferSize, internalBufferSize);

    if (!waitReady(timeoutMillis))
        return std::nullopt;

    return acceptBlocking(resolvedRecvBuf, resolvedSendBuf, resolvedInternalBuf, soRecvTimeoutMillis,
                          soSendTimeoutMillis, tcpNoDelay, keepAlive, nonBlocking);
}

Socket ServerSocket::acceptBlocking(const std::optional<std::size_t> recvBufferSize,
                                    const std::optional<std::size_t> sendBufferSize,
                                    const std::optional<std::size_t> internalBufferSize, int soRecvTimeoutMillis,
                                    int soSendTimeoutMillis, bool tcpNoDelay, bool keepAlive, bool nonBlocking) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("Server socket is not initialized or already closed.");

    sockaddr_storage clientAddr{};
    socklen_t clientAddrLen = sizeof(clientAddr);

    const SOCKET clientSocket = ::accept(getSocketFd(), reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
    if (clientSocket == INVALID_SOCKET)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    const auto [recvResolved, sendResolved, internalResolved] =
        resolveBuffers(recvBufferSize, sendBufferSize, internalBufferSize);

    return {clientSocket,        clientAddr,          clientAddrLen, recvResolved, sendResolved, internalResolved,
            soRecvTimeoutMillis, soSendTimeoutMillis, tcpNoDelay,    keepAlive,    nonBlocking};
}

std::optional<Socket> ServerSocket::acceptNonBlocking(const std::optional<std::size_t> recvBufferSize,
                                                      const std::optional<std::size_t> sendBufferSize,
                                                      const std::optional<std::size_t> internalBufferSize,
                                                      const int soRecvTimeoutMillis, const int soSendTimeoutMillis,
                                                      const bool tcpNoDelay, const bool keepAlive,
                                                      const bool nonBlocking) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("Server socket is not initialized or already closed.");

    sockaddr_storage clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);

    const SOCKET clientSocket = ::accept(getSocketFd(), reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    if (clientSocket == INVALID_SOCKET)
    {
        const int err = GetSocketError();
#ifdef _WIN32
        if (err == WSAEWOULDBLOCK)
#else
        // NOLINTNEXTLINE
        if (err == EWOULDBLOCK || err == EAGAIN)
#endif
        {
            return std::nullopt;
        }
        throw SocketException(err, "accept() failed");
    }

    const auto [recvResolved, sendResolved, internalResolved] =
        resolveBuffers(recvBufferSize, sendBufferSize, internalBufferSize);

    return Socket(clientSocket, clientAddr, addrLen, recvResolved, sendResolved, internalResolved, soRecvTimeoutMillis,
                  soSendTimeoutMillis, tcpNoDelay, keepAlive, nonBlocking);
}

std::future<Socket> ServerSocket::acceptAsync(std::optional<std::size_t> recvBufferSize,
                                              std::optional<std::size_t> sendBufferSize,
                                              const std::optional<std::size_t> internalBufferSize,
                                              const int soRecvTimeoutMillis, const int soSendTimeoutMillis,
                                              const bool tcpNoDelay, const bool keepAlive, const bool nonBlocking) const
{
    // Note: capturing `this` is safe as long as the ServerSocket outlives the future.
    // Document this in your thread-safety notes!
    return std::async(std::launch::async,
                      [this, recvBufferSize, sendBufferSize, internalBufferSize, soRecvTimeoutMillis,
                       soSendTimeoutMillis, tcpNoDelay, keepAlive, nonBlocking]() -> Socket
                      {
                          return this->accept(recvBufferSize, sendBufferSize, internalBufferSize, soRecvTimeoutMillis,
                                              soSendTimeoutMillis, tcpNoDelay, keepAlive, nonBlocking);
                      });
}

void ServerSocket::acceptAsync(std::function<void(std::optional<Socket>, std::exception_ptr)> callback,
                               std::optional<std::size_t> recvBufferSize, std::optional<std::size_t> sendBufferSize,
                               const std::optional<std::size_t> internalBufferSize, const int soRecvTimeoutMillis,
                               const int soSendTimeoutMillis, const bool tcpNoDelay, const bool keepAlive,
                               const bool nonBlocking) const
{
    // Start a background thread to perform the blocking accept
    std::thread(
        [this, callback = std::move(callback), recvBufferSize, sendBufferSize, internalBufferSize, soRecvTimeoutMillis,
         soSendTimeoutMillis, tcpNoDelay, keepAlive, nonBlocking]()
        {
            try
            {
                // Try to accept a client (may throw on error)
                Socket client = this->accept(recvBufferSize, sendBufferSize, internalBufferSize, soRecvTimeoutMillis,
                                             soSendTimeoutMillis, tcpNoDelay, keepAlive, nonBlocking);
                callback(std::move(client), nullptr); // Success: call callback with client and null exception
            }
            catch (...)
            {
                // Failure: pass the exception to the callback
                callback(std::nullopt, std::current_exception());
            }
        })
        .detach(); // Detach so thread runs independently; lifetime is managed by the OS
}

bool ServerSocket::waitReady(const std::optional<int> timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
    {
        throw SocketException(EINVAL, "waitReady() failed: server socket is not initialized");
    }

    // Determine the effective timeout to use: user-provided or socket's configured default
    const int millis = timeoutMillis.value_or(_soTimeoutMillis);

#ifndef _WIN32
    // --- POSIX: Use poll() for readiness notification ---
    pollfd pfd{};
    pfd.fd = getSocketFd();
    pfd.events = POLLIN; // Monitor for readability (incoming connection)
    pfd.revents = 0;

    // poll() timeout: <0 = infinite, 0 = poll, >0 = wait up to millis
    const int timeout = (millis < 0) ? -1 : millis;
    int result = poll(&pfd, 1, timeout);
    if (result < 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }
    // poll() returns the number of fds with events. Check for POLLIN.
    return result > 0 && (pfd.revents & POLLIN);
#else
    // --- Windows: Use select() for readiness notification ---
    if (getSocketFd() >= FD_SETSIZE)
    {
        throw SocketException("waitReady() failed: socket descriptor exceeds FD_SETSIZE (" +
                              std::to_string(FD_SETSIZE) + "), select() cannot be used");
    }
    // Prepare the file descriptor set for select().
    // We want to monitor this server socket for readability (i.e., incoming connection).
    fd_set readFds;
    FD_ZERO(&readFds);               // Clear the set of file descriptors
    FD_SET(getSocketFd(), &readFds); // Add our server socket to the set

    // Prepare the timeout structure for select()
    timeval tv{};
    const timeval* tvPtr = nullptr; // Null pointer means select() will wait indefinitely

    if (millis > 0)
    {
        // Convert milliseconds to seconds + microseconds for struct timeval
        tv.tv_sec = millis / 1000;
        tv.tv_usec = (millis % 1000) * 1000;
        tvPtr = &tv; // Use this timeout in select()
    }
    else if (millis == 0)
    {
        // Zero timeout means select() returns immediately (polling)
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        tvPtr = &tv;
    }
    // If millis is negative, we leave tvPtr as nullptr — meaning select() will block indefinitely.

    // Call select() to wait for the server socket to become readable (i.e., a client is attempting to connect).
    int result = select(static_cast<int>(getSocketFd()) + 1, &readFds, nullptr, nullptr, tvPtr);
    if (result < 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }
    // select() returns the number of sockets that are ready.
    // If it's greater than 0, our server socket is ready to accept a connection.
    return result > 0;
#endif
}
