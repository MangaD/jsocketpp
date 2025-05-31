#pragma once

#include "DatagramSocket.hpp"
#include "common.hpp"

#include <string>

namespace jsocketpp
{

/**
 * @brief MulticastSocket: a UDP socket with multicast support (Java-style interface).
 *
 * Inherits from DatagramSocket and adds methods to join/leave multicast groups,
 * set multicast-specific socket options, and send/receive multicast datagrams.
 *
 * Supports both IPv4 and IPv6.
 *
 * @note Not thread-safe. Should be used from a single thread at a time.
 */
class MulticastSocket : public DatagramSocket
{
public:
    /**
     * @brief Constructs a multicast socket optionally bound to a local port and buffer size.
     * @param port The local UDP port to bind to (0 means any port).
     * @param bufferSize The size of the receive buffer (default: 2048 bytes).
     * @throws SocketException on failure.
     */
    explicit MulticastSocket(unsigned short port = 0, std::size_t bufferSize = 2048);

    /**
     * @brief Joins a multicast group.
     * @param groupAddr Multicast group address (e.g., "239.255.0.1" or "ff02::1").
     * @param iface Optional: name or IP of local interface to join on (default: any).
     * @throws SocketException on failure.
     */
    void joinGroup(const std::string& groupAddr, const std::string& iface = "");

    /**
     * @brief Leaves a multicast group.
     * @param groupAddr Multicast group address (e.g., "239.255.0.1" or "ff02::1").
     * @param iface Optional: name or IP of local interface to leave on (default: any).
     * @throws SocketException on failure.
     */
    void leaveGroup(const std::string& groupAddr, const std::string& iface = "");

    /**
     * @brief Sets the outgoing interface for multicast packets.
     * @param iface Name or IP address of the interface.
     * @throws SocketException on failure.
     */
    void setMulticastInterface(const std::string& iface);

    /**
     * @brief Sets the multicast TTL (Time To Live).
     * @param ttl Number of network hops allowed (1=local, 255=max).
     * @throws SocketException on failure.
     */
    void setTimeToLive(int ttl);

    /**
     * @brief Enables or disables multicast loopback.
     * @param enabled true to enable, false to disable.
     * @throws SocketException on failure.
     */
    void setLoopbackMode(bool enabled);

    /**
     * @brief Returns the last multicast group joined (for reference/debug).
     */
    std::string getCurrentGroup() const;

private:
    std::string _currentGroup;       ///< Last joined multicast group address.
    std::string _currentInterface;   ///< Interface used for multicast.
    int _ttl = 1;                    ///< Multicast TTL (default 1).
    bool _loopbackEnabled = true;    ///< Whether multicast loopback is enabled.
};

}