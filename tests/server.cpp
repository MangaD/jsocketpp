// Modern jsocketpp server test: C++20, comments, and feature coverage
#include "jsocketpp/DatagramSocket.hpp"
#include "jsocketpp/ServerSocket.hpp"
#include "jsocketpp/Socket.hpp"
#include "jsocketpp/SocketInitializer.hpp"
#include "jsocketpp/UnixSocket.hpp"
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

using namespace std;
using namespace jsocketpp;

// Forward declarations
void test_tcp(Port port);
void test_udp(Port port);
#ifndef _WIN32
void test_unix(const std::string& path);
#endif
void test_error_handling();

/**
 * @brief Test TCP server functionality: accept, receive, send, close.
 */
void test_tcp(const Port port)
{
    cout << "[TCP] Starting server on port " << port << endl;
    ServerSocket serverSocket(port);
    serverSocket.bind();
    serverSocket.listen();
    cout << "[TCP] Waiting for client..." << endl;
    Socket conn = serverSocket.accept();
    cout << "[TCP] Client connected from: " << conn.getRemoteSocketAddress() << endl;
    const string msg = conn.read<string>();
    cout << "[TCP] Client says: " << msg << endl;
    conn.writeAll("Hello client! (TCP)");
    conn.close();
    serverSocket.close();
}

/**
 * @brief Test UDP server functionality: receive, send, close.
 */
void test_udp(const Port port)
{
    cout << "[UDP] Starting UDP server on port " << port << endl;
    DatagramSocket udp(port);
    udp.setTimeout(5000);
    udp.setNonBlocking(false);
    vector<char> buf(512);
    string sender;
    Port senderPort;
    int n = udp.recvFrom(buf.data(), buf.size(), sender, senderPort);
    cout << "[UDP] Got " << n << " bytes from " << sender << ": " << string(buf.data(), static_cast<size_t>(n)) << endl;
    string reply = "Hello client! (UDP)";
    udp.sendTo(reply.data(), reply.size(), sender, senderPort);
    udp.close();
}

#ifndef _WIN32
/**
 * @brief Test Unix domain socket server functionality (POSIX only).
 */
void test_unix(const string& path)
{
    cout << "[UNIX] Starting Unix domain socket server at " << path << endl;
    UnixSocket usock(path, true); // Construct as server
    usock.listen();
    cout << "[UNIX] Waiting for client..." << endl;
    UnixSocket client = usock.accept();
    string msg = client.read<string>();
    cout << "[UNIX] Client says: " << msg << endl;
    client.write("Hello client! (UNIX)");
    client.close();
    usock.close();
    unlink(path.c_str());
}
#endif

/**
 * @brief Test error handling by attempting to bind to an invalid port.
 */
void test_error_handling()
{
    cout << "[ERROR] Testing error handling..." << endl;
    try
    {
        ServerSocket bad(0);
        bad.bind();
        bad.listen();
    }
    catch (const SocketException& se)
    {
        cout << "[ERROR] Caught expected: " << se.what() << endl;
    }
}

int main()
{
    SocketInitializer sockInit;
    Port port;
    cout << "Type a port to start listening at: ";
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
        test_tcp(port);
        test_udp(++port); // UDP server on port+1
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
