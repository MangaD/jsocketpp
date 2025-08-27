// SocketOptions.cpp

#include "jsocketpp/SocketOptions.hpp"
#include "jsocketpp/SocketException.hpp"

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
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
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
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
}

// NOLINTNEXTLINE(readability-make-member-function-const) – modifies socket state
void SocketOptions::setReuseAddress(const bool on)
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

void SocketOptions::setSoRecvTimeout(const int millis)
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

void SocketOptions::setSoSendTimeout(const int millis)
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
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
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

void SocketOptions::setBroadcast(const bool on)
{
    setOption(SOL_SOCKET, SO_BROADCAST, on ? 1 : 0);
}

bool SocketOptions::getBroadcast() const
{
    return getOption(SOL_SOCKET, SO_BROADCAST) != 0;
}

#if defined(IPV6_V6ONLY)

void SocketOptions::setIPv6Only(const bool enable)
{
    if (_sockFd == INVALID_SOCKET)
        throw SocketException("setIPv6Only() failed: socket is not open.");

    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);
    if (::getsockname(_sockFd, reinterpret_cast<sockaddr*>(&ss), &len) != 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

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
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (ss.ss_family != AF_INET6)
        throw SocketException("getIPv6Only() failed: socket is not IPv6");

    return getOption(IPPROTO_IPV6, IPV6_V6ONLY) != 0;
}

#endif

#if defined(SO_REUSEPORT)

void SocketOptions::setReusePort(const bool enable)
{
    setOption(SOL_SOCKET, SO_REUSEPORT, enable ? 1 : 0);
}

bool SocketOptions::getReusePort() const
{
    return getOption(SOL_SOCKET, SO_REUSEPORT) != 0;
}

#endif

namespace
{

// Detect the bound family for this socket (AF_INET or AF_INET6).
int detect_family(const SOCKET fd)
{
    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&ss), &len) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
    return ss.ss_family;
}

} // namespace

// NOLINTNEXTLINE(readability-make-member-function-const) – modifies socket state
void SocketOptions::setMulticastTTL(const int ttl)
{
    if (ttl < 0 || ttl > 255)
        throw SocketException("setMulticastTTL() failed: TTL must be in [0,255].");

    const int family = detect_family(_sockFd);

#if defined(_WIN32)
    using sockopt_multicast_t = DWORD;
#else
    using sockopt_multicast_t = int;
#endif

    const auto v = static_cast<sockopt_multicast_t>(ttl);
    const int level = (family == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6;
    // NOLINTNEXTLINE - same value on some operating systems
    const int optName = (family == AF_INET) ? IP_MULTICAST_TTL : IPV6_MULTICAST_HOPS;

    setOption(level, optName, &v, sizeof(v));
}

int SocketOptions::getMulticastTTL() const
{
    if (const int family = detect_family(_sockFd); family == AF_INET)
    {
        return getOption(IPPROTO_IP, IP_MULTICAST_TTL);
    }
    return getOption(IPPROTO_IPV6, IPV6_MULTICAST_HOPS);
}

// NOLINTNEXTLINE(readability-make-member-function-const) – modifies socket state
void SocketOptions::setMulticastLoopback(const bool enable)
{
    const int family = detect_family(_sockFd);

    const int level = (family == AF_INET6) ? IPPROTO_IPV6 : IPPROTO_IP;
    // NOLINTNEXTLINE - same value on some OS's6666
    const int optName = (family == AF_INET6) ? IPV6_MULTICAST_LOOP : IP_MULTICAST_LOOP;

    const int v = enable ? 1 : 0;
    setOption(level, optName, &v, sizeof(v));
}

bool SocketOptions::getMulticastLoopback() const
{
    if (const int family = detect_family(_sockFd); family == AF_INET6)
    {
        return getOption(IPPROTO_IPV6, IPV6_MULTICAST_LOOP) != 0;
    }
    return getOption(IPPROTO_IP, IP_MULTICAST_LOOP) != 0;
}

void SocketOptions::setMulticastInterfaceIPv4(const in_addr addr)
{
#if defined(_WIN32)
    // Windows expects a DWORD containing the IPv4 address in network byte order
    const auto v = static_cast<DWORD>(addr.s_addr);
    setOption(IPPROTO_IP, IP_MULTICAST_IF, &v, sizeof(v));
#else
    // POSIX expects struct in_addr
    setOption(IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr));
#endif
}

void SocketOptions::setMulticastInterfaceIPv6(const unsigned int ifindex)
{
#if defined(_WIN32)
    // Windows expects a DWORD interface index
    const auto v = static_cast<DWORD>(ifindex);
    setOption(IPPROTO_IPV6, IPV6_MULTICAST_IF, &v, sizeof(v));
#else
    // POSIX expects an unsigned int interface index
    setOption(IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex));
#endif
}

} // namespace jsocketpp
