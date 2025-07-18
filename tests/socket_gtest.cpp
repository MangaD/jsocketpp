// GoogleTest unit tests for jsocketpp
#include "jsocketpp/DatagramSocket.hpp"
#include "jsocketpp/ServerSocket.hpp"
#include "jsocketpp/Socket.hpp"
#include "jsocketpp/SocketInitializer.hpp"
#include "jsocketpp/UnixSocket.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace jsocketpp;

TEST(SocketTest, TcpConnectInvalid)
{
    SocketInitializer init;
    EXPECT_THROW(
        {
            const Socket s("256.256.256.256", 12345);
            s.connect();
        },
        SocketException);
}

TEST(SocketTest, TcpBindInvalidPort)
{
    SocketInitializer init;
    EXPECT_THROW(
        {
            ServerSocket s(0);
            s.bind();
        },
        SocketException);
}

#ifndef _WIN32
TEST(SocketTest, UnixSocketInvalidPath)
{
    EXPECT_THROW({ UnixSocket s("/tmp/does_not_exist.sock"); }, SocketException);
}
#endif

TEST(SocketTest, TcpConnectTimeout)
{
    SocketInitializer init;
    Socket s("10.255.255.1", 65000); // unroutable IP
    s.setSoTimeout(100);             // 100ms
    EXPECT_THROW(s.connect(), SocketException);
}

TEST(SocketTest, TcpNonBlocking)
{
    SocketInitializer init;
    const Socket s("10.255.255.1", 65000);
    s.setNonBlocking(true);
    // connect() should fail immediately or throw
    EXPECT_ANY_THROW(s.connect());
}

TEST(SocketTest, TcpSetGetOption)
{
    SocketInitializer init;
    Socket s("127.0.0.1", 1); // port 1 is usually closed
    // setTimeout and setNonBlocking should not throw
    EXPECT_NO_THROW(s.setSoTimeout(100));
    EXPECT_NO_THROW(s.setNonBlocking(true));
}

TEST(SocketTest, UdpSendRecvLoopback)
{
    SocketInitializer init;
    DatagramSocket server(54321);
    DatagramSocket client(0); // Bind to any available port
    std::string msg = "gtest-udp";
    EXPECT_NO_THROW(client.sendTo(msg.data(), msg.size(), "127.0.0.1", 54321));
    std::string sender;
    Port senderPort;
    std::vector<char> buf(32);
    int n = server.recvFrom(buf.data(), buf.size(), sender, senderPort);
    EXPECT_GT(n, 0);
    EXPECT_EQ(std::string(buf.data(), n), msg);
    server.close();
    client.close();
}

TEST(SocketTest, UdpTimeout)
{
    SocketInitializer init;
    DatagramSocket s(54322);
    s.setTimeout(100); // 100ms
    std::string sender;
    Port senderPort;
    std::vector<char> buf(32);
    EXPECT_THROW(s.recvFrom(buf.data(), buf.size(), sender, senderPort), SocketException);
    s.close();
}

#ifndef _WIN32
#include <cstdio>
TEST(SocketTest, UnixSocketBindConnect)
{
    const char* path = "/tmp/gtest_unixsock.sock";
    UnixSocket server(path);
    EXPECT_NO_THROW(server.bind());
    EXPECT_NO_THROW(server.listen());
    UnixSocket client(path);
    EXPECT_NO_THROW(client.connect());
    std::string msg = "unix-gtest";
    EXPECT_NO_THROW(client.write(msg));
    std::string rcvd = server.accept().read<std::string>();
    EXPECT_EQ(rcvd, msg);
    client.close();
    server.close();
    std::remove(path);
}
TEST(SocketTest, UnixSocketTimeoutAndNonBlocking)
{
    const char* path = "/tmp/gtest_unixsock2.sock";
    UnixSocket s(path);
    EXPECT_NO_THROW(s.bind());
    EXPECT_NO_THROW(s.listen());
    // No client connects, so accept() should block or fail
    // (We can't easily test timeout here, but we can test non-blocking)
    // This is a placeholder for future expansion
    s.close();
    std::remove(path);
}
#endif

// Add more tests as needed for UDP, timeouts, non-blocking, etc.
