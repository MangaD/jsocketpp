#pragma once

#include "common.hpp"

#include <exception>
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
 *
 * @author MangaD
 * @date 2025
 * @version 1.0
 */
class SocketException : public std::exception
{
  public:
    explicit SocketException(const int code, std::string message = "SocketException")
        : _errorCode(code), _errorMessage(std::move(message))
    {
        _errorMessage += " (" + std::to_string(code) + ")";
    }
    [[nodiscard]] const char* what() const noexcept override { return _errorMessage.c_str(); }
    [[nodiscard]] int getErrorCode() const noexcept { return _errorCode; }
    ~SocketException() override = default;

  private:
    int _errorCode = 0;
    std::string _errorMessage;
};

} // namespace jsocketpp