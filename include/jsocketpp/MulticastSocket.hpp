#pragma once

#include "DatagramSocket.hpp"
#include "common.hpp"

#include <string>

namespace jsocketpp
{

/**
 * @brief MulticastSocket: a UDP socket with multicast support (Java-style interface).
 *
 * Multicast is a form of communication that allows a single sender to transmit data to multiple
 * receivers simultaneously. It's like a radio broadcast where:
 * - Multiple receivers can "tune in" to receive the same data
 * - Receivers must explicitly join a multicast group (like tuning to a specific frequency)
 * - Data is sent once but received by all group members
 *
 * Key Concepts:
 * - Multicast Groups: Special IP addresses (224.0.0.0 to 239.255.255.255 for IPv4,
 *   or ff00::/8 for IPv6) that represent a group of interested receivers
 * - TTL (Time To Live): Controls how far multicast packets can travel through the network
 * - Loopback: Determines if the sender can receive its own multicast packets
 *
 * Common Use Cases:
 * - Live video/audio streaming to multiple clients
 * - Service discovery in local networks
 * - Real-time data distribution (e.g., stock quotes)
 *
 * Inherits from DatagramSocket and adds methods to join/leave multicast groups,
 * set multicast-specific socket options, and send/receive multicast datagrams.
 *
 * Example usage:
 * ```cpp
 * MulticastSocket socket(0);  // Create socket on any available port
 * socket.setTimeToLive(32);   // Allow packets to traverse multiple networks
 * socket.joinGroup("239.0.0.1"); // Join a multicast group
 * socket.send(data, length, "239.0.0.1", 8888); // Send to all group members
 * ```
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
     * @param port The local UDP port to bind to (0 means any port). On a server, you typically
     *            want to specify a fixed port that clients can connect to. On clients, using
     *            port 0 lets the system assign any available port.
     * @param bufferSize The size of the receive buffer (default: 2048 bytes). Larger values
     *                   may be needed for high-volume data reception.
     * @throws SocketException on failure.
     */
    explicit MulticastSocket(unsigned short port = 0, std::size_t bufferSize = 2048);

    /**
     * @brief Joins a multicast group.
     * @param groupAddr Multicast group address (e.g., "239.255.0.1" or "ff02::1").
     *                 IPv4 multicast addresses range from 224.0.0.0 to 239.255.255.255.
     *                 IPv6 multicast addresses start with 'ff'.
     * @param iface Optional: name or IP of local interface to join on (default: any).
     *             Useful when the host has multiple network interfaces and you want
     *             to receive multicast only on a specific one.
     * @throws SocketException on failure.
     */
    void joinGroup(const std::string& groupAddr, const std::string& iface = "");

    /**
     * @brief Leaves a multicast group.
     * @param groupAddr Multicast group address to leave (e.g., "239.255.0.1" or "ff02::1").
     * @param iface Optional: name or IP of local interface to leave on (default: any).
     *             Must match the interface specified in joinGroup() if one was used.
     * @throws SocketException on failure.
     */
    void leaveGroup(const std::string& groupAddr, const std::string& iface = "");

    /**
     * @brief Sets the default interface for outgoing multicast packets.
     *
     * For IPv4, the interface is specified as an IP address (e.g., "192.168.1.5").
     * For IPv6, the interface is specified as an interface name (e.g., "eth0") or numeric index (e.g., "2").
     *
     * @param iface The interface address (IPv4) or name/index (IPv6). An empty string disables interface selection.
     * @throws SocketException if setting the option fails or input is invalid.
     */
    void setMulticastInterface(const std::string& iface);

    /**
     * @brief Gets the currently set interface for outgoing multicast packets.
     *
     * Returns the interface that was set using setMulticastInterface():
     * - For IPv4: the IP address of the interface (e.g., "192.168.1.5")
     * - For IPv6: the interface name (e.g., "eth0") or index
     * - Empty string if no specific interface is set
     *
     * @return The current multicast interface address/name or empty string.
     */
    std::string getMulticastInterface() const { return _currentInterface; }

    /**
     * @brief Set the time-to-live (TTL) for outgoing multicast packets.
     *
     * For IPv4, this sets the IP_MULTICAST_TTL socket option.
     * For IPv6, this sets the IPV6_MULTICAST_HOPS socket option.
     *
     * @param ttl Number of network hops allowed (0-255):
     *           - 0: Restricted to the same host
     *           - 1: Restricted to the same subnet (default)
     *           - 32: Restricted to the same site
     *           - 64: Restricted to the same region
     *           - 128: Restricted to the same continent
     *           - 255: Unrestricted
     * @throws SocketException on failure.
     */
    void setTimeToLive(int ttl);

    /**
     * @brief Get the current time-to-live (TTL) value for outgoing multicast packets.
     *
     * Returns the TTL value that determines how many network hops multicast packets can traverse:
     * - 0: Restricted to the same host
     * - 1: Restricted to the same subnet
     * - 32: Restricted to the same site
     * - 64: Restricted to the same region
     * - 128: Restricted to the same continent
     * - 255: Unrestricted
     *
     * @return The current TTL value (0-255).
     */
    int getTimeToLive() const { return _ttl; }

    /**
     * @brief Enables or disables multicast loopback.
     * @param enable True to enable (receive own multicast packets), false to disable (don't receive own packets).
     *               Enable for testing or when the local host needs to process the same multicast packets as other
     *               receivers.
     * @throws SocketException on failure.
     */
    void setLoopbackMode(bool enable);

    /**
     * @brief Get the current multicast loopback mode setting.
     *
     * Returns whether the socket is configured to receive its own multicast packets:
     * - true: The socket will receive its own multicast packets (loopback enabled)
     * - false: The socket will not receive its own multicast packets (loopback disabled)
     *
     * @return true if loopback is enabled, false if disabled.
     */
    int getLoopbackMode() const { return _loopbackEnabled; }

    /**
     * @brief Returns the last multicast group joined (for reference/debug).
     * @return String containing the IP address of the last joined multicast group.
     */
    std::string getCurrentGroup() const;

  private:
    std::string _currentGroup{};     ///< Last joined multicast group address.
    std::string _currentInterface{}; ///< Interface used for multicast.

    /**
     * @brief Default TTL (Time To Live) value for multicast packets.
     *
     * Default value is 1, which means:
     * - Packets will only be delivered within the local subnet
     * - Packets will not cross any routers
     * - Ideal for local network discovery and communication
     *
     * This conservative default prevents accidental network flooding
     * and provides a safe starting point for most applications.
     */
    int _ttl = 1;

    /**
     * @brief Controls whether multicast packets loop back to the sending host.
     *
     * Default is true, which means:
     * - The sending host can receive its own multicast packets
     * - Useful for testing and debugging
     * - Allows applications on the same host to communicate
     * - May be disabled for production if self-reception is not needed
     */
    bool _loopbackEnabled = true;
};

} // namespace jsocketpp