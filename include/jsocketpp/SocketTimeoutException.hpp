#pragma once

#include "SocketException.hpp"
#include "common.hpp"

namespace jsocketpp
{

/**
 * @ingroup tcp
 * @brief Exception class for socket operations that time out.
 *
 * This exception is thrown when a socket operation (such as `accept()`, `read()`, or `connect()`) exceeds
 * its allotted timeout period without completing. It provides a clear, platform-appropriate error code and message.
 *
 * Inherits from @ref SocketException and carries the same error code and message format.
 *
 * On Windows, the default error code is WSAETIMEDOUT; on POSIX systems, it is ETIMEDOUT.
 * The default message is generated using @ref SocketErrorMessageWrap for the timeout code.
 *
 * ### Example:
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
     * @brief Construct a new SocketTimeoutException.
     *
     * @param errorCode Platform-specific error code for timeouts (default: JSOCKETPP_TIMEOUT_CODE).
     *        - On Windows: WSAETIMEDOUT
     *        - On POSIX: ETIMEDOUT
     * @param message Descriptive message for the timeout (default: result of SocketErrorMessageWrap for the timeout
     * code).
     *
     * This constructor allows you to override the error code and message, but in most cases the defaults are correct.
     */
    explicit SocketTimeoutException(const int errorCode = JSOCKETPP_TIMEOUT_CODE,
                                    std::string message = SocketErrorMessageWrap(JSOCKETPP_TIMEOUT_CODE))
        : SocketException(errorCode, std::move(message))
    {
    }
};
} // namespace jsocketpp