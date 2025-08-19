/**
 * @file SocketTimeoutException.hpp
 * @brief Exception class for socket operation timeouts in jsocketpp.
 */

#pragma once

#include "common.hpp"
#include "SocketException.hpp"

namespace jsocketpp
{

/**
 * @class SocketTimeoutException
 * @ingroup exceptions
 * @brief Exception class for socket operations that time out.
 *
 * This exception is thrown when a socket operation (such as `accept()`, `read()`, or `connect()`) exceeds
 * its allotted timeout period without completing. It provides a platform-appropriate error code and
 * a human-readable message.
 *
 * Inherits from @ref SocketException and uses a default platform-specific error code:
 * - Windows: `WSAETIMEDOUT`
 * - POSIX: `ETIMEDOUT`
 *
 * The default message is generated using @ref SocketErrorMessage for the given timeout error code.
 *
 * ### Example
 * @code
 * try {
 *     Socket client = server.accept(5000); // Wait up to 5 seconds
 * } catch (const SocketTimeoutException& e) {
 *     std::cerr << "Timeout: " << e.what() << std::endl;
 * }
 * @endcode
 */
class SocketTimeoutException final : public SocketException
{
  public:
    /**
     * @brief Construct a new SocketTimeoutException with the specified or default timeout code and message.
     * @param errorCode The platform-specific timeout code (default: JSOCKETPP_TIMEOUT_CODE).
     * @param message Optional error message. If omitted, it will be generated from the error code.
     */
    explicit SocketTimeoutException(const int errorCode = JSOCKETPP_TIMEOUT_CODE, std::string message = "")
        : SocketException(errorCode, message.empty() ? SocketErrorMessage(errorCode) : std::move(message))
    {
    }
};

} // namespace jsocketpp
