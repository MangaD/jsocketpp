# Comprehensive Guide to `ioctl`, `fcntl`, `ioctlsocket`,
`WSAIoctl`, and Related Functions: Commands, Use Cases, and Windows Support

<!--!
\defgroup ioctl_fcntl_ioctlsocket_guide Comprehensive Guide to `ioctl`, `fcntl`, `ioctlsocket`, `WSAIoctl`, and Related Functions: Commands, Use Cases, and Windows Support
\ingroup docs
\hidegroupgraph
[TOC]
-->

[![CC0](https://licensebuttons.net/p/zero/1.0/88x31.png)](https://creativecommons.org/publicdomain/zero/1.0/)

*Disclaimer: Grok generated document*.

Below is a comprehensive exploration of the `ioctl`, `fcntl`, `ioctlsocket`, `WSAIoctl`, and related functions, diving
into their details, commands, use cases, and support across Windows versions, including Windows XP. Given the request
for exhaustive detail, this response covers their functionality, historical context, common commands, practical
examples, and platform-specific nuances, with a focus on Windows compatibility.

---

### Overview of Functions

These functions are low-level interfaces for controlling and configuring file descriptors or sockets, primarily in
Unix-like systems (`ioctl`, `fcntl`) and Windows (`ioctlsocket`, `WSAIoctl`). They are used to perform operations not
covered by standard I/O functions like `read` or `write`, such as setting non-blocking modes, querying device states, or
configuring network interfaces. Each function has a specific scope, with overlapping but distinct purposes.

- **ioctl (Unix/Linux)**: A general-purpose system call for device-specific I/O control, used for character devices,
  sockets, and other special files.
- **fcntl (Unix/Linux)**: A versatile system call for file descriptor operations, including file control, locking, and
  some I/O control tasks overlapping with `ioctl`.
- **ioctlsocket (Windows)**: A Winsock function for socket-specific I/O control, supporting a subset of Berkeley socket
  ioctls.
- **WSAIoctl (Windows)**: An extended Winsock function for broader socket control, including Windows-specific and
  BSD-compatible operations.
- **Related Functions**: Functions like `setsockopt`/`getsockopt`, `tcsetattr`/`tcgetattr`, and sysfs interfaces in
  Linux, which overlap or replace these functions in specific contexts.

---

### 1. ioctl (Unix/Linux)

#### Description

`ioctl` (Input/Output Control) is a system call in Unix-like systems (Linux, BSD, etc.) for performing device-specific
operations on file descriptors, especially for character devices (e.g., terminals, serial ports, network interfaces).
It’s a catch-all for operations that don’t fit standard file I/O, such as configuring hardware, querying status, or
executing driver-specific commands. It’s highly flexible but device-dependent, making it non-portable and complex.

#### Syntax

```c
#include <sys/ioctl.h>
int ioctl(int fd, unsigned long request, ...);
```

- **fd**: Open file descriptor (from `open(2)`) for a device or socket.
- **request**: Device-specific operation code, typically defined using macros like `_IO`, `_IOR`, `_IOW`, `_IOWR` from
  `<asm/ioctl.h>`.
- **... (arg)**: Optional argument, usually a pointer to data (e.g., `void *`), but can be an integer or other type
  depending on the request.

On some systems (e.g., musl libc), the second argument is `int` instead of `unsigned long`.

#### Request Code Structure

Request codes are 32-bit values (in Linux) with fields:

- **Direction** (2 bits): 0 (none), 1 (write), 2 (read), 3 (read/write).
- **Size** (14 bits): Size of the argument data (e.g., `sizeof(struct)`).
- **Type** (8 bits): Magic number (e.g., 'T' for terminals).
- **Number** (8 bits): Serial number for the command.

Macros to define requests:

- `_IO(type, nr)`: No data transfer.
- `_IOR(type, nr, type)`: Read from kernel to user.
- `_IOW(type, nr, type)`: Write from user to kernel.
- `_IOWR(type, nr, type)`: Bidirectional.

#### Return Values

- **Success**: 0 or a positive value (e.g., bytes processed).
- **Failure**: -1, with `errno` set (e.g., `EBADF`, `EFAULT`, `EINVAL`, `ENOTTY`).

#### Common Errors

- `EBADF`: Invalid file descriptor.
- `EFAULT`: `arg` points to inaccessible memory.
- `EINVAL`: Invalid `request` or `arg`.
- `ENOTTY`: `fd` not a character device or operation inapplicable.
- `EIO`: I/O error.
- `ENXIO`: Device-specific sub-device error.
- POSIX STREAMS errors: `EINTR`, `ENOSR`, etc.

#### Common Commands

- **Terminal/Console**:
    - `TIOCGWINSZ`: Get terminal window size (`struct winsize`).
    - `TIOCSWINSZ`: Set window size.
    - `TIOCSETD`: Set line discipline.
    - `TIOCSCTTY`: Set controlling terminal.
- **Sockets**:
    - `FIONBIO`: Set/clear non-blocking mode (non-zero to enable).
    - `FIONREAD`: Get number of readable bytes.
    - `SIOCGIFADDR`: Get network interface address.
    - `SIOCSIFADDR`: Set interface address.
    - `SIOCGIFMTU`: Get MTU size.
- **Disk/Storage**:
    - `HDIO_GETGEO`: Get disk geometry.
    - `BLKGETSIZE`: Get block device size.
- **Custom Drivers**: User-defined via `_IO*` macros for kernel modules.

#### Use Cases

- **Terminal Management**: Adjusting terminal settings (e.g., window size for `tmux` or `vim`).
- **Network Configuration**: Setting IP addresses, MTU, or querying interface status.
- **Device Drivers**: Controlling hardware (e.g., serial ports, GPUs).
- **Kernel Modules**: Custom ioctls for proprietary hardware.

#### Example: Terminal Window Size (Linux)

```cpp
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
        perror("ioctl");
        return 1;
    }
    printf("Terminal size: %d rows x %d columns\n", ws.ws_row, ws.ws_col);
    return 0;
}
```

#### Example: Custom Device Driver (Linux)

User-space code interacting with a kernel module:

```cpp
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#define MY_MAGIC 'a'
#define WR_VALUE _IOW(MY_MAGIC, 1, int)
#define RD_VALUE _IOR(MY_MAGIC, 2, int)

int main() {
    int fd = open("/dev/mydevice", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    int value = 42;
    if (ioctl(fd, WR_VALUE, &value) == -1) {
        perror("ioctl write");
    }
    if (ioctl(fd, RD_VALUE, &value) == -1) {
        perror("ioctl read");
    } else {
        printf("Read value: %d\n", value);
    }
    close(fd);
    return 0;
}
```

Kernel-side (simplified):

```cpp
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#define MY_MAGIC 'a'
#define WR_VALUE _IOW(MY_MAGIC, 1, int)
#define RD_VALUE _IOR(MY_MAGIC, 2, int)

static int value;

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    int temp;
    switch (cmd) {
        case WR_VALUE:
            if (copy_from_user(&temp, (int __user *)arg, sizeof(temp)))
                return -EFAULT;
            value = temp;
            break;
        case RD_VALUE:
            if (copy_to_user((int __user *)arg, &value, sizeof(value)))
                return -EFAULT;
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}

static struct file_operations fops = {
    .unlocked_ioctl = device_ioctl,
};

static int __init my_init(void) {
    return register_chrdev(0, "mydevice", &fops);
}

static void __exit my_exit(void) {
    unregister_chrdev(0, "mydevice");
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
```

#### Windows Support

`ioctl` is not supported on Windows, as it’s a Unix-specific system call. Windows uses device I/O control via
`DeviceIoControl` for similar purposes, but it’s not a direct equivalent and uses different semantics.

#### Notes

- **Portability**: Varies across Unix systems; Linux uses `_IO*` macros, while BSD may differ. POSIX defines `ioctl` for
  STREAMS (obsolescent).
- **Alternatives**: sysfs, netlink, or specific syscalls like `tcsetattr` in Linux.
- **Security**: Kernel-side validation is critical to avoid crashes or escalations.

---

### 2. fcntl (Unix/Linux)

#### Description

`fcntl` (File Control) is a Unix system call for manipulating file descriptor attributes, including file status flags,
locks, and some I/O controls. It overlaps with `ioctl` for socket operations (e.g., non-blocking mode) but is more
general for file operations. It’s part of POSIX and more portable than `ioctl`.

#### Syntax

```c
#include <fcntl.h>
int fcntl(int fd, int cmd, ...);
```

- **fd**: File descriptor.
- **cmd**: Operation (e.g., `F_GETFL`, `F_SETFL`).
- **... (arg)**: Optional argument, typically `int` or a pointer (e.g., `struct flock` for locking).

#### Common Commands

- **File Status**:
    - `F_GETFL`: Get file status flags (e.g., `O_NONBLOCK`).
    - `F_SETFL`: Set flags (e.g., enable `O_NONBLOCK`).
- **File Locking**:
    - `F_GETLK`: Test a lock.
    - `F_SETLK`: Set or clear a lock.
    - `F_SETLKW`: Set lock, wait if blocked.
- **Descriptor Management**:
    - `F_DUPFD`: Duplicate descriptor.
    - `F_GETFD`/`F_SETFD`: Get/set close-on-exec flag (`FD_CLOEXEC`).
- **Socket-Specific** (overlaps with `ioctl`):
    - `FIONBIO`: Set/clear non-blocking mode.
    - `FIONREAD`: Get readable bytes.
    - `FIOASYNC`: Enable/disable async I/O.

#### Return Values

- **Success**: Varies (e.g., flags for `F_GETFL`, 0 for `F_SETFL`).
- **Failure**: -1, with `errno` (e.g., `EBADF`, `EINVAL`).

#### Common Errors

- `EBADF`: Invalid `fd`.
- `EINVAL`: Invalid `cmd` or `arg`.
- `EAGAIN`/`EACCES`: Lock conflicts.
- `ENOLCK`: No lock resources.

#### Use Cases

- **File Locking**: Advisory locking for multi-process file access.
- **Non-Blocking I/O**: Setting `O_NONBLOCK` for sockets or pipes.
- **Descriptor Management**: Duplicating or setting close-on-exec.
- **Signal-Driven I/O**: Enabling `FIOASYNC` for notifications.

#### Example: Set Non-Blocking Socket (Linux)

```cpp
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return 1;
    }
    int flags = fcntl(s, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get");
        close(s);
        return 1;
    }
    if (fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl set");
        close(s);
        return 1;
    }
    printf("Socket set to non-blocking\n");
    close(s);
    return 0;
}
```

#### Windows Support

`fcntl` is not supported on Windows. Windows uses `ioctlsocket` or `WSAIoctl` for socket operations and `CreateFile`/
`DeviceIoControl` for file/device control.

#### Notes

- **Portability**: POSIX-compliant, more portable than `ioctl`.
- **Overlap with ioctl**: For sockets, `FIONBIO` and `FIONREAD` are identical in `fcntl` and `ioctl`.
- **Best Practice**: Use `fcntl` for non-blocking settings over `ioctl` for portability.

---

### 3. ioctlsocket (Windows)

#### Description

`ioctlsocket` is a Winsock function for controlling socket I/O modes, providing BSD-compatible socket operations. It’s a
subset of Unix `ioctl` for sockets, supporting commands like non-blocking mode or readable data queries. It requires
Winsock initialization via `WSAStartup`.

#### Syntax

```c
#include <winsock2.h>
int ioctlsocket(SOCKET s, long cmd, u_long *argp);
```

- **s**: Socket descriptor.
- **cmd**: Command (e.g., `FIONBIO`).
- **argp**: Pointer to input/output parameter.

#### Common Commands

- **FIONBIO**: Enable/disable non-blocking mode (`*argp` non-zero to enable).
- **FIONREAD**: Get bytes available to read.
- **SIOCATMARK**: Check if out-of-band (OOB) data is available.

#### Return Values

- **Success**: 0.
- **Failure**: `SOCKET_ERROR` (-1); use `WSAGetLastError` for codes like `WSAENOTSOCK`, `WSAEFAULT`.

#### Common Errors

- `WSANOTINITIALISED`: Winsock not initialized.
- `WSAENETDOWN`: Network subsystem failed.
- `WSAENOTSOCK`: Invalid socket.
- `WSAEFAULT`: Invalid `argp`.
- `WSAEINVAL`: Invalid `cmd` or `argp`.

#### Use Cases

- **Non-Blocking Sockets**: Enabling `FIONBIO` for asynchronous I/O.
- **Data Queries**: Checking readable bytes with `FIONREAD`.
- **OOB Data**: Handling urgent data with `SIOCATMARK`.

#### Example: Non-Blocking Socket (Windows)

```cpp
#include <winsock2.h>
#include <stdio.h>

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        printf("socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    u_long iMode = 1; // Non-blocking
    if (ioctlsocket(s, FIONBIO, &iMode) != 0) {
        printf("ioctlsocket failed: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();
        return 1;
    }
    printf("Socket set to non-blocking\n");
    closesocket(s);
    WSACleanup();
    return 0;
}
```

#### Windows Support

- **Windows XP**: Fully supported (introduced in Windows Sockets 2.0, available since Windows 95/NT 4.0).
- **Later Versions**: Supported in all subsequent versions (Vista, 7, 8, 8.1, 10, 11, Server 2003–2022).
- **Requirements**: `Ws2_32.lib`, `Ws2_32.dll`.

#### Notes

- **Limitations**: Fewer commands than Unix `ioctl`; lacks `FIOASYNC` equivalent.
- **Alternative**: `WSAIoctl` for extended functionality.
- **Compatibility**: Aligns with BSD socket ioctls but requires `WSAStartup`.

---

### 4. WSAIoctl (Windows)

#### Description

`WSAIoctl` is a more powerful Winsock function, extending `ioctlsocket` with support for Windows-specific and
BSD-compatible socket operations. It handles a broader range of commands, including network interface control, QoS, and
provider-specific extensions.

#### Syntax

```c
#include <winsock2.h>
int WSAIoctl(
    SOCKET s,
    DWORD dwIoControlCode,
    LPVOID lpvInBuffer,
    DWORD cbInBuffer,
    LPVOID lpvOutBuffer,
    DWORD cbOutBuffer,
    LPDWORD lpcbBytesReturned,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);
```

- **s**: Socket descriptor.
- **dwIoControlCode**: 32-bit control code (e.g., `SIO_GET_INTERFACE_LIST`).
- **lpvInBuffer**, **cbInBuffer**: Input buffer and size.
- **lpvOutBuffer**, **cbOutBuffer**: Output buffer and size.
- **lpcbBytesReturned**: Bytes written to output buffer.
- **lpOverlapped**, **lpCompletionRoutine**: For asynchronous operation (optional).

#### Common Commands

- **BSD-Compatible**:
    - `FIONBIO`, `FIONREAD`, `SIOCATMARK` (same as `ioctlsocket`).
- **Windows-Specific**:
    - `SIO_RCVALL`: Enable promiscuous mode (raw sockets).
    - `SIO_KEEPALIVE_VALS`: Configure TCP keep-alive.
    - `SIO_GET_INTERFACE_LIST`: List network interfaces.
    - `SIO_ADDRESS_LIST_QUERY`: Get local addresses.
    - `SIO_ROUTING_INTERFACE_QUERY`: Get routing interface.
- **QoS and Extensions**:
    - `SIO_SET_QOS`: Set Quality of Service parameters.
    - `SIO_GET_EXTENSION_FUNCTION_POINTER`: Retrieve function pointers for Winsock extensions.

#### Return Values

- **Success**: 0; `lpcbBytesReturned` indicates output size.
- **Failure**: `SOCKET_ERROR`; use `WSAGetLastError`.

#### Common Errors

- Similar to `ioctlsocket`, plus:
    - `WSAEINVAL`: Invalid buffer sizes or control code.
    - `WSAEFAULT`: Invalid buffer pointers.
    - `WSAEOPNOTSUPP`: Unsupported operation.

#### Use Cases

- **Network Monitoring**: `SIO_RCVALL` for packet sniffing (requires admin privileges).
- **Interface Management**: Querying interfaces with `SIO_GET_INTERFACE_LIST`.
- **Advanced Socket Config**: Keep-alive settings or QoS.
- **Extensibility**: Accessing Winsock extension functions.

#### Example: Get Interface List (Windows)

```cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        printf("socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    char buffer[1024];
    DWORD bytesReturned;
    if (WSAIoctl(s, SIO_GET_INTERFACE_LIST, NULL, 0, buffer, sizeof(buffer), &bytesReturned, NULL, NULL) == SOCKET_ERROR) {
        printf("WSAIoctl failed: %d\n", WSAGetLastError());
        closesocket(s);
        WSACleanup();
        return 1;
    }
    INTERFACE_INFO *pInfo = (INTERFACE_INFO *)buffer;
    int count = bytesReturned / sizeof(INTERFACE_INFO);
    for (int i = 0; i < count; i++) {
        printf("Interface %d: Flags=0x%lx\n", i, pInfo[i].iiFlags);
    }
    closesocket(s);
    WSACleanup();
    return 0;
}
```

#### Windows Support

- **Windows XP**: Supported (introduced in Winsock 2.0, available since Windows 95/NT 4.0).
- **Later Versions**: Supported in Vista, 7, 8, 8.1, 10, 11, Server 2003–2022.
- **Requirements**: `Ws2_32.lib`, `Ws2_32.dll`.

#### Notes

- **Superset of ioctlsocket**: Handles all `ioctlsocket` commands plus more.
- **Asynchronous Support**: Uses overlapped I/O for scalability.
- **Extensibility**: Supports vendor-specific codes via `SIO_GET_EXTENSION_FUNCTION_POINTER`.

---

### 5. Related Functions

#### setsockopt/getsockopt

- **Description**: Set/get socket options (e.g., `SO_REUSEADDR`, `SO_KEEPALIVE`). More portable than `ioctl`/
  `ioctlsocket` for socket configuration.
- **Use Cases**: Setting timeouts, buffer sizes, or enabling keep-alive.
- **Commands**:
    - `SO_REUSEADDR`: Reuse local addresses.
    - `SO_LINGER`: Control socket close behavior.
    - `SO_SNDBUF`/`SO_RCVBUF`: Set buffer sizes.
- **Windows Support**: Fully supported in Windows XP and later (Winsock 1.1+).
- **Example**:

```cpp
#include <winsock2.h>
#include <stdio.h>

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int enable = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&enable, sizeof(enable)) == SOCKET_ERROR) {
        printf("setsockopt failed: %d\n", WSAGetLastError());
    } else {
        printf("SO_REUSEADDR enabled\n");
    }
    closesocket(s);
    WSACleanup();
    return 0;
}
```

#### tcsetattr/tcgetattr

- **Description**: POSIX functions for terminal attribute control (e.g., baud rate, echo). Replaces `ioctl` for terminal
  settings in portable code.
- **Use Cases**: Configuring serial ports or terminal modes.
- **Windows Support**: Not supported; use Windows Console API (`SetConsoleMode`).
- **Example** (Unix):

```cpp
#include <termios.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    struct termios term;
    if (tcgetattr(STDIN_FILENO, &term) == -1) {
        perror("tcgetattr");
        return 1;
    }
    term.c_lflag &= ~ECHO; // Disable echo
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) == -1) {
        perror("tcsetattr");
        return 1;
    }
    printf("Echo disabled\n");
    return 0;
}
```

#### DeviceIoControl (Windows)

- **Description**: Windows equivalent to `ioctl` for device control, used with file handles.
- **Use Cases**: Controlling drivers (e.g., USB, disk).
- **Windows Support**: Supported in Windows XP and later (Win32 API).
- **Example**: Not provided due to complexity; see MSDN for `IOCTL_DISK_GET_DRIVE_GEOMETRY`.

---

### Windows Version Support Summary

| Function                | Windows XP    | Vista         | 7             | 8/8.1         | 10/11         | Server 2003–2022 |
|-------------------------|---------------|---------------|---------------|---------------|---------------|------------------|
| **ioctl**               | Not supported | Not supported | Not supported | Not supported | Not supported | Not supported    |
| **fcntl**               | Not supported | Not supported | Not supported | Not supported | Not supported | Not supported    |
| **ioctlsocket**         | Supported     | Supported     | Supported     | Supported     | Supported     | Supported        |
| **WSAIoctl**            | Supported     | Supported     | Supported     | Supported     | Supported     | Supported        |
| **setsockopt**          | Supported     | Supported     | Supported     | Supported     | Supported     | Supported        |
| **getsockopt**          | Supported     | Supported     | Supported     | Supported     | Supported     | Supported        |
| **tcsetattr/tcgetattr** | Not supported | Not supported | Not supported | Not supported | Not supported | Not supported    |
| **DeviceIoControl**     | Supported     | Supported     | Supported     | Supported     | Supported     | Supported        |

- **Windows XP**: Supports Winsock 2.0 (`ioctlsocket`, `WSAIoctl`, `setsockopt`, `getsockopt`) and `DeviceIoControl`.
  Unix functions (`ioctl`, `fcntl`, `tcsetattr`) are unavailable; use Cygwin/MSYS2 for partial emulation.
- **Later Versions**: No changes in core functionality; additional `WSAIoctl` commands (e.g., `SIO_BSP_HANDLE`) added in
  newer versions.

---

### Detailed Notes

- **Portability**: Use `fcntl` or `setsockopt` for socket operations in cross-platform code. `ioctl` and `WSAIoctl` are
  less portable due to device-specific commands.
- **Security**: Kernel-side `ioctl` handlers must validate inputs to prevent crashes or privilege escalation. User-space
  `WSAIoctl` requires admin privileges for some commands (e.g., `SIO_RCVALL`).
- **Alternatives**: In Linux, sysfs or netlink reduce `ioctl` usage. In Windows, `WSAIoctl` is preferred over
  `ioctlsocket` for new code.
- **Performance**: `ioctl` and `WSAIoctl` involve kernel/user transitions, so minimize calls in hot paths.
- **Historical Context**: `ioctl` originated in Unix V7; `ioctlsocket` was introduced for BSD socket compatibility in
  Windows. `WSAIoctl` added Windows-specific extensions.

---

### Practical Considerations

- **Debugging**: Always check return values and error codes (`errno` or `WSAGetLastError`). `ENOTTY` in Unix often
  indicates an inapplicable command.
- **Thread Safety**: `ioctl` and `fcntl` are thread-safe but may have race conditions with shared descriptors.
  `ioctlsocket` and `WSAIoctl` are safe after `WSAStartup`.
- **Windows XP**: Ensure Winsock 2.2 for full `WSAIoctl` support; older versions may lack some commands.
- **Cross-Platform Code**: Use libraries like `libuv` or `Boost.Asio` to abstract differences.
