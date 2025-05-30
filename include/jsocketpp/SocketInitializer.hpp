#pragma once

#include "SocketException.h"
#include "common.hpp"

namespace jsocketpp
{
/**
 * @brief Helper class to initialize and cleanup sockets (RAII).
 *
 * On Windows, calls WSAStartup/WSACleanup. On Linux/POSIX, does nothing.
 * Throws SocketException on failure.
 */
class SocketInitializer
{
  public:
    /**
     * @brief Initialize socket system (WSAStartup on Windows).
     * @throws SocketException if initialization fails.
     */
    SocketInitializer()
    {
        if (InitSockets() != 0)
            throw SocketException(GetSocketError(), SocketErrorMessage(GetSocketError()));
    }

    /**
     * @brief Clean up socket system (WSACleanup on Windows).
     * @note Will log any cleanup failures to stderr but not throw.
     */
    ~SocketInitializer() noexcept
    {
        if (CleanupSockets() != 0)
            std::cerr << "Socket cleanup failed: " << SocketErrorMessageWrap(GetSocketError()) << ": "
                      << GetSocketError() << std::endl;
    }

    /**
     * @brief Deleted copy constructor (non-copyable).
     */
    SocketInitializer(const SocketInitializer& rhs) = delete;

    /**
     * @brief Deleted copy assignment operator (non-copyable).
     */
    SocketInitializer& operator=(const SocketInitializer& rhs) = delete;
};

} // namespace jsocketpp