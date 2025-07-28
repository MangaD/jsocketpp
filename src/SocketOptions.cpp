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
    else
    {
        return getOption(SOL_SOCKET, SO_REUSEADDR) != 0;
    }
#else
    return getOption(SOL_SOCKET, SO_REUSEADDR) != 0;
#endif
}

} // namespace jsocketpp
