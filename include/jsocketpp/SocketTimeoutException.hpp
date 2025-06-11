#pragma once

#include "SocketException.hpp"

namespace jsocketpp
{

/**
 * @ingroup tcp
 * @brief Exception class for socket operations that time out.
 *
 * This exception is thrown when a socket operation, such as `accept()` or `read()`,
 * exceeds its allotted timeout period without completing.
 *
 * It inherits from `SocketException` and carries the same error code and message format.
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
     * @param errorCode Platform-specific error code (typically 0 for timeouts).
     * @param message Descriptive message for the timeout.
     */
    SocketTimeoutException(const int errorCode, std::string message) : SocketException(errorCode, std::move(message)) {}
};
} // namespace jsocketpp