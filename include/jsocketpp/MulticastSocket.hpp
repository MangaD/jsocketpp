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
     * @brief Select the default outgoing interface for multicast transmissions.
     * @ingroup udp
     *
     * Sets the per-socket **egress interface** used when sending **multicast** packets.
     * This is a convenience wrapper that accepts a human-friendly string and maps it
     * to the appropriate per-family socket option:
     *
     * - **IPv4 egress** — set by specifying an **IPv4 literal address** of the desired
     *   interface (e.g., `"192.0.2.10"`). Internally maps to `IP_MULTICAST_IF` with
     *   a `struct in_addr`.
     * - **IPv6 egress** — set by specifying an **interface index** as a decimal string
     *   (e.g., `"12"`). On POSIX you may also pass an **interface name** (e.g., `"eth0"`),
     *   which is resolved via `if_nametoindex()`. Internally maps to `IPV6_MULTICAST_IF`
     *   with an unsigned interface index.
     * - **Reset to defaults** — pass an empty string (`""`) to restore system defaults
     *   (IPv4: `INADDR_ANY`; IPv6: index `0`).
     *
     * This function does **not** join any multicast groups; it only selects the default
     * *outgoing* interface for subsequent multicast sends on this socket. Group membership
     * is managed via the corresponding join/leave APIs.
     *
     * @param[in] iface
     *   Interface selector:
     *   - `""` (empty) --- reset IPv4/IPv6 multicast egress to the system defaults;
     *   - IPv4 literal (e.g., `"192.0.2.10"`) --- choose that address as the IPv4 egress;
     *   - Decimal number (e.g., `"12"`) --- choose that IPv6 **interface index** as egress;
     *   - POSIX only: interface name (e.g., `"eth0"`) --- resolved to an index with
     *     `if_nametoindex()` and used for IPv6 egress.
     *
     * @pre
     * - The underlying descriptor is a valid datagram (UDP) socket.
     * - For IPv4 selection, @p iface must be a valid IPv4 literal or empty.
     * - For IPv6 selection, @p iface must be a decimal index string or (on POSIX) an
     *   interface name recognized by `if_nametoindex()`; empty resets to default.
     *
     * @post
     * - The socket’s default egress interface is updated for the relevant family
     *   (IPv4 when an IPv4 literal is provided; IPv6 when an index/name is provided).
     * - Subsequent multicast transmissions from this socket use the selected egress
     *   for that family until changed again.
     *
     * @par Platform mapping
     * - **IPv4:** `setsockopt(fd, IPPROTO_IP,  IP_MULTICAST_IF,  &in_addr, sizeof(in_addr))`
     *   - On Windows the value is passed as a `DWORD` containing the IPv4 address
     *     in network byte order.
     * - **IPv6:** `setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex))`
     *   - On Windows, `ifindex` is a `DWORD`; on POSIX it is `unsigned int`.
     *
     * @par Error conditions
     * - The socket is invalid or closed.
     * - @p iface is malformed or does not denote a valid interface on this host
     *   (e.g., bad IPv4 literal, unknown interface name, non-numeric index string).
     * - The operation is unsupported on the current platform (e.g., interface names
     *   for IPv6 on Windows).
     * - The underlying `setsockopt` call fails (e.g., `ENOTSOCK`, `EINVAL`, `ENOPROTOOPT`,
     *   or Windows equivalents `WSAENOTSOCK`, `WSAEINVAL`, `WSAENOPROTOOPT`).
     *
     * @throws SocketException
     *   If validation fails or the OS rejects the option. The exception includes the
     *   OS error code and a descriptive message from `SocketErrorMessage(...)`.
     *
     * @note
     * - This setting is **per-socket** and affects only **outbound multicast** routing.
     *   It does not influence which local interfaces receive multicast --- receivers
     *   are determined by group membership on each socket.
     * - IPv6 requires an **interface index**; IPv6 literal addresses are not accepted
     *   for selecting egress. Use a decimal index (any platform) or a name (POSIX only).
     * - For deterministic behavior, set the egress during initialization, before any
     *   concurrent sends.
     *
     * @par Related options
     * - @ref setMulticastTTL(int) / @ref getMulticastTTL() --- control/query multicast scope.
     * - @ref setMulticastLoopback(bool) / @ref getMulticastLoopback() --- control/query local delivery of this socket’s
     * own multicast.
     * - @ref joinGroup(const std::string&, const std::string&) / @ref leaveGroup(const std::string&, const
     * std::string&) --- manage multicast group membership.
     *
     * @code
     * // Reset to system defaults for both families
     * sock.setMulticastInterface("");
     *
     * // Choose a specific IPv4 egress address
     * sock.setMulticastInterface("192.0.2.10");
     *
     * // Choose IPv6 egress by index (any platform)
     * sock.setMulticastInterface("12");
     *
     * // POSIX only: choose IPv6 egress by interface name
     * sock.setMulticastInterface("eth0");
     * @endcode
     */
    void setMulticastInterface(const std::string& iface);

    /**
     * @brief Get the currently selected egress interface for multicast transmissions.
     * @ingroup udp
     *
     * Returns the **human-friendly identifier** that this object currently holds as the
     * default **outgoing multicast interface**, as last set via
     * @ref setMulticastInterface(const std::string&). This is a cached value intended
     * for diagnostics and UI; it does not perform a fresh system query.
     *
     * Possible return forms (depending on how the interface was selected):
     * - **IPv4 egress:** the interface’s IPv4 **literal address** (e.g., `"192.168.1.5"`).
     * - **IPv6 egress:** a **numeric interface index** as a decimal string (e.g., `"12"`),
     *   or on POSIX the **interface name** originally provided (e.g., `"eth0"`).
     * - **Empty string (`""`):** no explicit egress configured — the system defaults
     *   are in effect (IPv4: `INADDR_ANY`; IPv6: index `0`).
     *
     * @return
     *   The current multicast egress selector (IPv4 address, IPv6 name/index as a string),
     *   or an empty string if the system default is in use.
     *
     * @pre
     * - None. This is a read-only accessor.
     *
     * @post
     * - Socket state is unchanged.
     *
     * @note
     * - This method returns the value **cached by this object** when
     *   @ref setMulticastInterface(const std::string&) last succeeded. It does **not**
     *   re-read kernel state. If code outside this class (e.g., direct calls to
     *   @ref setMulticastInterfaceIPv4(in_addr) or
     *   @ref setMulticastInterfaceIPv6(unsigned int)) changes the egress after that,
     *   this cached string may no longer reflect the OS setting.
     * - The return format mirrors the input you supplied: IPv4 address for IPv4 egress,
     *   decimal index (or POSIX name) for IPv6 egress, or `""` for default routing.
     * - This accessor is about **multicast egress selection only**; it does not imply
     *   any group membership. Use join/leave APIs to manage membership.
     *
     * @par Related options
     * - @ref setMulticastInterface(const std::string&) — set the egress interface via a
     *   user-friendly identifier.
     * - @ref setMulticastInterfaceIPv4(in_addr) /
     *   @ref setMulticastInterfaceIPv6(unsigned int) — low-level per-family setters.
     * - @ref setMulticastTTL(int) / @ref getMulticastTTL() — control/query multicast scope.
     * - @ref setMulticastLoopback(bool) / @ref getMulticastLoopback() — control/query local
     *   delivery of this socket’s own multicast.
     *
     * @code
     * MulticastSocket sock;
     *
     * // Default (no explicit egress)
     * assert(sock.getMulticastInterface().empty());
     *
     * // Choose IPv4 egress by address
     * sock.setMulticastInterface("192.0.2.10");
     * assert(sock.getMulticastInterface() == "192.0.2.10");
     *
     * // POSIX: choose IPv6 egress by name (or use a numeric index string on any platform)
     * sock.setMulticastInterface("eth0");          // POSIX name
     * // or: sock.setMulticastInterface("12");     // numeric index string
     * auto current = sock.getMulticastInterface(); // "eth0" or "12"
     * @endcode
     */
    [[nodiscard]] std::string getMulticastInterface() const { return _currentInterface; }

    /**
     * @brief Set the time-to-live (TTL) / hop limit for outgoing multicast packets.
     * @ingroup udp
     *
     * Sets this socket’s **default multicast scope** for subsequent transmissions:
     * - **IPv4:** applies to `IP_MULTICAST_TTL` (time-to-live).
     * - **IPv6:** applies to `IPV6_MULTICAST_HOPS` (hop limit).
     *
     * The value constrains how far outbound **multicast** packets may propagate:
     * - `0` — restricted to the local host (no egress onto the network);
     * - `1` — restricted to the local link/subnet (commonly the default);
     * - higher values — permit traversal across additional multicast routers up to the limit
     *   (e.g., `32` ~ site, `64` ~ region, `128` ~ continent, `255` ~ unrestricted).
     *
     * This setting is **per-socket** and affects **multicast only**. Unicast scope is
     * controlled independently via `IP_TTL` / `IPV6_UNICAST_HOPS`. Per-message control
     * data (e.g., `IP_MULTICAST_TTL` or `IPV6_HOPLIMIT` with `sendmsg`) can override this
     * default for individual packets.
     *
     * @param[in] ttl
     *   Hop limit (IPv6) or TTL (IPv4), in the range `[0, 255]`.
     *
     * @pre
     * - The underlying descriptor is a valid socket for `AF_INET` or `AF_INET6` (typically UDP).
     *
     * @post
     * - The socket’s default multicast TTL/hop limit is updated. Future multicast sends from
     *   this socket use the new value unless overridden per message.
     *
     * @par Platform mapping
     * - **IPv4:** `setsockopt(fd, IPPROTO_IP,  IP_MULTICAST_TTL,  &v, sizeof(v))`
     * - **IPv6:** `setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &v, sizeof(v))`
     *
     * @par Error conditions
     * - `ttl` is outside `[0, 255]`.
     * - The socket is invalid or closed.
     * - The socket family cannot be determined or is unsupported.
     * - The underlying `setsockopt` call fails (e.g., `ENOTSOCK`, `EINVAL`, `ENOPROTOOPT`;
     *   Windows equivalents: `WSAENOTSOCK`, `WSAEINVAL`, `WSAENOPROTOOPT`).
     *
     * @throws SocketException
     *   If validation fails or the operating system rejects the option. The exception
     *   includes the OS error code and a descriptive message from `SocketErrorMessage(...)`.
     *
     * @note
     * - Typical initial values are implementation-defined (often `1`); set an explicit
     *   value during initialization for deterministic behavior.
     * - If other threads call this function concurrently with sends, packets already
     *   queued by the kernel may still carry the previous TTL/hop limit.
     *
     * @par Related options
     * - @ref getMulticastTTL() — query the current default TTL/hop limit.
     * - @ref setMulticastLoopback(bool) / @ref getMulticastLoopback() — control/query local
     *   receipt of this socket’s own multicast.
     * - @ref setMulticastInterface(const std::string&) — select the outgoing interface.
     *
     * @code
     * // Limit announcements to the local link
     * MulticastSocket sock;
     * sock.setMulticastInterface("eth0"); // choose an egress (POSIX name or index string)
     * sock.setTimeToLive(1);              // link-local only
     *
     * // Widen scope later (e.g., site level)
     * sock.setTimeToLive(32);
     *
     * // Confirm current default
     * int ttl = sock.getMulticastTTL();
     * @endcode
     */
    void setTimeToLive(int ttl);

    /**
     * @brief Get the current default multicast TTL / hop limit cached on this socket.
     * @ingroup socketopts
     *
     * Returns this object’s **cached** per-socket default scope for outbound
     * **multicast** transmissions as last set via @ref setTimeToLive(int).
     * It does **not** perform a fresh kernel query and does **not** affect any state.
     *
     * Semantics of the returned value (applies to multicast only):
     * - `0`  — restricted to the local host (no network egress);
     * - `1`  — restricted to the local link / subnet (commonly the default);
     * - `32` — typically site scope;
     * - `64` — typically region scope;
     * - `128` — typically continent scope;
     * - `255` — unrestricted (global), subject to network policy.
     *
     * @return
     *   The cached multicast TTL / hop-limit in the range `[0, 255]`.
     *
     * @pre
     * - None. This is a read-only accessor.
     *
     * @post
     * - No side effects; socket and object state are unchanged.
     *
     * @note
     * - This accessor reports the value cached by this class (e.g., updated by
     *   @ref setTimeToLive(int)). If other code changes the OS-level default directly
     *   (e.g., via @ref setMulticastTTL(int) on @ref SocketOptions), this cached value
     *   may not reflect the kernel’s current setting.
     * - The value applies to **multicast** only. Unicast scope is controlled by
     *   `IP_TTL` / `IPV6_UNICAST_HOPS`.
     *
     * @par Related options
     * - @ref setTimeToLive(int) — update this cached default and the OS setting.
     * - @ref getMulticastTTL() — query the OS-level default (fresh `getsockopt`).
     * - @ref setMulticastLoopback(bool) / @ref getMulticastLoopback() — control/query
     *   local delivery of this socket’s own multicast.
     *
     * @code
     * // Ensure at least link-local reach for multicast
     * if (sock.getTimeToLive() < 1) {
     *     sock.setTimeToLive(1);
     * }
     * @endcode
     */
    int getTimeToLive() const { return _ttl; }

    /**
     * @brief Enable or disable multicast loopback for this socket.
     * @ingroup udp
     *
     * Controls whether **multicast datagrams sent by this socket** may be delivered
     * back to the local host (i.e., whether the sender can observe its own multicast
     * transmissions). This affects **local delivery only** --- it does not change what
     * remote receivers get.
     *
     * - **Enabled (`true`)**: the kernel may deliver local copies of outbound multicast
     *   to sockets on the same host that have joined the destination group, including
     *   this socket if it has joined.
     * - **Disabled (`false`)**: local delivery of this socket’s own multicast traffic
     *   is suppressed; other hosts on the network are unaffected.
     *
     * This setting is **per-socket** and applies to **multicast** only. It does not
     * affect unicast traffic.
     *
     * @param[in] enable
     *   Set `true` to allow local delivery of this socket’s multicast transmissions,
     *   or `false` to suppress local delivery.
     *
     * @pre
     * - The underlying descriptor is a valid UDP-capable socket (`AF_INET` or `AF_INET6`).
     *
     * @post
     * - The socket’s multicast loopback policy is updated. Future multicast sends from
     *   this socket follow the new policy; in-flight packets are unaffected.
     *
     * @par Platform mapping
     * - **IPv4:** `setsockopt(fd, IPPROTO_IP,  IP_MULTICAST_LOOP,  &flag, sizeof(flag))`
     * - **IPv6:** `setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &flag, sizeof(flag))`
     *
     * @par Error conditions
     * - The socket is invalid or closed.
     * - The socket family cannot be determined or is unsupported on this platform.
     * - The underlying `setsockopt` call fails, e.g., `ENOTSOCK`, `EINVAL`, `ENOPROTOOPT`
     *   (Windows: `WSAENOTSOCK`, `WSAEINVAL`, `WSAENOPROTOOPT`).
     *
     * @throws SocketException
     *   If the option cannot be applied. The exception carries the OS error code and
     *   a descriptive message produced by `SocketErrorMessage(...)`.
     *
     * @note
     * - To actually receive your own multicast transmissions on this socket, two
     *   conditions must hold: (1) loopback is enabled here, and (2) the socket has
     *   **joined** the destination group.
     * - Typical default is enabled on many stacks, but this is implementation-defined.
     *   Use @ref getTimeToLive() / @ref getMulticastTTL() and @ref getMulticastLoopback()
     *   to confirm defaults in your environment.
     *
     * @par Related options
     * - @ref getMulticastLoopback() --- query the current loopback flag.
     * - @ref setTimeToLive(int) / @ref getTimeToLive() --- control/query multicast TTL.
     * - @ref setMulticastInterface(const std::string&) --- select the outgoing interface.
     *
     * @code
     * // Suppress receiving our own announcements on this host
     * MulticastSocket sock;
     * sock.setMulticastInterface("eth0");
     * sock.joinGroup("239.1.2.3");
     * sock.setLoopbackMode(false);  // do not receive our own packets locally
     * @endcode
     */
    void setLoopbackMode(bool enable);

    /**
     * @brief Get the current multicast loopback mode cached on this socket.
     * @ingroup socketopts
     *
     * Reports whether this object is configured to receive **local copies of its own
     * multicast transmissions** (“loopback”). The value reflects the last successful call
     * to @ref setLoopbackMode(bool) made through this class; it does **not** perform a
     * fresh kernel query.
     *
     * Semantics of the returned flag:
     * - `true`  — loopback enabled; if this socket has joined the destination group, the
     *             kernel may deliver local copies of this socket’s outbound multicast.
     * - `false` — loopback disabled; local delivery of this socket’s outbound multicast
     *             is suppressed on the host.
     *
     * This accessor concerns **multicast** only; it does not affect nor report unicast behavior.
     *
     * @return
     *   `true` if multicast loopback is enabled according to this object’s cached state;
     *   otherwise `false`.
     *
     * @pre
     * - None. This is a read-only accessor.
     *
     * @post
     * - No side effects; neither socket nor object state is modified.
     *
     * @note
     * - The value is **cached** in this class (e.g., `_loopbackEnabled`). If other code
     *   changes the OS-level setting directly (e.g., via @ref setMulticastLoopback(bool)
     *   on the underlying options object), this cached flag may not reflect the kernel’s
     *   current configuration. Use @ref getMulticastLoopback() to perform a fresh OS query.
     * - To actually receive your own transmissions on this socket, two conditions must hold:
     *   (1) loopback is enabled, and (2) the socket has **joined** the destination group.
     *
     * @par Related options
     * - @ref setLoopbackMode(bool) — update this cached flag and the OS setting.
     * - @ref getMulticastLoopback() — query the OS-level loopback flag (fresh `getsockopt`).
     * - @ref setTimeToLive(int) / @ref getTimeToLive() — control/query multicast scope.
     * - @ref setMulticastInterface(const std::string&) — select the outgoing interface.
     *
     * @code
     * // Ensure we will receive our own announcements on this host:
     * if (!sock.getLoopbackMode()) {
     *     sock.setLoopbackMode(true);
     * }
     * @endcode
     */
    [[nodiscard]] bool getLoopbackMode() const { return _loopbackEnabled; }

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
