#include "jsocketpp/common.hpp"

using namespace jsocketpp;

std::string jsocketpp::SocketErrorMessage(int error, [[maybe_unused]] const bool gaiStrerror /* = false */)
{
    // 0 means "no error" in both errno and WSA error spaces.
    if (error == 0)
        return {};

    // Some APIs return negative errno-like values; normalize to positive for lookups.
    if (error < 0)
        error = -error;

#ifdef _WIN32
    // ------------------------- Windows path -------------------------

    // If the caller told us this is an EAI_* code (from getaddrinfo/getnameinfo),
    // first try the dedicated resolver. If it fails or yields an empty/unknown
    // string, we'll fall through to the general fallbacks below.
    if (gaiStrerror)
    {
        // Windows exposes an ANSI variant: gai_strerrorA.
        if (const char* m = ::gai_strerrorA(error); m && *m)
            return {m}; // copy immediately (gai_strerrorA uses a static buffer)
        // Fall through: try generic system mapping next.
    }

    // Prefer FormatMessageA: it understands many system and WSA* codes (10000–11999).
    {
        LPSTR buffer = nullptr;                                  // System-allocated buffer for the message text
        constexpr DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER   // ask system to allocate 'buffer'
                                | FORMAT_MESSAGE_FROM_SYSTEM     // look up system message tables
                                | FORMAT_MESSAGE_IGNORE_INSERTS; // we provide no %1-style inserts
        constexpr DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT); // user/system locale

        // Query the OS for a localized message string.
        const DWORD size = ::FormatMessageA(flags,
                                            nullptr,                          // no external message source
                                            static_cast<DWORD>(error),        // the error code we're describing
                                            lang,                             // language preference
                                            reinterpret_cast<LPSTR>(&buffer), // (OUT) system allocates this buffer
                                            0,                                // minimum chars (0 lets OS choose)
                                            nullptr                           // no inserts
        );

        if (size != 0 && buffer)
        {
            // Copy into std::string before freeing the system buffer.
            std::string msg(buffer, size);
            ::LocalFree(buffer);

            // FormatMessage often appends CR/LF and sometimes a trailing period or spaces.
            while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ' || msg.back() == '.'))
            {
                msg.pop_back();
            }

            if (!msg.empty())
                return msg; // good, human-readable message obtained
            // else: keep falling through to additional fallbacks
        }
        // If FormatMessage failed, continue to fallbacks.
    }

    // Fallback #1: C++ standard library mapping for system errors.
    // This may succeed for a subset of codes depending on the runtime.
    try
    {
        if (std::string m = std::system_category().message(error); !m.empty())
            return m;
    }
    catch (...)
    {
        // Never let exceptions escape from an error-to-string helper.
        // We'll keep trying other fallbacks below.
    }

    // Fallback #2: MSVC's thread-safe strerror_s (writes into caller-provided buffer).
    {
        char buf[256] = {};
        if (::strerror_s(buf, sizeof buf, error) == 0 && buf[0] != '\0')
            return {buf};
    }

    // Ultimate fallback: at least show the numeric code.
    return "Unknown error " + std::to_string(error);

#else
    // -------------------------- POSIX path --------------------------

    // If this is a getaddrinfo/getnameinfo return code, gai_strerror() is the canonical mapper.
    if (gaiStrerror)
    {
        if (const char* m = ::gai_strerror(error); m && *m)
            return {m};
        // Fall through to generic errno-based mapping if this failed.
    }

    // Prefer the standard library's system_category() (generally thread-safe and descriptive).
    try
    {
        std::string m = std::system_category().message(error);
        if (!m.empty())
            return m;
    }
    catch (...)
    {
        // Swallow and continue to strerror() below.
    }

    // Last-ditch POSIX fallback: strerror() (note: may use a static buffer on some libcs).
    if (const char* m = ::strerror(error); m && *m)
        return {m};

    // If everything else fails, return a generic string with the numeric code.
    return "Unknown error " + std::to_string(error);
#endif
}

#ifdef _WIN32
/**
 * Redefine because not available on Windows XP
 * http://stackoverflow.com/questions/13731243/what-is-the-windows-xp-equivalent-of-inet-pton-or-inetpton
 */
const char* jsocketpp::inet_ntop_aux(const int af, const void* src, char* dst, socklen_t size)
{
    sockaddr_storage ss{};

    ZeroMemory(&ss, sizeof(ss));
    ss.ss_family = static_cast<short>(af);

    switch (af)
    {
        case AF_INET:
            reinterpret_cast<sockaddr_in*>(&ss)->sin_addr = *(static_cast<const in_addr*>(src));
            break;
        case AF_INET6:
            reinterpret_cast<sockaddr_in6*>(&ss)->sin6_addr = *(static_cast<const in6_addr*>(src));
            break;
        default:
            return nullptr;
    }

        // Handle the case where we need wide characters on 64-bit Windows
#ifdef _WIN64
    wchar_t w_dst[INET6_ADDRSTRLEN]; // Wide character buffer for 64-bit Windows
    unsigned long w_s = sizeof(w_dst) / sizeof(wchar_t);

    if (WSAAddressToStringW(reinterpret_cast<sockaddr*>(&ss), sizeof(ss), nullptr, w_dst, &w_s) == 0)
    {
        // Convert wide char to char* if needed, assuming the buffer size is enough
        wcstombs(dst, w_dst, size); // Convert wide string to narrow string (char*)
        return dst;
    }
#else
    unsigned long s = size; // Size for WSAAddressToStringA

    // For 32-bit Windows and other platforms, use char* directly
    if (WSAAddressToStringA(reinterpret_cast<sockaddr*>(&ss), sizeof(ss), nullptr, dst, &s) == 0)
    {
        return dst;
    }
#endif

    return nullptr; // If WSAAddressToString fails
}
#endif

std::vector<std::string> jsocketpp::getHostAddr()
{
    std::vector<std::string> ips;

#ifdef _WIN32
    constexpr int MAX_TRIES = 3;
    DWORD dwRetVal = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
    ULONG outBufLen = 15000;
    ULONG Iterations = 0;
    char buff[100];

    do
    {
        constexpr ULONG family = AF_UNSPEC;
        constexpr ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
        pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(HeapAlloc(GetProcessHeap(), 0, outBufLen));
        if (pAddresses == nullptr)
        {
            std::cerr << "Memory allocation failed for IP_ADAPTER_ADDRESSES struct." << std::endl;
            return ips;
        }
        dwRetVal = GetAdaptersAddresses(family, flags, nullptr, pAddresses, &outBufLen);
        if (dwRetVal == ERROR_BUFFER_OVERFLOW)
        {
            HeapFree(GetProcessHeap(), 0, pAddresses);
            pAddresses = nullptr;
        }
        else
        {
            break;
        }
        Iterations++;
    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

    if (dwRetVal == NO_ERROR)
    {
        for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr; pCurr = pCurr->Next)
        {
            constexpr socklen_t buffLen = 100;
            // Unicast
            for (const auto* p = pCurr->FirstUnicastAddress; p; p = p->Next)
            {
                if (p->Address.lpSockaddr->sa_family == AF_INET)
                {
                    const auto* sa = reinterpret_cast<sockaddr_in*>(p->Address.lpSockaddr);
                    ips.emplace_back(std::string(pCurr->AdapterName) + " IPv4 Address " +
                                     inet_ntop_aux(AF_INET, &sa->sin_addr, buff, buffLen));
                }
                else if (p->Address.lpSockaddr->sa_family == AF_INET6)
                {
                    const auto* sa = reinterpret_cast<sockaddr_in6*>(p->Address.lpSockaddr);
                    ips.emplace_back(std::string(pCurr->AdapterName) + " IPv6 Address " +
                                     inet_ntop_aux(AF_INET6, &sa->sin6_addr, buff, buffLen));
                }
            }

            // Anycast
            for (const auto* p = pCurr->FirstAnycastAddress; p; p = p->Next)
            {
                if (p->Address.lpSockaddr->sa_family == AF_INET)
                {
                    const auto* sa = reinterpret_cast<sockaddr_in*>(p->Address.lpSockaddr);
                    ips.emplace_back(std::string(pCurr->AdapterName) + " IPv4 Address " +
                                     inet_ntop_aux(AF_INET, &sa->sin_addr, buff, buffLen));
                }
                else if (p->Address.lpSockaddr->sa_family == AF_INET6)
                {
                    const auto* sa = reinterpret_cast<sockaddr_in6*>(p->Address.lpSockaddr);
                    ips.emplace_back(std::string(pCurr->AdapterName) + " IPv6 Address " +
                                     inet_ntop_aux(AF_INET6, &sa->sin6_addr, buff, buffLen));
                }
            }

            // Multicast
            for (const auto* p = pCurr->FirstMulticastAddress; p; p = p->Next)
            {
                if (p->Address.lpSockaddr->sa_family == AF_INET)
                {
                    const auto* sa = reinterpret_cast<sockaddr_in*>(p->Address.lpSockaddr);
                    ips.emplace_back(std::string(pCurr->AdapterName) + " IPv4 Address " +
                                     inet_ntop_aux(AF_INET, &sa->sin_addr, buff, buffLen));
                }
                else if (p->Address.lpSockaddr->sa_family == AF_INET6)
                {
                    const auto* sa = reinterpret_cast<sockaddr_in6*>(p->Address.lpSockaddr);
                    ips.emplace_back(std::string(pCurr->AdapterName) + " IPv6 Address " +
                                     inet_ntop_aux(AF_INET6, &sa->sin6_addr, buff, buffLen));
                }
            }
        }
    }
    else
    {
        if (pAddresses)
        {
            HeapFree(GetProcessHeap(), 0, pAddresses);
        }
        throw SocketException(static_cast<int>(dwRetVal), SocketErrorMessage(static_cast<int>(dwRetVal)));
    }

    if (pAddresses)
    {
        HeapFree(GetProcessHeap(), 0, pAddresses);
    }
#else
    ifaddrs* ifAddrStruct = nullptr;
    const ifaddrs* ifa = nullptr;
    const void* tmpAddrPtr = nullptr;

    if (getifaddrs(&ifAddrStruct))
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
            continue;

        DIAGNOSTIC_PUSH()
        DIAGNOSTIC_IGNORE("-Wcast-align")
        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            tmpAddrPtr = &(reinterpret_cast<sockaddr_in*>(ifa->ifa_addr))->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            ips.emplace_back(std::string(ifa->ifa_name) + " IPv4 Address " + addressBuffer);
        }
        else if (ifa->ifa_addr->sa_family == AF_INET6)
        {
            tmpAddrPtr = &(reinterpret_cast<sockaddr_in6*>(ifa->ifa_addr))->sin6_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            ips.emplace_back(std::string(ifa->ifa_name) + " IPv6 Address " + addressBuffer);
        }
        DIAGNOSTIC_POP()
    }

    if (ifAddrStruct)
        freeifaddrs(ifAddrStruct);
#endif

    return ips;
}

std::string jsocketpp::ipFromSockaddr(const sockaddr* addr, const bool convertIPv4Mapped)
{
    char buf[INET6_ADDRSTRLEN] = {};

    if (addr->sa_family == AF_INET)
    {
        const auto* sa = reinterpret_cast<const sockaddr_in*>(addr);
        if (!inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf)))
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }
    }
    else if (addr->sa_family == AF_INET6)
    {
        const auto* sa6 = reinterpret_cast<const sockaddr_in6*>(addr);

        if (convertIPv4Mapped && IN6_IS_ADDR_V4MAPPED(&sa6->sin6_addr))
        {
            const uint8_t* b = &sa6->sin6_addr.s6_addr[12];
            return std::to_string(b[0]) + '.' + std::to_string(b[1]) + '.' + std::to_string(b[2]) + '.' +
                   std::to_string(b[3]);
        }

        if (!inet_ntop(AF_INET6, &sa6->sin6_addr, buf, sizeof(buf)))
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }
    }
    else
    {
        throw SocketException("Unsupported address family in ipFromSockaddr");
    }

    return {buf};
}

Port jsocketpp::portFromSockaddr(const sockaddr* addr)
{
    switch (addr->sa_family)
    {
        case AF_INET:
            return ntohs(reinterpret_cast<const sockaddr_in*>(addr)->sin_port);

        case AF_INET6:
            return ntohs(reinterpret_cast<const sockaddr_in6*>(addr)->sin6_port);

        default:
            throw SocketException("Unsupported address family in portFromSockaddr");
    }
}

std::string internal::getBoundLocalIp(const SOCKET sockFd)
{
    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (getsockname(sockFd, reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
    {
        const int err = GetSocketError();
        throw SocketException(err, SocketErrorMessage(err));
    }

    char ipStr[NI_MAXHOST] = {};
    const int ret = getnameinfo(reinterpret_cast<const sockaddr*>(&addr), addrLen, ipStr, sizeof(ipStr), nullptr, 0,
                                NI_NUMERICHOST);
    if (ret != 0)
    {
        throw SocketException(ret, SocketErrorMessage(ret, true));
    }

    return {ipStr};
}

bool internal::ipAddressesEqual(const std::string& ip1, const std::string& ip2)
{
    in6_addr addr1{}, addr2{};

    auto normalizeToIPv6 = [](const std::string& ip, in6_addr& out) -> bool
    {
        // Try IPv6 first
        if (inet_pton(AF_INET6, ip.c_str(), &out) == 1)
            return true;

        // Try IPv4 → map to IPv4-mapped IPv6 (::ffff:a.b.c.d)
        in_addr ipv4{};
        if (inet_pton(AF_INET, ip.c_str(), &ipv4) != 1)
            return false;

        std::memset(&out, 0, sizeof(out));
        std::memcpy(&out.s6_addr[12], &ipv4, sizeof(ipv4));
        out.s6_addr[10] = 0xff;
        out.s6_addr[11] = 0xff;
        return true;
    };

    const bool ok1 = normalizeToIPv6(ip1, addr1);
    const bool ok2 = normalizeToIPv6(ip2, addr2);

    if (!ok1 || !ok2)
        return false;

    return std::memcmp(&addr1, &addr2, sizeof(in6_addr)) == 0;
}

std::string jsocketpp::addressToString(const sockaddr_storage& addr)
{
    char ip[INET6_ADDRSTRLEN] = {0};
    char port[6] = {0};

    if (addr.ss_family == AF_INET)
    {
        const auto* addr4 = reinterpret_cast<const sockaddr_in*>(&addr);
        if (const auto ret = getnameinfo(reinterpret_cast<const sockaddr*>(addr4), sizeof(sockaddr_in), ip, sizeof(ip),
                                         port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
            ret != 0)
        {
            throw SocketException(ret, SocketErrorMessage(ret, true));
        }
    }
    else if (addr.ss_family == AF_INET6)
    {
        const auto* addr6 = reinterpret_cast<const sockaddr_in6*>(&addr);
        const auto ret = getnameinfo(reinterpret_cast<const sockaddr*>(addr6), sizeof(sockaddr_in6), ip, sizeof(ip),
                                     port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
        if (ret != 0)
        {
            throw SocketException(
#ifdef _WIN32
                GetSocketError(), SocketErrorMessage(GetSocketError(), true));
#else
                ret, SocketErrorMessage(ret, true));
#endif
        }
    }
    else
    {
        return "unknown";
    }
    return std::string(ip) + ":" + port;
}

void jsocketpp::stringToAddress(const std::string& str, sockaddr_storage& addr)
{
    std::memset(&addr, 0, sizeof(addr));
    // Find last ':' (to allow IPv6 addresses with ':')
    const auto pos = str.rfind(':');
    if (pos == std::string::npos)
        throw SocketException("Invalid address format: " + str);

    const std::string host = str.substr(0, pos);
    const Port port = static_cast<Port>(std::stoi(str.substr(pos + 1)));

    const internal::AddrinfoPtr res =
        internal::resolveAddress(host, port, AF_UNSPEC, SOCK_STREAM, 0, AI_NUMERICHOST | AI_NUMERICSERV);

    std::memcpy(&addr, res->ai_addr, res->ai_addrlen);
}

void internal::sendExact(const SOCKET fd, const void* data, std::size_t size)
{
    if (fd == INVALID_SOCKET)
        throw SocketException("sendExact(): invalid socket");

    int flags = 0;
#ifndef _WIN32
    flags = MSG_NOSIGNAL;
#endif

    const int sent = ::send(fd,
#ifdef _WIN32
                            static_cast<const char*>(data), static_cast<int>(size),
#else
                            data, size,
#endif
                            flags);

    if (sent == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (static_cast<std::size_t>(sent) != size)
        throw SocketException("sendExact(): partial datagram was sent.");
}

void internal::sendExactTo(const SOCKET fd, const void* data, std::size_t size, const sockaddr* addr,
                           const socklen_t addrLen)
{
    if (fd == INVALID_SOCKET)
        throw SocketException("sendExactTo(): invalid socket");

    int flags = 0;
#ifndef _WIN32
    flags = MSG_NOSIGNAL;
#endif

    const int sent = ::sendto(fd,
#ifdef _WIN32
                              static_cast<const char*>(data), static_cast<int>(size),
#else
                              data, size,
#endif
                              flags, addr, addrLen);

    if (sent == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    if (static_cast<std::size_t>(sent) != size)
        throw SocketException("sendExactTo(): partial datagram was sent.");
}
