/**
 * @file buffer_traits.hpp
 * @brief Type traits and utilities for detecting and validating buffer types.
 *
 * This header provides compile-time type traits to detect and validate different
 * kinds of buffer types used throughout the jsocketpp library. It defines traits
 * for checking buffer properties such as:
 * - Byte-like element types (char, unsigned char, std::byte, etc.)
 * - Dynamic buffer capabilities (resize, data access, size queries)
 * - Fixed-size buffer characteristics
 *
 * ### Key Concepts
 * - **Byte-like types:** Types that can represent raw bytes (char, unsigned char, std::byte)
 * - **Dynamic buffers:** Containers that support resize(), data(), and size() operations
 * - **Fixed buffers:** Containers with data() and size() but no resize capability
 *
 * ### Primary Traits
 * - is_byte_like<T>: Checks if T is a valid byte-type
 * - is_dynamic_buffer_v<T>: Validates dynamic buffer requirements
 * - is_fixed_buffer_v<T>: Validates fixed-size buffer requirements
 *
 * @author MangaD
 * @date 2025
 * @version 1.0
 *
 * @ingroup detail
 */

#pragma once

#include <cstddef>
#include <type_traits>

/**
 * @namespace jsocketpp::detail
 * @brief Implementation details and type traits for jsocketpp buffer management.
 *
 * This namespace contains internal type traits and utilities used to validate and
 * classify different types of buffers used throughout the jsocketpp library.
 * It provides compile-time checks for buffer requirements and characteristics.
 *
 * ### Key Components
 * - Byte-type validation traits
 * - Buffer capability detection (resize, data access, size)
 * - Dynamic vs fixed buffer classification
 *
 * @note These are implementation details and not part of the public API.
 *
 * @ingroup detail
 */
namespace jsocketpp::detail
{

/**
 * @brief Type trait to detect byte-like types.
 *
 * Determines if a type qualifies as a "byte-like" type that can represent raw bytes.
 * Valid byte-like types include:
 * - char
 * - unsigned char
 * - std::byte
 * - std::uint8_t
 *
 * @tparam T The type to check
 *
 * @ingroup detail
 */
template <typename T>
struct is_byte_like : std::bool_constant<std::is_same_v<T, char> || std::is_same_v<T, unsigned char> ||
                                         std::is_same_v<T, std::byte> || std::is_same_v<T, std::uint8_t>>
{
};

/**
 * @brief Detection helper for resize() member function.
 *
 * Detects if a type T has a resize() member function that takes a size_t parameter.
 *
 * @tparam T The type to check
 * @tparam typename SFINAE enabler parameter
 */
template <typename, typename = void> struct has_resize : std::false_type
{
};
template <typename T>
struct has_resize<T, std::void_t<decltype(std::declval<T&>().resize(std::size_t{}))>> : std::true_type
{
};

/**
 * @brief Detection helper for data() member function.
 *
 * Detects if a type T has a data() member function that provides access to underlying storage.
 *
 * @tparam T The type to check
 * @tparam typename SFINAE enabler parameter
 */
template <typename, typename = void> struct has_data : std::false_type
{
};
template <typename T> struct has_data<T, std::void_t<decltype(std::declval<T&>().data())>> : std::true_type
{
};

/**
 * @brief Detection helper for size() member function.
 *
 * Detects if a type T has a size() member function that returns the container's size.
 *
 * @tparam T The type to check
 * @tparam typename SFINAE enabler parameter
 */
template <typename, typename = void> struct has_size : std::false_type
{
};
template <typename T> struct has_size<T, std::void_t<decltype(std::declval<const T&>().size())>> : std::true_type
{
};

/**
 * @brief Type trait for dynamic buffer types.
 *
 * Checks if type T meets all requirements for a dynamic buffer:
 * - Has resize() member function
 * - Has data() member function
 * - Has size() member function
 * - Has a byte-like value_type
 *
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_dynamic_buffer_v =
    has_resize<T>::value && has_data<T>::value && has_size<T>::value && is_byte_like<typename T::value_type>::value;

/**
 * @brief Type trait for fixed-size buffer types.
 *
 * Checks if type T meets all requirements for a fixed buffer:
 * - Has data() member function
 * - Has size() member function
 * - Does NOT have resize() member function
 * - Has a byte-like value_type
 *
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_fixed_buffer_v =
    has_data<T>::value && has_size<T>::value && !has_resize<T>::value && is_byte_like<typename T::value_type>::value;

} // namespace jsocketpp::detail
