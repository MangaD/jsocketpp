#include "jsocketpp/DatagramSocket.hpp"
#include "jsocketpp/SocketException.hpp"

using namespace jsocketpp;

DatagramSocket::DatagramSocket(const Port port, const std::size_t bufferSize)
    : DatagramSocket("", port, bufferSize) // Use nullptr for host to bind to all interfaces
{
}

DatagramSocket::DatagramSocket(const std::string_view host, const Port port, const std::size_t bufferSize)
    : SocketOptions(INVALID_SOCKET), _buffer(bufferSize), _port(port)
{
    // Prepare the hints structure for getaddrinfo to specify the desired socket type and protocol.
    addrinfo hints{}; // Initialize the hints structure to zero

    // Specifies the address family: AF_UNSPEC allows both IPv4 (AF_INET) and IPv6 (AF_INET6)
    // addresses to be returned by getaddrinfo, enabling the server to support dual-stack networking
    // (accepting connections over both protocols). This makes the code portable and future-proof,
    // as it can handle whichever protocol is available on the system or preferred by clients.
    hints.ai_family = AF_UNSPEC;

    // Specifies the desired socket type. Setting it to `SOCK_DGRAM` requests a datagram socket (UDP),
    // which provides connectionless, unreliable, message-oriented communication—this is the standard
    // socket type for UDP. By specifying `SOCK_DGRAM`, the code ensures that only address results suitable
    // for UDP (as opposed to stream-based protocols like TCP, which use `SOCK_STREAM`) are returned by `getaddrinfo`.
    // This guarantees that the created socket will support the semantics required for UDP operation, such as
    // message delivery without connection establishment and no guarantee of delivery order or reliability.
    hints.ai_socktype = SOCK_DGRAM;

    // This field ensures that only address results suitable for UDP sockets are returned by
    // `getaddrinfo`. While `SOCK_DGRAM` already implies UDP in most cases, explicitly setting
    // `ai_protocol` to `IPPROTO_UDP` eliminates ambiguity and guarantees that the created socket
    // will use the UDP protocol. This is particularly important on platforms where multiple
    // protocols may be available for a given socket type, or where protocol selection is not
    // implicit. By specifying `IPPROTO_UDP`, the code ensures that the socket will provide
    // connectionless, unreliable, message-oriented communication as required for a UDP socket.
    hints.ai_protocol = IPPROTO_UDP;

    // Resolve the local address and port to be used by the server
    const std::string portStr = std::to_string(_port);
    addrinfo* rawAddrInfo = nullptr;
    const auto ret =
        ::getaddrinfo((host.empty() ? nullptr : host.data()), // Node (hostname or IP address); nullptr means local host
                      portStr.c_str(),                        // Service (port number or service name) as a C-string
                      &hints,      // Pointer to struct addrinfo with hints about the type of socket
                      &rawAddrInfo // Output: pointer to a linked list of results (set by getaddrinfo)
        );

    if (ret != 0)
    {
        throw SocketException(
#ifdef _WIN32
            GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
            ret, SocketErrorMessageWrap(ret, true));
#endif
    }

    _addrInfoPtr.reset(rawAddrInfo); // transfer ownership

    // Try all available addresses (IPv6/IPv4) to create a SOCKET for
    for (addrinfo* p = _addrInfoPtr.get(); p != nullptr; p = p->ai_next)
    {
        setSocketFd(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
        if (getSocketFd() != INVALID_SOCKET)
        {
            // Store the address that worked
            _selectedAddrInfo = p;
            break;
        }
    }
    if (getSocketFd() == INVALID_SOCKET)
    {
        cleanupAndThrow(GetSocketError());
    }
}

void DatagramSocket::cleanupAndThrow(const int errorCode)
{
    if (getSocketFd() != INVALID_SOCKET)
    {
        CloseSocket(getSocketFd());
        setSocketFd(INVALID_SOCKET);

        // CloseSocket error is ignored.
        // TODO: Consider adding an internal flag, nested exception, or user-configurable error handler
        //       to report errors in future versions.
    }
    _addrInfoPtr.reset();
    _selectedAddrInfo = nullptr;
    throw SocketException(errorCode, SocketErrorMessage(errorCode));
}

DatagramSocket::~DatagramSocket() noexcept
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

void DatagramSocket::close()
{
    if (getSocketFd() != INVALID_SOCKET)
    {
        if (CloseSocket(getSocketFd()))
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));

        setSocketFd(INVALID_SOCKET);
    }
    _addrInfoPtr.reset();
    _selectedAddrInfo = nullptr;
}

void DatagramSocket::bind(std::string_view host, Port port)
{
    if (_isConnected)
    {
        throw SocketException("DatagramSocket::bind(): socket is already connected");
    }

    if (_isBound)
    {
        throw SocketException("DatagramSocket::bind(): socket is already bound");
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM;  // Datagram socket
    hints.ai_protocol = IPPROTO_UDP; // UDP
    hints.ai_flags = AI_PASSIVE;     // Allow wildcard addresses (e.g., 0.0.0.0)

    const std::string portStr = std::to_string(port);
    addrinfo* rawResult = nullptr;

    if (const int ret = ::getaddrinfo(host.empty() ? nullptr : host.data(), portStr.c_str(), &hints, &rawResult);
        ret != 0 || rawResult == nullptr)
    {
#ifdef _WIN32
        throw SocketException(GetSocketError(), "DatagramSocket::bind(): getaddrinfo() failed");
#else
        throw SocketException(ret, "DatagramSocket::bind(): getaddrinfo() failed");
#endif
    }

    const internal::AddrinfoPtr result{rawResult};

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

    const int error = GetSocketError();
    throw SocketException(error, "DatagramSocket::bind(): bind() failed");
}

void DatagramSocket::bind(const Port port)
{
    bind("0.0.0.0", port);
}

void DatagramSocket::bind()
{
    bind("0.0.0.0", 0);
}

void DatagramSocket::connect(const int timeoutMillis)
{
    // Ensure that we have already selected an address during construction
    if (_selectedAddrInfo == nullptr)
    {
        throw SocketException("connect() failed: no valid addrinfo found");
    }

    // Flag to track whether we're in non-blocking mode
    const bool isNonBlocking = (timeoutMillis >= 0);

    // Set the socket to non-blocking mode if a timeout is specified
    if (isNonBlocking)
    {
        setNonBlocking(true); // This sets the socket to non-blocking mode
    }

    // Attempt to connect to the remote host
    const auto res = ::connect(getSocketFd(), _selectedAddrInfo->ai_addr,
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
            FD_SET(getSocketFd(), &fds);

#ifdef _WIN32
            // On Windows, the first parameter to select is ignored, but must be set to 0
            auto result = select(0, nullptr, &fds, nullptr, &tv);
#else
            // On Linux/Unix, the first parameter to select should be the highest file descriptor + 1
            auto result = select(getSocketFd() + 1, nullptr, &fds, nullptr, &tv);
#endif

            // If select() timed out or failed, reset the socket to blocking mode
            if (result <= 0)
            {
                // Reset the socket to blocking mode before throwing the timeout exception
                setNonBlocking(false);
                throw SocketException("Connection timed out or failed.");
            }
        }
    }

    // After the connection attempt, if we used non-blocking mode, reset to blocking mode
    if (isNonBlocking)
    {
        setNonBlocking(false); // Reset the socket back to blocking mode
    }

    _isConnected = true;
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

size_t DatagramSocket::write(const DatagramPacket& packet) const
{
    if (packet.buffer.empty())
        return 0; // Nothing to send

    int flags = 0;
#ifndef _WIN32
    flags = MSG_NOSIGNAL; // Don't send SIGPIPE on broken pipe
#endif

    if (!packet.address.empty() && packet.port != 0)
    {
        // Connectionless send
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;

        const std::string portStr = std::to_string(packet.port);
        addrinfo* res = nullptr;
        if (const auto ret = getaddrinfo(packet.address.c_str(), portStr.c_str(), &hints, &res); ret != 0)
        {
            throw SocketException(
#ifdef _WIN32
                GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
                ret, SocketErrorMessageWrap(ret, true));
#endif
        }
        const auto sent = sendto(getSocketFd(), packet.buffer.data(),
#ifdef _WIN32
                                 static_cast<int>(packet.buffer.size()), // Windows: cast to int
#else
                                 packet.buffer.size(), // Linux/Unix: use size_t directly
#endif
                                 flags, res->ai_addr,
#ifdef _WIN32
                                 static_cast<int>(res->ai_addrlen)); // Windows: cast to int
#else
                                 res->ai_addrlen); // Linux/Unix: use socklen_t directly
#endif

        const int error = GetSocketError(); // capture immediately after syscall
        freeaddrinfo(res);
        if (sent < 0)
            throw SocketException(error, SocketErrorMessage(error));
        return static_cast<size_t>(sent);
    }

    // Connected send
    const auto sent = send(getSocketFd(), packet.buffer.data(),
#ifdef _WIN32
                           static_cast<int>(packet.buffer.size()), // Windows: cast to int
#else
                           packet.buffer.size(), // Linux/Unix: use size_t directly
#endif
                           flags);
    if (sent < 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    return static_cast<size_t>(sent);
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

void DatagramSocket::setNonBlocking(bool nonBlocking) const
{
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(getSocketFd(), FIONBIO, &mode) != 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    int flags = fcntl(getSocketFd(), F_GETFL, 0);
    if (flags == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (nonBlocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    if (fcntl(getSocketFd(), F_SETFL, flags) == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif
}

void DatagramSocket::setTimeout(int millis) const
{
#ifdef _WIN32
    if (setsockopt(getSocketFd(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&millis), sizeof(millis)) ==
            SOCKET_ERROR ||
        setsockopt(getSocketFd(), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&millis), sizeof(millis)) ==
            SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    timeval tv{0, 0};
    if (millis >= 0)
    {
        tv.tv_sec = millis / 1000;
        tv.tv_usec = (millis % 1000) * 1000;
    }
    if (setsockopt(getSocketFd(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == SOCKET_ERROR ||
        setsockopt(getSocketFd(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif
}

std::string DatagramSocket::getLocalSocketAddress() const
{
    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);

    if (getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &len) != 0)
    {
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }

    char host[NI_MAXHOST]{};
    char serv[NI_MAXSERV]{};
    if (const auto ret = getnameinfo(reinterpret_cast<sockaddr*>(&addr), len, host, sizeof(host), serv, sizeof(serv),
                                     NI_NUMERICHOST | NI_NUMERICSERV);
        ret != 0)
    {
        throw SocketException(
#ifdef _WIN32
            GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
            ret, SocketErrorMessageWrap(ret, true));
#endif
    }
    return std::string(host) + ":" + serv;
}

/**
 * @brief Enable or disable the ability to send broadcast packets.
 * @param enable Set to true to enable broadcast, false to disable.
 * @throws SocketException on error.
 */
void DatagramSocket::enableBroadcast(const bool enable) const
{
    const int flag = enable ? 1 : 0;
    if (setsockopt(getSocketFd(), SOL_SOCKET, SO_BROADCAST,
#ifdef _WIN32
                   reinterpret_cast<const char*>(&flag),
#else
                   &flag,
#endif
                   sizeof(flag)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
}
