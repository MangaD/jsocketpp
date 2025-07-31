#include "jsocketpp/MulticastSocket.hpp"

using namespace jsocketpp;

MulticastSocket::MulticastSocket(const Port port, const std::size_t bufferSize) : DatagramSocket(port, bufferSize)
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
        throw SocketException(
#ifdef _WIN32
            GetSocketError(),
#else
            ret,
#endif
            SocketErrorMessageWrap(
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
            if (unsigned long idx = strtoul(iface.c_str(), &end, 10); end && *end == '\0' && idx > 0)
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
            SocketErrorMessageWrap(
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
            if (unsigned long idx = strtoul(iface.c_str(), &end, 10); end && *end == '\0' && idx > 0)
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
    if (iface.empty())
    {
        // Default: OS selects the interface automatically
#ifdef _WIN32
        // IPv4: set to INADDR_ANY
        auto addr = INADDR_ANY;
        if (setsockopt(getSocketFd(), IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&addr), sizeof(addr)) <
            0)
            throw SocketException(GetSocketError(), "Failed to clear IPv4 multicast interface");
        // IPv6: set to 0
        DWORD idx = 0;
        if (setsockopt(getSocketFd(), IPPROTO_IPV6, IPV6_MULTICAST_IF, reinterpret_cast<const char*>(&idx),
                       sizeof(idx)) < 0)
            throw SocketException(GetSocketError(), "Failed to clear IPv6 multicast interface");
#else
        // IPv4: set to INADDR_ANY
        in_addr addr{};
        addr.s_addr = htonl(INADDR_ANY);
        if (setsockopt(getSocketFd(), IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) < 0)
            throw SocketException(GetSocketError(), "Failed to clear IPv4 multicast interface");
        // IPv6: set to 0
        unsigned int idx = 0;
        if (setsockopt(getSocketFd(), IPPROTO_IPV6, IPV6_MULTICAST_IF, &idx, sizeof(idx)) < 0)
            throw SocketException(GetSocketError(), "Failed to clear IPv6 multicast interface");
#endif
        _currentInterface.clear();
        return;
    }

    // Try to interpret as IPv4 address
    in_addr v4addr{};
    if (inet_pton(AF_INET, iface.c_str(), &v4addr) == 1)
    {
        // Set IPv4 outgoing interface
#ifdef _WIN32
        DWORD addr = v4addr.s_addr;
        if (setsockopt(getSocketFd(), IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&addr), sizeof(addr)) <
            0)
            throw SocketException(GetSocketError(), "Failed to set IPv4 multicast interface to " + iface);
#else
        if (setsockopt(getSocketFd(), IPPROTO_IP, IP_MULTICAST_IF, &v4addr, sizeof(v4addr)) < 0)
            throw SocketException(GetSocketError(), "Failed to set IPv4 multicast interface to " + iface);
#endif
        _currentInterface = iface;
        return;
    }

    // IPv6: treat as interface name or index
#ifdef _WIN32
    char* end = nullptr;
    DWORD idx = strtoul(iface.c_str(), &end, 10);
    if (!iface.empty() && end && *end == '\0' && idx > 0)
    {
        // Numeric index specified as string
        if (setsockopt(getSocketFd(), IPPROTO_IPV6, IPV6_MULTICAST_IF, reinterpret_cast<const char*>(&idx),
                       sizeof(idx)) < 0)
            throw SocketException(GetSocketError(), "Failed to set IPv6 multicast interface to index " + iface);
        _currentInterface = iface;
        return;
    }
    throw SocketException("On Windows, IPv6 multicast interface must be a numeric index (e.g., \"2\").");
#else
    // Try as name, fallback to numeric index
    unsigned int idx = if_nametoindex(iface.c_str());
    if (idx == 0)
    {
        // Try as numeric index string
        char* end = nullptr;
        idx = static_cast<unsigned int>(strtoul(iface.c_str(), &end, 10));
        if (!iface.empty() && (!end || *end != '\0' || idx == 0))
            throw SocketException("Invalid IPv6 interface: " + iface);
    }
    if (setsockopt(getSocketFd(), IPPROTO_IPV6, IPV6_MULTICAST_IF, &idx, sizeof(idx)) < 0)
        throw SocketException(GetSocketError(), "Failed to set IPv6 multicast interface to " + iface);
    _currentInterface = iface;
#endif
}

void MulticastSocket::setTimeToLive(int ttl)
{
    if (ttl < 0 || ttl > 255)
        throw SocketException("TTL value must be between 0 and 255");

    if (ttl == _ttl)
        return;

    // Set TTL for IPv4 multicast
    // Windows wants a DWORD, Linux wants an int
#ifdef _WIN32
    auto v4ttl = static_cast<DWORD>(ttl);
    if (setsockopt(getSocketFd(), IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&v4ttl), sizeof(v4ttl)) <
        0)
        throw SocketException(GetSocketError(), "Failed to set IPv4 multicast TTL");
#else
    int v4ttl = ttl;
    if (setsockopt(getSocketFd(), IPPROTO_IP, IP_MULTICAST_TTL, &v4ttl, sizeof(v4ttl)) < 0)
        throw SocketException(GetSocketError(), "Failed to set IPv4 multicast TTL");
#endif

    // Set hop limit for IPv6 multicast
#ifdef _WIN32
    auto v6hops = static_cast<DWORD>(ttl);
    if (setsockopt(getSocketFd(), IPPROTO_IPV6, IPV6_MULTICAST_HOPS, reinterpret_cast<const char*>(&v6hops),
                   sizeof(v6hops)) < 0)
        throw SocketException(GetSocketError(), "Failed to set IPv6 multicast hop limit");
#else
    int v6hops = ttl;
    if (setsockopt(getSocketFd(), IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &v6hops, sizeof(v6hops)) < 0)
        throw SocketException(GetSocketError(), "Failed to set IPv6 multicast hop limit");
#endif

    _ttl = ttl;
}

void MulticastSocket::setLoopbackMode(const bool enable)
{
    // Determine address family
    int family = AF_UNSPEC;
    if (_selectedAddrInfo)
        family = _selectedAddrInfo->ai_family;
    else if (_addrInfoPtr)
        family = _addrInfoPtr->ai_family;
    // else: default to IPv4

    const int flag = enable ? 1 : 0;

    int result = 0;
    if (family == AF_INET6)
    {
        // IPv6 multicast loopback
        result = setsockopt(getSocketFd(), IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
#ifdef _WIN32
                            reinterpret_cast<const char*>(&flag),
#else
                            &flag,
#endif
                            sizeof(flag));
    }
    else
    {
        // IPv4 multicast loopback (default)
        result = setsockopt(getSocketFd(), IPPROTO_IP, IP_MULTICAST_LOOP,
#ifdef _WIN32
                            reinterpret_cast<const char*>(&flag),
#else
                            &flag,
#endif
                            sizeof(flag));
    }
    if (result == SOCKET_ERROR)
    {
        throw SocketException(GetSocketError(), "Failed to set multicast loopback mode.");
    }
    _loopbackEnabled = enable; // (Optional: if you want to store it as a private member)
}

std::string MulticastSocket::getCurrentGroup() const
{
    return _currentGroup;
}
