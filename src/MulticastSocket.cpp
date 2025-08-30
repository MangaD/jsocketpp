#include "jsocketpp/MulticastSocket.hpp"

#include <charconv>

using namespace jsocketpp;

MulticastSocket::MulticastSocket(const Port localPort, const std::string_view localAddress,
                                 const std::optional<std::size_t> recvBufferSize,
                                 const std::optional<std::size_t> sendBufferSize,
                                 const std::optional<std::size_t> internalBufferSize, const bool reuseAddress,
                                 const int soRecvTimeoutMillis, const int soSendTimeoutMillis, const bool nonBlocking,
                                 const bool dualStack, const bool autoBind)
    : DatagramSocket(localPort, localAddress, recvBufferSize, sendBufferSize, internalBufferSize, reuseAddress,
                     soRecvTimeoutMillis, soSendTimeoutMillis, nonBlocking, dualStack, autoBind)
{
    setTimeToLive(_ttl);
    setLoopbackMode(_loopbackEnabled);
}

void MulticastSocket::setMulticastInterface(const std::string& iface)
{
    // Fast path: unchanged
    if (iface == _currentInterface)
    {
        return;
    }

    // Case 1: empty => reset to default for THIS socket's family only
    if (iface.empty())
    {
        if (const int fam = detectFamily(getSocketFd()); fam == AF_INET)
        {
            in_addr any4{};
            any4.s_addr = htonl(INADDR_ANY);
            setMulticastInterfaceIPv4(any4);
        }
        else if (fam == AF_INET6)
        {
            setMulticastInterfaceIPv6(0); // default IPv6 route
        }
        else
        {
            throw SocketException("setMulticastInterface: unsupported socket family");
        }
        _currentInterface.clear();
        return;
    }

    // Case 2: IPv4 literal => use as IPv4 egress address
    {
        in_addr v4{};
        if (inet_pton(AF_INET, iface.c_str(), &v4) == 1)
        {
            setMulticastInterfaceIPv4(v4);
            _currentInterface = iface;
            return;
        }
    }

    // Case 3: IPv6 interface index (decimal) or POSIX interface name
    // (Windows: names unsupported; toIfIndexFromString() will throw)
    {
        const unsigned int idx = toIfIndexFromString(iface);
        setMulticastInterfaceIPv6(idx);
        _currentInterface = iface;
    }
}

void MulticastSocket::setTimeToLive(const int ttl)
{
    if (ttl == _ttl)
        return;

    setMulticastTTL(ttl);

    _ttl = ttl;
}

void MulticastSocket::setLoopbackMode(const bool enable)
{
    if (enable == _loopbackEnabled)
        return;

    setMulticastLoopback(enable);

    _loopbackEnabled = enable;
}

std::string MulticastSocket::getCurrentGroup() const
{
    return _currentGroup;
}

in_addr MulticastSocket::resolveIPv4(const std::string_view host)
{
    if (host.empty())
    {
        throw SocketException("resolveIPv4: host must not be empty");
    }

    // inet_pton requires a NUL-terminated C string -> make a local copy
    const std::string literal{host};

    in_addr out{};
    if (inet_pton(AF_INET, literal.c_str(), &out) == 1)
    {
        // Literal IPv4; already in network byte order
        return out;
    }

    // Fallback: use project resolver for AF_INET
    const auto ai = internal::resolveAddress(host, Port{0}, AF_INET, SOCK_DGRAM, IPPROTO_UDP, 0);
    for (const addrinfo* p = ai.get(); p != nullptr; p = p->ai_next)
    {
        if (p->ai_family == AF_INET)
        {
            return reinterpret_cast<const sockaddr_in*>(p->ai_addr)->sin_addr;
        }
    }

    throw SocketException(std::string("resolveIPv4: no AF_INET result for host: ").append(host));
}

in6_addr MulticastSocket::resolveIPv6(const std::string_view host)
{
    if (host.empty())
    {
        throw SocketException("resolveIPv6: host must not be empty");
    }

    // inet_pton requires a NUL-terminated C string -> make a local copy
    const std::string literal{host};

    in6_addr out{};
    if (inet_pton(AF_INET6, literal.c_str(), &out) == 1)
    {
        return out; // literal
    }

    const auto ai = internal::resolveAddress(host, Port{0}, AF_INET6, SOCK_DGRAM, IPPROTO_UDP, 0);
    for (const addrinfo* p = ai.get(); p != nullptr; p = p->ai_next)
    {
        if (p->ai_family == AF_INET6)
        {
            return reinterpret_cast<const sockaddr_in6*>(p->ai_addr)->sin6_addr;
        }
    }

    throw SocketException(std::string("resolveIPv6: no AF_INET6 result for host: ").append(host));
}

unsigned int MulticastSocket::toIfIndexFromString(const std::string& iface)
{
    if (iface.empty())
        return 0;

    unsigned int idx = 0;
    const char* b = iface.data();
    const char* e = b + iface.size();
    if (auto [ptr, ec] = std::from_chars(b, e, idx); ec == std::errc{} && ptr == e)
        return idx;

#if !defined(_WIN32)
    if (unsigned int n = if_nametoindex(iface.c_str()); n != 0)
        return n;
    throw SocketException("Unknown IPv6 interface name: " + iface);
#else
    throw SocketException(
        "Invalid IPv6 interface identifier on Windows. Provide a numeric interface index (e.g., \"12\").");
#endif
}

void MulticastSocket::joinGroup(const std::string& groupAddr, const std::string& iface)
{
    if (groupAddr.empty())
    {
        throw SocketException("joinGroup: groupAddr must not be empty");
    }

    // Fast path: literal v4?
    in_addr g4{};
    if (inet_pton(AF_INET, groupAddr.c_str(), &g4) == 1)
    {
        if (!IN_MULTICAST(ntohl(g4.s_addr)))
        {
            throw SocketException("joinGroup: not an IPv4 multicast address: " + groupAddr);
        }
        in_addr if4{};
        if4.s_addr = htonl(INADDR_ANY);
        if (!iface.empty())
        {
            // IPv4 interface is specified by its IPv4 address
            if4 = resolveIPv4(iface);
        }
        joinGroupIPv4(g4, if4); // SocketOptions helper
        _currentGroup = groupAddr;
        _currentInterface = iface; // keep what user passed
        return;
    }

    // Fast path: literal v6?
    in6_addr g6{};
    if (inet_pton(AF_INET6, groupAddr.c_str(), &g6) == 1)
    {
        if (!IN6_IS_ADDR_MULTICAST(&g6))
        {
            throw SocketException("joinGroup: not an IPv6 multicast address: " + groupAddr);
        }
        const unsigned int ifindex = toIfIndexFromString(iface); // 0 if empty
        joinGroupIPv6(g6, ifindex);                              // SocketOptions helper
        _currentGroup = groupAddr;
        _currentInterface = iface;
        return;
    }

    // Resolve non-literals using your resolver; prefer v4 first, then v6
    try
    {
        g4 = resolveIPv4(groupAddr);
        if (!IN_MULTICAST(ntohl(g4.s_addr)))
        {
            throw SocketException("joinGroup: resolved address is not IPv4 multicast: " + groupAddr);
        }
        in_addr if4{};
        if4.s_addr = htonl(INADDR_ANY);
        if (!iface.empty())
            if4 = resolveIPv4(iface);
        joinGroupIPv4(g4, if4);
        _currentGroup = groupAddr;
        _currentInterface = iface;
        return;
    }
    catch (const SocketException&)
    {
        // fall through to try v6
    }

    g6 = resolveIPv6(groupAddr);
    if (!IN6_IS_ADDR_MULTICAST(&g6))
    {
        throw SocketException("joinGroup: resolved address is not IPv6 multicast: " + groupAddr);
    }
    const unsigned int ifindex = toIfIndexFromString(iface);
    joinGroupIPv6(g6, ifindex);
    _currentGroup = groupAddr;
    _currentInterface = iface;
}

void MulticastSocket::leaveGroup(const std::string& groupAddr, const std::string& iface)
{
    if (groupAddr.empty())
    {
        throw SocketException("leaveGroup: groupAddr must not be empty");
    }

    in_addr g4{};
    if (inet_pton(AF_INET, groupAddr.c_str(), &g4) == 1)
    {
        in_addr if4{};
        if4.s_addr = htonl(INADDR_ANY);
        if (!iface.empty())
            if4 = resolveIPv4(iface);
        leaveGroupIPv4(g4, if4);
        // (Optionally) clear caches if they match
        if (_currentGroup == groupAddr && _currentInterface == iface)
        {
            _currentGroup.clear();
            _currentInterface.clear();
        }
        return;
    }

    in6_addr g6{};
    if (inet_pton(AF_INET6, groupAddr.c_str(), &g6) == 1)
    {
        const unsigned int ifindex = toIfIndexFromString(iface);
        leaveGroupIPv6(g6, ifindex);
        if (_currentGroup == groupAddr && _currentInterface == iface)
        {
            _currentGroup.clear();
            _currentInterface.clear();
        }
        return;
    }

    // Resolve non-literals
    try
    {
        g4 = resolveIPv4(groupAddr);
        in_addr if4{};
        if4.s_addr = htonl(INADDR_ANY);
        if (!iface.empty())
            if4 = resolveIPv4(iface);
        leaveGroupIPv4(g4, if4);
    }
    catch (const SocketException&)
    {
        g6 = resolveIPv6(groupAddr);
        const unsigned int ifindex = toIfIndexFromString(iface);
        leaveGroupIPv6(g6, ifindex);
    }

    if (_currentGroup == groupAddr && _currentInterface == iface)
    {
        _currentGroup.clear();
        _currentInterface.clear();
    }
}
