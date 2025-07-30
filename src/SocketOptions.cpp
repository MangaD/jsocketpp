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

void SocketOptions::setSoRecvTimeout(int millis)
{
    if (millis < 0)
        throw SocketException("setSoRecvTimeout() failed: timeout must be non-negative.");

#ifdef _WIN32
    // On Windows, timeout is specified directly as an integer in milliseconds
    setOption(SOL_SOCKET, SO_RCVTIMEO, millis);
#else
    // On POSIX, use struct timeval
    timeval tv{};
    tv.tv_sec = millis / 1000;
    tv.tv_usec = (millis % 1000) * 1000;

    setOption(SOL_SOCKET, SO_RCVTIMEO, &tv, static_cast<socklen_t>(sizeof(tv)));
#endif
}

void SocketOptions::setSoSendTimeout(int millis)
{
    if (millis < 0)
        throw SocketException("setSoSendTimeout() failed: timeout must be non-negative.");

#ifdef _WIN32
    setOption(SOL_SOCKET, SO_SNDTIMEO, millis);
#else
    timeval tv{};
    tv.tv_sec = millis / 1000;
    tv.tv_usec = (millis % 1000) * 1000;

    setOption(SOL_SOCKET, SO_SNDTIMEO, &tv, static_cast<socklen_t>(sizeof(tv)));
#endif
}

int SocketOptions::getSoRecvTimeout() const
{
#ifdef _WIN32
    return getOption(SOL_SOCKET, SO_RCVTIMEO);
#else
    timeval tv{};
    socklen_t len = static_cast<socklen_t>(sizeof(tv));

    getOption(SOL_SOCKET, SO_RCVTIMEO, &tv, &len);

    return static_cast<int>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

int SocketOptions::getSoSendTimeout() const
{
#ifdef _WIN32
    return getOption(SOL_SOCKET, SO_SNDTIMEO);
#else
    timeval tv{};
    socklen_t len = static_cast<socklen_t>(sizeof(tv));

    getOption(SOL_SOCKET, SO_SNDTIMEO, &tv, &len);

    return static_cast<int>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

// NOLINTNEXTLINE(readability-make-member-function-const) - changes socket state
void SocketOptions::setNonBlocking(bool nonBlocking)
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("setNonBlocking() failed: socket is not open.");

#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    if (::ioctlsocket(_sockFd, FIONBIO, &mode) != 0)
        throw SocketException(GetSocketError(), "ioctlsocket(FIONBIO) failed");
#else
    int flags = ::fcntl(_sockFd, F_GETFL, 0);
    if (flags < 0)
        throw SocketException(errno, "fcntl(F_GETFL) failed");

    const int newFlags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (::fcntl(_sockFd, F_SETFL, newFlags) < 0)
        throw SocketException(errno, "fcntl(F_SETFL) failed");
#endif
}

bool SocketOptions::getNonBlocking() const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("getNonBlocking() failed: socket is not open.");

#ifdef _WIN32
    // Windows has no ioctl/FIONBIO read mode — not supported by design.
    // You must track non-blocking state in application logic.
    return false;
#else
    const int flags = ::fcntl(_sockFd, F_GETFL, 0);
    if (flags < 0)
        throw SocketException(errno, "fcntl(F_GETFL) failed");

    return (flags & O_NONBLOCK) != 0;
#endif
}

void SocketOptions::setTcpNoDelay(const bool on)
{
    setOption(IPPROTO_TCP, TCP_NODELAY, on ? 1 : 0);
}

bool SocketOptions::getTcpNoDelay() const
{
    return getOption(IPPROTO_TCP, TCP_NODELAY) != 0;
}

#if defined(IPV6_V6ONLY)

void SocketOptions::setIPv6Only(const bool enable)
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("setIPv6Only() failed: socket is not open.");

    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);
    if (::getsockname(_sockFd, reinterpret_cast<sockaddr*>(&ss), &len) != 0)
        throw SocketException(GetSocketError(), "setIPv6Only() failed: getsockname() failed");

    if (ss.ss_family != AF_INET6)
        throw SocketException("setIPv6Only() failed: socket is not IPv6");

    setOption(IPPROTO_IPV6, IPV6_V6ONLY, enable ? 1 : 0);
}

bool SocketOptions::getIPv6Only() const
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("getIPv6Only() failed: socket is not open.");

    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);
    if (::getsockname(_sockFd, reinterpret_cast<sockaddr*>(&ss), &len) != 0)
        throw SocketException(GetSocketError(), "getIPv6Only() failed: getsockname() failed");

    if (ss.ss_family != AF_INET6)
        throw SocketException("getIPv6Only() failed: socket is not IPv6");

    return getOption(IPPROTO_IPV6, IPV6_V6ONLY) != 0;
}

#endif

} // namespace jsocketpp
