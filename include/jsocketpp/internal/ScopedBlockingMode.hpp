#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <stdexcept>

namespace jsocketpp::internal
{

class ScopedBlockingMode
{
  public:
    ScopedBlockingMode(const SOCKET sock, bool temporaryNonBlocking) : _sock(sock), _wasBlocking(false)
    {
#ifdef _WIN32
        u_long mode = 0;
        if (ioctlsocket(_sock, FIONBIO, &mode) == SOCKET_ERROR)
            throw std::runtime_error("Failed to get socket mode");

        _wasBlocking = (mode == 0);
#else
        const int flags = fcntl(_sock, F_GETFL, 0);
        if (flags == -1)
            throw std::runtime_error("Failed to get socket flags");

        _wasBlocking = !(flags & O_NONBLOCK);
#endif

        if (_wasBlocking == temporaryNonBlocking)
        {
#ifdef _WIN32
            u_long newMode = temporaryNonBlocking ? 1 : 0;
            if (ioctlsocket(_sock, FIONBIO, &newMode) == SOCKET_ERROR)
                throw std::runtime_error("Failed to set socket mode");
#else
            if (const int newFlags = temporaryNonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
                fcntl(_sock, F_SETFL, newFlags) == -1)
                throw std::runtime_error("Failed to set socket mode");
#endif
        }
    }

    ~ScopedBlockingMode()
    {
        try
        {
#ifdef _WIN32
            u_long mode = _wasBlocking ? 0 : 1;
            ioctlsocket(_sock, FIONBIO, &mode);
#else
            if (const int flags = fcntl(_sock, F_GETFL, 0); flags != -1)
            {
                const int newFlags = _wasBlocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
                fcntl(_sock, F_SETFL, newFlags);
            }
#endif
        }
        catch (...)
        {
            // Swallow exceptions in destructor
        }
    }

  private:
    SOCKET _sock;
    bool _wasBlocking;
};

} // namespace jsocketpp::internal
