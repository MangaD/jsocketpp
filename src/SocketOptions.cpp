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
void SocketOptions::setOption(const int level, const int optName, const int value)
{
    setOption(level, optName,
#ifdef _WIN32
              reinterpret_cast<const char*>(&value),
#else
              static_cast<const void*>(&value),
#endif
              static_cast<socklen_t>(sizeof(value)));
}

// NOLINTNEXTLINE(readability-make-member-function-const) - changes socket state
void SocketOptions::setOption(const int level, const int optName, const void* value, const socklen_t len)
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("setOption() failed: socket not open.");

    if (!value || len == 0)
        throw SocketException("setOption() failed: null buffer or zero length.");

    if (::setsockopt(_sockFd, level, optName,
#ifdef _WIN32
                     static_cast<const char*>(value),
#else
                     value,
#endif
                     len) < 0)
    {
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }
}

int SocketOptions::getOption(const int level, const int optName) const
{
    int value = 0;
    socklen_t len = sizeof(value);

    getOption(level, optName, &value, &len);
    return value;
}

void SocketOptions::getOption(const int level, const int optName, void* result, socklen_t* len) const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("getOption() failed: socket not open.");

    if (!result || !len || *len == 0)
        throw SocketException("getOption() failed: invalid buffer or length.");

    if (::getsockopt(_sockFd, level, optName,
#ifdef _WIN32
                     static_cast<char*>(result),
#else
                     result,
#endif
                     len) < 0)
    {
        throw SocketException(GetSocketError(), "getsockopt() failed");
    }
}

// NOLINTNEXTLINE(readability-make-member-function-const) – modifies socket state
void SocketOptions::setReuseAddress(bool on)
{
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
    setOption(SOL_SOCKET, SO_RCVBUF, static_cast<int>(size));
}

int SocketOptions::getReceiveBufferSize() const
{
    return getOption(SOL_SOCKET, SO_RCVBUF);
}

void SocketOptions::setSendBufferSize(const std::size_t size)
{
    setOption(SOL_SOCKET, SO_SNDBUF, static_cast<int>(size));
}

int SocketOptions::getSendBufferSize() const
{
    return getOption(SOL_SOCKET, SO_SNDBUF);
}

// NOLINTNEXTLINE(readability-make-member-function-const) – modifies socket state
void SocketOptions::setSoLinger(const bool enable, const int seconds)
{
    if (seconds < 0)
        throw SocketException("setSoLinger() failed: timeout cannot be negative.");

    linger lin{};
    lin.l_onoff = enable ? 1 : 0;
    lin.l_linger = static_cast<u_short>(seconds); // portable: matches Windows + POSIX

    setOption(SOL_SOCKET, SO_LINGER, &lin, static_cast<socklen_t>(sizeof(linger)));
}

std::pair<bool, int> SocketOptions::getSoLinger() const
{
    linger lin{};
    auto len = static_cast<socklen_t>(sizeof(lin));

    getOption(SOL_SOCKET, SO_LINGER, &lin, &len);

    return {lin.l_onoff != 0, static_cast<int>(lin.l_linger)};
}

void SocketOptions::setKeepAlive(const bool on)
{
    setOption(SOL_SOCKET, SO_KEEPALIVE, on ? 1 : 0);
}

bool SocketOptions::getKeepAlive() const
{
    return getOption(SOL_SOCKET, SO_KEEPALIVE) != 0;
}

} // namespace jsocketpp
