#pragma once

#include "common.hpp"

#include <exception>
#include <string>

namespace jsocketpp
{

/**
 * @brief Exception class for socket errors.
 *
 * Stores an error code and a descriptive error message. Thrown by all socket operations on error.
 */
class SocketException final : public std::exception
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