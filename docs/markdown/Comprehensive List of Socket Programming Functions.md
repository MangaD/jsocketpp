# Comprehensive List of Socket Programming Functions

<!--!
\defgroup list_bsd_socket_functions Comprehensive List of Socket Programming Functions
\ingroup docs
\hidegroupgraph
[TOC]
-->

Below is a comprehensive list of commonly used functions in socket programming, focusing on the Berkeley Sockets API (also known as BSD sockets), which is widely used in C/C++ for network programming on Unix-like systems and Windows (via Winsock). Each function is described with its purpose, typical usage, and key details. This list includes functions for creating, configuring, binding, connecting, sending, receiving, and managing sockets, as well as utility functions for address handling. The descriptions aim to be concise yet informative, suitable for inclusion in a technical library to educate users about socket programming.

### 1. Core Socket Creation and Management
- **`socket()`**
  - **Purpose**: Creates a new socket.
  - **Description**: Initializes a socket with a specified address family (e.g., `AF_INET` for IPv4, `AF_INET6` for IPv6), socket type (e.g., `SOCK_STREAM` for TCP, `SOCK_DGRAM` for UDP), and protocol (e.g., `IPPROTO_TCP` or `0` for default).
  - **Usage**: `int socket(int domain, int type, int protocol);`
  - **Example**: `int sockfd = socket(AF_INET, SOCK_STREAM, 0);` creates a TCP socket for IPv4.
  - **Returns**: A socket file descriptor on success, `-1` on error.

- **`close()`** (Unix) / **`closesocket()`** (Windows)
  - **Purpose**: Closes a socket and releases its resources.
  - **Description**: Terminates the socket, freeing the file descriptor and any associated resources. On Unix, `close()` is used; on Windows, `closesocket()` is required.
  - **Usage**: `int close(int sockfd);` or `int closesocket(SOCKET sockfd);`
  - **Example**: `close(sockfd);` closes a socket after use.
  - **Returns**: `0` on success, `-1` on error.

- **`shutdown()`**
  - **Purpose**: Disables sending or receiving on a socket.
  - **Description**: Allows partial closure of a socket, disabling further send (`SHUT_WR`), receive (`SHUT_RD`), or both (`SHUT_RDWR`) operations, while keeping the socket open for the other direction if needed.
  - **Usage**: `int shutdown(int sockfd, int how);`
  - **Example**: `shutdown(sockfd, SHUT_RDWR);` stops both sending and receiving.
  - **Returns**: `0` on success, `-1` on error.

### 2. Address Binding and Connection
- **`bind()`**
  - **Purpose**: Associates a socket with a local address and port.
  - **Description**: Explicitly binds a socket to a local IP address and port, typically used by servers or clients needing specific local endpoints. The address structure (e.g., `sockaddr_in`) specifies the IP and port.
  - **Usage**: `int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);`
  - **Example**: Binds a server socket to `0.0.0.0:8080` for listening on all interfaces.
  - **Returns**: `0` on success, `-1` on error (e.g., address in use).

- **`connect()`**
  - **Purpose**: Initiates a connection to a remote address or sets a default remote address for UDP.
  - **Description**: For TCP, establishes a connection to a remote server. For UDP, sets a default remote address for subsequent send/receive operations. Triggers implicit binding if `bind()` wasn’t called.
  - **Usage**: `int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);`
  - **Example**: `connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));` connects to a server.
  - **Returns**: `0` on success, `-1` on error.

- **`listen()`**
  - **Purpose**: Marks a socket as a listening socket for incoming connections (TCP only).
  - **Description**: Prepares a TCP socket to accept incoming connections, specifying the maximum number of pending connections in the queue.
  - **Usage**: `int listen(int sockfd, int backlog);`
  - **Example**: `listen(sockfd, 5);` allows up to 5 pending connections.
  - **Returns**: `0` on success, `-1` on error.

- **`accept()`**
  - **Purpose**: Accepts an incoming connection on a listening TCP socket.
  - **Description**: Creates a new socket for an incoming client connection, returning the new socket’s file descriptor and the client’s address.
  - **Usage**: `int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);`
  - **Example**: `int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &addrlen);`
  - **Returns**: New socket descriptor on success, `-1` on error.

### 3. Data Transfer
- **`send()`**
  - **Purpose**: Sends data over a connected socket (TCP or connected UDP).
  - **Description**: Transmits data to the remote endpoint of a connected socket. Used after `connect()` for TCP or UDP.
  - **Usage**: `ssize_t send(int sockfd, const void *buf, size_t len, int flags);`
  - **Example**: `send(sockfd, "Hello", 5, 0);` sends a message.
  - **Returns**: Number of bytes sent on success, `-1` on error.

- **`sendto()`**
  - **Purpose**: Sends data to a specific remote address (typically UDP).
  - **Description**: Sends data to a specified remote address, used for unconnected UDP sockets or when overriding the default address set by `connect()`.
  - **Usage**: `ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);`
  - **Example**: Sends a UDP datagram to a specific address.
  - **Returns**: Number of bytes sent, `-1` on error.

- **`recv()`**
  - **Purpose**: Receives data from a connected socket (TCP or connected UDP).
  - **Description**: Reads incoming data from a connected socket into a buffer.
  - **Usage**: `ssize_t recv(int sockfd, void *buf, size_t len, int flags);`
  - **Example**: `recv(sockfd, buffer, sizeof(buffer), 0);` receives data.
  - **Returns**: Number of bytes received, `0` on connection close (TCP), `-1` on error.

- **`recvfrom()`**
  - **Purpose**: Receives data and the sender’s address (typically UDP).
  - **Description**: Reads incoming data and captures the sender’s address, used for unconnected UDP sockets or to identify the source of datagrams.
  - **Usage**: `ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);`
  - **Example**: Receives a UDP datagram and the sender’s address.
  - **Returns**: Number of bytes received, `-1` on error.

- **`sendmsg()`**
  - **Purpose**: Sends data with advanced options (e.g., multiple buffers).
  - **Description**: Sends data using a `msghdr` structure, allowing control over multiple buffers, ancillary data, and flags. Useful for complex scenarios like sending file descriptors or control messages.
  - **Usage**: `ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);`
  - **Example**: Used in advanced applications like passing file descriptors over Unix domain sockets.
  - **Returns**: Number of bytes sent, `-1` on error.

- **`recvmsg()`**
  - **Purpose**: Receives data with advanced options.
  - **Description**: Receives data into multiple buffers with a `msghdr` structure, supporting ancillary data and detailed control. Often used in advanced UDP or Unix domain socket applications.
  - **Usage**: `ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);`
  - **Example**: Receives data with sender information or control messages.
  - **Returns**: Number of bytes received, `-1` on error.

### 4. Address and Name Resolution
- **`getaddrinfo()`**
  - **Purpose**: Resolves hostnames and service names to socket addresses.
  - **Description**: Converts a hostname (e.g., "example.com") and service (e.g., "http" or "80") into a list of `sockaddr` structures, supporting both IPv4 and IPv6. Preferred over older functions like `gethostbyname()`.
  - **Usage**: `int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);`
  - **Example**: `getaddrinfo("example.com", "80", &hints, &res);` resolves a web server address.
  - **Returns**: `0` on success, non-zero error code on failure.

- **`freeaddrinfo()`**
  - **Purpose**: Frees memory allocated by `getaddrinfo()`.
  - **Description**: Releases the linked list of `addrinfo` structures returned by `getaddrinfo()` to prevent memory leaks.
  - **Usage**: `void freeaddrinfo(struct addrinfo *res);`
  - **Example**: `freeaddrinfo(res);` after using `getaddrinfo()` results.
  - **Returns**: None.

- **`getnameinfo()`**
  - **Purpose**: Converts a socket address to hostname and service name.
  - **Description**: Reverse-resolves a `sockaddr` structure to a hostname and service name, useful for logging or displaying client information.
  - **Usage**: `int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen, char *serv, size_t servlen, int flags);`
  - **Example**: Retrieves the hostname and port of a connected client.
  - **Returns**: `0` on success, non-zero error code on failure.

- **`inet_addr()`**
  - **Purpose**: Converts an IPv4 address string to binary format.
  - **Description**: Converts a dotted-decimal IPv4 address (e.g., "192.168.1.1") to a binary `in_addr_t`. Deprecated in favor of `inet_pton()`.
  - **Usage**: `in_addr_t inet_addr(const char *cp);`
  - **Example**: `in_addr_t addr = inet_addr("192.168.1.1");`
  - **Returns**: Binary address or `INADDR_NONE` on error.

- **`inet_ntoa()`**
  - **Purpose**: Converts a binary IPv4 address to a string.
  - **Description**: Converts a binary `in_addr` structure to a dotted-decimal string. Deprecated in favor of `inet_ntop()`.
  - **Usage**: `char *inet_ntoa(struct in_addr in);`
  - **Example**: `char *ip = inet_ntoa(addr.sin_addr);`
  - **Returns**: Pointer to a static string (not thread-safe).

- **`inet_pton()`**
  - **Purpose**: Converts an address string to binary format (IPv4 or IPv6).
  - **Description**: Converts a presentation-format address (e.g., "192.168.1.1" or "2001:db8::1") to binary for a specified address family. Modern replacement for `inet_addr()`.
  - **Usage**: `int inet_pton(int af, const char *src, void *dst);`
  - **Example**: `inet_pton(AF_INET, "192.168.1.1", &addr.sin_addr);`
  - **Returns**: `1` on success, `0` on invalid input, `-1` on error.

- **`inet_ntop()`**
  - **Purpose**: Converts a binary address to a string (IPv4 or IPv6).
  - **Description**: Converts a binary address to a presentation-format string, supporting both IPv4 and IPv6. Safer and more modern than `inet_ntoa()`.
  - **Usage**: `const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);`
  - **Example**: `inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN);`
  - **Returns**: Pointer to the destination string on success, `NULL` on error.

### 5. Socket Information and Configuration
- **`getsockname()`**
  - **Purpose**: Retrieves the local address bound to a socket.
  - **Description**: Returns the local IP address and port associated with a socket, useful for determining the assigned address after implicit binding.
  - **Usage**: `int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);`
  - **Example**: `getsockname(sockfd, (struct sockaddr*)&local_addr, &addrlen);`
  - **Returns**: `0` on success, `-1` on error.

- **`getpeername()`**
  - **Purpose**: Retrieves the remote address of a connected socket.
  - **Description**: Returns the IP address and port of the remote endpoint for a connected socket (TCP or connected UDP).
  - **Usage**: `int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);`
  - **Example**: `getpeername(sockfd, (struct sockaddr*)&peer_addr, &addrlen);`
  - **Returns**: `0` on success, `-1` on error.

- **`getsockopt()`**
  - **Purpose**: Retrieves socket options.
  - **Description**: Queries the current value of a socket option, such as buffer sizes, timeout settings, or protocol-specific options.
  - **Usage**: `int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);`
  - **Example**: `getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, &optlen);`
  - **Returns**: `0` on success, `-1` on error.

- **`setsockopt()`**
  - **Purpose**: Sets socket options.
  - **Description**: Configures socket options, such as enabling reuse of addresses (`SO_REUSEADDR`), setting timeouts, or adjusting buffer sizes.
  - **Usage**: `int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);`
  - **Example**: `setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));`
  - **Returns**: `0` on success, `-1` on error.

### 6. Non-Blocking and Asynchronous I/O
- **`select()`**
  - **Purpose**: Monitors multiple sockets for I/O readiness.
  - **Description**: Checks a set of sockets for readability, writability, or errors, with an optional timeout. Used for multiplexing I/O operations.
  - **Usage**: `int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);`
  - **Example**: Monitors multiple sockets for incoming data.
  - **Returns**: Number of ready descriptors, `0` on timeout, `-1` on error.

- **`poll()`**
  - **Purpose**: Monitors sockets for events, similar to `select()`.
  - **Description**: More scalable than `select()`, it monitors sockets for events (e.g., `POLLIN`, `POLLOUT`) with a specified timeout.
  - **Usage**: `int poll(struct pollfd *fds, nfds_t nfds, int timeout);`
  - **Example**: Polls a set of sockets for incoming data or errors.
  - **Returns**: Number of ready descriptors, `0` on timeout, `-1` on error.

- **`epoll_create()`** (Linux-specific)
  - **Purpose**: Creates an epoll instance for efficient I/O event monitoring.
  - **Description**: Initializes an epoll instance for scalable I/O event handling, used with `epoll_ctl()` and `epoll_wait()`.
  - **Usage**: `int epoll_create(int size);` (size is a hint, ignored in modern kernels).
  - **Example**: `int epfd = epoll_create(1);`
  - **Returns**: Epoll file descriptor on success, `-1` on error.

- **`epoll_ctl()`** (Linux-specific)
  - **Purpose**: Manages epoll interest list.
  - **Description**: Adds, modifies, or removes sockets from an epoll instance, specifying events to monitor (e.g., `EPOLLIN`, `EPOLLOUT`).
  - **Usage**: `int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);`
  - **Example**: Adds a socket to an epoll instance for monitoring.
  - **Returns**: `0` on success, `-1` on error.

- **`epoll_wait()`** (Linux-specific)
  - **Purpose**: Waits for events on an epoll instance.
  - **Description**: Blocks until events occur on monitored sockets or a timeout is reached, returning ready sockets.
  - **Usage**: `int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);`
  - **Example**: Waits for I/O events on multiple sockets.
  - **Returns**: Number of ready events, `0` on timeout, `-1` on error.

- **`fcntl()`**
  - **Purpose**: Sets socket to non-blocking mode (among other uses).
  - **Description**: Modifies socket properties, such as enabling non-blocking I/O with `O_NONBLOCK`.
  - **Usage**: `int fcntl(int sockfd, int cmd, ...);`
  - **Example**: `fcntl(sockfd, F_SETFL, O_NONBLOCK);` enables non-blocking mode.
  - **Returns**: Depends on command, typically `0` on success, `-1` on error.

- **`ioctl()`**
  - **Purpose**: Controls socket properties (e.g., non-blocking mode, interface info).
  - **Description**: Performs device-specific operations, such as setting non-blocking mode or retrieving interface information.
  - **Usage**: `int ioctl(int sockfd, unsigned long request, ...);`
  - **Example**: `ioctl(sockfd, FIONBIO, &nonblock);` enables non-blocking I/O.
  - **Returns**: `0` on success, `-1` on error.

### 7. Miscellaneous
- **`gethostbyname()`** (Deprecated)
  - **Purpose**: Resolves a hostname to an IPv4 address.
  - **Description**: Converts a hostname to a binary IPv4 address. Replaced by `getaddrinfo()` for IPv6 support and thread safety.
  - **Usage**: `struct hostent *gethostbyname(const char *name);`
  - **Example**: Resolves "example.com" to an IP address.
  - **Returns**: Pointer to `hostent` structure or `NULL` on error.

- **`gethostbyaddr()`** (Deprecated)
  - **Purpose**: Resolves an IP address to a hostname.
  - **Description**: Performs reverse DNS lookup for an IPv4 address. Replaced by `getnameinfo()`.
  - **Usage**: `struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type);`
  - **Example**: Resolves an IP address to a hostname.
  - **Returns**: Pointer to `hostent` structure or `NULL` on error.

- **`htons()`** / **`htonl()`**
  - **Purpose**: Converts short/long integers from host to network byte order.
  - **Description**: Ensures proper byte order for port numbers (`htons`) or IP addresses (`htonl`) across different architectures.
  - **Usage**: `uint16_t htons(uint16_t hostshort);` / `uint32_t htonl(uint32_t hostlong);`
  - **Example**: `serv_addr.sin_port = htons(8080);`
  - **Returns**: Converted value.

- **`ntohs()`** / **`ntohl()`**
  - **Purpose**: Converts short/long integers from network to host byte order.
  - **Description**: Reverses `htons()`/`htonl()` for interpreting received data.
  - **Usage**: `uint16_t ntohs(uint16_t netshort);` / `uint32_t ntohl(uint32_t netlong);`
  - **Example**: `port = ntohs(addr.sin_port);`
  - **Returns**: Converted value.

## Notes
- **Platform Differences**: On Windows, the Winsock API requires `WSAStartup()` to initialize the socket library and `WSACleanup()` to clean up. Most functions listed above are available in Winsock, but `closesocket()` replaces `close()`, and some functions (e.g., `epoll_*`) are Linux-specific.
- **Deprecated Functions**: Functions like `gethostbyname()`, `gethostbyaddr()`, `inet_addr()`, and `inet_ntoa()` are deprecated due to lack of IPv6 support and thread-safety issues. Use `getaddrinfo()`, `getnameinfo()`, `inet_pton()`, and `inet_ntop()` instead.
- **Error Handling**: Most functions return `-1` on error, with the error code stored in `errno` (Unix) or retrievable via `WSAGetLastError()` (Windows).
- **Non-Blocking I/O**: Functions like `select()`, `poll()`, and `epoll_*` are critical for handling multiple sockets efficiently, especially in high-performance servers.

## Example Usage Context
The following pseudocode illustrates a typical TCP server using several of these functions:
1. Resolve server address with `getaddrinfo()`.
2. Create a socket with `socket()`.
3. Set options like `SO_REUSEADDR` with `setsockopt()`.
4. Bind to a local address with `bind()`.
5. Listen for connections with `listen()`.
6. Accept client connections with `accept()`.
7. Send/receive data with `send()`/`recv()`.
8. Close the socket with `close()`.

For a UDP client:
1. Resolve server address with `getaddrinfo()`.
2. Create a socket with `socket()`.
3. Optionally bind with `bind()` or let `connect()`/`sendto()` trigger implicit binding.
4. Send data with `sendto()` or receive with `recvfrom()`.
5. Close the socket with `close()`.

## Conclusion
This list covers the essential functions for socket programming in the Berkeley Sockets API, providing a foundation for building network applications. Each function serves a specific role, from creating and configuring sockets to handling data transfer and address resolution. By understanding these functions, developers can create robust client-server applications for both TCP and UDP protocols. This guide is a valuable resource for your library, offering clear explanations and examples to educate users on socket programming.
