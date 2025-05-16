#include "libsocket/UnixSocket.hpp"
#include "libsocket/SocketException.hpp"

using namespace libsocket;

#if defined(_WIN32) && defined(AF_UNIX) || defined(__unix__) || defined(__APPLE__)

UnixSocket::UnixSocket(const std::string& path, const std::size_t bufferSize)
    : _socketPath(path), _addr(), _buffer(bufferSize)
{
    std::memset(&_addr, 0, sizeof(_addr));
    _addr.sun_family = AF_UNIX;
    std::strncpy(_addr.sun_path, path.c_str(), sizeof(_addr.sun_path) - 1);

    _sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_sockfd == INVALID_SOCKET)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
}

UnixSocket::~UnixSocket() noexcept
{
    try
    {
        close();
        if (!_socketPath.empty())
        {
#ifdef _WIN32
            _unlink(_socketPath.c_str());
#else
            unlink(_socketPath.c_str());
#endif
        }
    }
    catch (...)
    {
        // Suppress all exceptions in destructor
    }
}

void UnixSocket::bind()
{
    // Remove any existing socket file before binding
    if (!_socketPath.empty())
    {
#ifdef _WIN32
        _unlink(_socketPath.c_str());
#else
        unlink(_socketPath.c_str());
#endif
    }
    if (::bind(_sockfd, reinterpret_cast<sockaddr*>(&_addr), sizeof(_addr)) == SOCKET_ERROR)
    {
        CloseSocket(_sockfd);
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
}

void UnixSocket::listen(int backlog) const
{
    if (::listen(_sockfd, backlog) == SOCKET_ERROR)
    {
        CloseSocket(_sockfd);
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
}

void UnixSocket::connect()
{
    if (::connect(_sockfd, reinterpret_cast<sockaddr*>(&_addr), sizeof(_addr)) == SOCKET_ERROR)
    {
        CloseSocket(_sockfd);
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
}

void UnixSocket::close()
{
    if (_sockfd != INVALID_SOCKET)
    {
        CloseSocket(_sockfd);
        _sockfd = INVALID_SOCKET;
    }
}

UnixSocket UnixSocket::accept() const
{
    sockaddr_un client_addr{};
    socklen_t len = sizeof(client_addr);
    const auto client_fd = ::accept(_sockfd, reinterpret_cast<sockaddr*>(&client_addr), &len);
    if (client_fd == INVALID_SOCKET)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    UnixSocket client;
    client._sockfd = client_fd;
    client._addr = client_addr;
    client._socketPath = client_addr.sun_path;
    return client;
}

int UnixSocket::write(std::string_view message) const
{
    const auto ret = ::send(_sockfd, message.data(),
#ifdef _WIN32
                            static_cast<int>(message.size()), // Windows: cast to int
#else
                            message.size(), // Linux/Unix: use size_t directly
#endif
                            0);
    if (ret < 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

#ifdef _WIN32
    return ret;
#else
    return static_cast<int>(ret);
#endif
}

int UnixSocket::read(char* buffer, std::size_t len) const
{
    const auto ret = ::recv(_sockfd, buffer,
#ifdef _WIN32
                            static_cast<int>(len),
#else
                            len,
#endif
                            0);
    if (ret == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

#ifdef _WIN32
    return ret;
#else
    return static_cast<int>(ret);
#endif
}

void UnixSocket::setNonBlocking(bool nonBlocking) const
{
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    if (ioctlsocket(_sockfd, FIONBIO, &mode) != 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    int flags = fcntl(_sockfd, F_GETFL, 0);
    if (flags == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    if (nonBlocking)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    if (fcntl(_sockfd, F_SETFL, flags) == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif
}

void UnixSocket::setTimeout(int millis) const
{
#ifdef _WIN32
    const int timeout = millis;
    if (setsockopt(_sockfd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) ==
            SOCKET_ERROR ||
        setsockopt(_sockfd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) ==
            SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    timeval tv{millis / 1000, (millis % 1000) * 1000};
    if (setsockopt(_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == SOCKET_ERROR ||
        setsockopt(_sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif
}

#endif