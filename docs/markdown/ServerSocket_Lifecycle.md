# ServerSocket Construction, Binding, and Listening: Design Patterns and Best Practices

<!--!
\defgroup ServerSocket_Lifecycle ServerSocket Construction, Binding, and Listening: Design Patterns and Best Practices
\ingroup docs
\hidegroupgraph
[TOC]
-->

In network programming, **how and when you construct, bind, and listen with a server socket** has significant impact on
flexibility, safety, and ease of use. This article explains the design adopted by `jsocketpp`, contrasts it with Java
and POSIX, and justifies why the port is required at construction.

---

## 1. Traditional Socket Initialization: POSIX Approach

In classic C/POSIX code, creating a server socket is a multi-step, highly manual process:

```c
int sockfd = socket(...);
setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, ...);
bind(sockfd, ...);
listen(sockfd, ...);
```

* **Socket Creation:** `socket()`
* **Set Options:** `setsockopt()` (must be called after `socket()` but before `bind()`)
* **Bind:** `bind()` associates the socket with a local address/port.
* **Listen:** `listen()` makes it ready to accept connections.

> **Drawback:**
> The user is responsible for the correct sequence, handling errors at every step, and often for repeated IPv4/IPv6
> logic with `getaddrinfo()`.

---

## 2. Java ServerSocket: Constructor Does All Steps

Java’s `ServerSocket(int port)` **combines all steps** in its constructor:

```java
ServerSocket server = new ServerSocket(8080);
```

* Internally, this resolves addresses, creates the socket, sets options (as possible), binds, and listens.
* This is simple but **less flexible**: if you want to set options (like address reuse) before binding, you must use an
  alternate constructor or factory.

**Advanced use:**

```java
ServerSocket server = new ServerSocket();
server.setReuseAddress(true);
server.bind(new InetSocketAddress(8080));
```

* This is more like POSIX, but less commonly used in Java code.

---

## 3. The jsocketpp Approach: Port in Constructor, Bind/Listen Explicit

**jsocketpp** aims for a **balance of safety, flexibility, and Java-inspired clarity**:

* The `ServerSocket` constructor **requires the port** (and optionally address/interface).
* It uses `getaddrinfo()` to resolve all possible local addresses and stores the list.
* The socket itself is **created immediately** (possibly attempting both IPv4 and IPv6).
* The socket is **not yet bound or listening**.

This enables a safe, clear sequence:

```cpp
jsocketpp::ServerSocket server(8080);       // Port is *required* here
server.setReuseAddress(true);               // Set options before bind
server.bind();                              // Bind to address/port
server.listen();                            // Begin listening
```

### Why require the port in the constructor?

* **getaddrinfo() needs the port:** For cross-platform support and dual-stack (IPv4/IPv6), you want to call
  `getaddrinfo()` early, before bind.
* **Socket creation depends on address info:** You must know the family/protocol/port before you can call `socket()`.
* **Allows options to be set after construction but before binding:**
  You can safely set `SO_REUSEADDR`, timeouts, non-blocking, etc., in this window.

**Comparison Table:**

| Step          | jsocketpp                                   | Java (Typical)                            | POSIX          |
|---------------|---------------------------------------------|-------------------------------------------|----------------|
| Create socket | `ServerSocket(port)`                        | `new ServerSocket(port)`                  | `socket()`     |
| Set options   | `setReuseAddress` (after ctor, before bind) | not possible, or via alternate ctor       | `setsockopt()` |
| Bind          | `bind()`                                    | in constructor, or explicit via `.bind()` | `bind()`       |
| Listen        | `listen()`                                  | in constructor                            | `listen()`     |

---

## 4. Advantages and Rationale

### a) Flexibility and Safety

* **Can set options at the right time** (after socket creation, before bind).
* **No risk of binding without first setting key options** (as can happen in Java’s default constructor).
* **Consistent with best practices:** This is the recommended sequence for most robust servers.

### b) Portability

* Works well with both IPv4 and IPv6, on all platforms.
* Mirrors best practices for systems code while retaining a Java-inspired API.

### c) Clear API

* New users get a familiar, simple Java-like experience.
* Advanced users can control every step, essential for custom protocols, security, or debugging.

---

## 5. Alternative Designs: Pros and Cons

### **a) Constructor Does All (Java Default Style)**

* **Pros:**

    * One-liner for simple servers.
    * Fewer steps for the user.
* **Cons:**

    * Cannot set options before binding.
    * Harder to debug or customize.

### **b) Fully Step-by-Step (POSIX Style)**

* **Pros:**

    * Total control.
* **Cons:**

    * Verbose and error-prone.
    * Not friendly for new users.

### **c) jsocketpp Hybrid**

* **Pros:**

    * Safe, clear, and portable.
    * Options can be set at the right time.
    * Construction always succeeds or throws if it cannot resolve the address or create the socket.
* **Cons:**

    * Port must be known at construction (usually not a problem for TCP servers).
    * More steps than the Java "shortcut" constructor.

---

## 6. Advanced Scenarios

* **Binding to wildcard or specific address:**
  Pass an address (IP, or empty for all interfaces) to the constructor.
* **Dual-stack servers:**
  Handled transparently by using `getaddrinfo()` at construction.

---

## 7. Example Usage

```cpp
#include <jsocketpp/ServerSocket.hpp>

int main() {
    try {
        jsocketpp::ServerSocket server(8080);
        server.setReuseAddress(true);
        server.bind();
        server.listen();
        // Accept connections...
    } catch (const jsocketpp::SocketException& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }
}
```

---

## 8. Recommendations

* Use the **jsocketpp** approach for safety and flexibility.
* If your server needs more complex lifecycle control, consider using advanced constructors or additional configuration
  methods as you would in Java.
* Remember that setting options like `SO_REUSEADDR` is only possible *after* socket creation but *before* bind.

---

## 9. Summary

* **jsocketpp** provides a robust, cross-platform, and flexible approach by requiring the port in the constructor and
  separating `bind()`/`listen()`.
* This design enables both Java-like ease-of-use and POSIX-level control, without sacrificing safety or best practices.
