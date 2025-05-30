#include "jsocketpp/ServerSocket.hpp"

using namespace jsocketpp;

ServerSocket::ServerSocket(const unsigned short port) : _port(port)
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
    auto ret = getaddrinfo(nullptr,         // Node (hostname or IP address); nullptr means local host
                           portStr.c_str(), // Service (port number or service name) as a C-string
                           &hints,          // Pointer to struct addrinfo with hints about the type of socket
                           &_srvAddrInfo    // Output: pointer to a linked list of results (set by getaddrinfo)
    );
    if (ret != 0)
    {
        throw SocketException(
#ifdef _WIN32
            GetSocketError(),
#else
            ret,
#endif
            SocketErrorMessageWrap(GetSocketError(), true));
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

            // Configure the socket for dual-stack (IPv4 + IPv6) if it's IPv6
            const int optionValue = 0; // 0 means allow both IPv4 and IPv6 connections
            if (setsockopt(_serverSocket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&optionValue),
                           sizeof(optionValue)) == SOCKET_ERROR)
            {
                cleanupAndThrow(GetSocketError());
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
    constexpr int optVal = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, getSocketReuseOption(), reinterpret_cast<const char*>(&optVal),
                   sizeof(optVal)) == SOCKET_ERROR)
    {
        cleanupAndThrow(GetSocketError());
    }
}
void ServerSocket::cleanupAndThrow(const int errorCode)
{
    // Clean up addrinfo and throw exception with the error
    freeaddrinfo(_srvAddrInfo); // ignore errors from freeaddrinfo
    _srvAddrInfo = nullptr;
    _selectedAddrInfo = nullptr;
    throw SocketException(errorCode, SocketErrorMessage(errorCode));
}

int ServerSocket::getSocketReuseOption()
{
#ifdef _WIN32
    return SO_EXCLUSIVEADDRUSE; // Windows-specific
#else
    return SO_REUSEADDR; // Unix/Linux-specific
#endif
}

ServerSocket::~ServerSocket() noexcept
{
    try
    {
        close();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
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
        try
        {
            // It's good practice to call shutdown before closing a listening socket,
            // to ensure all resources are released and connections are properly terminated.
            shutdown();
        }
        catch (...)
        {
            // Ignore shutdown errors, proceed to close anyway.
        }
        if (CloseSocket(this->_serverSocket))
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));

        this->_serverSocket = INVALID_SOCKET;
    }
}

void ServerSocket::shutdown() const
{
    if (this->_serverSocket != INVALID_SOCKET)
    {
        constexpr int how =
#ifdef _WIN32
            SD_BOTH;
#else
            SHUT_RDWR;
#endif

        if (::shutdown(this->_serverSocket, how))
            throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError()));
    }
}

void ServerSocket::bind() const
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
}

void ServerSocket::listen(const int backlog /* = SOMAXCONN */) const
{
    if (::listen(_serverSocket, backlog) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
}

Socket ServerSocket::accept(const std::size_t bufferSize /* = 512 */) const
{
    // Create structure to hold client address
    sockaddr_storage cli_addr{};
    socklen_t cliLen = sizeof(cli_addr);

    // Accept an incoming connection
    const SOCKET clientSocket = ::accept(_serverSocket, reinterpret_cast<struct sockaddr*>(&cli_addr), &cliLen);

    // Handle socket creation failure
    if (clientSocket == INVALID_SOCKET)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    Socket client(clientSocket, cli_addr, cliLen, bufferSize);

    return client;
}