/**
 * @file BufferView.hpp
 * @brief Represents a raw writable memory region for scatter/gather I/O.
 */

#pragma once

#include <cstddef>

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
} // namespace jsocketpp
