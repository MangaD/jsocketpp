# jsocketpp Examples

Welcome to the `jsocketpp` **examples** directory!
These examples demonstrate typical and advanced usage of the `jsocketpp` C++20 cross-platform socket library, inspired
by Java’s socket API.

Below you’ll find a list of example programs with brief descriptions.

---

## Example List

| File                      | Description                                                         |
|---------------------------|---------------------------------------------------------------------|
| `minimal_tcp_client.cpp`  | Connect to a TCP server, send/receive simple data.                  |
| `minimal_tcp_server.cpp`  | Minimal TCP server that accepts a connection and reads/writes data. |
| `echo_server.cpp`         | TCP server that echoes received messages to clients (multi-client). |
| `echo_client.cpp`         | TCP client that communicates with the echo server.                  |
| `udp_sender.cpp`          | Send a UDP datagram to a specified host/port.                       |
| `udp_receiver.cpp`        | Receive UDP datagrams and print sender info.                        |
| `udp_echo_server.cpp`     | UDP server that echoes received datagrams back to sender.           |
| `udp_connected.cpp`       | Demonstrates connected UDP usage for sending/receiving packets.     |
| `unix_socket_server.cpp`  | Unix domain socket server (if supported on your platform).          |
| `unix_socket_client.cpp`  | Unix domain socket client (if supported).                           |
| `multicast_receiver.cpp`  | Join a multicast group and receive multicast packets.               |
| `multicast_sender.cpp`    | Send packets to a multicast group.                                  |
| `udp_broadcast.cpp`       | Send and receive UDP broadcast messages.                            |
| `timeout_nonblocking.cpp` | Demonstrate timeout and non-blocking mode in TCP/UDP sockets.       |

---

## How to Build and Run Examples

**Prerequisites:**

- You have built and installed `jsocketpp`.
- Your C++ compiler supports C++20.
- Platform supports the socket type used (e.g., UNIX domain sockets may not work on Windows).

**Build:**

From the project root:

```sh
mkdir build && cd build
cmake ..
cmake --build .
````

Or, if using presets:

```sh
cmake --preset=debug
cmake --build --preset=debug
```

**Run:**

Navigate to the `examples/` directory and run the desired example:

```sh
./examples/minimal_tcp_server
./examples/echo_client
# etc.
```

---

## Contributions

Contributions of new or improved examples are welcome! See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines.

---

## License

Examples are provided under the same license as `jsocketpp`.
