#include "jsocketpp/MulticastSocket.hpp"

using namespace jsocketpp;

MulticastSocket::MulticastSocket(unsigned short port, std::size_t bufferSize)
    : DatagramSocket(port, bufferSize),
      _ttl(1),
      _loopbackEnabled(true)
{
    // Optionally, you could set default multicast options here if desired.
}

void MulticastSocket::joinGroup(const std::string& groupAddr, const std::string& iface)
{
    if (groupAddr.empty())
        throw SocketException(0, "Multicast group address cannot be empty.");

    // Determine if it's IPv4 or IPv6
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* res = nullptr;
    int ret = getaddrinfo(groupAddr.c_str(), nullptr, &hints, &res);
    if (ret != 0 || !res)
        throw SocketException(ret, "Invalid multicast group address.");

    if (res->ai_family == AF_INET)
    {
        // IPv4
        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = ((sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
        if (!iface.empty())
            mreq.imr_interface.s_addr = inet_addr(iface.c_str());
        else
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (setsockopt(_sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                       reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0)
        {
            freeaddrinfo(res);
            throw SocketException(GetSocketError(), "Failed to join IPv4 multicast group.");
        }
    }
    else if (res->ai_family == AF_INET6)
    {
        // IPv6
        ipv6_mreq mreq6{};
        mreq6.ipv6mr_multiaddr = ((sockaddr_in6*)res->ai_addr)->sin6_addr;
        // Interface index: 0 means "any"
        if (!iface.empty())
        {
#ifdef _WIN32
            mreq6.ipv6mr_interface = inet_addr(iface.c_str());
#else
            mreq6.ipv6mr_interface = if_nametoindex(iface.c_str());
#endif
        }
        else
        {
            mreq6.ipv6mr_interface = 0;
        }

        if (setsockopt(_sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                       reinterpret_cast<const char*>(&mreq6), sizeof(mreq6)) < 0)
        {
            freeaddrinfo(res);
            throw SocketException(GetSocketError(), "Failed to join IPv6 multicast group.");
        }
    }
    else
    {
        freeaddrinfo(res);
        throw SocketException(0, "Unsupported address family for multicast.");
    }
    _currentGroup = groupAddr;
    _currentInterface = iface;
    freeaddrinfo(res);
}

void MulticastSocket::leaveGroup(const std::string& groupAddr, const std::string& iface)
{
    if (groupAddr.empty())
        throw SocketException(0, "Multicast group address cannot be empty.");

    // Determine if it's IPv4 or IPv6
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* res = nullptr;
    int ret = getaddrinfo(groupAddr.c_str(), nullptr, &hints, &res);
    if (ret != 0 || !res)
        throw SocketException(ret, "Invalid multicast group address.");

    if (res->ai_family == AF_INET)
    {
        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = ((sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
        if (!iface.empty())
            mreq.imr_interface.s_addr = inet_addr(iface.c_str());
        else
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (setsockopt(_sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                       reinterpret_cast<const char*>(&mreq), sizeof(mreq)) < 0)
        {
            freeaddrinfo(res);
            throw SocketException(GetSocketError(), "Failed to leave IPv4 multicast group.");
        }
    }
    else if (res->ai_family == AF_INET6)
    {
        ipv6_mreq mreq6{};
        mreq6.ipv6mr_multiaddr = ((sockaddr_in6*)res->ai_addr)->sin6_addr;
        if (!iface.empty())
        {
#ifdef _WIN32
            mreq6.ipv6mr_interface = inet_addr(iface.c_str());
#else
            mreq6.ipv6mr_interface = if_nametoindex(iface.c_str());
#endif
        }
        else
        {
            mreq6.ipv6mr_interface = 0;
        }

        if (setsockopt(_sockfd, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
                       reinterpret_cast<const char*>(&mreq6), sizeof(mreq6)) < 0)
        {
            freeaddrinfo(res);
            throw SocketException(GetSocketError(), "Failed to leave IPv6 multicast group.");
        }
    }
    else
    {
        freeaddrinfo(res);
        throw SocketException(0, "Unsupported address family for multicast.");
    }
    _currentGroup.clear();
    _currentInterface.clear();
    freeaddrinfo(res);
}

void MulticastSocket::setMulticastInterface(const std::string& iface)
{
    if (iface.empty())
        throw SocketException(0, "Interface cannot be empty.");

    // This sets the outgoing interface for multicast
    // For IPv4, interface is specified by address (INADDR)
    // For IPv6, by index

    // Try to set for IPv4
    in_addr addr{};
    addr.s_addr = inet_addr(iface.c_str());
    if (addr.s_addr != INADDR_NONE)
    {
        if (setsockopt(_sockfd, IPPROTO_IP, IP_MULTICAST_IF,
                       reinterpret_cast<const char*>(&addr), sizeof(addr)) < 0)
        {
            throw SocketException(GetSocketError(), "Failed to set IPv4 multicast interface.");
        }
    }
#ifndef _WIN32
    // For IPv6: use interface index
    unsigned int idx = if_nametoindex(iface.c_str());
    if (idx != 0)
    {
        if (setsockopt(_sockfd, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                       reinterpret_cast<const char*>(&idx), sizeof(idx)) < 0)
        {
            throw SocketException(GetSocketError(), "Failed to set IPv6 multicast interface.");
        }
    }
#endif
    _currentInterface = iface;
}

void MulticastSocket::setTimeToLive(int ttl)
{
    if (ttl < 0 || ttl > 255)
        throw SocketException(0, "TTL must be in [0,255]");

    // IPv4
    unsigned char v4ttl = static_cast<unsigned char>(ttl);
    if (setsockopt(_sockfd, IPPROTO_IP, IP_MULTICAST_TTL,
                   reinterpret_cast<const char*>(&v4ttl), sizeof(v4ttl)) < 0)
        throw SocketException(GetSocketError(), "Failed to set IPv4 multicast TTL");

    // IPv6
    int v6ttl = ttl;
    if (setsockopt(_sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                   reinterpret_cast<const char*>(&v6ttl), sizeof(v6ttl)) < 0)
        throw SocketException(GetSocketError(), "Failed to set IPv6 multicast hops");

    _ttl = ttl;
}

void MulticastSocket::setLoopbackMode(bool enabled)
{
    // IPv4
    unsigned char v4loop = enabled ? 1 : 0;
    if (setsockopt(_sockfd, IPPROTO_IP, IP_MULTICAST_LOOP,
                   reinterpret_cast<const char*>(&v4loop), sizeof(v4loop)) < 0)
        throw SocketException(GetSocketError(), "Failed to set IPv4 multicast loopback");

    // IPv6
    unsigned int v6loop = enabled ? 1 : 0;
    if (setsockopt(_sockfd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
                   reinterpret_cast<const char*>(&v6loop), sizeof(v6loop)) < 0)
        throw SocketException(GetSocketError(), "Failed to set IPv6 multicast loopback");

    _loopbackEnabled = enabled;
}

std::string MulticastSocket::getCurrentGroup() const
{
    return _currentGroup;
}
