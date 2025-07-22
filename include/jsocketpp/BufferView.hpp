/**
 * @file BufferView.hpp
 * @brief Represents a raw writable memory region for scatter/gather I/O.
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

#pragma once

#include "common.hpp"

#include <span>

namespace jsocketpp
{
/**
 * @brief Represents a raw writable memory region for scatter/gather I/O.
 * @ingroup core
 *
 * `BufferView` is used to describe non-contiguous writable memory ranges for
 * use with vectorized socket operations (e.g., `readv`, `writev`, `readvAll`).
 *
 * It allows efficient I/O without copying or concatenating buffers, enabling
 * zero-copy messaging and protocol framing.
 *
 * ### Fields
 * - `data`: Pointer to writable memory
 * - `size`: Size in bytes of the memory region
 *
 * ### Example Usage
 * @code{.cpp}
 * std::array<std::byte, 4> header;
 * std::array<std::byte, 128> payload;
 * std::array<jsocketpp::BufferView, 2> views = {
 *     jsocketpp::BufferView{header.data(), header.size()},
 *     jsocketpp::BufferView{payload.data(), payload.size()}
 * };
 * socket.readvAll(views);
 * @endcode
 *
 * @see Socket::readv()
 * @see Socket::writevFrom()
 */
struct BufferView
{
    void* data{};       ///< Pointer to the writable memory region
    std::size_t size{}; ///< Size in bytes of the writable region
};

namespace internal
{
#ifdef _WIN32

/**
 * @brief Convert a raw array of BufferView elements into a WSABUF array for use with Windows socket APIs.
 *
 * This utility function transforms a contiguous C-style array of `BufferView` structures into
 * a `std::vector<WSABUF>`, suitable for use with Windows socket functions such as `WSASend()` and `WSARecv()`.
 * Each `WSABUF` struct will point to the same memory region described by its corresponding `BufferView`.
 *
 * @param[in] buffers Pointer to a contiguous array of `BufferView` structures.
 * @param[in] count The number of elements in the `buffers` array.
 * @return A `std::vector<WSABUF>` with one entry per buffer, preserving memory addresses and sizes.
 *
 * @note This function performs shallow conversion—no memory is copied.
 * @note This function is only available on Windows (`_WIN32` defined).
 *
 * @see BufferView
 * @see WSABUF
 * @see WSASend()
 * @see WSARecv()
 *
 * @ingroup internal
 */
[[nodiscard]] inline std::vector<WSABUF> toWSABUF(const BufferView* buffers, const std::size_t count)
{
    std::vector<WSABUF> vec(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        vec[i].len = static_cast<ULONG>(buffers[i].size);
        vec[i].buf = static_cast<CHAR*>(buffers[i].data);
    }
    return vec;
}

/**
 * @brief Convert a span of BufferView elements into a WSABUF array (Windows).
 *
 * This overload provides a convenient interface for converting a `std::span<const BufferView>`
 * into a `std::vector<WSABUF>` for use with Windows socket APIs such as `WSASend()` and `WSARecv()`.
 *
 * @param[in] buffers A `std::span` containing one or more `BufferView` elements.
 * @return A `std::vector<WSABUF>` that references the same memory described by each `BufferView`.
 *
 * @note This function performs shallow conversion—no memory is copied.
 * @note This function is only available on Windows (`_WIN32` defined).
 *
 * @see BufferView
 * @see WSABUF
 * @see toWSABUF(const BufferView*, std::size_t)
 *
 * @ingroup internal
 */
[[nodiscard]] inline std::vector<WSABUF> toWSABUF(const std::span<const BufferView> buffers)
{
    return toWSABUF(buffers.data(), buffers.size());
}

#else

/**
 * @brief Convert a raw array of BufferView elements into an iovec array for POSIX `readv`/`writev`.
 *
 * This function converts a contiguous C-style array of `BufferView` entries into a
 * `std::vector<iovec>`, which can be passed directly to POSIX I/O functions like `readv()` and `writev()`.
 * Each `iovec` will reflect the same memory range described by the corresponding `BufferView`.
 *
 * @param[in] buffers Pointer to a contiguous array of `BufferView` elements.
 * @param[in] count The number of elements in the input array.
 * @return A `std::vector<iovec>` referencing the same memory regions.
 *
 * @note This function performs shallow conversion—no memory is copied.
 * @note Only available on non-Windows platforms (i.e., when `_WIN32` is not defined).
 *
 * @see BufferView
 * @see iovec
 * @see readv()
 * @see writev()
 *
 * @ingroup internal
 */
[[nodiscard]] inline std::vector<iovec> toIOVec(const BufferView* buffers, const std::size_t count)
{
    std::vector<iovec> vec(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        vec[i].iov_base = buffers[i].data;
        vec[i].iov_len = buffers[i].size;
    }
    return vec;
}

/**
 * @brief Convert a span of BufferView elements into an iovec array for POSIX vectorized I/O.
 *
 * This overload transforms a `std::span<const BufferView>` into a `std::vector<iovec>`,
 * which is suitable for use with POSIX APIs such as `readv()` and `writev()`.
 *
 * @param[in] buffers A span of `BufferView` elements.
 * @return A `std::vector<iovec>` referencing the same memory described by the span.
 *
 * @note This function performs shallow conversion—no memory is copied.
 * @note Only available on non-Windows platforms (i.e., when `_WIN32` is not defined).
 *
 * @see BufferView
 * @see iovec
 * @see toIOVec(const BufferView*, std::size_t)
 * @see readv()
 * @see writev()
 *
 * @ingroup internal
 */
[[nodiscard]] inline std::vector<iovec> toIOVec(const std::span<const BufferView> buffers)
{
    return toIOVec(buffers.data(), buffers.size());
}

#endif
} // namespace internal

} // namespace jsocketpp
