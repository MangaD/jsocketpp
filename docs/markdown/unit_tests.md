# Unit Testing and Integration Guide

<!--!
\defgroup unit_tests Unit Testing and Integration Guide
\ingroup docs
\hidegroupgraph
[TOC]
-->

Unit and integration testing for a cross-platform C++17 socket library can be quite challenging, but with the right
approach and tools, it can be done effectively. Here's a structured plan to guide you through setting up unit and
integration tests for your library.

### 1. **Unit Tests**:

Unit tests for a socket library typically test individual components of the library in isolation. You will mock or
simulate socket-related functionality in your unit tests to avoid real network activity.

#### Tools for Unit Testing in C++17:

* **Google Test (gtest)**: This is one of the most popular C++ testing frameworks, providing powerful features such as
  assertions, fixtures, and mock support. It works seamlessly on both **Windows** and **Linux** and integrates easily
  into CMake.
* **Google Mock (gmock)**: This is an extension of Google Test that allows you to mock classes and methods. Useful for
  mocking socket behavior and testing edge cases where actual socket communication isn't needed.

#### 1.1 **Mocking Socket Behavior**:

To unit test your socket library, you’ll need to mock socket-related functions like `socket()`, `recv()`, `send()`, etc.
This allows you to test your socket code without actually opening real network connections.

##### Example: Using Google Mock to Mock `recv` and `send`:

1. Define a mock class for socket functions:

```cpp
#include <gmock/gmock.h>
#include <sys/socket.h>

class MockSocket {
public:
    MOCK_METHOD(int, recv, (int sockfd, void *buf, size_t len, int flags), ());
    MOCK_METHOD(int, send, (int sockfd, const void *buf, size_t len, int flags), ());
};
```

2. Use Google Test and Mock in your test cases:

```cpp
#include <gtest/gtest.h>
#include "MockSocket.h"

class MySocketTest : public ::testing::Test {
protected:
    MockSocket mockSocket;
};

TEST_F(MySocketTest, TestSocketRead) {
    // Mock the recv function to return a fixed value
    EXPECT_CALL(mockSocket, recv(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(5));  // Simulate 5 bytes received

    // Call the function in your library that uses recv
    int result = mySocketLibraryReadFunction(mockSocket);

    ASSERT_EQ(result, 5);  // Assert that the result is as expected
}
```

#### 1.2 **Testing Socket Initialization and Cleanup**:

You can test the initialization, error handling, and cleanup of sockets using mock or actual socket functions, but avoid
creating real connections.

### 2. **Integration Tests**:

Integration tests test how components of the socket library interact with real resources (e.g., actual sockets, local
servers, etc.). These tests typically involve starting up a real server and connecting with a client, or testing how the
library handles real network connections in a controlled environment.

#### 2.1 **Setting Up Real Server and Client**:

For integration testing, you'll want to set up a **TCP server** and **TCP client** that test the real communication.

1. **Server Setup**: Use your socket library to implement a simple server that listens for incoming connections.

2. **Client Setup**: Implement a client that connects to the server and sends/receives data.

3. **Test Communication**: The integration tests will simulate actual network communication and ensure your library
   correctly handles socket connections, sends and receives data, and closes connections.

#### Example of a Simple Server-Client Integration Test:

```cpp
#include <gtest/gtest.h>
#include <thread>
#include "Socket.h"  // Your socket library

// Simple server that listens on a specific port
void startServer() {
    ServerSocket server(8080);
    server.listen();
    Socket client = server.accept();  // Accept a connection from the client
    char buffer[1024];
    client.read(buffer, sizeof(buffer));  // Read the data sent from the client
    client.write("ack", 3);  // Send an acknowledgment
    client.close();
}

// Simple client that connects to the server
void startClient() {
    Socket client("127.0.0.1", 8080);
    client.write("hello", 5);  // Send a message to the server
    char buffer[1024];
    client.read(buffer, sizeof(buffer));  // Read the server's acknowledgment
    client.close();
}

TEST(SocketIntegrationTest, TestServerClientCommunication) {
    // Run server in a separate thread
    std::thread serverThread(startServer);

    // Run client in the main thread
    startClient();

    serverThread.join();  // Ensure the server thread completes

    // Add assertions here, if necessary, to verify the data exchange
}
```

### 3. **Test Coverage for Cross-Platform**:

Since your library is cross-platform, it's important to ensure that tests work on both **Windows** and **Linux** (or
other platforms). Here are a few strategies to ensure cross-platform testing:

* **CMake Presets**: Use CMake presets to configure the build system for different platforms.
* **Docker**: For testing on Linux, you can use Docker containers to simulate different Linux environments.
* **CI/CD Pipelines**: Set up continuous integration (CI) pipelines to automatically run your tests on both Windows and
  Linux environments.

For example, use **GitHub Actions**, **Travis CI**, or **AppVeyor** to run your tests on multiple platforms.

#### 3.1 **Using CMake for Platform-Specific Tests**:

You can specify platform-specific configurations in your CMakeLists.txt to manage platform-specific test setups.

```cmake
if (WIN32)
    add_executable(SocketTestWindows test_socket.cpp)
    target_link_libraries(SocketTestWindows gtest gmock)
    set_target_properties(SocketTestWindows PROPERTIES
            COMPILE_DEFINITIONS "PLATFORM_WINDOWS"
    )
elseif (UNIX)
    add_executable(SocketTestUnix test_socket.cpp)
    target_link_libraries(SocketTestUnix gtest gmock)
    set_target_properties(SocketTestUnix PROPERTIES
            COMPILE_DEFINITIONS "PLATFORM_LINUX"
    )
endif ()
```

### 4. **Mocking or Stubbing Socket API for Unit Tests**:

For unit tests where you don't want to deal with actual network communication, you can mock socket calls like
`socket()`, `recv()`, `send()`, and `close()` to simulate different network conditions.

#### Mock Example with Google Mock:

```cpp
class MockSocketAPI {
public:
    MOCK_METHOD(int, socket, (int domain, int type, int protocol), ());
    MOCK_METHOD(int, send, (int sockfd, const void *buf, size_t len, int flags), ());
    MOCK_METHOD(int, recv, (int sockfd, void *buf, size_t len, int flags), ());
    MOCK_METHOD(int, close, (int sockfd), ());
};
```

#### 4.1 **Integration Test with Mocking**:

You can mock system calls for a more controlled environment during unit tests. This way, you can simulate various
network conditions without needing a live server or network connection.

### 5. **Test Cases to Consider**:

Here are some potential test cases for your socket library:

* **Connection Tests**:

    * Can you successfully connect a client to a server?
    * What happens when the server is unavailable?
    * Can the server handle multiple simultaneous clients?

* **Timeouts**:

    * Does the socket properly handle timeouts for both reading and writing?

* **Data Handling**:

    * Can the socket send and receive data reliably?
    * Does the socket handle large data transfers?

* **Error Handling**:

    * What happens when an invalid socket is used?
    * Does the library throw exceptions for failed operations?

### Conclusion:

* **Unit Tests**: Mock socket functions to test socket operations in isolation without actual network communication.
* **Integration Tests**: Set up real server-client communication to test the full flow of data over TCP connections.
* **Cross-Platform Support**: Ensure your tests work on both **Windows** and **Linux** by using CI/CD tools and
  platform-specific build configurations.

By leveraging **Google Test**, **Google Mock**, and **CI pipelines**, you can ensure your cross-platform socket library
is robust and works as expected across different systems.

Let me know if you need further help or more examples!
