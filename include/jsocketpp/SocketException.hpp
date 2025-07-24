/**
 * @file SocketException.hpp
 * @brief Exception class for socket-related errors in jsocketpp.
 * @author MangaD
 * @date 2025
 * @version 1.0
 */

#pragma once

#include "common.hpp"

#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>

namespace jsocketpp
{

/**
 * @class SocketException
 * @ingroup exceptions
 * @brief Represents socket-related errors in the jsocketpp library.
 *
 * SocketException is the standard exception type thrown by the jsocketpp socket library
 * whenever a socket operation fails (e.g., connect, bind, send, or receive).
 * It encapsulates both a platform-specific error code (e.g., errno or WSA error)
 * and a descriptive error message.
 *
 * The class also supports optional exception chaining through a stored `std::exception_ptr`,
 * allowing users to propagate and inspect nested causes of socket failures.
 *
 * ### Exception Chaining
 * Nested exceptions can be attached explicitly via constructor or with `std::throw_with_nested()`,
 * enabling structured error propagation and diagnostics across API boundaries.
 *
 * ### Example: Catching and Handling Socket Exceptions
 * @code
 * #include <jsocketpp/SocketException.hpp>
 * #include <jsocketpp/Socket.hpp>
 * using namespace jsocketpp;
 *
 * try {
 *     Socket sock("example.com", 80);
 *     sock.connect();
 *     // ... other operations ...
 * } catch (const SocketException& ex) {
 *     std::cerr << "Socket error (" << ex.getErrorCode() << "): " << ex.what() << std::endl;
 *     try {
 *         std::rethrow_if_nested(ex.getNestedException());
 *     } catch (const std::exception& nested) {
 *         std::cerr << "Caused by: " << nested.what() << std::endl;
 *     }
 * }
 * @endcode
 *
 * @note This class is exception-safe and designed for cross-platform error reporting.
 * @note Nested exceptions are supported but optional.
 */
class SocketException : public std::runtime_error
{
  public:
    /**
     * @brief Constructs a SocketException with a custom error message and no associated error code.
     * @ingroup exceptions
     *
     * This overload is typically used when the error does not correspond to a specific platform error code
     * (e.g., logic errors, precondition failures, etc.), but still warrants raising a socket-specific exception.
     *
     * @param message A human-readable description of the error context.
     *
     * @see SocketException(int, const std::string&)
     * @see getErrorCode()
     */
    explicit SocketException(const std::string& message = "SocketException")
        : std::runtime_error(message), _errorCode(0)
    {
    }

    /**
     * @brief Constructs a SocketException with a platform-specific error code and custom message.
     *
     * This overload is used when a socket-related failure produces a known system error code
     * (such as `errno` on POSIX or `WSAGetLastError()` on Windows), which is stored and included
     * in the final exception message.
     *
     * The formatted error message passed to `std::runtime_error` includes the original message
     * and the error code (e.g., "Connection failed (error code 111)").
     *
     * @param code    Integer error code returned by the operating system.
     * @param message Descriptive error message describing the failure context.
     *
     * @see getErrorCode()
     */
    explicit SocketException(int code, const std::string& message = "SocketException")
        : std::runtime_error(buildErrorMessage(message, code)), _errorCode(code)
    {
    }

    /**
     * @brief Constructs a SocketException with a message and a nested exception.
     *
     * This constructor is used to explicitly chain exceptions by attaching a previously caught exception
     * (via `std::current_exception()` or `std::throw_with_nested()`), preserving the original cause.
     *
     * This enables users to propagate contextual errors while retaining the underlying source of failure.
     * The nested exception can later be retrieved via `getNestedException()` and rethrown with
     * `std::rethrow_if_nested()`.
     *
     * ### Example: Manual nesting
     * @code
     * try {
     *     mayThrow();
     * } catch (...) {
     *     throw SocketException("socket failed", std::current_exception());
     * }
     * @endcode
     *
     * @param message Descriptive message for the higher-level socket failure.
     * @param nested  Exception pointer representing the original cause (typically captured from a catch block).
     *
     * @see getNestedException()
     * @see std::rethrow_if_nested
     */
    SocketException(const std::string& message, std::exception_ptr nested)
        : std::runtime_error(message), _errorCode(0), _nested(nested)
    {
    }

    /**
     * @brief Retrieves the platform-specific error code associated with this exception.
     *
     * The error code is typically captured from system APIs such as `errno` (on POSIX) or
     * `WSAGetLastError()` (on Windows), and represents the low-level cause of the socket failure.
     * This code is preserved separately from the textual message and can be used for diagnostics,
     * logging, or error-specific handling.
     *
     * @return Integer error code reported by the system at the time the exception was constructed.
     *
     * @see SocketException(int, const std::string&)
     */
    [[nodiscard]] int getErrorCode() const noexcept { return _errorCode; }

    /**
     * @brief Retrieves the nested exception captured at construction time, if any.
     *
     * This method returns the `std::exception_ptr` that was explicitly provided to the constructor
     * or captured using `std::current_exception()`. If no nested exception was set, the returned
     * pointer will be null.
     *
     * This facility enables exception chaining: higher-level exceptions (like SocketException)
     * can propagate contextual errors while retaining the original failure for inspection or rethrow.
     *
     * To rethrow the nested exception, use `std::rethrow_if_nested(getNestedException())`.
     *
     * @return An exception pointer referring to the nested exception, or `nullptr` if none was provided.
     *
     * @see std::current_exception()
     * @see std::rethrow_if_nested()
     * @see SocketException(const std::string&, std::exception_ptr)
     */
    [[nodiscard]] std::exception_ptr getNestedException() const noexcept { return _nested; }

    /**
     * @brief Destroys the SocketException.
     *
     * This destructor is explicitly marked `override` to ensure correct polymorphic behavior
     * when SocketException is used through a base class pointer (e.g., `std::exception*`).
     * It is declared `default` because the base class destructors (`std::runtime_error`)
     * and all members (`int`, `std::exception_ptr`) are trivially destructible.
     *
     * No resources are dynamically allocated by this class.
     */
    ~SocketException() override = default;

  private:
    int _errorCode;             ///< Platform-specific error code (e.g., errno, WSA error).
    std::exception_ptr _nested; ///< Captured nested exception for chaining, if any.

    /**
     * @brief Builds a formatted error message combining a textual description and an error code.
     *
     * This utility function is used internally to generate consistent exception messages
     * that include both the high-level context and the underlying system error code.
     *
     * The resulting string takes the form: `"message (error code 123)"`.
     *
     * @param msg   Descriptive error message explaining the context of the failure.
     * @param code  Platform-specific error code to append (e.g., errno, WSA error).
     * @return      Formatted string combining the message and error code.
     *
     * @note This method is used by constructors of SocketException and is not intended for external use.
     */
    static std::string buildErrorMessage(const std::string& msg, const int code)
    {
        std::ostringstream oss;
        oss << msg << " (error code " << code << ")";
        return oss.str();
    }
};

} // namespace jsocketpp
