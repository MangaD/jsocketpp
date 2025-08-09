/**
 * @file common.hpp
 * @brief Common platform and utility includes for jsocketpp.
 */

#pragma once

#include "SocketException.hpp"

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
 * @brief Returns a human-readable error message for a socket error code.
 *
 * On Windows:
 *   - For Winsock error codes (10000-11999), uses strerror (these include WSAETIMEDOUT).
 *   - For system error codes (from GetLastError()), uses FormatMessageA.
 * On POSIX:
 *   - For getaddrinfo-related errors, uses gai_strerror if gaiStrerror is true.
 *   - Otherwise, uses strerror.
 *
 * @param error The error code (platform-specific).
 * @param gaiStrerror If true, interpret error as a getaddrinfo error code (POSIX only).
 * @return A string describing the error.
 */
std::string SocketErrorMessage(int error, [[maybe_unused]] bool gaiStrerror = false);

/**
 * @brief Returns a human-readable error message for a socket error code, with exception safety.
 *
 * Like SocketErrorMessage, but guarantees not to throw exceptions (e.g., for use in destructors).
 * If an error occurs while generating the message, returns a fallback string (on Windows, uses strerror for Winsock
 * codes).
 *
 * @param error The error code (platform-specific).
 * @param gaiStrerror If true, interpret error as a getaddrinfo error code (POSIX only).
 * @return A string describing the error, or a fallback if an exception occurs.
 */
std::string SocketErrorMessageWrap(int error, [[maybe_unused]] bool gaiStrerror = false);

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
        throw SocketException(
#ifdef _WIN32
            GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true)
#else
            ret, SocketErrorMessageWrap(ret, true)
#endif
        );
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
void sendExactTo(SOCKET fd, const void* data, std::size_t size, const sockaddr* addr, socklen_t addrLen);

} // namespace jsocketpp::internal
