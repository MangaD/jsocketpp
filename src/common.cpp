#include "jsocketpp/common.hpp"

using namespace jsocketpp;

std::string jsocketpp::SocketErrorMessage(int error, [[maybe_unused]] bool gaiStrerror)
{
#ifdef _WIN32
    if (error == 0)
        return {};

    // Winsock error codes are in the range 10000-11999
    if (error >= 10000 && error <= 11999)
    {
        // Use strerror for Winsock error codes (including WSAETIMEDOUT)
        return {strerror(error)};
    }

    LPSTR buffer = nullptr;
    DWORD size; // Use DWORD for FormatMessageA

    // Try to get a human-readable error message for the given error code using FormatMessageA
    if ((size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |              // Allocate buffer for error message
                                   FORMAT_MESSAGE_FROM_SYSTEM |              // Get error message from system
                                   FORMAT_MESSAGE_IGNORE_INSERTS,            // Ignore insert sequences in message
                               nullptr,                                      // No source (system)
                               error,                                        // Error code
                               MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), // Use US English language
                               reinterpret_cast<LPSTR>(&buffer),             // Output buffer
                               0,                                            // Minimum size (let system decide)
                               nullptr)) == 0)                               // No arguments
    {
        // If FormatMessageA fails, print the error and reset the last error code
        std::cerr << "Format message failed: " << GetLastError() << std::endl;
        SetLastError(error);
        return {};
    }
    std::string errString(buffer, size);
    LocalFree(buffer);
    return errString;
#else
    if (gaiStrerror)
    {
        // Use gai_strerror for getaddrinfo/freeaddrinfo/getnameinfo errors
        return {gai_strerror(error)};
    }
    return std::strerror(error);
#endif
}

std::string jsocketpp::SocketErrorMessageWrap(const int error, [[maybe_unused]] const bool gaiStrerror)
{
    std::string errString{};
    try
    {
        errString = SocketErrorMessage(error, gaiStrerror);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
#ifdef _WIN32
        // Fallback: try strerror for Winsock codes
        if (error >= 10000 && error <= 11999)
        {
            errString = std::string(strerror(error));
        }
#endif
    }
    return errString;
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
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
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
            throw SocketException(GetSocketError(), "inet_ntop(AF_INET) failed");
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
            throw SocketException(GetSocketError(), "inet_ntop(AF_INET6) failed");
    }
    else
    {
        throw SocketException(0, "Unsupported address family in ipFromSockaddr");
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
            throw SocketException(0, "Unsupported address family in portFromSockaddr");
    }
}
