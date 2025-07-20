#include "jsocketpp/ServerSocket.hpp"
#include "jsocketpp/SocketTimeoutException.hpp"

#include <thread>

using namespace jsocketpp;

ServerSocket::ServerSocket(const Port port, const std::string_view localAddress, const bool autoBindListen,
                           const bool reuseAddress, const int soTimeoutMillis, const bool dualStack)
    : _port(port)
{
    // Prepare the hints structure for getaddrinfo to specify the desired socket type and protocol.
    addrinfo hints{}; // Initialize the hints structure to zero

    // Specifies the address family: AF_UNSPEC allows both IPv4 (AF_INET) and IPv6 (AF_INET6)
    // addresses to be returned by getaddrinfo, enabling the server to support dual-stack networking
    // (accepting connections over both protocols). This makes the code portable and future-proof,
    // as it can handle whichever protocol is available on the system or preferred by clients.
    hints.ai_family = AF_UNSPEC;

    // Specifies the desired socket type. Setting it to `SOCK_STREAM` requests a stream socket (TCP),
    // which provides reliable, connection-oriented, byte-stream communication—this is the standard
    // socket type for TCP. By specifying `SOCK_STREAM`, the code ensures that only address results suitable
    // for TCP (as opposed to datagram-based protocols like UDP, which use `SOCK_DGRAM`) are returned by `getaddrinfo`.
    // This guarantees that the created socket will support the semantics required for TCP server operation, such as
    // connection establishment, reliable data transfer, and orderly connection termination.
    hints.ai_socktype = SOCK_STREAM;

    // This field ensures that only address results suitable for TCP sockets are returned by
    // `getaddrinfo`. While `SOCK_STREAM` already implies TCP in most cases, explicitly setting
    // `ai_protocol` to `IPPROTO_TCP` eliminates ambiguity and guarantees that the created socket
    // will use the TCP protocol. This is particularly important on platforms where multiple
    // protocols may be available for a given socket type, or where protocol selection is not
    // implicit. By specifying `IPPROTO_TCP`, the code ensures that the socket will provide
    // reliable, connection-oriented, byte-stream communication as required for a TCP server.
    hints.ai_protocol = IPPROTO_TCP;

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
    hints.ai_flags = AI_PASSIVE;

    // Resolve the local address and port to be used by the server
    const std::string portStr = std::to_string(_port);
    const auto ret = getaddrinfo(
        localAddress.empty() ? nullptr : localAddress.data(), // Node (hostname or IP address); nullptr means local host
        portStr.c_str(),                                      // Service (port number or service name) as a C-string
        &hints,       // Pointer to struct addrinfo with hints about the type of socket
        &_srvAddrInfo // Output: pointer to a linked list of results (set by getaddrinfo)
    );
    if (ret != 0)
    {
#ifdef _WIN32
        cleanupAndThrow(GetSocketError());
#else
        cleanupAndThrow(ret);
#endif
    }

    // First, try to create a socket with IPv6 (prioritized)
    for (addrinfo* p = _srvAddrInfo; p != nullptr; p = p->ai_next)
    {
        if (p->ai_family == AF_INET6)
        {
            // Attempt to create a socket using the current address
            _serverSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (_serverSocket == INVALID_SOCKET)
                continue; // If socket creation failed, try the next address

            // Store the address that worked
            _selectedAddrInfo = p;

            // Configure the socket for dual-stack (IPv4 + IPv6) or IPv6-only as requested
            try
            {
                setIPv6Only(!dualStack);
            }
            catch (const SocketException& se)
            {
                cleanupAndThrow(se.getErrorCode());
            }

            break; // Exit after successfully creating the IPv6 socket
        }
    }

    // If no IPv6 address worked, fallback to IPv4
    if (_serverSocket == INVALID_SOCKET)
    {
        for (addrinfo* p = _srvAddrInfo; p != nullptr; p = p->ai_next)
        {
            // Only attempt IPv4 if no valid IPv6 address has been used
            if (p->ai_family == AF_INET)
            {
                // Attempt to create a socket using the current address (IPv4)
                _serverSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                if (_serverSocket == INVALID_SOCKET)
                    continue; // If socket creation failed, try the next address

                // Store the address that worked
                _selectedAddrInfo = p;
                break; // Exit after successfully creating the IPv4 socket
            }
        }
    }

    if (_serverSocket == INVALID_SOCKET)
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
        cleanupAndThrow(GetSocketError());
    }

    // Set accept timeout as requested
    setSoTimeout(soTimeoutMillis);

    if (autoBindListen)
    {
        bind();
        listen();
    }
}

void ServerSocket::cleanupAndThrow(const int errorCode)
{
    if (_serverSocket != INVALID_SOCKET)
    {
        CloseSocket(_serverSocket);
        _serverSocket = INVALID_SOCKET;
    }
    if (_srvAddrInfo != nullptr)
    {
        freeaddrinfo(_srvAddrInfo); // ignore errors from freeaddrinfo
        _srvAddrInfo = nullptr;
    }
    _selectedAddrInfo = nullptr;
    throw SocketException(errorCode, SocketErrorMessage(errorCode));
}

std::string ServerSocket::getInetAddress() const
{
    if (_serverSocket == INVALID_SOCKET)
        return "";

    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);
    if (getsockname(_serverSocket, reinterpret_cast<sockaddr*>(&addr), &len) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    char host[INET6_ADDRSTRLEN] = {0};
    if (getnameinfo(reinterpret_cast<sockaddr*>(&addr), len, host, sizeof(host), nullptr, 0, NI_NUMERICHOST) != 0)
        return "";

    return {host};
}

Port ServerSocket::getLocalPort() const
{
    if (_serverSocket == INVALID_SOCKET)
        return 0;

    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);
    if (getsockname(_serverSocket, reinterpret_cast<sockaddr*>(&addr), &len) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    if (addr.ss_family == AF_INET)
    {
        return ntohs(reinterpret_cast<sockaddr_in*>(&addr)->sin_port);
    }
    if (addr.ss_family == AF_INET6)
    {
        return ntohs(reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port);
    }
    return 0;
}

std::string ServerSocket::getLocalSocketAddress() const
{
    if (_serverSocket == INVALID_SOCKET)
        return "";

    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);
    if (getsockname(_serverSocket, reinterpret_cast<sockaddr*>(&addr), &len) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    char host[INET6_ADDRSTRLEN]{};
    char serv[6]{};
    if (getnameinfo(reinterpret_cast<sockaddr*>(&addr), len, host, sizeof(host), serv, sizeof(serv),
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0)
        return "";

    return std::string(host) + ":" + serv;
}

int ServerSocket::getSocketReuseOption()
{
#ifdef _WIN32
    return SO_EXCLUSIVEADDRUSE; // Windows-specific
#else
    return SO_REUSEADDR; // Unix/Linux-specific
#endif
}

// NOLINTNEXTLINE(readability-make-member-function-const) - changes socket state
void ServerSocket::setReuseAddress(const bool enable)
{
    const int opt = enable ? 1 : 0;
    setOption(SOL_SOCKET, getSocketReuseOption(), opt);
}

bool ServerSocket::getReuseAddress() const
{
    if (_serverSocket == INVALID_SOCKET)
        throw SocketException(0, "getReuseAddress() failed: socket not open.");

    auto value = getOption(SOL_SOCKET, getSocketReuseOption());

#ifdef _WIN32
    // On Windows, SO_EXCLUSIVEADDRUSE is the opposite of SO_REUSEADDR
    return value == 0; // If exclusive use is *not* set, reuse is allowed
#else
    return value != 0;
#endif
}

ServerSocket::~ServerSocket() noexcept
{
    try
    {
        close();
    }
    catch (...)
    {
    }
    if (_srvAddrInfo)
    {
        freeaddrinfo(_srvAddrInfo); // ignore errors from freeaddrinfo
        _srvAddrInfo = nullptr;
    }
}

void ServerSocket::close()
{
    if (this->_serverSocket != INVALID_SOCKET)
    {
        if (CloseSocket(this->_serverSocket))
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));

        this->_serverSocket = INVALID_SOCKET;
        _isBound = false;
        _isListening = false;
    }
}

void ServerSocket::bind()
{
    // Ensure that we have already selected an address during construction
    if (_selectedAddrInfo == nullptr)
        throw SocketException(0, "bind() failed: no valid addrinfo found");

    const auto res = ::bind(
        _serverSocket,              // The socket file descriptor to bind.
        _selectedAddrInfo->ai_addr, // Pointer to the sockaddr structure (address and port) to bind to.
#ifdef _WIN32
        static_cast<int>(_selectedAddrInfo->ai_addrlen) // Size of the sockaddr structure (cast to int for Windows).
#else
        _selectedAddrInfo->ai_addrlen // Size of the sockaddr structure (for Linux/Unix).
#endif
    );

    if (res == SOCKET_ERROR)
    {
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }

    _isBound = true;
}

void ServerSocket::listen(const int backlog /* = 128 */)
{
    if (::listen(_serverSocket, backlog) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    _isListening = true;
}

Socket ServerSocket::accept(const std::optional<std::size_t> recvBufferSize,
                            const std::optional<std::size_t> sendBufferSize) const
{
    if (_serverSocket == INVALID_SOCKET)
        throw SocketException(0, "Server socket is not initialized or already closed.");

    const auto [resolvedRecvBuf, resolvedSendBuf] = resolveBuffers(recvBufferSize, sendBufferSize);

    if (!waitReady())
        throw SocketTimeoutException{};

    return acceptBlocking(resolvedRecvBuf, resolvedSendBuf);
}

Socket ServerSocket::accept(int timeoutMillis, std::optional<std::size_t> recvBufferSize,
                            std::optional<std::size_t> sendBufferSize) const
{
    if (_serverSocket == INVALID_SOCKET)
        throw SocketException(0, "Server socket is not initialized or already closed.");

    const auto [resolvedRecvBuf, resolvedSendBuf] = resolveBuffers(recvBufferSize, sendBufferSize);

    if (!waitReady(timeoutMillis))
        throw SocketTimeoutException{};

    return acceptBlocking(resolvedRecvBuf, resolvedSendBuf);
}

std::optional<Socket> ServerSocket::tryAccept(const std::optional<std::size_t> recvBufferSize,
                                              const std::optional<std::size_t> sendBufferSize) const
{
    if (_serverSocket == INVALID_SOCKET)
        throw SocketException(0, "Server socket is not initialized or already closed.");

    const auto [resolvedRecvBuf, resolvedSendBuf] = resolveBuffers(recvBufferSize, sendBufferSize);

    if (!waitReady())
        return std::nullopt;

    return acceptBlocking(resolvedRecvBuf, resolvedSendBuf);
}

std::optional<Socket> ServerSocket::tryAccept(int timeoutMillis, std::optional<std::size_t> recvBufferSize,
                                              std::optional<std::size_t> sendBufferSize) const
{
    if (_serverSocket == INVALID_SOCKET)
        throw SocketException(0, "Server socket is not initialized or already closed.");

    const auto [resolvedRecvBuf, resolvedSendBuf] = resolveBuffers(recvBufferSize, sendBufferSize);

    if (!waitReady(timeoutMillis))
        return std::nullopt;

    return acceptBlocking(resolvedRecvBuf, resolvedSendBuf);
}

Socket ServerSocket::acceptBlocking(const std::optional<std::size_t> recvBufferSize,
                                    const std::optional<std::size_t> sendBufferSize) const
{
    if (_serverSocket == INVALID_SOCKET)
        throw SocketException(0, "Server socket is not initialized or already closed.");

    sockaddr_storage clientAddr{};
    socklen_t clientAddrLen = sizeof(clientAddr);

    const SOCKET clientSocket = ::accept(_serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
    if (clientSocket == INVALID_SOCKET)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    const auto [recvResolved, sendResolved] = resolveBuffers(recvBufferSize, sendBufferSize);
    return {clientSocket, clientAddr, clientAddrLen, recvResolved, sendResolved};
}

std::optional<Socket> ServerSocket::acceptNonBlocking(const std::optional<std::size_t> recvBufferSize,
                                                      const std::optional<std::size_t> sendBufferSize) const
{
    if (_serverSocket == INVALID_SOCKET)
        throw SocketException(0, "Server socket is not initialized or already closed.");

    sockaddr_storage clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);

    const SOCKET clientSocket = ::accept(_serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
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

    const auto [recv, send] = resolveBuffers(recvBufferSize, sendBufferSize);
    return Socket(clientSocket, clientAddr, addrLen, recv, send);
}

std::future<Socket> ServerSocket::acceptAsync(std::optional<std::size_t> recvBufferSize,
                                              std::optional<std::size_t> sendBufferSize) const
{
    // Note: capturing `this` is safe as long as the ServerSocket outlives the future.
    // Document this in your thread-safety notes!
    return std::async(std::launch::async, [this, recvBufferSize, sendBufferSize]() -> Socket
                      { return this->accept(recvBufferSize, sendBufferSize); });
}

void ServerSocket::acceptAsync(std::function<void(std::optional<Socket>, std::exception_ptr)> callback,
                               std::optional<std::size_t> recvBufferSize,
                               std::optional<std::size_t> sendBufferSize) const
{
    // Start a background thread to perform the blocking accept
    std::thread(
        [this, callback = std::move(callback), recvBufferSize, sendBufferSize]()
        {
            try
            {
                // Try to accept a client (may throw on error)
                Socket client = this->accept(recvBufferSize, sendBufferSize);
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

// NOLINTNEXTLINE(readability-make-member-function-const) - changes socket state
void ServerSocket::setOption(const int level, const int optName, int value)
{
    if (setsockopt(_serverSocket, level, optName,
#ifdef _WIN32
                   reinterpret_cast<const char*>(&value),
#else
                   &value,
#endif
                   sizeof(value)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
}

int ServerSocket::getOption(const int level, const int optName) const
{
    int value = 0;
#ifdef _WIN32
    int len = sizeof(value);
#else
    socklen_t len = sizeof(value);
#endif
    if (getsockopt(_serverSocket, level, optName,
#ifdef _WIN32
                   reinterpret_cast<char*>(&value),
#else
                   &value,
#endif
                   &len) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    return value;
}

// NOLINTNEXTLINE(readability-make-member-function-const) - changes socket state
void ServerSocket::setNonBlocking(bool nonBlocking)
{
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(_serverSocket, FIONBIO, &mode) != 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    int flags = fcntl(_serverSocket, F_GETFL, 0);
    if (flags == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (nonBlocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    if (fcntl(_serverSocket, F_SETFL, flags) == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif
}

bool ServerSocket::getNonBlocking() const
{
#ifdef _WIN32
    u_long mode = 0;
    if (ioctlsocket(_serverSocket, FIONBIO, &mode) != 0)
    {
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
    return mode != 0;
#else
    const int flags = fcntl(_serverSocket, F_GETFL, 0);
    if (flags == -1)
    {
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
    return (flags & O_NONBLOCK) != 0;
#endif
}

bool ServerSocket::waitReady(const std::optional<int> timeoutMillis) const
{
    if (_serverSocket == INVALID_SOCKET)
    {
        throw SocketException(EINVAL, "Socket is not initialized.");
    }

    // Determine the effective timeout to use.
    // If a specific timeout was passed, use it; otherwise, use the socket's configured SO_TIMEOUT value.
    const int millis = timeoutMillis.value_or(_soTimeoutMillis);

#ifndef _WIN32
    // --- POSIX: Use poll() for readiness notification ---
    pollfd pfd{};
    pfd.fd = _serverSocket;
    pfd.events = POLLIN; // Monitor for readability (incoming connection)
    pfd.revents = 0;

    // poll() timeout: <0 = infinite, 0 = poll, >0 = wait up to millis
    const int timeout = (millis < 0) ? -1 : millis;
    int result = poll(&pfd, 1, timeout);
    if (result < 0)
    {
        // poll() failed — throw an exception with the error code and message.
        throw SocketException(GetSocketError(), "Failed to wait for incoming connection.");
    }
    // poll() returns the number of fds with events. Check for POLLIN.
    return result > 0 && (pfd.revents & POLLIN);
#else
    // --- Windows: Use select() for readiness notification ---
    // Prepare the file descriptor set for select().
    // We want to monitor this server socket for readability (i.e., incoming connection).
    fd_set readFds;
    FD_ZERO(&readFds);               // Clear the set of file descriptors
    FD_SET(_serverSocket, &readFds); // Add our server socket to the set

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
    int result = select(static_cast<int>(_serverSocket) + 1, &readFds, nullptr, nullptr, tvPtr);
    if (result < 0)
    {
        // select() failed — throw an exception with the error code and message.
        throw SocketException(GetSocketError(), "Failed to wait for incoming connection.");
    }
    // select() returns the number of sockets that are ready.
    // If it's greater than 0, our server socket is ready to accept a connection.
    return result > 0;
#endif
}

#if defined(SO_REUSEPORT)
void ServerSocket::setReusePort(const bool enable)
{
    const int opt = enable ? 1 : 0;
    setOption(SOL_SOCKET, SO_REUSEPORT, opt);
}

bool ServerSocket::getReusePort() const
{
    return getOption(SOL_SOCKET, SO_REUSEPORT);
}
#endif

#if defined(IPV6_V6ONLY)

// NOLINTNEXTLINE(readability-make-member-function-const) - changes socket state
void ServerSocket::setIPv6Only(const bool enable)
{
    // Check if the socket is valid and AF_INET6
    if (_serverSocket == INVALID_SOCKET)
        throw SocketException(0, "setIPv6Only: Socket is not open.");

    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);
    if (getsockname(_serverSocket, reinterpret_cast<sockaddr*>(&ss), &len) != 0)
        throw SocketException(GetSocketError(), "setIPv6Only: getsockname() failed.");

    if (ss.ss_family != AF_INET6)
        throw SocketException(0, "setIPv6Only: Socket is not IPv6.");

    const int opt = enable ? 1 : 0;
    setOption(IPPROTO_IPV6, IPV6_V6ONLY, opt);
}

bool ServerSocket::getIPv6Only() const
{
    if (_serverSocket == INVALID_SOCKET)
        throw SocketException(0, "getIPv6Only: Socket is not open.");

    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);
    if (getsockname(_serverSocket, reinterpret_cast<sockaddr*>(&ss), &len) != 0)
        throw SocketException(GetSocketError(), "getIPv6Only: getsockname() failed.");

    if (ss.ss_family != AF_INET6)
        throw SocketException(0, "getIPv6Only: Socket is not IPv6.");

    return getOption(IPPROTO_IPV6, IPV6_V6ONLY);
}

#endif // IPV6_V6ONLY
