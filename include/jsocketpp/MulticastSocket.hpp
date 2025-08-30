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

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

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
     * @brief Join a multicast group on an optional interface (string-friendly).
     * @ingroup udp
     *
     * Adds this socket to the multicast group identified by @p groupAddr, optionally
     * scoping the membership to a specific local interface indicated by @p iface.
     * The method accepts human-friendly identifiers and performs resolution and
     * validation before delegating to the OS via `setsockopt`.
     *
     * Accepted forms
     * - @p groupAddr:
     *   - IPv4 literal (e.g., "239.1.2.3") or hostname that resolves to IPv4.
     *   - IPv6 literal (e.g., "ff15::abcd") or hostname that resolves to IPv6.
     *   - Must resolve to a **multicast** address (IPv4 224.0.0.0/4 or IPv6 ff00::/8).
     * - @p iface (optional selector of the local interface used for membership):
     *   - **IPv4 membership:** IPv4 literal of the interface’s unicast address
     *     (e.g., "192.0.2.10"). Empty selects the default (`INADDR_ANY`).
     *   - **IPv6 membership:** decimal **interface index** as a string (e.g., "12").
     *     On POSIX, an **interface name** (e.g., "eth0") is also accepted and
     *     converted via `if_nametoindex()`. Empty selects index `0` (default).
     *     On Windows, interface **names are not supported**; use a numeric index.
     *
     * Behavior
     * - Resolves @p groupAddr to either `in_addr` or `in6_addr` (IPv4/IPv6), using a
     *   fast literal path first, then the project resolver (`resolveAddress(...)`).
     * - Verifies the resolved address is multicast (`IN_MULTICAST` / `IN6_IS_ADDR_MULTICAST`).
     * - Resolves @p iface as described above and binds the membership to that interface.
     * - Invokes the centralized option helpers:
     *   - IPv4: `joinGroupIPv4(in_addr group, in_addr iface)`
     *   - IPv6: `joinGroupIPv6(in6_addr group, unsigned int ifindex)`
     * - Updates internal caches (e.g., `_currentGroup`, `_currentInterface`) **only
     *   after** the OS call succeeds.
     *
     * Notes
     * - This method **does not** select the **egress** interface for outbound traffic;
     *   use @ref setMulticastInterface(const std::string&) (or the per-family variants)
     *   to choose where your sends go. Membership here controls **what you receive**.
     * - For link-local IPv6 groups (`ff02::/16`), specifying a correct interface index
     *   is often required by the OS; prefer a non-empty @p iface in that case.
     * - Repeat joins of the same (group, interface) may be ignored or rejected by the
     *   OS depending on the stack; behavior is implementation-defined.
     *
     * @param[in] groupAddr  Multicast group (literal or resolvable name), IPv4 or IPv6.
     * @param[in] iface      Optional interface selector as described above; empty uses the default.
     *
     * @throws SocketException
     * - @p groupAddr is empty or does not resolve to a multicast address.
     * - @p iface is malformed (e.g., bad IPv4 literal; unknown POSIX name; non-numeric
     *   index string on Windows).
     * - Name resolution fails (exception carries the `getaddrinfo` code/message).
     * - The underlying membership call fails:
     *   - IPv4: `setsockopt(IP_ADD_MEMBERSHIP)`
     *   - IPv6 (POSIX): `setsockopt(IPV6_JOIN_GROUP)`
     *   - IPv6 (Windows): `setsockopt(IPV6_ADD_MEMBERSHIP)`
     *
     * @par Related
     * - @ref leaveGroup(const std::string&, const std::string&) — leave a group.
     * - @ref setMulticastInterface(const std::string&) — choose egress interface.
     * - @ref setTimeToLive(int) / @ref getTimeToLive() — control cached TTL/hops.
     * - @ref setMulticastTTL(int) / @ref getMulticastTTL() — OS-level multicast TTL/hops.
     *
     * @code
     * // IPv4: join on default interface
     * sock.joinGroup("239.1.2.3", "");
     *
     * // IPv4: join on a specific local interface address
     * sock.joinGroup("239.1.2.3", "192.0.2.10");
     *
     * // IPv6: join using numeric interface index (works on all platforms)
     * sock.joinGroup("ff15::feed", "12");
     *
     * // POSIX only: join using interface name
     * sock.joinGroup("ff15::feed", "eth0");
     * @endcode
     */
    void joinGroup(const std::string& groupAddr, const std::string& iface);

    /**
     * @brief Leave a multicast group on an optional interface (string-friendly).
     * @ingroup udp
     *
     * Removes this socket’s membership from the multicast group identified by
     * @p groupAddr. The interface used for the leave operation can be selected via
     * @p iface and follows the same conventions as @ref joinGroup.
     *
     * Accepted forms
     * - @p groupAddr:
     *   - IPv4 literal (e.g., "239.1.2.3") or hostname that resolves to IPv4.
     *   - IPv6 literal (e.g., "ff15::abcd") or hostname that resolves to IPv6.
     *   - Must resolve to a multicast address (IPv4 224.0.0.0/4, IPv6 ff00::/8).
     * - @p iface (optional; selects the local interface whose membership is removed):
     *   - IPv4 leave: IPv4 literal of the interface’s unicast address (e.g., "192.0.2.10").
     *     Empty string selects the default interface (INADDR_ANY).
     *   - IPv6 leave: decimal interface index as a string (e.g., "12"). On POSIX, an
     *     interface name (e.g., "eth0") is also accepted and mapped via if_nametoindex().
     *     Empty string selects index 0 (system default). On Windows, names are not
     *     supported; use a numeric index.
     *
     * Behavior
     * - Resolves @p groupAddr to either an in_addr or in6_addr via a fast literal path
     *   first, and otherwise through the project resolver (resolveAddress).
     * - Validates that the resolved address is multicast (IN_MULTICAST / IN6_IS_ADDR_MULTICAST).
     * - Interprets @p iface as described above to obtain the IPv4 interface address or
     *   IPv6 interface index used when the membership was created.
     * - Delegates to centralized option helpers:
     *   - IPv4: setsockopt(IP_DROP_MEMBERSHIP) via leaveGroupIPv4(in_addr group, in_addr iface)
     *   - IPv6 (POSIX): setsockopt(IPV6_LEAVE_GROUP) via leaveGroupIPv6(in6_addr group, unsigned ifindex)
     *   - IPv6 (Windows): setsockopt(IPV6_DROP_MEMBERSHIP) via leaveGroupIPv6(...)
     *
     * Notes
     * - The interface provided to leave should match the one used to join the group on
     *   this socket. If it does not, some stacks return an error (for example EADDRNOTAVAIL)
     *   or ignore the request.
     * - This method affects only **membership** (what the socket can receive). It does not
     *   change the **egress** interface for sending; use setMulticastInterface(...) for that.
     * - Repeated leaves for the same (group, interface) may be ignored or may fail depending
     *   on the OS; behavior is implementation-defined.
     *
     * @param[in] groupAddr  Multicast group to leave (literal or resolvable name), IPv4 or IPv6.
     * @param[in] iface      Optional interface selector as described above; default is the empty
     *                       string, which uses the system default (IPv4 INADDR_ANY, IPv6 index 0).
     *
     * @pre
     * - The socket was previously joined to the specified group on the selected interface.
     *
     * @post
     * - The socket is no longer a member of the specified group on that interface. Internal
     *   caches tracking the “current” group/interface may be cleared if they match.
     *
     * @throws SocketException
     * - @p groupAddr is empty or does not resolve to a multicast address.
     * - @p iface is malformed (invalid IPv4 literal, unknown POSIX interface name, or
     *   non-numeric index on Windows where names are unsupported).
     * - Name resolution fails; the exception carries the getaddrinfo code/message.
     * - The underlying membership removal fails (e.g., ENOTSOCK, EINVAL, EADDRNOTAVAIL,
     *   ENOPROTOOPT, or Windows equivalents) when invoking:
     *   - IPv4: IP_DROP_MEMBERSHIP
     *   - IPv6 (POSIX): IPV6_LEAVE_GROUP
     *   - IPv6 (Windows): IPV6_DROP_MEMBERSHIP
     *
     * @par Related
     * - @ref joinGroup(const std::string&, const std::string&) — add membership.
     * - @ref setMulticastInterface(const std::string&) — choose egress interface.
     * - @ref setLoopbackMode(bool) / @ref getLoopbackMode() — control/query local delivery of
     *   this socket’s own multicast.
     *
     * @code
     * // IPv4: leave on default interface
     * sock.leaveGroup("239.1.2.3");
     *
     * // IPv4: leave on a specific local interface address
     * sock.leaveGroup("239.1.2.3", "192.0.2.10");
     *
     * // IPv6: leave using numeric interface index (any platform)
     * sock.leaveGroup("ff15::feed", "12");
     *
     * // POSIX-only: leave using interface name
     * sock.leaveGroup("ff15::feed", "eth0");
     * @endcode
     */
    void leaveGroup(const std::string& groupAddr, const std::string& iface = "");

    /**
     * @brief Select the default outgoing interface for multicast transmissions.
     * @ingroup udp
     *
     * Sets the per-socket **egress interface** that will be used for subsequent
     * **multicast** sends. This is a convenience wrapper that accepts human-friendly
     * identifiers and delegates to the per-family setters.
     *
     * Accepted forms for @p iface
     * - Empty string: reset the egress to the **system default** for this socket’s
     *   family only (IPv4: `INADDR_ANY`; IPv6: index `0`). The family is determined
     *   via `detectFamily(_sockFd)`, so a single-family socket will not attempt to
     *   set options for the other family.
     * - IPv4 literal (e.g., "192.0.2.10"): choose that address as the **IPv4**
     *   multicast egress; maps to `IP_MULTICAST_IF`.
     * - IPv6 interface identifier:
     *   - Decimal **interface index** string (e.g., "12") on any platform.
     *   - **POSIX only:** an interface **name** (e.g., "eth0"), resolved with
     *     `if_nametoindex()`.
     *   These map to `IPV6_MULTICAST_IF`. On Windows, names are **not** supported;
     *   supply a numeric index string instead.
     *
     * Behavior
     * - If @p iface is empty, the method resets the egress **only** for the socket’s
     *   actual family (IPv4 or IPv6) and clears the cached `_currentInterface`.
     * - If @p iface parses as an IPv4 literal, the IPv4 egress is set and the cache
     *   is updated to the provided string.
     * - Otherwise, @p iface is interpreted as an IPv6 identifier and converted to an
     *   index using `toIfIndexFromString(iface)`; the IPv6 egress is set and the cache
     *   is updated to the provided string.
     * - The cached `_currentInterface` is updated **only after** the OS call succeeds.
     *
     * Notes
     * - This method does **not** join or leave multicast groups; it affects only
     *   where *outbound* multicast is sent. Use the join/leave APIs to control what
     *   the socket receives.
     * - For link-local IPv6 destinations, using a correct interface index is often
     *   mandatory; prefer a non-empty IPv6 identifier in those cases.
     *
     * @param[in] iface
     *   Interface selector as described above. Pass `""` to reset to the per-family
     *   system default.
     *
     * @throws SocketException
     * - The socket family cannot be determined or is unsupported.
     * - @p iface is malformed or unsupported on the current platform (e.g., an
     *   interface name on Windows for IPv6).
     * - The underlying `setsockopt` call fails (`IP_MULTICAST_IF` for IPv4,
     *   `IPV6_MULTICAST_IF` for IPv6). The exception includes an OS error code and
     *   a descriptive message.
     *
     * @par Related
     * - setMulticastInterfaceIPv4(in_addr), setMulticastInterfaceIPv6(unsigned int)
     * - joinGroup(const std::string&, const std::string&)
     * - setTimeToLive(int) / getTimeToLive()
     * - setMulticastTTL(int) / getMulticastTTL()
     * - setLoopbackMode(bool) / getLoopbackMode()
     *
     * @code
     * MulticastSocket sock;
     *
     * // Reset to system default for this socket's family
     * sock.setMulticastInterface("");
     *
     * // Choose a specific IPv4 egress address
     * sock.setMulticastInterface("192.0.2.10");
     *
     * // Choose IPv6 egress by numeric index (any platform)
     * sock.setMulticastInterface("12");
     *
     * // POSIX: choose IPv6 egress by interface name
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

  protected:
    /**
     * @brief Resolve a host string to an IPv4 address (`in_addr`, network byte order).
     * @ingroup udp
     *
     * Converts @p host into an IPv4 address suitable for socket APIs.
     * Resolution strategy:
     *  1. Fast path: if @p host is an IPv4 literal (e.g., "239.1.2.3"), parse it with
     *     `inet_pton(AF_INET, ...)` without any name-service lookup.
     *  2. Fallback: otherwise, call the project helper
     *     `resolveAddress(host, Port{0}, AF_INET, SOCK_DGRAM, IPPROTO_UDP, 0)` and
     *     extract the first `AF_INET` result.
     *
     * The returned `in_addr` is in network byte order, ready for `setsockopt`, `sendto`,
     * and multicast membership structures.
     *
     * @param[in] host
     *     Host string to resolve. Accepts IPv4 literals ("A.B.C.D") or names/hostnames
     *     resolvable by the system’s name services. Must not be empty.
     *
     * @return
     *     Resolved IPv4 address in network byte order.
     *
     * @pre
     *  - @p host is not empty.
     *
     * @post
     *  - No persistent side effects; this function does not modify socket state.
     *
     * @throws SocketException
     *  - If @p host is empty.
     *  - If resolution succeeds but yields no `AF_INET` address.
     *  - If name resolution fails; the exception carries the OS or getaddrinfo error
     *    code and a descriptive message from `SocketErrorMessage(..., true)`.
     *
     * @note
     *  - CIDR notation and interface suffixes are not supported; pass a single host
     *    literal or resolvable name.
     *  - If a name resolves to multiple IPv4 addresses, the first result from
     *    `resolveAddress` is used.
     *  - Use this helper before joining IPv4 multicast groups to ensure a correct
     *    address family and byte order.
     *
     * @par Related
     *  - @ref resolveIPv6(std::string_view) — IPv6 counterpart.
     *  - @ref is_ipv4_multicast(in_addr) — multicast-range predicate.
     *  - @ref joinGroupIPv4(in_addr, in_addr) — join IPv4 multicast groups.
     *
     * @code
     * // Parse a literal IPv4 multicast group
     * in_addr g = resolveIPv4("239.1.2.3");
     * if (!is_ipv4_multicast(g)) {
     *     throw SocketException("Expected an IPv4 multicast address");
     * }
     *
     * // Resolve a hostname and join on the default interface
     * in_addr g2 = resolveIPv4("mcast.example.com");
     * in_addr any{};
     * any.s_addr = htonl(INADDR_ANY);
     * joinGroupIPv4(g2, any);
     *
     * // Resolve an interface IPv4 address (egress) by literal
     * in_addr eg = resolveIPv4("192.0.2.10");
     * joinGroupIPv4(g, eg);
     * @endcode
     */
    static in_addr resolveIPv4(std::string_view host);

    /**
     * @brief Resolve a host string to an IPv6 address (`in6_addr`).
     * @ingroup udp
     *
     * Converts @p host into an IPv6 address suitable for socket APIs.
     * Resolution strategy:
     *  1. Fast path: if @p host is an IPv6 literal (e.g., "ff02::1"), parse it with
     *     `inet_pton(AF_INET6, ...)` and avoid any name-service lookup.
     *  2. Fallback: otherwise, call the project helper
     *     `resolveAddress(host, Port{0}, AF_INET6, SOCK_DGRAM, IPPROTO_UDP, 0)` and
     *     extract the first `AF_INET6` result.
     *
     * The returned `in6_addr` is in the canonical struct form expected by socket calls,
     * multicast membership records, and IPv6 `setsockopt` options.
     *
     * @param[in] host
     *     Host string to resolve. Accepts IPv6 literals (e.g., "ff02::1", "2001:db8::1")
     *     or names/hostnames resolvable by the system’s name services. Must not be empty.
     *
     * @return
     *     Resolved IPv6 address.
     *
     * @pre
     *  - @p host is not empty.
     *
     * @post
     *  - No persistent side effects; this function does not modify socket state.
     *
     * @throws SocketException
     *  - If @p host is empty.
     *  - If resolution succeeds but yields no `AF_INET6` address.
     *  - If name resolution fails; the exception carries the OS or getaddrinfo error
     *    code and a descriptive message from `SocketErrorMessage(..., true)`.
     *
     * @note
     *  - Zone identifiers such as "ff02::1%eth0" are not interpreted here. Supply the
     *    interface index separately where required (e.g., for multicast membership
     *    or egress selection).
     *  - If a name resolves to multiple IPv6 addresses, the first result from
     *    `resolveAddress` is used.
     *
     * @par Related
     *  - @ref resolveIPv4(std::string_view) — IPv4 counterpart.
     *  - @ref is_ipv6_multicast(const in6_addr&) — multicast-range predicate.
     *  - @ref joinGroupIPv6(in6_addr, unsigned int) — join IPv6 multicast groups.
     *
     * @code
     * // Parse a literal IPv6 multicast group and join on interface index 0 (default)
     * in6_addr g6 = resolveIPv6("ff02::1:3");
     * if (!is_ipv6_multicast(g6)) {
     *     throw SocketException("Expected an IPv6 multicast address");
     * }
     * joinGroupIPv6(g6, 0); // 0 = default interface
     *
     * // Resolve a hostname and join on a specific interface index
     * unsigned int ifidx = 12; // e.g., result of if_nametoindex("eth0") on POSIX
     * in6_addr g6host = resolveIPv6("mcast6.example.com");
     * joinGroupIPv6(g6host, ifidx);
     * @endcode
     */
    static in6_addr resolveIPv6(std::string_view host);

    /**
     * @brief Convert a human-friendly interface identifier to an IPv6 interface index.
     *
     * Interprets @p iface and returns a numeric **interface index** suitable for APIs
     * that require an IPv6 egress/interface selector (e.g., `IPV6_MULTICAST_IF`,
     * IPv6 multicast join/leave).
     *
     * Accepted forms:
     * - **Empty string**: returns `0`, meaning “use the system default interface”.
     * - **Decimal digits** (e.g., `"12"`): parsed with `std::from_chars` and returned
     *   as the index (no whitespace, signs, or hex prefixes allowed).
     * - **POSIX only**: an **interface name** (e.g., `"eth0"`, `"en0"`). Resolved via
     *   `if_nametoindex()`. On Windows, interface **names are not supported** here.
     *
     * This helper performs no socket I/O; it only converts/looks up the identifier.
     *
     * @param[in] iface
     *   Interface selector as described above. Pass `""` to request the default (index 0).
     *
     * @return
     *   The IPv6 interface index corresponding to @p iface. Returns `0` for the empty string.
     *
     * @pre
     * - None. The function is total for any input string, but non-conforming inputs will
     *   raise an exception (see below).
     *
     * @post
     * - No side effects; the function does not modify socket state or global configuration.
     *
     * @throws SocketException
     * - If @p iface contains non-decimal characters when a numeric index is expected,
     *   or the parsed value overflows the target type.
     * - On POSIX, if @p iface is a name that `if_nametoindex()` cannot resolve on this host.
     * - On Windows, if @p iface is a non-empty, non-numeric string (names unsupported).
     *
     * @note
     * - Numeric parsing uses `std::from_chars` and requires the **entire** string to be
     *   consumed (no trailing characters). Leading `+`/`-`, whitespace, and hex notation
     *   are not accepted.
     * - The returned index is not validated for “up/running” status—only that the name
     *   exists (POSIX) or the string parses as a number.
     * - Use index `0` to reset to default interface behavior in calls like
     *   `setsockopt(IPV6_MULTICAST_IF, ...)` or when joining groups.
     *
     * @par Related
     * - `setMulticastInterfaceIPv6(unsigned int)` — set IPv6 multicast egress by index.
     * - `setMulticastInterface(const std::string&)` — convenience wrapper accepting
     *   name/index/IPv4 address.
     * - `joinGroupIPv6(in6_addr, unsigned int)` / `leaveGroupIPv6(in6_addr, unsigned int)`.
     *
     * @code
     * // Example 1: default interface
     * unsigned int idx = toIfIndexFromString("");
     * // idx == 0
     *
     * // Example 2: numeric index (any platform)
     * unsigned int idx2 = toIfIndexFromString("12");  // returns 12
     *
     * // Example 3 (POSIX): interface name
     * unsigned int idx3 = toIfIndexFromString("eth0"); // resolves via if_nametoindex()
     *
     * // Example 4 (Windows): names unsupported
     * // toIfIndexFromString("Ethernet") -> throws SocketException
     *
     * // Use with IPv6 multicast egress
     * sock.setMulticastInterfaceIPv6(toIfIndexFromString("12"));
     *
     * // Join an IPv6 multicast group on a specific interface
     * in6_addr grp{};
     * inet_pton(AF_INET6, "ff02::1:3", &grp);
     * joinGroupIPv6(grp, toIfIndexFromString("12"));
     * @endcode
     */
    static unsigned int toIfIndexFromString(const std::string& iface);

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
