#pragma once

#include "common.hpp"

#include <exception>
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
 * whenever a socket-related operation fails (such as connect, bind, send, or receive).
 * It stores both an error code and a human-readable message describing the error.
 *
 * The exception can be caught using a standard C++ try-catch block, making error handling robust and portable.
 *
 * This exception inherits from `std::runtime_error` for compatibility with standard exception handling patterns and
 * tools, and also from `std::nested_exception` to allow users to use `std::throw_with_nested()` for advanced error
 * chaining.
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
 *     std::cerr << "Socket error (" << ex.code() << "): " << ex.what() << std::endl;
 * }
 * @endcode
 *
 * @note This exception stores a copy of the error message and error code. It is not tied to any particular platform
 *       error format—use the code/message for display or logging.
 * @note jsocketpp itself does not throw nested exceptions, but this class enables developers using the library to opt
 * into exception chaining if desired. This class is safe to use without exception chaining. If you don't need nested
 * exceptions, simply construct and throw it normally.
 *
 * @author MangaD
 * @date 2025
 * @version 1.0
 */
class SocketException : public std::runtime_error, public std::nested_exception
{
  public:
    explicit SocketException(const int code, const std::string& message = "SocketException")
        : std::runtime_error(message + " (" + std::to_string(code) + ")"), _errorCode(code)
    {
    }

    [[nodiscard]] int getErrorCode() const noexcept { return _errorCode; }

    ~SocketException() override = default;

  private:
    int _errorCode;
};

} // namespace jsocketpp
