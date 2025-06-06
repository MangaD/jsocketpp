# Understanding Socket States

Sockets are a fundamental building block of network programming. Whether you are writing a TCP server or client,
understanding the various **states** a socket transitions through is essential for robust and efficient code. This guide
explains the key socket states, why they matter, and their practical implications—particularly for developers using the
`jsocketpp` library.

---

## What is a Socket State?

A **socket state** describes the current lifecycle phase of a socket (network endpoint) as managed by the operating
system. Each state reflects what operations the socket can perform and what system resources it is using.

---

## TCP Socket Lifecycle

TCP is a connection-oriented protocol, so its sockets transition through well-defined states as they connect,
communicate, and close. The states are defined by
the [TCP protocol specification (RFC 793)](https://tools.ietf.org/html/rfc793).

### Main TCP Socket States

| State             | Description                                                                                      | Typical API Calls                |
|-------------------|--------------------------------------------------------------------------------------------------|----------------------------------|
| **CLOSED**        | The socket is not being used.                                                                    | (Initial state, after `close()`) |
| **LISTEN**        | Server socket is listening for incoming connections.                                             | `bind()`, then `listen()`        |
| **SYN\_SENT**     | Client has started a connection; SYN sent, waiting for reply.                                    | `connect()`                      |
| **SYN\_RECEIVED** | Server has received a connection request (SYN), waiting for confirmation.                        | (Internal)                       |
| **ESTABLISHED**   | The connection is open; data can be sent and received.                                           | After `accept()`/`connect()`     |
| **FIN\_WAIT\_1**  | One side has requested connection close (sent FIN).                                              | `close()`, `shutdown()`          |
| **FIN\_WAIT\_2**  | Waiting for remote end to finish closing.                                                        | (Internal)                       |
| **CLOSE\_WAIT**   | The socket received a close request from the peer; waiting for local close.                      | (Internal)                       |
| **CLOSING**       | Both sides are closing; waiting for all data to be acknowledged.                                 | (Internal)                       |
| **LAST\_ACK**     | Waiting for final acknowledgment of close.                                                       | (Internal)                       |
| **TIME\_WAIT**    | The socket is closed, but the OS waits before freeing resources to ensure all data is delivered. | (Internal)                       |

---

### TCP State Transitions

Here’s a simplified diagram of the most common transitions:

```text
Server:                   Client:

[CLOSED]                  [CLOSED]
   |                         |
  bind()                  connect()
   |                         |
[LISTEN]                [SYN_SENT]
   |       <----->       /
accept()   connect()    /
   |                 /
[ESTABLISHED] <-----/
   |
shutdown()/close()
   |
[FIN_WAIT_1]/[CLOSE_WAIT]
   |
   .
   .
[CLOSED]
```

---

## What About UDP and UNIX Sockets?

UDP and UNIX domain sockets are connectionless, so they don’t follow the TCP state machine. Their states are simpler:

* **UNBOUND:** Socket created, not yet bound to an address/port (optional for UDP/UNIX clients).
* **BOUND:** Socket is associated with a local address/port (after `bind()`).
* **CONNECTED:** (Optional, for “connected UDP”) The socket has a default peer address.
* **CLOSED:** After `close()` is called or the object is destroyed.

---

## Practical Implications

* **LISTEN state** is only for servers. Only after `listen()` can a socket accept clients.
* **ESTABLISHED** is where most communication happens. For clients, it is entered after `connect()`. For servers, after
  `accept()`.
* **TIME\_WAIT** matters for servers: If you rapidly restart a server, the port may remain “busy” for a short time due
  to this state. This is why options like `SO_REUSEADDR` are sometimes set.
* **CLOSED** means the socket is no longer usable. All system resources are freed.
* **UDP Sockets:** There’s no connection state, but the socket must be bound before receiving data. "Connecting" a UDP
  socket just sets a default destination for send/receive operations.

---

## How This Relates to `jsocketpp`

When using the `jsocketpp` library, you control these states via method calls:

* **Server Example:**

  ```cpp
  jsocketpp::ServerSocket server(8080);
  server.setReuseAddress(true); // before bind
  server.bind();                // moves socket to BOUND
  server.listen();              // moves socket to LISTEN
  while (true) {
      jsocketpp::Socket client = server.accept(); // each client is in ESTABLISHED
  }
  ```

* **Client Example:**

  ```cpp
  jsocketpp::Socket sock("example.com", 8080);
  sock.connect();  // moves to ESTABLISHED
  sock.write("Hello");
  sock.close();    // moves to CLOSED
  ```

---

## More Resources

* [Beej’s Guide to Network Programming](https://beej.us/guide/bgnet/)
* [TCP State Diagram (Wikipedia)](https://en.wikipedia.org/wiki/Transmission_Control_Protocol#Protocol_operation)

---

**Tip:** Understanding socket states will help you debug, optimize, and write safer network code!
