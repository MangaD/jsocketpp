// SocketOptions.cpp
#include "jsocketpp/SocketOptions.hpp"
#include "jsocketpp/SocketException.hpp"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

namespace jsocketpp
{

// NOLINTNEXTLINE(readability-make-member-function-const) - changes socket state
void SocketOptions::setOption(const int level, const int optName, int value)
{
    if (_sockFd == INVALID_SOCKET)
    {
        throw SocketException("Cannot set option: socket is invalid");
    }

    if (::setsockopt(_sockFd, level, optName,
#ifdef _WIN32
                     reinterpret_cast<const char*>(&value),
#else
                     &value,
#endif
                     sizeof(value)) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
}

int SocketOptions::getOption(const int level, const int optName) const
{
    if (_sockFd == INVALID_SOCKET)
    {
        throw SocketException("Cannot get option: socket is invalid");
    }

    int value = 0;
#ifdef _WIN32
    int len = sizeof(value);
#else
    socklen_t len = sizeof(value);
#endif
    if (::getsockopt(_sockFd, level, optName,
#ifdef _WIN32
                     reinterpret_cast<char*>(&value),
#else
                     &value,
#endif
                     &len) == SOCKET_ERROR)
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    return value;
}

// NOLINTNEXTLINE(readability-make-member-function-const) – modifies socket state
void SocketOptions::setReuseAddress(bool on)
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("setReuseAddress() failed: socket not open.");

#ifdef _WIN32
    if (isPassiveSocket())
    {
        // For listening sockets: disable exclusive use to allow reuse
        const int value = on ? 0 : 1;
        setOption(SOL_SOCKET, SO_EXCLUSIVEADDRUSE, value);
    }
    else
    {
        // For clients, UDP, Unix — use SO_REUSEADDR
        setOption(SOL_SOCKET, SO_REUSEADDR, on ? 1 : 0);
    }
#else
    setOption(SOL_SOCKET, SO_REUSEADDR, on ? 1 : 0);
#endif
}

bool SocketOptions::getReuseAddress() const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("getReuseAddress() failed: socket not open.");

#ifdef _WIN32
    if (isPassiveSocket())
    {
        // Reuse is allowed if exclusive use is disabled
        return getOption(SOL_SOCKET, SO_EXCLUSIVEADDRUSE) == 0;
    }
    return getOption(SOL_SOCKET, SO_REUSEADDR) != 0;
#else
    return getOption(SOL_SOCKET, SO_REUSEADDR) != 0;
#endif
}

void SocketOptions::setReceiveBufferSize(const std::size_t size)
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("setReceiveBufferSize() failed: socket not open.");

    setOption(SOL_SOCKET, SO_RCVBUF, static_cast<int>(size));
}

int SocketOptions::getReceiveBufferSize() const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("getReceiveBufferSize() failed: socket not open.");

    return getOption(SOL_SOCKET, SO_RCVBUF);
}

void SocketOptions::setSendBufferSize(const std::size_t size)
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("setSendBufferSize() failed: socket not open.");

    setOption(SOL_SOCKET, SO_SNDBUF, static_cast<int>(size));
}

int SocketOptions::getSendBufferSize() const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("getSendBufferSize() failed: socket not open.");

    return getOption(SOL_SOCKET, SO_SNDBUF);
}

// NOLINTNEXTLINE(readability-make-member-function-const) – modifies socket state
void SocketOptions::setSoLinger(const bool enable, const int seconds)
{
    if (seconds < 0)
        throw SocketException("setSoLinger() failed: timeout cannot be negative.");

    if (_sockFd == INVALID_SOCKET)
        throw SocketException("setSoLinger() failed: socket not open.");

    linger lin{};
    lin.l_onoff = enable ? 1 : 0;
    lin.l_linger = static_cast<u_short>(seconds); // safe on both Windows and POSIX

    if (const int ret =
            ::setsockopt(_sockFd, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&lin), sizeof(linger));
        ret < 0)
        throw SocketException(GetSocketError(), "setsockopt(SO_LINGER) failed");
}

std::pair<bool, int> SocketOptions::getSoLinger() const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("getSoLinger() failed: socket not open.");

    linger lin{};
    socklen_t len = sizeof(linger);

    if (const int ret = ::getsockopt(_sockFd, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&lin), &len); ret < 0)
        throw SocketException(GetSocketError(), "getsockopt(SO_LINGER) failed");

    return {lin.l_onoff != 0, static_cast<int>(lin.l_linger)};
}

} // namespace jsocketpp
