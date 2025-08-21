/**
 * @file DatagramPacket.hpp
 * @brief UDP datagram packet class for jsocketpp.
 */

#pragma once

#include "common.hpp"

#include <string>
#include <vector>

namespace jsocketpp
{

/**
 * @class DatagramPacket
 * @ingroup udp
 * @brief Represents a UDP datagram packet, encapsulating both payload and addressing information.
 *
 * The `DatagramPacket` class provides a convenient way to manage the data and addressing needed
 * for sending or receiving UDP packets using the `DatagramSocket` class. Each datagram packet
 * can hold a buffer for the payload, a remote IP address, and a port number.
 *
 * Typical usage:
 * @code
 * jsocketpp::DatagramPacket packetToSend("Hello, world!", "192.168.1.10", 12345);
 * socket.write(packetToSend);
 *
 * jsocketpp::DatagramPacket receivedPacket(1024); // Prepare a buffer for receiving
 * socket.read(receivedPacket);
 * std::cout << "Received from: " << receivedPacket.address << ":" << receivedPacket.port << std::endl;
 * std::cout << "Data: " << std::string(receivedPacket.buffer.begin(), receivedPacket.buffer.end()) << std::endl;
 * @endcode
 *
 * @note
 * - For sending: set the buffer, address, and port before passing to `DatagramSocket::write`.
 * - For receiving: use an empty `DatagramPacket` with a pre-sized buffer; after `read`, the address/port
 *   will be filled in.
 *
 * @see jsocketpp::DatagramSocket
 *
 * @author MangaD
 * @date 2025
 * @version 1.0
 */
class DatagramPacket
{
  public:
    /**
     * @brief Data buffer for the packet payload.
     *
     * - On sending: Contains the data to transmit.
     * - On receiving: Filled with received data (size indicates bytes received).
     */
    std::vector<char> buffer;

    /**
     * @brief Remote address (IPv4/IPv6) for the destination/source.
     * - On send: set to the destination address.
     * - On receive: will be filled with sender's address.
     */
    std::string address{};

    /**
     * @brief Remote UDP port for the destination/source.
     * - On send: set to the destination port.
     * - On receive: will be filled with sender's port.
     */
    Port port = 0;

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
    DatagramPacket(std::string_view data, std::string addr, const Port prt)
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
    DatagramPacket(const char* data, const size_t len, std::string addr, const Port prt)
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

    /**
     * @brief Report whether this packet specifies an explicit destination (address + port).
     * @ingroup udp
     *
     * @details
     * Returns `true` if the packet’s destination fields are set to a usable value for sending:
     * a non-blank `address` (not just whitespace) **and** a non-zero UDP `port`.
     * This is a **syntactic** check only; it does not validate that the address string is a
     * resolvable host/IP or that the port is reachable.
     *
     * Typical uses:
     * - In `DatagramSocket::write(const DatagramPacket&)`, choose between sending to the socket’s
     *   connected peer (when this returns `false`) or sending to the packet’s explicit destination
     *   via `sendto` (when this returns `true`).
     * - After a receive, indicates whether the source endpoint fields (`address`, `port`) were filled.
     *
     * @return `true` if `address` contains any non-whitespace character and `port != 0`; otherwise `false`.
     *
     * @code
     * DatagramPacket p;
     * p.buffer.assign(data.begin(), data.end());
     *
     * // No destination yet:
     * assert(!p.hasDestination());
     *
     * // Set destination for send:
     * p.address = "2001:db8::1";
     * p.port    = 5353;
     * assert(p.hasDestination());
     * @endcode
     */
    [[nodiscard]] bool hasDestination() const noexcept
    {
        if (port == 0)
            return false;

        // Treat strings that are empty or only whitespace as "no address".
        for (const auto ch : address)
        {
            if (!std::isspace(ch))
                return true;
        }
        return false;
    }
};

} // namespace jsocketpp
