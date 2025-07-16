// Modern jsocketpp client test: C++20, comments, and feature coverage
#include "jsocketpp/DatagramSocket.hpp"
#include "jsocketpp/Socket.hpp"
#include "jsocketpp/UnixSocket.hpp"
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

using namespace std;
using namespace jsocketpp;

// Forward declarations
void test_tcp(const std::string& ip, unsigned short port);
void test_udp(const std::string& ip, unsigned short port);
#ifndef _WIN32
void test_unix(const std::string& path);
#endif
void test_error_handling();

/**
 * @brief Test TCP client functionality: connect, send, receive, close.
 */
void test_tcp(const string& ip, unsigned short port)
{
    cout << "[TCP] Connecting to " << ip << ":" << port << endl;
    Socket conn(ip, port);
    conn.setTimeout(2000); // 2s timeout
    conn.setNonBlocking(false);
    conn.connect();
    conn.writeAll("Hello server! (TCP)");
    const string response = conn.read<string>();
    cout << "[TCP] Server says: " << response << endl;
    conn.close();
}

/**
 * @brief Test UDP client functionality: send, receive, close.
 */
void test_udp(const string& ip, unsigned short port)
{
    cout << "[UDP] Sending to " << ip << ":" << port << endl;
    DatagramSocket udp{port};
    udp.setTimeout(2000);
    udp.setNonBlocking(false);
    string msg = "Hello server! (UDP)";
    udp.write(msg.data(), msg.size(), ip, port);
    string sender;
    unsigned short senderPort;
    vector<char> buf(512);
    int n = udp.recvFrom(buf.data(), buf.size(), sender, senderPort);
    cout << "[UDP] Got " << n << " bytes from " << sender << ": " << string(buf.data(), static_cast<size_t>(n)) << endl;
    udp.close();
}

#ifndef _WIN32
/**
 * @brief Test Unix domain socket client functionality (POSIX only).
 */
void test_unix(const string& path)
{
    cout << "[UNIX] Connecting to " << path << endl;
    UnixSocket usock(path, false); // Construct as client
    usock.write("Hello server! (UNIX)");
    string response = usock.read<string>();
    cout << "[UNIX] Server says: " << response << endl;
    usock.close();
}
#endif

/**
 * @brief Test error handling by attempting to connect to an invalid address.
 */
void test_error_handling()
{
    cout << "[ERROR] Testing error handling..." << endl;
    try
    {
        Socket bad("256.256.256.256", 12345);
        bad.connect();
    }
    catch (const SocketException& se)
    {
        cout << "[ERROR] Caught expected: " << se.what() << endl;
    }
}

int main()
{
    SocketInitializer sockInit;
    string ip;
    unsigned short port;
    cout << "Type the IP to connect to (127.0.0.1 for this machine): ";
    getline(cin, ip);
    cout << "Type the port to connect to: ";
    while (true)
    {
        string input;
        getline(cin, input);
        stringstream myStream(input);
        if (myStream >> port)
            break;
        cout << "Error: Invalid port number. Port must be between 0 and 65535." << endl;
    }
    try
    {
        test_tcp(ip, port);
        test_udp(ip, ++port); // Assume UDP server is on port+1
#ifndef _WIN32
        test_unix("/tmp/jsocketpp_test.sock");
#endif
        test_error_handling();
    }
    catch (const SocketException& se)
    {
        cerr << "[FATAL] Error code: " << se.getErrorCode() << endl;
        cerr << "[FATAL] Error message: " << se.what() << endl;
        return 1;
    }
    cout << "All tests completed successfully." << endl;
    return 0;
}
