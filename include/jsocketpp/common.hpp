/**
 * @file common.hpp
 * @brief Common platform and utility includes for jsocketpp.
 */

#pragma once

#include "SocketException.hpp"

#include <cstddef> // std::size_t
#include <source_location>
#include <span>
#include <string>
#include <utility> // std::pair

#ifdef __GNUC__
#define QUOTE(s) #s
#define DIAGNOSTIC_PUSH() _Pragma("GCC diagnostic push")
#define DIAGNOSTIC_IGNORE(warning) _Pragma(QUOTE(GCC diagnostic ignored warning))
#define DIAGNOSTIC_POP() _Pragma("GCC diagnostic pop")
#else
#define DIAGNOSTIC_PUSH()
#define DIAGNOSTIC_IGNORE(warning)
#define DIAGNOSTIC_POP()
#endif

#include <cstring> // Use std::memset()
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32

// Do not reorder includes here, because Windows headers have specific order requirements.
// clang-format off
#include <winsock2.h> // Must come first: socket, bind, listen, accept, etc.
#include <ws2tcpip.h> // TCP/IP functions: getaddrinfo, getnameinfo, inet_ntop, inet_pton
#include <windows.h>  // General Windows headers (e.g., FormatMessageA)
#include <iphlpapi.h> // GetAdaptersInfo, network adapter info
// clang-format on

// https://msdn.microsoft.com/en-us/library/6sehtctf.aspx
#if !defined(WINVER) || (WINVER < _WIN32_WINNT_WINXP)
#error WINVER must be defined to something equal or above to 0x0501 // Win XP
#endif // WINVER
#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < _WIN32_WINNT_WINXP)
#error _WIN32_WINNT must be defined to something equal or above to 0x0501 // Win XP
#endif // _WIN32_WINNT

/* If compiled with Visual C++
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms737629%28v=vs.85%29.aspx
 * http://stackoverflow.com/questions/3484434/what-does-pragma-comment-mean
 * http://stackoverflow.com/questions/70013/how-to-detect-if-im-compiling-code-with-visual-studio-2008
 */
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")   // Winsock library
#pragma comment(lib, "iphlpapi.lib") // GetAdaptersInfo
#endif

#else

// Assuming Linux
#include <arpa/inet.h>   //inet_ntoa
#include <cerrno>        //errno
#include <cstring>       //strerror
#include <fcntl.h>       //fcntl
#include <ifaddrs.h>     //getifaddrs
#include <net/if.h>      //if_nametoindex
#include <netdb.h>       //addrinfo
#include <netinet/in.h>  //sockaddr_in, sockaddr_in6
#include <netinet/tcp.h> //TCP_NODELAY, TCP_MAXSEG
#include <poll.h>        //poll
#include <sys/ioctl.h>   //ioctl
#include <sys/select.h>  //select
#include <sys/socket.h>  //socket
#include <sys/time.h>    //timeval
#include <sys/types.h>   //socket
#include <sys/uio.h>     //writev,readv,iovec
#include <sys/un.h>      //for Unix domain sockets
#include <unistd.h>      //close

#endif

/**
 * @defgroup jsocketpp jsocketpp: C++20 cross-platform socket library
 * @brief All core classes and functions of the jsocketpp networking library.
 *
 * jsocketpp is a modern, cross-platform C++20 networking library with a Java-like API.
 * It supports TCP, UDP, and UNIX sockets, as well as advanced features like dual-stack IPv4/IPv6,
 * easy resource management, and comprehensive error handling.
 *
 * This group contains all the primary classes (ServerSocket, Socket, DatagramSocket, MulticastSocket)
 * and essential types and utilities provided by jsocketpp.
 *
 * Example usage:
 * @code
 * #include <jsocketpp/ServerSocket.hpp>
 * #include <jsocketpp/Socket.hpp>
 * @endcode
 */

/**
 * @defgroup core Core Utilities and Types
 * @ingroup jsocketpp
 * @brief Internal types, platform abstractions, and utility functions used across the jsocketpp library.
 *
 * This group contains foundational components of the jsocketpp socket library,
 * including common type aliases, byte-order conversion utilities, and
 * cross-platform compatibility helpers.
 *
 * Components in this group are typically not used directly by application-level code,
 * but are essential for internal implementation and protocol correctness.
 *
 * ### Includes:
 * - `jsocketpp::Port` – type alias for network ports
 * - `jsocketpp::net` – byte order conversion utilities (host ↔ network)
 * - Platform-specific type abstractions (`SOCKET`, `INVALID_SOCKET`, etc.)
 *
 * @see jsocketpp::net
 * @see Port
 */

/**
 * @defgroup internal Internal Helpers
 * @ingroup jsocketpp
 * @brief Implementation-only utilities for internal use.
 *
 * These functions and types are not part of the public API.
 * They are intended for internal glue code, platform compatibility, and cross-cutting concerns.
 *
 * @warning Do not rely on this module from user code. It is subject to change without notice.
 */

/**
 * @defgroup tcp TCP Sockets
 * @ingroup jsocketpp
 * @brief Classes and functions for TCP networking.
 */

/**
 * @defgroup udp UDP Sockets
 * @ingroup jsocketpp
 * @brief Classes and functions for UDP datagram networking.
 */

/**
 * @defgroup unix Unix Domain Sockets
 * @ingroup jsocketpp
 * @brief Classes and functions for Unix domain socket networking.
 */

/**
 * @defgroup exceptions Exception Classes
 * @ingroup jsocketpp
 * @brief Exception types used in jsocketpp for error handling.
 */

/**
 * @defgroup socketopts Socket Options
 * @ingroup jsocketpp
 * @brief Low-level socket configuration options exposed via the `Socket` and related classes.
 *
 * This module contains functions and accessors for configuring standard socket options
 * such as SO_LINGER, SO_RCVTIMEO, TCP_NODELAY, SO_REUSEADDR, and SO_KEEPALIVE.
 *
 * These options provide fine-grained control over low-level socket behavior such as
 * timeout behavior, connection shutdown semantics, buffering, and performance tuning.
 *
 * All functions in this group are cross-platform abstractions over `setsockopt()` and `getsockopt()`.
 *
 * @note Some options may not be available or meaningful on all platforms. Platform-specific
 * behavior is documented per function.
 *
 * @see Socket::setSoLinger(), Socket::getSoLinger(), Socket::setTcpNoDelay(), etc.
 */

/**
 * @defgroup utils Utility Functions
 * @ingroup jsocketpp
 * @brief Public utility functions for working with socket addresses, conversions, and formatting.
 *
 * This module includes general-purpose helpers that simplify common socket-related tasks such as:
 * - Converting socket addresses to strings
 * - Parsing string representations into sockaddr structures
 * - Formatting or inspecting address data across socket types
 *
 * These functions are protocol-agnostic and can be used with TCP, UDP, and Unix domain sockets.
 *
 * @see addressToString()
 * @see stringToAddress()
 */

/**
 * @namespace jsocketpp
 * @brief A C++ socket library providing Java-style networking interfaces
 *
 * The jsocketpp namespace contains classes and utilities for network programming
 * with an API design inspired by Java's networking classes. It provides:
 *
 * Core Socket Classes:
 * - Socket: TCP client socket implementation for stream-based communication
 * - ServerSocket: TCP server socket for accepting client connections
 * - DatagramSocket: UDP socket implementation for connectionless communication
 * - MulticastSocket: Extended UDP socket supporting IP multicast operations
 * - UnixSocket: Unix domain socket implementation for local IPC
 *
 * Features:
 * - Exception-based error handling (SocketException)
 * - RAII-compliant resource management
 * - Support for both IPv4 and IPv6
 * - Cross-platform compatibility (Windows/Unix)
 * - Modern C++ design with move semantics
 *
 * Design Philosophy:
 * - Familiar interface for Java developers
 * - Modern C++ practices and idioms
 * - Exception-based error handling
 * - Resource safety through RAII
 * - Explicit over implicit behavior
 *
 * @note All classes in this namespace are not thread-safe unless explicitly stated.
 *       Each socket should be used from a single thread at a time.
 */
namespace jsocketpp
{
#ifdef _WIN32

typedef long ssize_t;

inline int InitSockets()
{
    WSADATA WSAData;
    return WSAStartup(MAKEWORD(2, 2), &WSAData);
}

inline int CleanupSockets()
{
    return WSACleanup();
}

inline int GetSocketError()
{
    return WSAGetLastError();
}

// NOLINTNEXTLINE(misc-const-correctness) - changes socket state
inline int CloseSocket(SOCKET fd)
{
    return closesocket(fd);
}

const char* inet_ntop_aux(int af, const void* src, char* dst, socklen_t size);

#define JSOCKETPP_TIMEOUT_CODE WSAETIMEDOUT

#else

typedef int SOCKET;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr SOCKET SOCKET_ERROR = -1;
typedef sockaddr_un SOCKADDR_UN;

#define JSOCKETPP_TIMEOUT_CODE ETIMEDOUT

constexpr int InitSockets()
{
    return 0;
}
constexpr int CleanupSockets()
{
    return 0;
}
inline int GetSocketError()
{
    return errno;
}
inline int CloseSocket(const SOCKET fd)
{
    return close(fd);
}

inline int ioctlsocket(const SOCKET fd, const long cmd, u_long* argp)
{
    return ioctl(fd, static_cast<unsigned long>(cmd), argp);
}

#endif

/**
 * @brief Get all local network interface addresses as strings.
 *
 * @return Vector of strings describing each interface and address.
 */
std::vector<std::string> getHostAddr();

/**
 * @brief Convert a socket-related error code to a human-readable message.
 *
 * @details
 * This helper produces best-effort, descriptive text for errors originating from:
 * - Traditional socket/Winsock APIs that report failure via errno or WSAGetLastError().
 * - Modern address APIs (getaddrinfo/getnameinfo) that return EAI_* codes directly.
 *
 * Usage rules:
 * - For APIs that set errno or WSAGetLastError(), pass that code with @p gaiStrerror set to false.
 * - For getaddrinfo/getnameinfo failures, pass the function's return value with @p gaiStrerror set to true.
 *   Do not call WSAGetLastError() for getaddrinfo/getnameinfo, as those functions return their error code.
 *
 * Platform behavior:
 * - Windows: Attempts FormatMessageA for system and WSA* codes; for EAI_* codes, uses gai_strerrorA.
 *   The returned text is trimmed of trailing newlines/spaces. Fallbacks include std::system_category().message()
 *   and strerror_s(). The function is written to avoid throwing; if resolution fails, it returns a generic message.
 * - POSIX: Uses std::system_category().message() for errno values and gai_strerror() for EAI_* codes,
 *   falling back to strerror() when necessary. The function is written to avoid throwing.
 *
 * Thread-safety notes:
 * - The implementation copies any message obtained from gai_strerror/gai_strerrorA immediately into a std::string,
 *   avoiding reliance on static internal buffers after return.
 *
 * @param[in] error
 *     Numeric error code. For errno/WSA* paths, pass errno or GetSocketError() (WSAGetLastError()).
 *     For getaddrinfo/getnameinfo, pass the function's non-zero return value (an EAI_* code).
 *
 * @param[in] gaiStrerror
 *     Set to true if @p error is an address-resolution error from getaddrinfo/getnameinfo (EAI_* domain).
 *     Set to false for traditional socket/Winsock errors (errno/WSA* domain). Default is false.
 *
 * @return
 *     A best-effort, human-readable description of the error. Returns an empty string when @p error is zero.
 *
 * @note
 *     Some environments may produce localized messages. Message content is not guaranteed to be stable
 *     across operating systems, C libraries, or language settings.
 *
 * @throws
 *     This function is designed not to throw. In failure cases, it returns a conservative string
 *     such as "Unknown error <code>".
 *
 * @code{.cpp}
 * // Example: traditional socket/Winsock error
 * if (sendto(sock, buf, len, 0, addr, addrlen) == SOCKET_ERROR) {
 *     const int err = GetSocketError(); // wraps WSAGetLastError() on Windows, errno elsewhere
 *     std::string msg = SocketErrorMessage(err, false);
 *     // handle or log 'msg'
 * }
 *
 * // Example: getaddrinfo failure (EAI_* code)
 * addrinfo* ai = nullptr;
 * const int ret = ::getaddrinfo(host, service, &hints, &ai);
 * if (ret != 0) {
 *     std::string msg = SocketErrorMessage(ret, true);
 *     // handle or log 'msg'
 * }
 * @endcode
 */
std::string SocketErrorMessage(int error, bool gaiStrerror = false);

/**
 * @brief Enum for socket shutdown modes.
 *
 * Used to specify how to shutdown a socket (read, write, or both).
 */
enum class ShutdownMode
{
    Read,  ///< Shutdown read operations (SHUT_RD or SD_RECEIVE)
    Write, ///< Shutdown write operations (SHUT_WR or SD_SEND)
    Both   ///< Shutdown both read and write operations (SHUT_RDWR or SD_BOTH)
};

/**
 * @typedef Port
 * @brief Type alias representing a TCP or UDP port number (1–65535).
 * @ingroup core
 *
 * This alias provides strong typing for network port numbers across the
 * `jsocketpp` library. It improves readability and makes function signatures
 * semantically clearer when dealing with socket operations.
 *
 * Using `Port` instead of a plain `uint16_t` or `unsigned short` helps:
 * - Differentiate port values from other numeric parameters
 * - Assist with static analysis and overload resolution
 * - Align with best practices in modern C++ design
 *
 * @note Although a `Port` is technically an integer, it represents a
 * well-defined semantic domain (TCP/UDP port number).
 */
using Port = std::uint16_t;

/**
 * @brief Default internal buffer size (in bytes) for socket read operations.
 * @ingroup core
 *
 * This constant defines the default size (4096 bytes / 4 KB) of the internal read buffer
 * used across various socket classes in the library unless explicitly overridden.
 *
 * ### Applies to
 * - **TCP client sockets** (`Socket`) when constructed without a custom `internalBufferSize`
 * - **Accepted TCP sockets** via `ServerSocket::accept()`
 * - **UDP sockets** (`DatagramSocket`) when no explicit `bufferSize` is passed
 * - May also serve as a guideline for other buffer sizes (e.g., send/receive buffers)
 *
 * ### Rationale
 * - **Memory-efficient:** 4096 bytes aligns with the typical memory page size on most systems.
 * - **Performance-optimized:** Large enough for common protocol messages (e.g., HTTP, DNS, WebSocket)
 * - **Concurrency-friendly:** Balances throughput and memory usage across many simultaneous sockets.
 *
 * ### Customization
 * Override this value by:
 * - Passing a custom buffer size to socket constructors (e.g., `DatagramSocket(port, 8192)`)
 * - Calling `setInternalBufferSize()` after construction
 * - Tuning system-level buffers via `setReceiveBufferSize()` or `setSendBufferSize()`
 *
 * ### When to Adjust
 * Increase if:
 * - Your application transfers large datagrams or stream data (e.g., file uploads, media streams)
 * - You want to reduce syscall overhead by reading more per call
 *
 * Decrease if:
 * - Operating in memory-constrained environments (e.g., embedded systems)
 * - Managing a large number of idle sockets where footprint matters more than speed
 *
 * @see Socket
 * @see ServerSocket
 * @see DatagramSocket
 * @see Socket::setInternalBufferSize()
 * @see SocketOptions::setReceiveBufferSize()
 * @see SocketOptions::setSendBufferSize()
 */
inline constexpr std::size_t DefaultBufferSize = 4096;

/**
 * @brief Fallback receive size (in bytes) for UDP datagrams when the exact size is unknown.
 * @ingroup core
 *
 * This constant defines the **default number of bytes** we ask the OS to deliver when
 * receiving a UDP datagram **and** the platform cannot report the pending datagram's exact
 * size (via `FIONREAD` or the POSIX `MSG_PEEK|MSG_TRUNC` probe) **and** the caller has not
 * provisioned `_internalBuffer` (i.e., its size is 0).
 *
 * ### Applies to
 * - **UDP sockets** (`DatagramSocket`) in `read()` overloads that construct a `std::string`
 *   or otherwise need a temporary buffer when the exact size is unavailable and
 *   `_internalBuffer` is unset.
 * - May be used by other UDP-related helpers where a conservative per-call buffer is required.
 *
 * ### Semantics & policy
 * - We **do not** resize `_internalBuffer` based on this value; it is a *one-shot* receive size
 *   when the caller has not sized a reusable buffer.
 * - If the incoming datagram is **larger** than this value and the OS cannot tell us the exact
 *   size up-front, the payload may be **truncated** to this size (standard UDP behavior).
 * - A safety cap of **65,507 bytes** (maximum safe UDP payload) is always honored, even if this
 *   constant is set higher.
 *
 * ### Rationale (default: 8192 / 8 KiB)
 * - **Practicality:** Big enough for common application messages while avoiding large, frequent
 *   allocations when size probing is unavailable.
 * - **Performance:** Reduces syscall overhead compared to very small defaults without bloating
 *   per-receive memory.
 *
 * ### Customization
 * - Increase if your application expects larger datagrams and you prefer to minimize truncation
 *   when size probing is unavailable.
 * - Decrease in memory-constrained environments if your datagrams are typically small.
 *
 * ### Examples
 * @code
 * // Typical path inside read<std::string>():
 * // 1) Try exact size via nextDatagramSize(fd)
 * // 2) If 0 and _internalBuffer.size() == 0, use DefaultDatagramReceiveSize
 * // 3) Read up to std::min(DefaultDatagramReceiveSize, 65507)
 * @endcode
 *
 * @note When `_internalBuffer` is already sized by the user, its capacity takes precedence
 *       as the "caller-provided buffer" (Java-like semantics); this constant is not used then.
 *
 * @see jsocketpp::internal::nextDatagramSize()
 * @see DatagramSocket
 * @since 1.0
 */
inline constexpr std::size_t DefaultDatagramReceiveSize = 8192;

/**
 * @brief Maximum UDP payload size (in bytes) that is safely valid across common stacks.
 * @ingroup core
 *
 * This constant represents a conservative upper bound for a single UDP payload that is
 * widely supported across platforms and socket APIs. It equals the IPv4 maximum payload:
 *
 *   65,535 (IPv4 total length)
 * −      20 (minimum IPv4 header)
 * −       8 (UDP header)
 * =  65,507 bytes
 *
 * ### Why a "safe" cap?
 * - Many APIs and stacks historically enforce the IPv4 limit even for IPv6 sockets for
 *   compatibility. Using this value avoids surprises (e.g., EMSGSIZE) and simplifies
 *   buffer sizing when the address family is not known in advance.
 *
 * ### Usage
 * - Clamp opportunistic receive sizes to @c MaxDatagramPayloadSafe to prevent oversizing.
 * - When sending, avoid constructing payloads larger than this unless you *know* you are
 *   on IPv6 and have validated larger limits (see @ref MaxUdpPayloadIPv6).
 *
 * @see MaxUdpPayloadIPv4
 * @see MaxUdpPayloadIPv6
 * @since 1.0
 */
inline constexpr std::size_t MaxDatagramPayloadSafe = 65507;

/**
 * @brief Maximum UDP payload size (in bytes) over IPv4.
 * @ingroup core
 *
 * Computed as: 65,535 (IPv4 total length) − 20 (IPv4 header) − 8 (UDP header) = 65,507 bytes.
 * Use this when you know the socket/address family is IPv4.
 *
 * @see MaxDatagramPayloadSafe
 * @since 1.0
 */
inline constexpr std::size_t MaxUdpPayloadIPv4 = 65507;

/**
 * @brief Theoretical maximum UDP payload size (in bytes) over IPv6.
 * @ingroup core
 *
 * Computed as: 65,535 (IPv6 *payload* length, excludes the 40-byte IPv6 header)
 * −           8 (UDP header)
 * =      65,527 bytes.
 *
 * @note Real-world effective limits can be smaller due to extension headers, PMTU, and stack
 *       constraints. Many stacks still behave as if the IPv4 limit applies. Prefer
 *       @ref MaxDatagramPayloadSafe unless you have verified larger datagrams are supported.
 *
 * @see MaxDatagramPayloadSafe
 * @since 1.0
 */
inline constexpr std::size_t MaxUdpPayloadIPv6 = 65527;

/**
 * @brief Checks if a given sockaddr_in6 represents an IPv4-mapped IPv6 address.
 * @ingroup core
 *
 * IPv4-mapped IPv6 addresses allow IPv6-only sockets to interoperate with IPv4 clients
 * by embedding an IPv4 address inside a special IPv6 format:
 *
 * @code
 * ::ffff:a.b.c.d   // 0000:0000:0000:0000:0000:ffff:abcd:efgh
 * @endcode
 *
 * This function identifies such addresses so they can be normalized to pure IPv4.
 *
 * @param addr6 Pointer to a sockaddr_in6 structure.
 * @return true if the address is IPv4-mapped; false otherwise.
 */
inline bool isIPv4MappedIPv6(const sockaddr_in6* addr6)
{
    return IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr);
}

/**
 * @brief Converts an IPv4-mapped IPv6 address to a pure IPv4 sockaddr_in.
 * @ingroup core
 *
 * This function extracts the embedded IPv4 address from an IPv4-mapped IPv6 address.
 * It preserves the port number and fills a fully valid `sockaddr_in`.
 *
 * ### Preconditions
 * - The input must be an IPv4-mapped IPv6 address.
 * - You should call `isIPv4MappedIPv6()` before using this function.
 *
 * @param addr6 A sockaddr_in6 known to be IPv4-mapped.
 * @return A sockaddr_in representing the embedded IPv4 address and port.
 */
inline sockaddr_in convertIPv4MappedIPv6ToIPv4(const sockaddr_in6& addr6)
{
    sockaddr_in addr4{};
    addr4.sin_family = AF_INET;
    addr4.sin_port = addr6.sin6_port;
    std::memcpy(&addr4.sin_addr.s_addr, addr6.sin6_addr.s6_addr + 12, sizeof(addr4.sin_addr.s_addr));
    return addr4;
}

/**
 * @brief Extracts a human-readable IP address from a socket address structure.
 *
 * This function returns the numeric IP address (IPv4 or IPv6) contained in the given sockaddr.
 * If the address is an IPv4-mapped IPv6 (::ffff:a.b.c.d) and @p convertIPv4Mapped is true,
 * the result is converted to the original IPv4 form (e.g., "192.168.1.10").
 *
 * @param addr Pointer to a sockaddr structure (must be AF_INET or AF_INET6).
 * @param convertIPv4Mapped Whether to convert IPv4-mapped IPv6 addresses to plain IPv4.
 * @return IP address as a string (e.g., "127.0.0.1" or "fe80::1").
 *
 * @throws SocketException if the address family is unsupported or conversion fails.
 */
std::string ipFromSockaddr(const sockaddr* addr, bool convertIPv4Mapped = true);

/**
 * @brief Extracts the port number from a socket address structure.
 *
 * Retrieves the port from a sockaddr containing either an IPv4 or IPv6 address,
 * converting it from network byte order to host byte order.
 *
 * @param addr Pointer to a sockaddr structure (must be AF_INET or AF_INET6).
 * @return Port number in host byte order (e.g., 8080).
 *
 * @throws SocketException if the address family is unsupported.
 */
Port portFromSockaddr(const sockaddr* addr);

/**
 * @brief Converts a socket address to a human-readable "IP:port" string.
 * @ingroup utils
 *
 * This utility function transforms a `sockaddr_storage` structure into a
 * string representation using `getnameinfo()`, suitable for logging,
 * diagnostics, or display. It supports both IPv4 (`AF_INET`) and IPv6 (`AF_INET6`)
 * addresses and outputs the address in the form:
 *
 * - `"192.168.1.10:8080"` for IPv4
 * - `"[::1]:12345"` for IPv6 (note: bracket wrapping is *not* added automatically)
 *
 * For unknown or unsupported address families, the function returns `"unknown"`.
 *
 * @param[in] addr  A fully populated `sockaddr_storage` containing the IP address and port.
 *
 * @return A string in the format `"IP:port"` (e.g., `"127.0.0.1:8080"` or `"::1:53"`).
 *
 * @throws SocketException
 *         If `getnameinfo()` fails to resolve the IP or port for the provided address.
 *
 * @note This function does not add square brackets around IPv6 addresses. If you need
 *       bracketed formatting (e.g., for URL embedding), you must post-process the result.
 *
 * @code
 * sockaddr_storage addr = ...;
 * std::string str = addressToString(addr);
 * std::cout << "Connected to " << str << "\n";
 * @endcode
 */
std::string addressToString(const sockaddr_storage& addr);

/**
 * @brief Parses an "IP:port" string into a sockaddr_storage structure.
 * @ingroup utils
 *
 * This utility function takes a string of the form `"host:port"` and resolves it
 * into a platform-compatible `sockaddr_storage` structure using `getaddrinfo()`.
 * It supports both IPv4 and IPv6 addresses, including:
 *
 * - `"127.0.0.1:8080"`
 * - `"::1:1234"`
 * - IPv6 addresses **without square brackets** (e.g., `"2001:db8::1:443"`)
 *
 * The resulting structure can be passed directly to socket functions like
 * `connect()`, `bind()`, or `sendto()`.
 *
 * @param[in] str   The string to parse, in the format `"ip:port"`.
 *                  Must not include brackets around IPv6 addresses.
 * @param[out] addr The output `sockaddr_storage` structure to populate.
 *
 * @throws SocketException
 *         - If the string is missing a `:` separator
 *         - If the port cannot be parsed
 *         - If `getaddrinfo()` fails to resolve the address
 *
 * @note This function assumes numeric host and port. No DNS resolution is performed.
 *
 * @code
 * sockaddr_storage addr;
 * stringToAddress("192.168.0.10:9000", addr);
 * connect(sockFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
 * @endcode
 */
void stringToAddress(const std::string& str, sockaddr_storage& addr);

} // namespace jsocketpp

/**
 * @namespace jsocketpp::net
 * @brief Endianness utilities for network byte order conversion.
 * @ingroup core
 *
 * The `jsocketpp::net` namespace provides a collection of lightweight,
 * header-only functions for safely converting between host and network
 * byte order for integral types.
 *
 * ### Purpose
 * - Ensures portable binary encoding across different CPU architectures
 * - Enables interoperability with other systems that use big-endian formats
 * - Matches standard socket API behavior (e.g., htons/ntohl)
 *
 * ### Usage Example
 * @code{.cpp}
 * using namespace jsocketpp::net;
 *
 * uint32_t value = 123456;
 * uint32_t networkValue = toNetwork(value);     // Host → network (big-endian)
 * uint32_t restored = fromNetwork(networkValue); // Network → host
 * @endcode
 *
 * These functions are wrappers over platform-native byte order macros:
 * - Windows: `htons`, `ntohl`, etc. (from winsock2.h)
 * - POSIX: `htons`, `ntohl`, etc. (from arpa/inet.h)
 *
 * ### Recommended Use
 * - Apply to any integral header field before transmission over the wire
 * - Use in conjunction with `Socket::read<T>()` and `Socket::writeFromAll()`
 *   to ensure cross-platform safety for multi-byte fields
 *
 * @note Network byte order is always **big-endian**, as defined by RFC 1700.
 *
 * @see toNetwork() Host-to-network conversion
 * @see fromNetwork() Network-to-host conversion
 * @see Socket::readPrefixed() Uses these internally
 */
namespace jsocketpp::net
{

/**
 * @brief Converts a 16-bit unsigned integer from host to network byte order.
 */
inline uint16_t toNetwork(const uint16_t val)
{
    return htons(val);
}

/**
 * @brief Converts a 32-bit unsigned integer from host to network byte order.
 */
inline uint32_t toNetwork(const uint32_t val)
{
    return htonl(val);
}

/**
 * @brief Converts a 16-bit unsigned integer from network to host byte order.
 */
inline uint16_t fromNetwork(const uint16_t val)
{
    return ntohs(val);
}

/**
 * @brief Converts a 32-bit unsigned integer from network to host byte order.
 */
inline uint32_t fromNetwork(const uint32_t val)
{
    return ntohl(val);
}

} // namespace jsocketpp::net

namespace jsocketpp::internal
{

/**
 * @struct AddrinfoDeleter
 * @brief Custom deleter for `addrinfo*` pointers to support RAII-style cleanup.
 * @ingroup internal
 *
 * This deleter is intended for use with `std::unique_ptr<addrinfo, AddrinfoDeleter>`,
 * enabling automatic cleanup of dynamically allocated `addrinfo` structures returned
 * by `getaddrinfo()`. When the smart pointer goes out of scope, `freeaddrinfo()` will
 * be called automatically to release associated memory.
 *
 * ### Example
 * @code
 * addrinfo* raw = nullptr;
 * getaddrinfo("example.com", "80", &hints, &raw);
 * AddrinfoPtr info(raw); // Automatically cleaned up
 * @endcode
 *
 * @see AddrinfoPtr
 * @see getaddrinfo()
 * @see freeaddrinfo()
 */
struct AddrinfoDeleter
{
    /**
     * @brief Deletes an `addrinfo*` by calling `freeaddrinfo()`.
     * @param[in] p Pointer to an `addrinfo` structure to be deallocated.
     */
    void operator()(addrinfo* p) const noexcept
    {
        if (p)
            freeaddrinfo(p);
    }
};

/**
 * @typedef AddrinfoPtr
 * @brief Smart pointer that manages `addrinfo*` resources using `AddrinfoDeleter`.
 * @ingroup internal
 *
 * `AddrinfoPtr` wraps a raw `addrinfo*` (typically from `getaddrinfo()`) in a
 * `std::unique_ptr` with a custom deleter to ensure safe, automatic cleanup
 * using `freeaddrinfo()`. This eliminates manual memory management and guards
 * against memory leaks in error-prone network code.
 *
 * ### Example Usage
 * @code
 * addrinfo* raw = nullptr;
 * if (getaddrinfo("example.com", "80", &hints, &raw) == 0) {
 *     AddrinfoPtr info(raw); // Automatically cleaned up on scope exit
 *     // Use info.get() safely here
 * }
 * @endcode
 *
 * @see AddrinfoDeleter
 * @see getaddrinfo()
 * @see freeaddrinfo()
 */
using AddrinfoPtr = std::unique_ptr<addrinfo, AddrinfoDeleter>;

/**
 * @brief Resolves a hostname and port into a list of usable socket address structures.
 * @ingroup internal
 *
 * This internal helper wraps the standard `::getaddrinfo()` system call to resolve a hostname and port
 * into a linked list of `addrinfo` structures, which are used to create, bind, or connect sockets.
 * It provides explicit control over resolution parameters, supports both client and server use cases,
 * and ensures consistent error handling and memory cleanup across platforms.
 *
 * ---
 *
 * ### Overview
 *
 * Unlike direct `getaddrinfo()` usage, this helper:
 * - Accepts all key resolution parameters (`family`, `socktype`, `protocol`, `flags`)
 * - Returns a RAII-managed `AddrinfoPtr` (automatically frees memory via `freeaddrinfo()`)
 * - Throws a `SocketException` with detailed context if resolution fails
 * - Supports dual-stack fallback, wildcard binding, strict numeric resolution, and more
 *
 * ---
 *
 * ### Hints Structure Behavior
 *
 * This function populates the `hints` structure for `getaddrinfo()` with the following fields:
 *
 * - **ai_family**: Address family to return:
 *     - `AF_INET` — IPv4 only
 *     - `AF_INET6` — IPv6 only
 *     - `AF_UNSPEC` — Return both (default for dual-stack logic)
 *
 * - **ai_socktype**: Type of socket:
 *     - `SOCK_STREAM` — TCP
 *     - `SOCK_DGRAM` — UDP
 *     - `SOCK_RAW` — Raw IP (less common)
 *
 * - **ai_protocol**: Transport-layer protocol:
 *     - `IPPROTO_TCP` — for TCP sockets
 *     - `IPPROTO_UDP` — for UDP sockets
 *     - `0` — auto-detect based on `socktype`
 *
 * - **ai_flags**: Bitmask of resolution modifiers. Includes:
 *     - `AI_PASSIVE` — Use wildcard address (0.0.0.0 / ::) if `host` is empty (for server binding)
 *     - `AI_NUMERICHOST` — Require `host` to be a numeric IP; skip DNS
 *     - `AI_NUMERICSERV` — Require `port` to be numeric; skip service name lookup
 *     - `AI_CANONNAME` — Populate `ai_canonname` with canonical FQDN
 *     - `AI_ADDRCONFIG` — Only return families configured on the local machine
 *     - `AI_V4MAPPED` — Allow IPv4-mapped IPv6 addresses if `AF_INET6` is requested
 *
 * Flags may be combined using bitwise OR (e.g., `AI_PASSIVE | AI_ADDRCONFIG`).
 *
 * ---
 *
 * ### Parameters
 *
 * @param[in] host
 *     Hostname, domain, or IP address to resolve.
 *     - Use empty string if `AI_PASSIVE` is set to bind to all interfaces.
 *     - Must be numeric if `AI_NUMERICHOST` is specified.
 *
 * @param[in] port
 *     Port number to resolve, passed as an integer. Must be in the range `[0, 65535]`.
 *     Internally converted to a string before calling `getaddrinfo()`.
 *
 * @param[in] family
 *     Address family to restrict the result:
 *     - `AF_INET` for IPv4
 *     - `AF_INET6` for IPv6
 *     - `AF_UNSPEC` for both (default in most client cases)
 *
 * @param[in] socktype
 *     Desired socket type (e.g., `SOCK_STREAM`, `SOCK_DGRAM`)
 *
 * @param[in] protocol
 *     Desired protocol (e.g., `IPPROTO_TCP`, `IPPROTO_UDP`, or `0`)
 *
 * @param[in] flags
 *     Bitmask of `AI_*` flags. See the list above for all supported options.
 *
 * ---
 *
 * ### Return Value
 *
 * @return A smart pointer of type `AddrinfoPtr` holding a linked list of `addrinfo` structures.
 *         The returned list can be iterated to attempt socket creation or connection.
 *         Memory is released automatically via `freeaddrinfo()` when the pointer is destroyed.
 *
 * ---
 *
 * ### Throws
 *
 * @throws SocketException if `getaddrinfo()` fails.
 *     - On Windows: error code from `GetSocketError()`, with message from `gai_strerror()`
 *     - On POSIX: return code from `getaddrinfo()`, also with `gai_strerror()` message
 *     - The error message will include whether the failure occurred on the host or service name
 *
 * ---
 *
 * ### Example Usage
 *
 * @code
 * using namespace jsocketpp::internal;
 *
 * // Resolve a remote TCP address (client-side)
 * auto addrList = resolveAddress("example.com", 443, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP);
 *
 * // Iterate candidates and attempt connection
 * for (addrinfo* p = addrList.get(); p != nullptr; p = p->ai_next)
 * {
 *     int sockfd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
 *     if (sockfd != INVALID_SOCKET && ::connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
 *         break; // success
 *     // try next if socket or connect failed
 * }
 * @endcode
 *
 * ---
 *
 * ### Notes
 *
 * - This function is intended for internal use by `Socket`, `ServerSocket`, and `DatagramSocket`.
 * - It supports both binding (server-side) and connecting (client-side) resolution logic.
 * - Always use `AF_UNSPEC` and let `getaddrinfo()` return both IPv4 and IPv6 for best cross-platform support.
 *
 * @see getaddrinfo(), freeaddrinfo(), AddrinfoPtr, SocketException
 */
[[nodiscard]] inline AddrinfoPtr resolveAddress(const std::string_view host, const Port port, const int family,
                                                const int socktype, const int protocol, const int flags = 0)
{
    addrinfo hints{};
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_protocol = protocol;
    hints.ai_flags = flags;

    const std::string portStr = std::to_string(port);
    addrinfo* raw = nullptr;

    if (const int ret = ::getaddrinfo(host.empty() ? nullptr : host.data(), portStr.c_str(), &hints, &raw); ret != 0)
    {
        throw SocketException(ret, SocketErrorMessage(ret, true));
    }

    return AddrinfoPtr{raw};
}

/**
 * @brief Retrieves the local IP address to which the socket is currently bound.
 * @ingroup internal
 *
 * This function returns the **numeric IP address** of the socket's local endpoint,
 * based on the actual binding or connection state. It supports both IPv4 and IPv6 sockets.
 *
 * ---
 *
 * ### 🔧 Internal Mechanism
 * This method wraps two low-level system calls:
 *
 * - `getsockname()`:
 *   Obtains the local address (IP and port) that the socket is bound to.
 *   This works regardless of whether the socket was explicitly bound (`bind()`) or implicitly assigned
 *   a source address via `connect()` or `sendto()` on unconnected sockets.
 *
 * - `getnameinfo()`:
 *   Converts the raw `sockaddr` structure returned by `getsockname()` into a
 *   **numeric IP string** (e.g., `"127.0.0.1"` or `"::1"`), independent of DNS.
 *
 * ---
 *
 * ### ✅ Use Cases
 * - Discover which local interface the OS selected after `bind()` or `connect()`
 * - Match the socket to a network adapter for MTU queries or interface statistics
 * - Print the socket’s local IP address for diagnostics or logging
 * - Support systems where multiple NICs or address families are in use
 *
 * ---
 *
 * ### ⚠️ Error Handling
 * - Throws `SocketException` if the socket is not open, not yet bound, or if address resolution fails.
 * - All errors include system-specific error codes and human-readable descriptions.
 *
 * ---
 *
 * @param[in] sockFd The socket descriptor (`SOCKET` on Windows, `int` on POSIX).
 *
 * @return A string containing the numeric IPv4 or IPv6 address the socket is bound to.
 *
 * @throws SocketException If:
 * - The socket is invalid or unbound
 * - `getsockname()` fails (e.g., bad descriptor)
 * - `getnameinfo()` fails (e.g., unsupported address format)
 *
 * @note This function does not return the remote peer address — use `getpeername()` for that.
 * @note This function does not include the port — only the IP address portion is returned.
 *
 * @see getLocalSocketAddress(), getpeername(), resolveAddress(), getMTU()
 */
[[nodiscard]] std::string getBoundLocalIp(SOCKET sockFd);

/**
 * @brief Compares two IP addresses for logical equality, accounting for IPv4-mapped IPv6 forms.
 *
 * This function compares two IP addresses represented as strings (e.g., `"192.168.1.1"` or `"::ffff:192.168.1.1"`)
 * and returns true if they refer to the same underlying address, regardless of representation format.
 *
 * ---
 *
 * ### Why This Matters
 * IPv4 addresses can be represented in IPv6 as *IPv4-mapped* addresses (e.g., `"::ffff:192.168.1.1"`),
 * which are equivalent to their native IPv4 forms but will **not compare equal as strings**.
 *
 * This helper resolves both input strings using `inet_pton()` and normalizes any IPv4 addresses into
 * the IPv6-mapped space for byte-wise comparison.
 *
 * ---
 *
 * ### Supported Inputs
 * - IPv4 addresses (e.g., `"10.0.0.1"`)
 * - IPv6 addresses (e.g., `"2001:db8::1"`)
 * - IPv4-mapped IPv6 addresses (e.g., `"::ffff:10.0.0.1"`)
 *
 * ---
 *
 * @param[in] ip1 First IP address as a numeric string.
 * @param[in] ip2 Second IP address as a numeric string.
 * @return `true` if the addresses are equivalent (including cross-family matches), `false` otherwise.
 *
 * @note This function performs no DNS resolution. Only numeric addresses are supported.
 * @note Both inputs must be valid IP addresses. Invalid strings return `false`.
 *
 * @see getBoundLocalIp(), getMTU(), inet_pton()
 */
[[nodiscard]] bool ipAddressesEqual(const std::string& ip1, const std::string& ip2);

/**
 * @brief Sends an entire datagram to a connected peer using `send()`.
 * @ingroup internal
 *
 * This internal utility transmits exactly @p size bytes from the given @p data
 * buffer over the specified socket @p fd, using the system `send()` call.
 * It is intended for use with connected UDP or TCP sockets where the destination
 * address is already established via @ref connect().
 *
 * ---
 *
 * ### ⚙️ Behavior
 * - Applies `MSG_NOSIGNAL` on POSIX systems to prevent `SIGPIPE` if the peer has closed the connection.
 * - Casts the buffer size appropriately for Windows (`int`) and POSIX (`size_t`) APIs.
 * - On success, guarantees that **all** @p size bytes are sent in a single system call.
 * - Throws an exception if:
 *   - The socket is invalid (`INVALID_SOCKET`)
 *   - `send()` fails for any reason
 *   - The number of bytes actually sent differs from @p size (partial send)
 *
 * ---
 *
 * ### 🧪 Example
 * @code
 * std::string message = "Hello, world!";
 * internal::sendExact(socketFd, message.data(), message.size());
 * @endcode
 *
 * ---
 *
 * @param[in] fd    The connected socket file descriptor to send data on.
 * @param[in] data  Pointer to the raw buffer containing data to transmit.
 * @param[in] size  Number of bytes to send from @p data.
 *
 * @throws SocketException
 *         If the socket is invalid, if `send()` returns an error,
 *         or if a partial datagram is sent.
 *
 * @warning This function does not perform retries or fragmentation.
 *          For UDP, datagrams larger than the network MTU may be dropped.
 * @warning Intended for internal use — use higher-level `write()` APIs instead in application code.
 *
 * @see sendExactTo() For sending to an explicit address without connecting first.
 * @see connect() To establish a connected peer before using this function.
 */
void sendExact(SOCKET fd, const void* data, std::size_t size);

/**
 * @brief Sends an entire datagram to a specific destination using `sendto()`.
 * @ingroup internal
 *
 * This internal utility transmits exactly @p size bytes from the given @p data
 * buffer over the specified socket @p fd, using the system `sendto()` call to
 * send to an explicit destination address. It is intended for unconnected
 * UDP sockets, where the destination may vary per call.
 *
 * ---
 *
 * ### ⚙️ Behavior
 * - Applies `MSG_NOSIGNAL` on POSIX systems to prevent `SIGPIPE` if the destination
 *   is unreachable or the peer has closed the socket.
 * - Casts the buffer size and address length appropriately for Windows (`int`) and
 *   POSIX (`size_t`, `socklen_t`) APIs.
 * - On success, guarantees that **all** @p size bytes are sent in a single system call.
 * - Throws an exception if:
 *   - The socket is invalid (`INVALID_SOCKET`)
 *   - `sendto()` fails for any reason
 *   - The number of bytes actually sent differs from @p size (partial send)
 *
 * ---
 *
 * ### 🧪 Example
 * @code
 * sockaddr_storage destAddr{};
 * // populate destAddr with desired IPv4/IPv6 destination...
 * internal::sendExactTo(socketFd, message.data(), message.size(),
 *                       reinterpret_cast<const sockaddr*>(&destAddr),
 *                       sizeof(sockaddr_in));
 * @endcode
 *
 * ---
 *
 * @param[in] fd       The socket file descriptor to send data on.
 * @param[in] data     Pointer to the raw buffer containing data to transmit.
 * @param[in] size     Number of bytes to send from @p data.
 * @param[in] addr     Pointer to the destination address structure (IPv4 or IPv6).
 * @param[in] addrLen  Length of the address structure in bytes.
 * @param[in] afterSuccess Optional callback invoked after a successful send (may be nullptr).
 * @param[in] ctx        Opaque pointer passed to @p afterSuccess (e.g., `this`).
 *
 * @throws SocketException
 *         If the socket is invalid, if `sendto()` returns an error,
 *         or if a partial datagram is sent.
 *
 * @warning This function does not perform retries or fragmentation.
 *          For UDP, datagrams larger than the network MTU may be dropped.
 * @warning Intended for internal use — use higher-level `writeTo()` or `write(DatagramPacket&)`
 *          APIs instead in application code.
 *
 * @see sendExact() For sending to a connected peer without specifying an address.
 * @see writeTo() For type-safe per-call destination sends.
 */
void sendExactTo(SOCKET fd, const void* data, std::size_t size, const sockaddr* addr, socklen_t addrLen,
                 void (*afterSuccess)(void* ctx), void* ctx);

/**
 * @brief Attempts to close a socket descriptor without throwing exceptions.
 * @ingroup internal
 *
 * This helper performs a best-effort close of the given socket descriptor.
 * It is specifically intended for use in destructors and cleanup routines
 * where exception safety is critical and socket closure failures must not
 * propagate.
 *
 * If the descriptor is already invalid (`INVALID_SOCKET`), the function
 * returns immediately with `true`.
 *
 * On a valid descriptor, the underlying platform‐specific `CloseSocket()`
 * is called. If it succeeds, the function returns `true`. If it fails,
 * the error is silently ignored (per project close policy) and `false`
 * is returned. Optional logging or diagnostics may be added at the marked
 * location in the implementation if desired.
 *
 * @param[in] fd The platform‐specific socket descriptor to close. If set
 *               to `INVALID_SOCKET`, no action is taken.
 * @return `true` if the socket was already invalid or successfully closed;
 *         `false` if closing failed (error is ignored).
 *
 * @note This function never throws. For public `close()` methods where
 *       errors must be reported, use a throwing variant such as
 *       `closeOrThrow()` instead.
 *
 * @code
 * // Example: safely closing in a destructor
 * ~SocketWrapper() noexcept {
 *     tryCloseNoexcept(_sockFd);
 *     _sockFd = INVALID_SOCKET;
 * }
 * @endcode
 */
inline bool tryCloseNoexcept(const SOCKET fd) noexcept
{
    if (fd == INVALID_SOCKET)
        return true;
    if (CloseSocket(fd) != 0)
    {
        // Optional: hook for logging, left silent per policy
        return false;
    }
    return true;
}

/**
 * @brief Closes a socket descriptor and throws on failure.
 * @ingroup internal
 *
 * This helper attempts to close the specified socket descriptor using the
 * platform‐specific `CloseSocket()` function. If the descriptor is invalid
 * (`INVALID_SOCKET`), the function returns immediately without error.
 *
 * If `CloseSocket()` fails, the function retrieves the platform error code
 * via `GetSocketError()` and throws a `SocketException` containing both
 * the numeric error and a descriptive message produced by
 * `SocketErrorMessage(error)`.
 *
 * This function is intended for use in public `close()` methods or other
 * contexts where socket closure errors must be explicitly reported to the
 * caller.
 *
 * @param[in] fd The platform‐specific socket descriptor to close.
 *               If set to `INVALID_SOCKET`, no action is taken.
 *
 * @throws SocketException If closing the socket fails, with the platform
 *         error code and message.
 *
 * @note This function may throw. For destructors or cleanup routines
 *       where exceptions are not allowed, use `tryCloseNoexcept()` instead.
 *
 * @code
 * // Example: throwing close in a public API
 * void MySocket::close() {
 *     closeOrThrow(_sockFd);
 *     _sockFd = INVALID_SOCKET;
 * }
 * @endcode
 */
inline void closeOrThrow(const SOCKET fd)
{
    if (fd == INVALID_SOCKET)
        return;
    if (CloseSocket(fd) != 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }
}

/**
 * @brief Query the exact size of the next UDP datagram, if the platform can provide it.
 *
 * @details
 * On many platforms, @c FIONREAD returns the size of the next pending datagram in bytes.
 * On POSIX systems where @c FIONREAD is not reliable, we fall back to a zero-length
 * @c recvfrom() with @c MSG_PEEK|MSG_TRUNC, which returns the exact datagram size without
 * consuming it. If neither path yields a size, returns 0 and the caller must choose a
 * fallback (e.g., internal buffer size or a default).
 *
 * This function is stateless and does not modify socket state.
 *
 * @param[in] fd Socket descriptor/handle.
 * @return Size in bytes of the next UDP datagram, or 0 if unknown.
 *
 * @par Example
 * @code
 * const std::size_t n = jsocketpp::internal::nextDatagramSize(sockFd);
 * if (n == 0) {
 *     // fall back to buffer/default
 * }
 * @endcode
 *
 * @since 1.0
 */
inline std::size_t nextDatagramSize(SOCKET fd) noexcept
{
#if defined(_WIN32)
    u_long pending = 0;
    return (ioctlsocket(fd, FIONREAD, &pending) == 0 && pending > 0) ? static_cast<std::size_t>(pending) : 0;
#else
    int pending = 0;
    if (ioctl(fd, FIONREAD, &pending) == 0 && pending > 0)
        return static_cast<std::size_t>(pending);

    // POSIX: MSG_PEEK|MSG_TRUNC returns exact datagram length without consuming it.
    sockaddr_storage tmp{};
    socklen_t tmpLen = static_cast<socklen_t>(sizeof(tmp));
    const ssize_t peekN = ::recvfrom(fd, nullptr, 0, MSG_PEEK | MSG_TRUNC, reinterpret_cast<sockaddr*>(&tmp), &tmpLen);
    return (peekN > 0) ? static_cast<std::size_t>(peekN) : 0;
#endif
}

/**
 * @brief Receive into a caller-provided span, routing to @c recv() or @c recvfrom() as needed.
 *
 * @details
 * If @p src and @p srcLen are non-null, this calls @c recvfrom() and fills the sender address;
 * otherwise, it calls @c recv(). The function performs the platform-specific cast for the
 * length parameter on Windows.
 *
 * @param[in]  fd      Socket descriptor/handle.
 * @param[in,out] dst  Destination buffer span. The function attempts to fill up to @c dst.size() bytes.
 * @param[in]  flags   Receive flags (e.g., 0, @c MSG_PEEK).
 * @param[out] src     Optional sender address storage for unconnected sockets; may be null.
 * @param[in,out] srcLen Length in/out of @p src; may be null if @p src is null.
 *
 * @retval >=0 The number of bytes received (may be 0 for zero-length UDP datagrams).
 * @retval SOCKET_ERROR On error; call GetSocketError() for details.
 *
 * @note This function does not throw; it mirrors the system call semantics.
 * @since 1.0
 */
inline ssize_t recvInto(const SOCKET fd, std::span<std::byte> dst, const int flags, sockaddr_storage* src,
                        socklen_t* srcLen)
{
    if (src && srcLen)
    {
        return ::recvfrom(fd, reinterpret_cast<char*>(dst.data()),
#if defined(_WIN32)
                          static_cast<int>(dst.size()),
#else
                          dst.size(),
#endif
                          flags, reinterpret_cast<sockaddr*>(src), srcLen);
    }

    return ::recv(fd, reinterpret_cast<char*>(dst.data()),
#if defined(_WIN32)
                  static_cast<int>(dst.size()),
#else
                  dst.size(),
#endif
                  flags);
}

/**
 * @brief Throw a SocketException for the last socket error, with optional source-location context.
 *
 * @details
 * Captures the thread-local last socket error via `GetSocketError()` and throws using the
 * project’s canonical two-argument pattern:
 *
 * @code
 * throw SocketException(err, SocketErrorMessage(err));
 * @endcode
 *
 * When internal error-context is enabled (see build-time switch `JSOCKETPP_INCLUDE_ERROR_CONTEXT`),
 * the human-readable message is augmented with call-site information derived from
 * `std::source_location` (i.e., `file:line function`). This function is marked `[[noreturn]]`
 * and always throws.
 *
 * @param[in] loc  Call-site source information. Defaults to `std::source_location::current()`.
 *
 * @throws SocketException Always thrown. The first argument is the integer error code returned
 *         by `GetSocketError()`. The second argument is the human-readable message produced by
 *         `SocketErrorMessage(err)`, optionally suffixed with the source location when enabled.
 *
 * @note Requires C++20 (`<source_location>`).
 * @note Thread-safe: reads a thread-local error code (`GetSocketError()` should return the error
 *       for the calling thread).
 *
 * @par Example
 * @code
 * // Typical usage in an error branch:
 * if (n == SOCKET_ERROR) {
 *     jsocketpp::internal::throwLastSockError(); // source location captured automatically
 * }
 * @endcode
 *
 * @see GetSocketError(), SocketErrorMessage(int), SocketException
 * @since 1.0
 */
[[noreturn]] inline void throwLastSockError(const std::source_location& loc = std::source_location::current())
{
    const int err = GetSocketError();
#if JSOCKETPP_INCLUDE_ERROR_CONTEXT
    std::string msg = SocketErrorMessage(err);
    msg.append(" [at ")
        .append(loc.file_name())
        .append(":")
        .append(std::to_string(loc.line()))
        .append(" ")
        .append(loc.function_name())
        .append("]");
    throw SocketException(err, std::move(msg));
#else
    (void) loc;
    throw SocketException(err, SocketErrorMessage(err));
#endif
}

/**
 * @brief Resolve numeric host and port from a socket address.
 * @ingroup core
 *
 * @details
 * Calls `getnameinfo()` with `NI_NUMERICHOST | NI_NUMERICSERV` (and `NI_NUMERICSCOPE` where
 * available) to obtain the sender’s **numeric** IP and service strings without DNS lookups.
 * Parses the service string to a `Port` and validates it fits the type’s range.
 * On failure, throws using the library’s cross-platform `getnameinfo` error pattern.
 *
 * @param[in]  sa Pointer to a valid `sockaddr` (e.g., `sockaddr_in`/`sockaddr_in6`).
 * @param[in]  len Size of the structure pointed to by @p sa.
 * @param[out] host Receives the numeric host string (e.g., `"192.0.2.10"`, `"fe80::1%eth0"`).
 * @param[out] port Receives the numeric service string (e.g., `"8080"`).
 *
 * @throws SocketException
 *         If `getnameinfo()` fails.
 *
 * @note Uses purely numeric resolution to avoid blocking DNS queries.
 * @since 1.0
 */
inline void resolveNumericHostPort(const sockaddr* sa, const socklen_t len, std::string& host, Port& port)
{
    char hostBuf[NI_MAXHOST]{};
    char servBuf[NI_MAXSERV]{};

    int flags = NI_NUMERICHOST | NI_NUMERICSERV;
#ifdef NI_NUMERICSCOPE
    flags |= NI_NUMERICSCOPE; // ensure scope id stays numeric on IPv6 link-local
#endif

    if (const int ret = ::getnameinfo(sa, len, hostBuf, sizeof(hostBuf), servBuf, sizeof(servBuf), flags); ret != 0)
    {
        throw SocketException(ret, SocketErrorMessage(ret, true));
    }

    // service is numeric due to NI_NUMERICSERV; parse without locale.
    const auto raw = std::strtoul(servBuf, nullptr, 10);
    if (raw > static_cast<unsigned long>((std::numeric_limits<Port>::max)()))
        throw SocketException("Port out of range for Port type.");

    host.assign(hostBuf);
    port = static_cast<Port>(raw);
}

} // namespace jsocketpp::internal
