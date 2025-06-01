#include "jsocketpp/UnixSocket.hpp"
#include "jsocketpp/SocketException.hpp"

using namespace jsocketpp;

#if defined(_WIN32) && defined(AF_UNIX) || defined(__unix__) || defined(__APPLE__)

UnixSocket::UnixSocket(const std::string_view path, const std::size_t bufferSize)
    : _socketPath(path), _buffer(bufferSize)
{
    if (path.length() >= sizeof(_addr.sun_path))
    {
        throw SocketException(0, "Unix domain socket path too long");
    }

    std::memset(&_addr, 0, sizeof(_addr));
    _addr.sun_family = AF_UNIX;
    std::strncpy(_addr.sun_path, path.data(), sizeof(_addr.sun_path) - 1);

    _sockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (_sockFd == INVALID_SOCKET)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
}

UnixSocket::~UnixSocket() noexcept
{
    try
    {
        close();
        // Only unlink if this is a listening socket
        if (_isListening && !_socketPath.empty())
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

UnixSocket::UnixSocket(UnixSocket&& other) noexcept
    : _sockFd(other._sockFd), _isListening(other._isListening), _socketPath(std::move(other._socketPath)),
      _addr(other._addr), _buffer(std::move(other._buffer))
{
    other._sockFd = INVALID_SOCKET;
    other._socketPath.clear();
    other._isListening = false;
    std::memset(&other._addr, 0, sizeof(other._addr));
}

UnixSocket& UnixSocket::operator=(UnixSocket&& other) noexcept
{
    if (this != &other)
    {
        close();

        _sockFd = other._sockFd;
        _socketPath = std::move(other._socketPath);
        _buffer = std::move(other._buffer);
        _addr = other._addr;
        _isListening = other._isListening;

        other._sockFd = INVALID_SOCKET;
        other._socketPath.clear();
        other._isListening = false;
        std::memset(&other._addr, 0, sizeof(other._addr));
    }
    return *this;
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
    if (::bind(_sockFd, reinterpret_cast<sockaddr*>(&_addr), sizeof(_addr)) == SOCKET_ERROR)
    {
        CloseSocket(_sockFd);
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
    _isListening = true;
}

void UnixSocket::listen(int backlog) const
{
    if (::listen(_sockFd, backlog) == SOCKET_ERROR)
    {
        CloseSocket(_sockFd);
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
}

void UnixSocket::connect()
{
    if (::connect(_sockFd, reinterpret_cast<sockaddr*>(&_addr), sizeof(_addr)) == SOCKET_ERROR)
    {
        CloseSocket(_sockFd);
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
}

void UnixSocket::close()
{
    if (_sockFd != INVALID_SOCKET)
    {
        CloseSocket(_sockFd);
        _sockFd = INVALID_SOCKET;
    }
}

UnixSocket UnixSocket::accept() const
{
    sockaddr_un client_addr{};
    socklen_t len = sizeof(client_addr);
    const auto client_fd = ::accept(_sockFd, reinterpret_cast<sockaddr*>(&client_addr), &len);
    if (client_fd == INVALID_SOCKET)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    UnixSocket client;
    client._sockFd = client_fd;
    client._addr = client_addr;
    client._socketPath = client_addr.sun_path;
    return client;
}

size_t UnixSocket::write(std::string_view data) const
{
    const auto ret = ::send(_sockFd, data.data(),
#ifdef _WIN32
                            static_cast<int>(data.size()), // Windows: cast to int
#else
                            data.size(), // Linux/Unix: use size_t directly
#endif
                            0);
    if (ret < 0)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    return static_cast<size_t>(ret);
}

size_t UnixSocket::read(char* buffer, std::size_t len) const
{
    const auto ret = ::recv(_sockFd, buffer,
#ifdef _WIN32
                            static_cast<int>(len),
#else
                            len,
#endif
                            0);
    if (ret == -1)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));

    return static_cast<size_t>(ret);
}

void UnixSocket::setNonBlocking(bool nonBlocking) const
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

void UnixSocket::setTimeout(int millis) const
{
#ifdef _WIN32
    const int timeout = millis;
    if (setsockopt(_sockFd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) ==
            SOCKET_ERROR ||
        setsockopt(_sockFd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) ==
            SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#else
    timeval tv{millis / 1000, (millis % 1000) * 1000};
    if (setsockopt(_sockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == SOCKET_ERROR ||
        setsockopt(_sockFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
#endif
}

#endif