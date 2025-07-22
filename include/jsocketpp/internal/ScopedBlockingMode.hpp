/**
 * @file ScopedBlockingMode.hpp
 * @brief RAII helper for temporarily overriding a socket's blocking mode.
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

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

/**
 * @namespace jsocketpp::internal
 * @brief Implementation-only utilities and platform abstractions for jsocketpp.
 *
 * This namespace contains internal helper classes, platform-specific adapters,
 * and low-level abstractions used by the jsocketpp library. These components are
 * not part of the public API and may change at any time without notice.
 *
 * ### Contents
 * - RAII wrappers (e.g., `ScopedBlockingMode`)
 * - Platform-specific buffer conversion tools (e.g., `toIOVec()`, `toWSABUF()`)
 * - Utility functions for socket state, error codes, and descriptor flags
 *
 * ### Usage
 * Developers using jsocketpp should not depend on symbols in this namespace unless
 * they are contributing to the library itself or extending its internals.
 *
 * @warning Not part of the public API. Subject to change or removal in future versions.
 *
 * @ingroup internal
 */
namespace jsocketpp::internal
{

/**
 * @class ScopedBlockingMode
 * @brief RAII helper for temporarily overriding a socket's blocking mode.
 *
 * This class manages a temporary change to a socket's blocking/non-blocking state.
 * On construction, it queries the socket's current mode and applies the requested override.
 * When the object is destroyed (typically at the end of a scope), the original blocking mode
 * is restored automatically, ensuring consistent socket state even in the presence of exceptions.
 *
 * ### Use Case
 * Primarily used in socket operations like `connect()` that require temporarily switching to
 * non-blocking mode to perform `select()`-based timeout handling without permanently altering
 * the socket's mode.
 *
 * ### Platform Behavior
 * - **POSIX:** Uses `fcntl()` with `O_NONBLOCK`
 * - **Windows:** Uses `ioctlsocket()` with `FIONBIO`
 *
 * ### Safety
 * - This class is exception-safe: if an error occurs during the override, the constructor throws.
 * - If the socket is externally modified via `setNonBlocking()` during the lifetime of a
 *   `ScopedBlockingMode`, the restored state may be incorrect. **Do not call `setNonBlocking()`**
 *   on the same socket while a `ScopedBlockingMode` is active.
 *
 * ### Example
 * @code
 * {
 *     jsocketpp::internal::ScopedBlockingMode guard(sockFd, true); // temporarily non-blocking
 *     // Perform connect() or poll()
 * } // original mode restored
 * @endcode
 *
 * @ingroup internal
 *
 * @see setNonBlocking()
 * @see Socket::connect()
 */
class ScopedBlockingMode
{
  public:
    /**
     * @brief Construct a `ScopedBlockingMode` that temporarily overrides the socket's blocking mode.
     *
     * This constructor queries the current blocking state of the specified socket and sets it to the desired
     * temporary mode (`blocking` or `non-blocking`). Upon destruction, the original mode is restored.
     *
     * This is typically used to safely override a socket's mode during a scoped operation (e.g. non-blocking
     * `connect()`), without permanently modifying the socket's configuration.
     *
     * @param sock The native socket descriptor (platform-specific type: `SOCKET` on Windows, `int` on POSIX).
     * @param temporaryNonBlocking If `true`, the socket will be set to non-blocking mode during the scope.
     *                             If `false`, it will be temporarily set to blocking mode.
     *
     * @throws std::runtime_error if querying or setting the socket mode fails.
     *
     * @note The constructor reads the current mode and only applies a change if necessary.
     * @note If `setNonBlocking()` is called on the same socket while the object is alive,
     *       the final restored state may be incorrect.
     *
     * @see ~ScopedBlockingMode()
     * @see setNonBlocking()
     * @ingroup internal
     */
    ScopedBlockingMode(const SOCKET sock, bool temporaryNonBlocking) : _sock(sock)
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

    /**
     * @brief Restore the socket's original blocking mode on destruction.
     *
     * This destructor attempts to revert the socket descriptor to the blocking mode
     * it had at the time of this object's construction. This ensures that any temporary
     * change to the blocking state (via the constructor) is automatically undone,
     * preserving consistent socket behavior after the scope ends.
     *
     * This restoration is performed even if an exception was thrown inside the guarded
     * scope, making this class suitable for safe use in exception-prone paths such as
     * `connect()` with timeout logic.
     *
     * @note Errors during restoration are silently ignored. This is intentional to
     *       maintain noexcept destructor semantics and prevent exceptions from escaping
     *       destructors.
     *
     * @warning Do not call `setNonBlocking()` on the same socket while a `ScopedBlockingMode`
     *          is active, as this will interfere with the mode restoration logic.
     *
     * @see ScopedBlockingMode()
     * @ingroup internal
     */
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
    SOCKET _sock;        ///< Socket descriptor being managed for temporary mode override.
    bool _wasBlocking{}; ///< Whether the socket was originally in blocking mode.
};

} // namespace jsocketpp::internal
