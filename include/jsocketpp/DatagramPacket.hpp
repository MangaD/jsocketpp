#pragma once

#include <string>
#include <vector>

namespace jsocketpp
{
/**
 * @brief Represents a UDP datagram packet, including data buffer, remote address, and port.
 *
 * This class encapsulates the payload buffer and addressing information required
 * to send or receive a UDP datagram. Use with DatagramSocket::send() and
 * DatagramSocket::receive() methods.
 */
class DatagramPacket
{
  public:
    /**
     * @brief Data buffer for the packet payload.
     *
     * On sending: Contains the data to transmit.
     * On receiving: Filled with received data (size indicates bytes received).
     */
    std::vector<char> buffer;

    /**
     * @brief Remote address (IPv4/IPv6) for the destination/source.
     * - On send: set to the destination address.
     * - On receive: will be filled with sender's address.
     */
    std::string address;

    /**
     * @brief Remote UDP port for the destination/source.
     * - On send: set to the destination port.
     * - On receive: will be filled with sender's port.
     */
    unsigned short port = 0;

    /**
     * @brief Construct an empty DatagramPacket with a specified buffer size.
     * @param size Initial size of the internal buffer (default: 0).
     */
    explicit DatagramPacket(const size_t size = 0) : buffer(size) {}

    /**
     * @brief Construct a DatagramPacket from a string_view and destination info.
     * @param data     Data to be copied into the packet buffer.
     * @param addr     Destination address.
     * @param prt      Destination UDP port.
     */
    DatagramPacket(std::string_view data, std::string addr, const unsigned short prt)
        : buffer(data.begin(), data.end()), address(std::move(addr)), port(prt)
    {
    }

    /**
     * @brief Construct a DatagramPacket from a raw pointer/size and destination info.
     *
     * @param data     Pointer to data buffer.
     * @param len      Number of bytes to copy.
     * @param addr     Destination address.
     * @param prt      Destination UDP port.
     */
    DatagramPacket(const char* data, size_t len, std::string addr, unsigned short prt)
        : buffer(data, data + len), address(std::move(addr)), port(prt)
    {
    }

    // Default copy and move constructors/operators.
    DatagramPacket(const DatagramPacket&) = default;
    DatagramPacket(DatagramPacket&&) noexcept = default;
    DatagramPacket& operator=(const DatagramPacket&) = default;
    DatagramPacket& operator=(DatagramPacket&&) noexcept = default;

    /**
     * @brief Resize the packet's internal buffer.
     * @param newSize The new size for the buffer.
     */
    void resize(const size_t newSize) { buffer.resize(newSize); }

    /**
     * @brief Get the current size of the buffer (valid bytes for receive/send).
     * @return Size of buffer in bytes.
     */
    [[nodiscard]] size_t size() const noexcept { return buffer.size(); }

    /**
     * @brief Reset the packet (clears buffer, address, and port).
     */
    void clear()
    {
        buffer.clear();
        address.clear();
        port = 0;
    }
};

} // namespace jsocketpp