/**
 * @file MulticastSocket.hpp
 * @brief Multicast UDP socket abstraction for jsocketpp.
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

#pragma once

#include "common.hpp"
#include "DatagramSocket.hpp"

#include <string>

namespace jsocketpp
{

/**
 * @class MulticastSocket
 * @ingroup udp
 * @brief Cross-platform multicast UDP socket class (IPv4/IPv6).
 *
 * The `MulticastSocket` class extends @ref jsocketpp::DatagramSocket "DatagramSocket" to provide high-level,
 * Java-inspired support for multicast networking. It works on both Windows and Linux, and supports both IPv4 and IPv6
 * multicast groups.
 *
 * ### What is Multicast?
 * - **Multicast** allows you to send a single UDP packet to multiple hosts subscribed to a multicast group address.
 * - It is useful for applications such as real-time data feeds, streaming media, and network discovery protocols.
 * - Each group is identified by a special IP address (224.0.0.0–239.255.255.255 for IPv4, `ff00::/8` for IPv6).
 *
 * ### Usage Example (Receiving)
 * @code
 * using namespace jsocketpp;
 * * // Create a multicast socket bound to port 5000
 * MulticastSocket socket(5000);
 * * // Join the multicast group "239.255.0.1" on the default interface
 * socket.joinGroup("239.255.0.1");
 * * // Receive a datagram packet (blocking)
 * DatagramPacket packet(1500); // 1500-byte buffer
 * size_t n = socket.read(packet);
 * std::string data(packet.buffer.begin(), packet.buffer.begin() + n);
 * std::cout << "Received: " << data << " from " << packet.address << ":" << packet.port << std::endl;
 *
 * // Leave the group when done
 * socket.leaveGroup("239.255.0.1");
 * @endcode
 *
 * ### Usage Example (Sending)
 * @code
 * using namespace jsocketpp;
 *
 * MulticastSocket socket;
 * socket.setTimeToLive(2); // Limit to 2 router hops
 * DatagramPacket packet("Hello, multicast!", "239.255.0.1", 5000);
 * socket.write(packet);
 * @endcode
 *
 * ### Features
 * - Join/leave multicast groups on a specific interface or all interfaces.
 * - Works with both IPv4 and IPv6 multicast groups.
 * - Set multicast TTL (time-to-live/hop limit).
 * - Set outgoing interface for multicast packets.
 * - Control whether multicast packets sent by this socket are received by itself (loopback).
 * - Modern, Java-style, exception-safe C++ API.
 *
 * @see jsocketpp::DatagramSocket
 * @see jsocketpp::DatagramPacket
 */
class MulticastSocket : public DatagramSocket
{
  public:
    /**
     * @brief Constructs a fully configurable multicast socket for receiving and sending datagrams.
     * @ingroup udp
     *
     * This advanced constructor provides complete control over the configuration of a multicast
     * UDP socket, including binding behavior, buffer sizes, timeouts, and dual-stack support.
     * It is designed for power users who need fine-grained socket tuning prior to joining one
     * or more multicast groups using @ref joinGroup().
     *
     * Unlike simpler constructors, this overload supports use cases such as:
     * - Binding to a specific local interface (e.g., "192.168.1.5" or "::1")
     * - Using system-assigned ephemeral ports (by passing `localPort = 0`)
     * - Tuning performance and memory characteristics with custom buffer sizes
     * - Enabling non-blocking mode for asynchronous usage
     * - Configuring receive/send timeouts for socket operations
     * - Enabling dual-stack mode for IPv4-mapped IPv6 support
     *
     * No multicast group is joined automatically — you must call @ref joinGroup() after construction.
     *
     * @param[in] localPort
     *            The local port to bind to. Use `0` to request an ephemeral port assigned by the OS.
     *
     * @param[in] localAddress
     *            Optional local interface or IP address to bind to. Common values:
     *            - `"0.0.0.0"`: any IPv4 interface
     *            - `"::"`: any IPv6 interface
     *            - A specific interface address (e.g., `"192.168.1.10"`)
     *
     * @param[in] recvBufferSize
     *            Optional size of the OS-level receive buffer, in bytes (e.g., 4096). If `std::nullopt`,
     *            the default size is used.
     *
     * @param[in] sendBufferSize
     *            Optional size of the OS-level send buffer, in bytes. If `std::nullopt`, the default is used.
     *
     * @param[in] internalBufferSize
     *            Optional size of the internal user-space buffer used by `read()` and related methods.
     *
     * @param[in] reuseAddress
     *            Whether to enable `SO_REUSEADDR` so multiple sockets can bind to the same port
     *            (useful for multiple receivers on the same machine).
     *
     * @param[in] soRecvTimeoutMillis
     *            Receive timeout in milliseconds. Use `-1` to disable.
     *
     * @param[in] soSendTimeoutMillis
     *            Send timeout in milliseconds. Use `-1` to disable.
     *
     * @param[in] nonBlocking
     *            If true, the socket is set to non-blocking mode after creation.
     *
     * @param[in] dualStack
     *            Enables IPv4-mapped IPv6 support when using `"::"` bind addresses.
     *            Has no effect for IPv4-only sockets.
     *
     * @param[in] autoBind
     *            If true, the socket will automatically call `bind()` to the given address and port.
     *            If false, you must call `bind()` manually before using the socket.
     *
     * @throws SocketException
     *         If socket creation, binding, or option setting fails for any reason.
     *
     * @note
     * This constructor does not automatically join a multicast group.
     * Use @ref joinGroup() after construction to receive multicast traffic.
     *
     * ### Example
     * @code
     * using namespace jsocketpp;
     *
     * MulticastSocket sock(
     *     4446,               // bind to port 4446
     *     "::",               // bind to all interfaces (IPv6)
     *     4096,               // OS receive buffer size
     *     4096,               // OS send buffer size
     *     8192,               // internal buffer size
     *     true,               // reuse address (SO_REUSEADDR)
     *     2000,               // receive timeout (ms)
     *     2000,               // send timeout (ms)
     *     false,              // blocking mode
     *     true,               // dual stack enabled
     *     true                // auto-bind
     * );
     *
     * sock.joinGroup("ff12::1234"); // Join multicast group explicitly
     * @endcode
     *
     * @see joinGroup()
     * @see leaveGroup()
     * @see setMulticastInterface()
     * @see setTimeToLive()
     * @see setLoopbackMode()
     */
    MulticastSocket(Port localPort, std::string_view localAddress,
                    std::optional<std::size_t> recvBufferSize = std::nullopt,
                    std::optional<std::size_t> sendBufferSize = std::nullopt,
                    std::optional<std::size_t> internalBufferSize = std::nullopt, bool reuseAddress = true,
                    int soRecvTimeoutMillis = -1, int soSendTimeoutMillis = -1, bool nonBlocking = false,
                    bool dualStack = true, bool autoBind = true);

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
    bool getLoopbackMode() const { return _loopbackEnabled; }

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
