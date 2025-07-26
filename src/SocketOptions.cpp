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

} // namespace jsocketpp
