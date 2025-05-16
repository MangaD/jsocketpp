#include "libsocket/common.hpp"

using namespace libsocket;

std::string libsocket::SocketErrorMessage(int error, [[maybe_unused]] bool gaiStrerror)
{
#ifdef _WIN32
    if (error == 0)
        return {};

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
        return std::string(gai_strerror(error));
    }
    return std::strerror(error);
#endif
}

std::string libsocket::SocketErrorMessageWrap(const int error, [[maybe_unused]] const bool gaiStrerror)
{
    std::string errString{};
    try
    {
        errString = SocketErrorMessage(error, gaiStrerror);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return errString;
}

#ifdef _WIN32
/**
 * Redefine because not available on Windows XP
 * http://stackoverflow.com/questions/13731243/what-is-the-windows-xp-equivalent-of-inet-pton-or-inetpton
 */
const char* libsocket::inet_ntop_aux(const int af, const void* src, char* dst, socklen_t size)
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

std::vector<std::string> libsocket::getHostAddr()
{
    std::vector<std::string> ips;

#ifdef _WIN32
    /* Declare and initialize variables */
    constexpr int MAX_TRIES = 3;
    DWORD dwRetVal = 0;
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
    ULONG outBufLen = 15000; // Allocate a 15 KB buffer to start with.
    ULONG Iterations = 0;
    char buff[100];

    do
    {
        constexpr ULONG family = AF_UNSPEC;
        constexpr ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
        pAddresses = static_cast<IP_ADAPTER_ADDRESSES*>(HeapAlloc(GetProcessHeap(), 0, (outBufLen)));
        if (pAddresses == nullptr)
        {
            std::cerr << "Memory allocation failed for IP_ADAPTER_ADDRESSES struct." << std::endl;
            return ips;
        }
        dwRetVal = GetAdaptersAddresses(family, flags, nullptr, pAddresses, &outBufLen);
        if (dwRetVal == ERROR_BUFFER_OVERFLOW)
        {
            HeapFree(GetProcessHeap(), 0, (pAddresses));
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
        unsigned int i = 0;
        PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = nullptr;
        PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = nullptr;
        PIP_ADAPTER_UNICAST_ADDRESS pUnicast = nullptr;
        PIP_ADAPTER_ADDRESSES pCurrAddresses = nullptr;
        // If successful, output some information from the data we received
        pCurrAddresses = pAddresses;
        while (pCurrAddresses)
        {
            constexpr socklen_t buffLen = 100; // DWORD
            pUnicast = pCurrAddresses->FirstUnicastAddress;
            if (pUnicast)
            {
                for (i = 0; pUnicast != nullptr; i++)
                {
                    if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
                    {
                        const auto* sa_in = reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr);
                        ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv4 Address " +
                                         inet_ntop_aux(AF_INET, &(sa_in->sin_addr), buff, buffLen));
                    }
                    else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6)
                    {
                        auto* sa_in6 = reinterpret_cast<sockaddr_in6*>(pUnicast->Address.lpSockaddr);
                        ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv6 Address " +
                                         inet_ntop_aux(AF_INET6, &(sa_in6->sin6_addr), buff, buffLen));
                    }
                    // else{printf("\tUNSPEC");}
                    pUnicast = pUnicast->Next;
                }
            }

            pAnycast = pCurrAddresses->FirstAnycastAddress;
            if (pAnycast)
            {
                for (i = 0; pAnycast != nullptr; i++)
                {
                    if (pAnycast->Address.lpSockaddr->sa_family == AF_INET)
                    {
                        const auto* sa_in = reinterpret_cast<sockaddr_in*>(pAnycast->Address.lpSockaddr);
                        ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv4 Address " +
                                         inet_ntop_aux(AF_INET, &(sa_in->sin_addr), buff, buffLen));
                    }
                    else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6)
                    {
                        const auto* sa_in6 = reinterpret_cast<sockaddr_in6*>(pAnycast->Address.lpSockaddr);
                        ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv6 Address " +
                                         inet_ntop_aux(AF_INET6, &(sa_in6->sin6_addr), buff, buffLen));
                    }
                    // else{printf("\tUNSPEC");}
                    pAnycast = pAnycast->Next;
                }
            }

            pMulticast = pCurrAddresses->FirstMulticastAddress;
            if (pMulticast)
            {
                for (i = 0; pMulticast != nullptr; i++)
                {
                    if (pMulticast->Address.lpSockaddr->sa_family == AF_INET)
                    {
                        const auto* sa_in = reinterpret_cast<sockaddr_in*>(pMulticast->Address.lpSockaddr);
                        ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv4 Address " +
                                         inet_ntop_aux(AF_INET, &(sa_in->sin_addr), buff, buffLen));
                    }
                    else if (pMulticast->Address.lpSockaddr->sa_family == AF_INET6)
                    {
                        const auto* sa_in6 = reinterpret_cast<sockaddr_in6*>(pMulticast->Address.lpSockaddr);
                        ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv6 Address " +
                                         inet_ntop_aux(AF_INET6, &(sa_in6->sin6_addr), buff, buffLen));
                    }
                    // else{printf("\tUNSPEC");}
                    pMulticast = pMulticast->Next;
                }
            }

            pCurrAddresses = pCurrAddresses->Next;
        }
    }
    else
    {
        if (pAddresses)
        {
            HeapFree(GetProcessHeap(), 0, (pAddresses));
        }
        throw SocketException(static_cast<int>(dwRetVal), SocketErrorMessage(static_cast<int>(dwRetVal)));
    }

    if (pAddresses)
    {
        HeapFree(GetProcessHeap(), 0, (pAddresses));
    }
#else
    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa = nullptr;
    void* tmpAddrPtr = nullptr;

    if (getifaddrs(&ifAddrStruct))
    {
        throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }

    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
        {
            continue;
        }
        DIAGNOSTIC_PUSH()
        DIAGNOSTIC_IGNORE("-Wcast-align")
        if (ifa->ifa_addr->sa_family == AF_INET)
        { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr = &(reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr))->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            ips.emplace_back(std::string(ifa->ifa_name) + " IPv4 Address " + addressBuffer);
        }
        else if (ifa->ifa_addr->sa_family == AF_INET6)
        { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr = &(reinterpret_cast<struct sockaddr_in6*>(ifa->ifa_addr))->sin6_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            ips.emplace_back(std::string(ifa->ifa_name) + " IPv6 Address " + addressBuffer);
        }
        DIAGNOSTIC_POP()
    }
    if (ifAddrStruct != nullptr)
        freeifaddrs(ifAddrStruct);
#endif

    return ips;
}
