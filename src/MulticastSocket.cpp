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

void MulticastSocket::joinGroup(const std::string& groupAddr, const std::string& iface)
{
    if (groupAddr.empty())
        throw SocketException("Multicast group address cannot be empty.");

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC; // Support both IPv4 and IPv6 groups
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* res = nullptr;
    if (const int ret = getaddrinfo(groupAddr.c_str(), nullptr, &hints, &res); ret != 0 || !res)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error, true));
    }

    if (res->ai_family == AF_INET)
    {
        // IPv4 multicast
        ip_mreq mReq{};
        mReq.imr_multiaddr.s_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr.s_addr;

        // Handle interface (address or name)
        if (!iface.empty())
        {
            in_addr interfaceAddr{};
            // Try to parse as IPv4 address
            if (inet_pton(AF_INET, iface.c_str(), &interfaceAddr) == 1)
            {
                mReq.imr_interface = interfaceAddr;
            }
            else
            {
                // If not an IPv4 address, try resolving as hostname
                addrinfo ifaceHints{};
                ifaceHints.ai_family = AF_INET;
                ifaceHints.ai_socktype = SOCK_DGRAM;
                ifaceHints.ai_protocol = IPPROTO_UDP;
                addrinfo* ifaceRes = nullptr;
                if (getaddrinfo(iface.c_str(), nullptr, &ifaceHints, &ifaceRes) == 0 && ifaceRes)
                {
                    mReq.imr_interface = reinterpret_cast<sockaddr_in*>(ifaceRes->ai_addr)->sin_addr;
                    freeaddrinfo(ifaceRes);
                }
                else
                {
                    freeaddrinfo(res);
                    throw SocketException("Failed to resolve IPv4 interface address: " + iface);
                }
            }
        }
        else
        {
            mReq.imr_interface.s_addr = htonl(INADDR_ANY);
        }

        if (setsockopt(getSocketFd(), IPPROTO_IP, IP_ADD_MEMBERSHIP,
#ifdef _WIN32
                       reinterpret_cast<const char*>(&mReq),
#else
                       &mReq,
#endif
                       sizeof(mReq)) < 0)
        {
            const int err = GetSocketError();
            freeaddrinfo(res);
            throw SocketException(err, "Failed to join IPv4 multicast group: " + SocketErrorMessage(err));
        }
    }
    else if (res->ai_family == AF_INET6)
    {
        // IPv6 multicast
        ipv6_mreq mReq6{};
        mReq6.ipv6mr_multiaddr = reinterpret_cast<sockaddr_in6*>(res->ai_addr)->sin6_addr;

        // Handle interface (name or index)
        mReq6.ipv6mr_interface = 0; // default to any

        if (!iface.empty())
        {
#ifdef _WIN32
            // Windows: requires the numeric interface index
            // Try to parse iface as integer
            char* end = nullptr;
            if (const unsigned long idx = strtoul(iface.c_str(), &end, 10); end && *end == '\0' && idx > 0)
            {
                mReq6.ipv6mr_interface = static_cast<unsigned int>(idx);
            }
            else
            {
                freeaddrinfo(res);
                throw SocketException("On Windows, the IPv6 interface must be a numeric index.");
            }
#else
            // POSIX: allow interface name or index
            unsigned int idx = if_nametoindex(iface.c_str());
            if (idx == 0)
            {
                // If not a name, try numeric
                char* end = nullptr;
                idx = static_cast<unsigned int>(strtoul(iface.c_str(), &end, 10));
                if (end && *end != '\0')
                {
                    freeaddrinfo(res);
                    throw SocketException("Invalid IPv6 interface: " + iface);
                }
            }
            mReq6.ipv6mr_interface = idx;
#endif
        }

        // Use platform-appropriate option name for joining group
        constexpr int level = IPPROTO_IPV6;
#ifdef _WIN32
        constexpr int option = IPV6_ADD_MEMBERSHIP;
#else
        constexpr int option = IPV6_JOIN_GROUP;
#endif
        if (setsockopt(getSocketFd(), level, option,
#ifdef _WIN32
                       reinterpret_cast<const char*>(&mReq6),
#else
                       &mReq6,
#endif
                       sizeof(mReq6)) < 0)
        {
            const int err = GetSocketError();
            freeaddrinfo(res);
            throw SocketException(err, "Failed to join IPv6 multicast group: " + SocketErrorMessage(err));
        }
    }
    else
    {
        freeaddrinfo(res);
        throw SocketException("Unsupported address family for multicast: " + std::to_string(res->ai_family));
    }

    // Store group/interface
    _currentGroup = groupAddr;
    _currentInterface = iface;
    freeaddrinfo(res);
}

void MulticastSocket::leaveGroup(const std::string& groupAddr, const std::string& iface)
{
    if (groupAddr.empty())
        throw SocketException("Multicast group address cannot be empty.");

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC; // Support both IPv4 and IPv6 groups
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* res = nullptr;
    if (const auto ret = getaddrinfo(groupAddr.c_str(), nullptr, &hints, &res); ret != 0 || !res)
    {
        throw SocketException(
#ifdef _WIN32
            GetSocketError(),
#else
            ret,
#endif
            SocketErrorMessage(
#ifdef _WIN32
                GetSocketError(),
#else
                ret,
#endif
                true));
    }

    if (res->ai_family == AF_INET)
    {
        // IPv4 multicast
        ip_mreq mReq{};
        mReq.imr_multiaddr.s_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr.s_addr;

        if (!iface.empty())
        {
            in_addr interfaceAddr{};
            if (inet_pton(AF_INET, iface.c_str(), &interfaceAddr) == 1)
            {
                mReq.imr_interface = interfaceAddr;
            }
            else
            {
                addrinfo ifaceHints{};
                ifaceHints.ai_family = AF_INET;
                ifaceHints.ai_socktype = SOCK_DGRAM;
                ifaceHints.ai_protocol = IPPROTO_UDP;
                addrinfo* ifaceRes = nullptr;
                if (getaddrinfo(iface.c_str(), nullptr, &ifaceHints, &ifaceRes) == 0 && ifaceRes)
                {
                    mReq.imr_interface = reinterpret_cast<sockaddr_in*>(ifaceRes->ai_addr)->sin_addr;
                    freeaddrinfo(ifaceRes);
                }
                else
                {
                    freeaddrinfo(res);
                    throw SocketException("Failed to resolve IPv4 interface address: " + iface);
                }
            }
        }
        else
        {
            mReq.imr_interface.s_addr = htonl(INADDR_ANY);
        }

        if (setsockopt(getSocketFd(), IPPROTO_IP, IP_DROP_MEMBERSHIP,
#ifdef _WIN32
                       reinterpret_cast<const char*>(&mReq),
#else
                       &mReq,
#endif
                       sizeof(mReq)) < 0)
        {
            const int err = GetSocketError();
            freeaddrinfo(res);
            throw SocketException(err, "Failed to leave IPv4 multicast group: " + SocketErrorMessage(err));
        }
    }
    else if (res->ai_family == AF_INET6)
    {
        // IPv6 multicast
        ipv6_mreq mReq6{};
        mReq6.ipv6mr_multiaddr = reinterpret_cast<sockaddr_in6*>(res->ai_addr)->sin6_addr;
        mReq6.ipv6mr_interface = 0; // default to any

        if (!iface.empty())
        {
#ifdef _WIN32
            // Windows: requires the numeric interface index
            char* end = nullptr;
            if (const unsigned long idx = strtoul(iface.c_str(), &end, 10); end && *end == '\0' && idx > 0)
            {
                mReq6.ipv6mr_interface = static_cast<unsigned int>(idx);
            }
            else
            {
                freeaddrinfo(res);
                throw SocketException("On Windows, the IPv6 interface must be a numeric index.");
            }
#else
            unsigned int idx = if_nametoindex(iface.c_str());
            if (idx == 0)
            {
                // Try as a numeric index string
                char* end = nullptr;
                idx = static_cast<unsigned int>(strtoul(iface.c_str(), &end, 10));
                if (end && *end != '\0')
                {
                    freeaddrinfo(res);
                    throw SocketException("Invalid IPv6 interface: " + iface);
                }
            }
            mReq6.ipv6mr_interface = idx;
#endif
        }

        // Use platform-appropriate option name for leaving group
        constexpr int level = IPPROTO_IPV6;
#ifdef _WIN32
        constexpr int option = IPV6_DROP_MEMBERSHIP;
#else
        constexpr int option = IPV6_LEAVE_GROUP;
#endif
        if (setsockopt(getSocketFd(), level, option,
#ifdef _WIN32
                       reinterpret_cast<const char*>(&mReq6),
#else
                       &mReq6,
#endif
                       sizeof(mReq6)) < 0)
        {
            const int err = GetSocketError();
            freeaddrinfo(res);
            throw SocketException(err, "Failed to leave IPv6 multicast group: " + SocketErrorMessage(err));
        }
    }
    else
    {
        freeaddrinfo(res);
        throw SocketException("Unsupported address family for multicast: " + std::to_string(res->ai_family));
    }

    // Optionally clear state if this matches current group/interface
    if (_currentGroup == groupAddr && _currentInterface == iface)
    {
        _currentGroup.clear();
        _currentInterface.clear();
    }
    freeaddrinfo(res);
}

void MulticastSocket::setMulticastInterface(const std::string& iface)
{
    // Fast path: no change
    if (iface == _currentInterface)
        return;

    // Case 1: empty => reset to defaults for both families
    if (iface.empty())
    {
        in_addr any4{};
        any4.s_addr = htonl(INADDR_ANY);
        setMulticastInterfaceIPv4(any4); // SocketOptions helper
        setMulticastInterfaceIPv6(0);    // default v6 route
        _currentInterface.clear();
        return;
    }

    // Case 2: IPv4 literal => use as egress address (IPv4 multicast only)
    in_addr v4{};
    if (inet_pton(AF_INET, iface.c_str(), &v4) == 1)
    {
        setMulticastInterfaceIPv4(v4);
        _currentInterface = iface;
        return;
    }

    // Case 3: numeric => treat as IPv6 interface index
    {
        unsigned int idx = 0;
        const char* b = iface.data();
        const char* e = iface.data() + iface.size();
        if (auto [ptr, ec] = std::from_chars(b, e, idx); ec == std::errc{} && ptr == e)
        {
            setMulticastInterfaceIPv6(idx);
            _currentInterface = iface;
            return;
        }
    }

    // Case 4: POSIX interface name => map to index (Windows: unsupported)
#if !defined(_WIN32)
    {
        const unsigned int idx = if_nametoindex(iface.c_str());
        if (idx == 0)
        {
            throw SocketException(std::string("Unknown interface name: ") + iface);
        }
        setMulticastInterfaceIPv6(idx);
        _currentInterface = iface;
    }
#else
    // On Windows, interface names are not accepted for IPV6_MULTICAST_IF.
    // Require a numeric index string instead.
    throw SocketException(
        "Invalid multicast interface identifier on Windows. "
        "Provide an IPv4 address (e.g., \"192.0.2.10\") or a numeric IPv6 interface index (e.g., \"12\").");
#endif
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
