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
#include <arpa/inet.h>   //for inet_ntoa
#include <cerrno>        //errno
#include <cstring>       //strerror
#include <fcntl.h>       //fcntl
#include <ifaddrs.h>     //getifaddrs
#include <net/if.h>      //if_nametoindex
#include <netdb.h>       //for struct addrinfo
#include <netinet/in.h>  //for sockaddr_in and sockaddr_in6
#include <netinet/tcp.h> //TCP_NODELAY, TCP_MAXSEG
#include <poll.h>        //poll
#include <sys/ioctl.h>   //ioctl
#include <sys/select.h>  //select
#include <sys/socket.h>  //socket
#include <sys/time.h>    //timeval
#include <sys/types.h>   //socket
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

} // namespace jsocketpp
