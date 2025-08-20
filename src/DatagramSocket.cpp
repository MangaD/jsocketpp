#include "jsocketpp/DatagramSocket.hpp"
#include "jsocketpp/internal/ScopedBlockingMode.hpp"
#include "jsocketpp/SocketException.hpp"
#include "jsocketpp/SocketTimeoutException.hpp"

#include <numeric>
#include <optional>

using namespace jsocketpp;

DatagramSocket::DatagramSocket(const Port localPort, const std::string_view localAddress,
                               const std::optional<std::size_t> recvBufferSize,
                               const std::optional<std::size_t> sendBufferSize,
                               const std::optional<std::size_t> internalBufferSize, const bool reuseAddress,
                               const int soRecvTimeoutMillis, const int soSendTimeoutMillis, const bool nonBlocking,
                               const bool dualStack, const bool autoBind, const bool autoConnect,
                               const std::string_view remoteAddress, const Port remotePort,
                               const int connectTimeoutMillis)
    : SocketOptions(INVALID_SOCKET), _internalBuffer(internalBufferSize.value_or(DefaultDatagramReceiveSize)),
      _port(localPort)
{
    const auto localAddrInfoPtr = internal::resolveAddress(
        localAddress, _port,
        // Specifies the address family: AF_UNSPEC allows both IPv4 (AF_INET) and IPv6 (AF_INET6)
        // addresses to be returned by getaddrinfo, enabling the server to support dual-stack networking
        // (accepting connections over both protocols). This makes the code portable and future-proof,
        // as it can handle whichever protocol is available on the system or preferred by clients.
        AF_UNSPEC,
        // Specifies the desired socket type. Setting it to `SOCK_DGRAM` requests a datagram socket (UDP),
        // which provides connectionless, unreliable, message-oriented communication—this is the standard
        // socket type for UDP. By specifying `SOCK_DGRAM`, the code ensures that only address results suitable
        // for UDP (as opposed to stream-based protocols like TCP, which use `SOCK_STREAM`) are returned by
        // `getaddrinfo`. This guarantees that the created socket will support the semantics required for UDP operation,
        // such as message delivery without connection establishment and no guarantee of delivery order or reliability.
        SOCK_DGRAM,
        // This field ensures that only address results suitable for UDP sockets are returned by
        // `getaddrinfo`. While `SOCK_DGRAM` already implies UDP in most cases, explicitly setting
        // `ai_protocol` to `IPPROTO_UDP` eliminates ambiguity and guarantees that the created socket
        // will use the UDP protocol. This is particularly important on platforms where multiple
        // protocols may be available for a given socket type, or where protocol selection is not
        // implicit. By specifying `IPPROTO_UDP`, the code ensures that the socket will provide
        // connectionless, unreliable, message-oriented communication as required for a UDP socket.
        IPPROTO_UDP,
        // This flag indicates that the returned socket addresses are intended for use in bind operations.
        // When `ai_flags` includes `AI_PASSIVE`, and the `node` argument to `getaddrinfo()` is `nullptr`,
        // the resulting address will be a "wildcard" address:
        // - For IPv4: INADDR_ANY
        // - For IPv6: in6addr_any
        //
        // This allows the socket to accept datagrams on **any local interface**, which is ideal for
        // server-side or bound sockets. If a specific `localAddress` is provided, `AI_PASSIVE` is ignored.
        //
        // Without this flag, `getaddrinfo()` would return addresses suitable for connecting to a peer,
        // not for binding a local endpoint.
        AI_PASSIVE);

    // Sort address candidates: prioritize IPv6 if dualStack is enabled, then IPv4 fallback
    std::vector<addrinfo*> sorted;
    for (addrinfo* p = localAddrInfoPtr.get(); p; p = p->ai_next)
        if (dualStack && p->ai_family == AF_INET6)
            sorted.push_back(p);
    for (addrinfo* p = localAddrInfoPtr.get(); p; p = p->ai_next)
        if (p->ai_family == AF_INET || !dualStack)
            sorted.push_back(p);

    // Attempt to create a socket using the sorted list of preferred addresses
    for (auto* p : sorted)
    {
        setSocketFd(::socket(p->ai_family, p->ai_socktype, p->ai_protocol));
        if (getSocketFd() != INVALID_SOCKET)
        {
            // If this is an IPv6 socket and dualStack is requested,
            // disable the IPV6_V6ONLY flag to allow it to receive IPv4-mapped datagrams as well.
            if (p->ai_family == AF_INET6)
            {
                try
                {
                    setIPv6Only(!dualStack); // false = allow dual-stack
                }
                catch (const SocketException&)
                {
                    cleanupAndRethrow();
                }
            }

            break; // success
        }
    }

    // If no socket could be created, throw an error
    if (getSocketFd() == INVALID_SOCKET)
        cleanupAndThrow(GetSocketError());

    const auto recvResolved = recvBufferSize.value_or(DefaultDatagramReceiveSize);
    const auto sendResolved = sendBufferSize.value_or(DefaultDatagramReceiveSize);
    const auto internalResolved = internalBufferSize.value_or(DefaultDatagramReceiveSize);

    // Apply socket-level configuration
    try
    {
        // Enable or disable SO_REUSEADDR before binding
        setReuseAddress(reuseAddress);

        // Apply the internal buffer size for high-level read<T>()/read<string>() usage
        setInternalBufferSize(internalResolved);

        // Apply OS-level buffer size to the receive buffer (SO_RCVBUF)
        setReceiveBufferSize(recvResolved);

        // Apply OS-level buffer size to the send buffer (SO_SNDBUF)
        setSendBufferSize(sendResolved);

        // Set optional socket timeouts
        if (soRecvTimeoutMillis >= 0)
            setSoRecvTimeout(soRecvTimeoutMillis);
        if (soSendTimeoutMillis >= 0)
            setSoSendTimeout(soSendTimeoutMillis);

        // Set the socket to non-blocking mode if requested
        if (nonBlocking)
            setNonBlocking(true);
    }
    catch (const SocketException&)
    {
        cleanupAndRethrow();
    }

    // If requested, bind immediately to the given address and port
    if (autoBind)
    {
        try
        {
            bind(localAddress, localPort);
        }
        catch (const SocketException&)
        {
            cleanupAndRethrow();
        }
    }

    if (autoConnect && !remoteAddress.empty() && remotePort != 0)
    {
        try
        {
            connect(remoteAddress, remotePort, connectTimeoutMillis);
        }
        catch (const SocketException&)
        {
            cleanupAndRethrow();
        }
    }
}

void DatagramSocket::cleanup()
{
    internal::tryCloseNoexcept(getSocketFd());
    setSocketFd(INVALID_SOCKET);
    _isBound = false;
    _isConnected = false;
}

void DatagramSocket::cleanupAndThrow(const int errorCode)
{
    cleanup();
    throw SocketException(errorCode, SocketErrorMessage(errorCode));
}

void DatagramSocket::cleanupAndRethrow()
{
    cleanup();
    throw;
}

DatagramSocket::~DatagramSocket() noexcept
{
    try
    {
        close();
    }
    catch (...)
    {
        // Suppress all exceptions to maintain noexcept guarantee.
        // TODO: Consider adding an internal flag or user-configurable error handler
        //       to report destructor-time errors in future versions.
    }
}

void DatagramSocket::close()
{
    internal::closeOrThrow(getSocketFd());
    setSocketFd(INVALID_SOCKET);
    _isBound = false;
    _isConnected = false;
}

void DatagramSocket::bind(const std::string_view localAddress, const Port localPort)
{
    if (_isConnected)
    {
        throw SocketException("DatagramSocket::bind(): socket is already connected");
    }

    if (_isBound)
    {
        throw SocketException("DatagramSocket::bind(): socket is already bound");
    }

    const auto result =
        internal::resolveAddress(localAddress, localPort, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, AI_PASSIVE);

    for (const addrinfo* p = result.get(); p != nullptr; p = p->ai_next)
    {
        if (::bind(getSocketFd(), p->ai_addr,
#ifdef _WIN32
                   static_cast<int>(p->ai_addrlen)
#else
                   p->ai_addrlen
#endif
                       ) == 0)
        {
            _isBound = true;
            return;
        }
    }

    const int error = GetSocketError();
    throw SocketException(error, SocketErrorMessage(error));
}

void DatagramSocket::bind(const Port localPort)
{
    // Empty host + AI_PASSIVE => family-appropriate wildcard (INADDR_ANY or in6addr_any)
    bind("", localPort);
}

void DatagramSocket::bind()
{
    // Empty host + AI_PASSIVE => family-appropriate wildcard
    bind("", 0);
}

void DatagramSocket::connect(const std::string_view host, const Port port, const int timeoutMillis)
{
    if (_isConnected)
    {
        throw SocketException("connect() called on an already-connected socket");
    }

    // Check FD_SETSIZE limit up-front if using select()
    if (timeoutMillis >= 0 && getSocketFd() >= FD_SETSIZE)
    {
        throw SocketException("connect(): socket descriptor exceeds FD_SETSIZE (" + std::to_string(FD_SETSIZE) +
                              "), select() cannot be used");
    }

    // Resolve the remote address
    const auto remoteInfo = internal::resolveAddress(host, port, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP);
    const addrinfo* target = remoteInfo.get();
    if (!target)
    {
        throw SocketException("connect() failed: could not resolve target address");
    }

    // Determine if we should use non-blocking connect logic
    const bool useNonBlocking = (timeoutMillis >= 0);

    // Automatically switch to non-blocking if needed, and restore original mode later
    // NOLINTNEXTLINE - Temporarily switch to non-blocking mode (RAII-reverted)
    std::optional<internal::ScopedBlockingMode> blockingGuard;
    if (useNonBlocking)
    {
        blockingGuard.emplace(getSocketFd(), true); // Set non-blocking temporarily
    }

    // Attempt to connect
    const int res = ::connect(getSocketFd(), target->ai_addr,
#ifdef _WIN32
                              static_cast<int>(target->ai_addrlen)
#else
                              target->ai_addrlen
#endif
    );

    if (res == SOCKET_ERROR)
    {
        auto error = GetSocketError();

        // On most platforms, these errors indicate a non-blocking connection in progress
#ifdef _WIN32
        const bool wouldBlock = (error == WSAEINPROGRESS || error == WSAEWOULDBLOCK);
#else
        const bool wouldBlock = (error == EINPROGRESS || error == EWOULDBLOCK);
#endif

        if (!useNonBlocking || !wouldBlock)
        {
            throw SocketException(error, SocketErrorMessage(error));
        }

        // Wait until socket becomes writable (connection ready or failed)
        timeval tv{};
        tv.tv_sec = timeoutMillis / 1000;
        tv.tv_usec = (timeoutMillis % 1000) * 1000;

        fd_set writeFds;
        FD_ZERO(&writeFds);
        FD_SET(getSocketFd(), &writeFds);

#ifdef _WIN32
        const int selectResult = ::select(0, nullptr, &writeFds, nullptr, &tv);
#else
        int selectResult;
        do
        {
            selectResult = ::select(getSocketFd() + 1, nullptr, &writeFds, nullptr, &tv);
        } while (selectResult < 0 && errno == EINTR);
#endif

        if (selectResult == 0)
            throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE,
                                         "Connection timed out after " + std::to_string(timeoutMillis) + " ms");
        if (selectResult < 0)
        {
            const int errorSelect = GetSocketError();
            throw SocketException(errorSelect, SocketErrorMessage(errorSelect));
        }

        // Even if select() reports writable, we must check if the connection actually succeeded
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        // SO_ERROR is always retrieved as int (POSIX & Windows agree on semantics)
        if (::getsockopt(getSocketFd(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len) < 0 ||
            so_error != 0)
        {
            throw SocketException(so_error, SocketErrorMessage(so_error));
        }
    }

    _isConnected = true;
    // Socket mode will be restored automatically via ScopedBlockingMode destructor
}

void DatagramSocket::disconnect()
{
    if (!_isConnected)
        return; // Already disconnected — no-op

    // Disconnect using AF_UNSPEC to break the peer association
    sockaddr_storage nullAddr{};
    nullAddr.ss_family = AF_UNSPEC;

    if (const int res = ::connect(getSocketFd(), reinterpret_cast<sockaddr*>(&nullAddr), sizeof(nullAddr));
        res == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    _isConnected = false;
}

void DatagramSocket::write(const std::string_view message) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write(std::string_view): socket is not open.");

    if (!_isConnected)
        throw SocketException(
            "DatagramSocket::write(std::string_view): socket is not connected. Use writeTo() instead.");

    // Guard the single datagram size for the connected peer.
    enforceSendCapConnected(message.size());

    if (message.empty())
        return;

    internal::sendExact(getSocketFd(), message.data(), message.size());
}

void DatagramSocket::write(const std::span<const std::byte> data) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write(std::span<const std::byte>): socket is not open.");

    if (!_isConnected)
        throw SocketException(
            "DatagramSocket::write(std::span<const std::byte>): socket is not connected. Use writeTo() instead.");

    enforceSendCapConnected(data.size());

    if (data.empty())
        return;

    internal::sendExact(getSocketFd(), data.data(), data.size());
}

void DatagramSocket::writeAll(const std::string_view message) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException(0, "DatagramSocket::writeAll(): socket is not open.");
    if (!isConnected())
        throw SocketException(0, "DatagramSocket::writeAll(): socket is not connected. Use writeTo() instead.");

    const std::size_t len = message.size();
    if (len == 0)
        return;

    enforceSendCapConnected(len);
    waitReady(Direction::Write, -1);
    internal::sendExact(getSocketFd(), message.data(), len);
}

void DatagramSocket::writeWithTimeout(const std::string_view data, const int timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException(0, "DatagramSocket::writeWithTimeout(): socket is not open.");
    if (!isConnected())
        throw SocketException(0, "DatagramSocket::writeWithTimeout(): socket is not connected. Use writeTo() instead.");

    const std::size_t len = data.size();
    if (len == 0)
        return;

    enforceSendCapConnected(len);
    waitReady(Direction::Write, timeoutMillis);
    internal::sendExact(getSocketFd(), data.data(), len);
}

void DatagramSocket::write(const DatagramPacket& packet) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write(DatagramPacket): socket is not open.");

    const std::size_t len = packet.buffer.size();
    if (len == 0)
        return;

    if (packet.hasDestination())
    {
        sendUnconnectedTo(packet.address, packet.port, packet.buffer.data(), len);
        return;
    }

    if (!isConnected())
        throw SocketException(
            "DatagramSocket::write(DatagramPacket): no destination specified and socket is not connected.");

    enforceSendCapConnected(len);
    internal::sendExact(getSocketFd(), packet.buffer.data(), len);
}

void DatagramSocket::writeFrom(const void* data, const std::size_t len) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException(0, "DatagramSocket::writeFrom(): socket is not open.");
    if (!isConnected())
        throw SocketException(0, "DatagramSocket::writeFrom(): socket is not connected. Use writeTo() instead.");

    if (len == 0)
        return;

    enforceSendCapConnected(len);
    internal::sendExact(getSocketFd(), data, len);
}

void DatagramSocket::writev(const std::span<const std::string_view> buffers) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException(0, "DatagramSocket::writev(): socket is not open.");
    if (!isConnected())
        throw SocketException(0, "DatagramSocket::writev(): socket is not connected. Use writeTo() instead.");

    // Compute total datagram size (sum of all fragments).
    std::size_t total = 0;
    for (const auto& b : buffers)
        total += b.size();

    if (total == 0)
        return; // nothing to send (empty datagram is valid, but we skip work)

    // Validate against UDP maxima for the connected peer (IPv4/IPv6-aware).
    enforceSendCapConnected(total);

    // Fast path: a single fragment — send it directly with no extra copy.
    if (buffers.size() == 1)
    {
        const auto& b = buffers.front();
        internal::sendExact(getSocketFd(), b.data(), b.size());
        return;
    }

    // General path: coalesce fragments into one contiguous buffer, then send once.
    // Note: We copy because UDP must be sent in a single syscall; if your platform
    // path uses sendmsg/WSASend with scatter-gather, you could add an SG path here.
    std::vector<std::byte> datagram;
    try
    {
        datagram.resize(total);
    }
    catch (const std::bad_alloc&)
    {
        throw SocketException(0, "DatagramSocket::writev(): allocation failed for datagram buffer of size " +
                                     std::to_string(total));
    }

    std::byte* dst = datagram.data();
    for (const auto& b : buffers)
    {
        if (!b.empty())
        {
            std::memcpy(dst, b.data(), b.size());
            dst += b.size();
        }
    }

    internal::sendExact(getSocketFd(), datagram.data(), datagram.size());
}

void DatagramSocket::writevAll(const std::span<const std::string_view> buffers) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException(0, "DatagramSocket::writevAll(): socket is not open.");
    if (!isConnected())
        throw SocketException(0, "DatagramSocket::writevAll(): socket is not connected. Use writeTo() instead.");

    // Compute the exact datagram size we intend to emit (sum of all fragments).
    std::size_t total = 0;
    for (const auto& b : buffers)
        total += b.size();

    if (total == 0)
        return; // empty datagram is valid; we choose to no-op for consistency

    // Enforce protocol-level maxima (IPv4/IPv6 aware) for the connected peer.
    enforceSendCapConnected(total);

    // Honor "All" semantics even on non-blocking sockets: wait until writable.
    // (-1 means wait indefinitely; adjust if you want a bounded internal timeout.)
    waitReady(Direction::Write, -1);

    // Fast path: a single fragment — send directly without extra allocation/copy.
    if (buffers.size() == 1)
    {
        const auto& b = buffers.front();
        internal::sendExact(getSocketFd(), b.data(), b.size());
        return;
    }

    // General path: coalesce fragments into one contiguous buffer, then send once.
    // (If you later add a scatter/gather path (sendmsg/WSASend), you can replace this copy.)
    std::vector<std::byte> datagram;
    try
    {
        datagram.resize(total);
    }
    catch (const std::bad_alloc&)
    {
        throw SocketException(0, "DatagramSocket::writevAll(): allocation failed for datagram buffer of size " +
                                     std::to_string(total));
    }

    std::byte* dst = datagram.data();
    for (const auto& b : buffers)
    {
        if (!b.empty())
        {
            std::memcpy(dst, b.data(), b.size());
            dst += b.size();
        }
    }

    internal::sendExact(getSocketFd(), datagram.data(), datagram.size());
}

void DatagramSocket::writeTo(const std::string_view host, const Port port, const std::string_view message) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::writeTo(host, port, std::string_view): socket is not open.");

    const std::size_t len = message.size();
    if (len == 0)
        return;

    sendUnconnectedTo(host, port, message.data(), len);
}

void DatagramSocket::writeTo(const std::string_view host, const Port port, const std::span<const std::byte> data) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::writeTo(host, port, std::span<const std::byte>): socket is not open.");

    const std::size_t len = data.size();
    if (len == 0)
        return;

    sendUnconnectedTo(host, port, data.data(), len);
}

std::size_t DatagramSocket::readIntoBuffer(char* buf, const std::size_t len, const DatagramReceiveMode mode,
                                           const int recvFlags, sockaddr_storage* outSrc, socklen_t* outSrcLen,
                                           std::size_t* outDatagramSz, bool* outTruncated) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readIntoBuffer(): socket is not open."); // message-only OK

    if ((buf == nullptr && len != 0) || (len == 0 && buf != nullptr))
        throw SocketException("DatagramSocket::readIntoBuffer(): invalid buffer/length.");

    // Fast path: avoid zero-length recv that could block on some platforms.
    if (len == 0)
    {
        if (outDatagramSz)
            *outDatagramSz = 0;
        if (outTruncated)
            *outTruncated = false;
        return 0;
    }

    // Preflight (safe clamp only — never exceeds len or MaxDatagramPayloadSafe)
    std::size_t request = (std::min) (len, MaxDatagramPayloadSafe);
    std::size_t probed = 0;

    if (mode != DatagramReceiveMode::NoPreflight)
    {
        try
        {
            if (const std::size_t exact = internal::nextDatagramSize(getSocketFd()); exact > 0)
            {
                probed = exact;
                request = (std::min) ((std::min) (probed, MaxDatagramPayloadSafe), len);
            }
        }
        catch (const SocketException&)
        {
            // degrade to NoPreflight
        }
    }

    if (outDatagramSz)
        *outDatagramSz = (probed > 0 ? probed : 0);
    if (outTruncated)
        *outTruncated = false;

    // EINTR-safe receive loop
    for (;;)
    {
        ssize_t n = SOCKET_ERROR;

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
        // --- POSIX path: use recvmsg to get precise truncation info ---
        ::msghdr msg{};
        ::iovec iov{};
        iov.iov_base = buf;
        iov.iov_len = request;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        if (outSrc)
        {
            msg.msg_name = outSrc;
            msg.msg_namelen = (outSrcLen ? *outSrcLen : 0);
        }

        int flags = recvFlags;
#if defined(__linux__)
        // On Linux, passing MSG_TRUNC causes recvmsg(...) to return the *full* datagram size,
        // even if it exceeds iov_len. msg_flags will still indicate MSG_TRUNC when truncated.
        flags |= MSG_TRUNC;
#endif

        n = ::recvmsg(getSocketFd(), &msg, flags);

        if (n == SOCKET_ERROR)
        {
            const int err = GetSocketError();
#ifndef _WIN32
            if (err == EINTR)
            {
                continue; // retry
            }
            // NOLINTNEXTLINE
            const bool wouldBlock = (err == EAGAIN || err == EWOULDBLOCK);
            const bool timedOut = wouldBlock; // SO_RCVTIMEO maps here on POSIX
#else
            const bool wouldBlock = (err == WSAEWOULDBLOCK);
            const bool timedOut = (err == WSAETIMEDOUT);
#endif
            if (timedOut)
                throw SocketTimeoutException();
            if (wouldBlock)
                throw SocketTimeoutException(err, SocketErrorMessage(err));
            throw SocketException(err, SocketErrorMessage(err));
        }

        // Success: compute copied bytes, truncation, and datagram size.
        if (outSrcLen)
            *outSrcLen = msg.msg_namelen;

        const bool msgTrunc = (msg.msg_flags & MSG_TRUNC) != 0;

#if defined(__linux__)
        // On Linux, n == FULL datagram size when MSG_TRUNC is passed.
        const auto fullSize = static_cast<std::size_t>(n);
        const std::size_t copied = (std::min) (request, fullSize);

        if (outTruncated)
            *outTruncated = msgTrunc && (fullSize > len);
        if (outDatagramSz && *outDatagramSz == 0)
            *outDatagramSz = fullSize;

        return copied;
#else
        // Other POSIX: n is the *copied* byte count. MSG_TRUNC indicates truncation occurred.
        const std::size_t copied = static_cast<std::size_t>(n);

        if (outTruncated)
            *outTruncated = msgTrunc || (copied == len);
        if (outDatagramSz && *outDatagramSz == 0)
            *outDatagramSz = copied; // full size unknown here

        return copied;
#endif

#else
        // --- Non-POSIX path (e.g., Windows): fallback to existing internal primitive ---
        n = internal::recvInto(getSocketFd(), std::span(reinterpret_cast<std::byte*>(buf), request), recvFlags, outSrc,
                               outSrcLen);

        if (n != SOCKET_ERROR)
        {
            const auto received = static_cast<std::size_t>(n);

            // Truncation reporting (best effort on this path)
            if (outTruncated)
            {
                if (probed > 0)
                {
                    *outTruncated = (probed > len);
                }
                else
                {
                    *outTruncated = (received == len); // heuristic
                }
            }

            if (outDatagramSz && *outDatagramSz == 0)
                *outDatagramSz = (probed > 0 ? probed : received);

            return received;
        }

        // Error path (Windows)
        const int err = GetSocketError();
        const bool wouldBlock = (err == WSAEWOULDBLOCK);
        const bool timedOut = (err == WSAETIMEDOUT);

        if (timedOut)
            throw SocketTimeoutException();
        if (wouldBlock)
            throw SocketTimeoutException(err, SocketErrorMessage(err));
        throw SocketException(err, SocketErrorMessage(err));
#endif
    }
}

DatagramReadResult DatagramSocket::read(DatagramPacket& packet, const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::read(DatagramPacket&,DatagramReadOptions): socket is not open.");

    DatagramReadResult result{};

    // Ensure some capacity exists (provision if allowed)
    std::size_t capacity = packet.size();
    if (capacity == 0)
    {
        if (opts.allowGrow)
        {
            constexpr std::size_t fallback = (std::min) (DefaultDatagramReceiveSize, MaxDatagramPayloadSafe);
            packet.resize(fallback);
            capacity = fallback;
        }
        else
        {
            throw SocketException("DatagramPacket buffer is empty; provide capacity or enable growth.");
        }
    }

    // Never request beyond our safety cap
    capacity = (std::min) (capacity, MaxDatagramPayloadSafe);

    // Decide whether a preflight would be beneficial:
    //  - explicit preflight mode, or
    //  - we want exact sizing to honor errorOnTruncate early, or
    //  - we’re allowed to grow and want to size precisely.
    const bool wantPreflight =
        (opts.mode != DatagramReceiveMode::NoPreflight) || opts.errorOnTruncate || opts.allowGrow;

    if (wantPreflight)
    {
        std::size_t probed = 0;
        try
        {
            if (const std::size_t exact = internal::nextDatagramSize(getSocketFd()); exact > 0)
            {
                probed = (std::min) (exact, MaxDatagramPayloadSafe);

                if (opts.allowGrow && probed > capacity)
                {
                    packet.resize(probed);
                    capacity = probed;
                }
                else if (opts.errorOnTruncate && capacity < probed)
                {
                    // Early, non-destructive failure — datagram remains queued.
                    throw SocketException("DatagramPacket buffer too small for incoming datagram (preflight).");
                }
                else
                {
                    // Keep current capacity but don’t ask for more than the datagram size.
                    capacity = (std::min) (capacity, probed);
                }
            }
        }
        catch (const SocketException&)
        {
            // Preflight not available; fall back to single recv below.
            probed = 0;
        }
    }

    // Receive exactly once via the low-level primitive.
    sockaddr_storage src{};
    auto srcLen = static_cast<socklen_t>(sizeof(src));
    std::size_t datagramSize = 0;
    bool truncated = false;

    const std::size_t n =
        readIntoBuffer(packet.buffer.data(), capacity,
                       /*mode=*/DatagramReceiveMode::NoPreflight, // avoid double work; we already probed if possible
                       opts.recvFlags, &src, &srcLen, &datagramSize, &truncated);

    // Enforce strict no-truncation if requested and we couldn’t fail early.
    if (opts.errorOnTruncate && (truncated || (datagramSize > 0 && datagramSize > capacity)))
    {
        throw SocketException("DatagramPacket receive truncated into destination buffer.");
    }

    // Fill result
    result.bytes = n;
    result.datagramSize = (datagramSize > 0 ? datagramSize : n);
    result.truncated = truncated;

    // Side effects as requested
    if (opts.updateLastRemote)
        rememberRemote(src, srcLen);

    if (opts.resolveNumeric)
        internal::resolveNumericHostPort(reinterpret_cast<const sockaddr*>(&src), srcLen, packet.address, packet.port);

    // Post-shrink (never shrink on truncation)
    if (opts.allowShrink && !truncated && packet.size() != n)
        packet.resize(n);

    return result;
}

[[nodiscard]] DatagramReadResult DatagramSocket::readInto(void* buffer, const std::size_t len,
                                                          const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readInto(void*,size_t): socket is not open.");

    if ((buffer == nullptr && len != 0) || (buffer != nullptr && len == 0))
        throw SocketException("DatagramSocket::readInto(void*,size_t): invalid buffer/length.");

    // Decide whether we can/should preflight size *here* to enforce early failure on truncation.
    // We only preflight here if:
    //  - caller requested PreflightSize, OR
    //  - caller allows NoPreflight but *demands* errorOnTruncate (so we try to avoid a post-read throw).
    const bool wantPreflight = (opts.mode != DatagramReceiveMode::NoPreflight) || opts.errorOnTruncate;

    std::size_t probed = 0;
    if (wantPreflight)
    {
        try
        {
            if (const std::size_t exact = internal::nextDatagramSize(getSocketFd()); exact > 0)
            {
                probed = exact;
                if (opts.errorOnTruncate && len < probed)
                {
                    // Early, non-destructive failure: datagram remains in the kernel queue.
                    throw SocketException(
                        "DatagramSocket::readInto(void*,size_t): buffer too small for incoming datagram.");
                }
            }
        }
        catch (const SocketException&)
        {
            // If the probe fails, we’ll fall back to single-recv below.
            probed = 0;
        }
    }

    // If we already probed, we can avoid double work by forcing NoPreflight for the actual read.
    const DatagramReceiveMode effectiveMode = (probed > 0 ? DatagramReceiveMode::NoPreflight : opts.mode);

    // Perform exactly one receive via the low-level primitive.
    sockaddr_storage src{};
    socklen_t srcLen = sizeof(src);
    std::size_t outSize = 0;
    bool truncated = false;

    const std::size_t n = readIntoBuffer(static_cast<char*>(buffer), len, effectiveMode,
                                         /*recvFlags=*/0, &src, &srcLen, &outSize, &truncated);

    // If caller demands strict no-truncation and we didn’t fail early, enforce now.
    if (opts.errorOnTruncate && (truncated || (outSize > 0 && outSize > len)))
    {
        throw SocketException("DatagramSocket::readInto(void*,size_t): datagram truncated into destination buffer.");
    }

    // Update "last remote" if requested and we have a source address.
    if (opts.updateLastRemote && srcLen > 0)
    {
        // If you resolve lazily, you can pass opts.resolveNumeric as needed.
        rememberRemote(src, srcLen);
    }

    DatagramReadResult res{};
    res.bytes = n;
    res.datagramSize = (outSize > 0 ? outSize : n);
    res.truncated = (truncated || (res.datagramSize > res.bytes));

    return res;
}

[[nodiscard]] DatagramReadResult DatagramSocket::readInto(std::span<char> out, const DatagramReadOptions& opts) const
{
    if (out.empty())
    {
        // Zero-length span: early exit, no syscalls, no state changes.
        return DatagramReadResult{0u, 0u, false};
    }
    return readInto(out.data(), out.size(), opts);
}

DatagramReadResult DatagramSocket::readExact(void* buffer, const std::size_t exactLen,
                                             const ReadExactOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readExact(void*,size_t): socket is not open.");
    if (buffer == nullptr)
        throw SocketException("DatagramSocket::readExact(void*,size_t): buffer is null.");
    if (exactLen == 0)
        throw SocketException("DatagramSocket::readExact(void*,size_t): exactLen must be > 0.");

    // Compose base options; prefer preflight so we can fail early on size policy.
    DatagramReadOptions base = opts.base;
    if (base.mode == DatagramReceiveMode::NoPreflight)
        base.mode = DatagramReceiveMode::PreflightSize;

    // Try to learn the datagram size first for early, non-destructive checks.
    std::size_t probed = 0;
    try
    {
        if (const std::size_t exact = internal::nextDatagramSize(getSocketFd()); exact > 0)
            probed = (std::min) (exact, MaxDatagramPayloadSafe);
    }
    catch (const SocketException&)
    {
        // No reliable probe; we’ll enforce policy after the read if needed.
        probed = 0;
    }

    // Early strict policy when we know the size.
    if (probed > 0 && opts.requireExact)
    {
        if (probed > exactLen)
        {
            // Oversized datagram can never satisfy "exact"
            throw SocketException("DatagramSocket::readExact: datagram larger than requested exactLen.");
        }
        if (probed < exactLen && !opts.padIfSmaller)
        {
            // Undersized and no padding allowed
            throw SocketException(
                "DatagramSocket::readExact: datagram smaller than requested exactLen and padding disabled.");
        }
    }

    // Avoid double preflight on the actual read if we already probed.
    if (probed > 0)
        base.mode = DatagramReceiveMode::NoPreflight;

    // If we might need to reject truncation post-read (when probe failed), enable strict truncation in the base.
    // - For requireExact: any oversize must be treated as an error → strict truncation.
    // - Otherwise: respect caller's existing base.errorOnTruncate (already part of base).
    if (probed == 0 && opts.requireExact)
        base.errorOnTruncate = true;

    // Perform the single receive via the policy-aware backbone.
    auto res = readInto(buffer, exactLen, base);

    // Post-read enforcement (covers probe-less and undersize-with-padding cases).
    if (opts.requireExact)
    {
        // If truncated (oversize) we must fail (strict "exact" semantics).
        if (res.truncated || (res.datagramSize > 0 && res.datagramSize > exactLen))
        {
            throw SocketException("DatagramSocket::readExact: datagram larger than requested exactLen.");
        }
        // If smaller than exactLen, either pad or fail.
        if (res.bytes < exactLen)
        {
            if (opts.padIfSmaller)
            {
                std::memset(static_cast<char*>(buffer) + res.bytes, 0, exactLen - res.bytes);
                // Adjust the reported byte count to reflect logical exact fill.
                res.bytes = exactLen;
            }
            else
            {
                throw SocketException("DatagramSocket::readExact: datagram smaller than requested exactLen.");
            }
        }
    }
    else
    {
        // Not strict: optionally pad when asked.
        if (opts.padIfSmaller && res.bytes < exactLen)
        {
            std::memset(static_cast<char*>(buffer) + res.bytes, 0, exactLen - res.bytes);
            res.bytes = exactLen;
        }
        // Truncation behavior (oversize) was already handled by base.errorOnTruncate.
    }

    return res;
}

DatagramReadResult DatagramSocket::readExact(std::span<char> out, const ReadExactOptions& opts) const
{
    if (out.empty())
        throw SocketException("DatagramSocket::readExact(span): span must have positive size.");
    return readExact(out.data(), out.size(), opts);
}

std::size_t DatagramSocket::readAtMost(const std::span<char> out, const DatagramReadOptions& opts) const
{
    // Zero-length destination: no syscalls, no side effects.
    if (out.empty())
        return 0;

    // Force single-recv, best-effort semantics; honor all other options (timeouts, updateLastRemote, etc.).
    DatagramReadOptions local = opts;
    local.mode = DatagramReceiveMode::NoPreflight;

    // Delegate to the policy-aware backbone; returns bytes copied (≤ out.size()).
    const auto res = readInto(out, local);
    return res.bytes;
}

std::string DatagramSocket::readAtMost(const std::size_t n) const
{
    if (n == 0)
        return {};

    std::string out;
    out.resize(n);

    DatagramReadOptions opts{};
    opts.mode = DatagramReceiveMode::NoPreflight; // single recv, best-effort

    const auto res = readInto(std::span(out.data(), out.size()), opts);
    out.resize(res.bytes);
    return out;
}

std::string DatagramSocket::readAvailable() const
{
    // Try to learn the exact size of the next datagram.
    std::size_t probed = 0;
    try
    {
        if (const std::size_t sz = internal::nextDatagramSize(getSocketFd()); sz > 0)
            probed = sz;
    }
    catch (const SocketException&)
    {
        // No reliable preflight on this platform/path; fall back below.
    }

    // Allocate either the exact size (when known) or the safe maximum.
    const std::size_t cap = (probed > 0) ? (std::min) (probed, MaxDatagramPayloadSafe) : MaxDatagramPayloadSafe;

    std::string out(cap, '\0');

    DatagramReadOptions opts{};
    // We already probed (or chose a safe cap), so perform exactly one recv.
    opts.mode = DatagramReceiveMode::NoPreflight;
    // Convenience API: return what we got; don't throw on truncation in the rare "oversize > safe cap" case.
    opts.errorOnTruncate = false;

    const auto res = readInto(std::span<char>(out.data(), out.size()), opts);
    out.resize(res.bytes);
    return out;
}

std::size_t DatagramSocket::readAvailable(std::span<char> out, const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readAvailable(span): socket is not open.");

    // Zero-length destination: no syscalls, no side effects.
    if (out.empty())
        return 0;

    // Try to learn the exact datagram size to enable early, non-destructive enforcement.
    std::size_t probed = 0;
    if (opts.mode != DatagramReceiveMode::NoPreflight || opts.errorOnTruncate)
    {
        try
        {
            const std::size_t sz = internal::nextDatagramSize(getSocketFd());
            if (sz > 0)
                probed = sz;
        }
        catch (const SocketException&)
        {
            // No reliable preflight available; fall back to single recv.
            probed = 0;
        }
    }

    // We'll always do exactly one recv on the actual read.
    constexpr auto effectiveMode = DatagramReceiveMode::NoPreflight;

    // Clamp request to caller capacity and our global safety cap.
    const std::size_t capacityCap = (std::min) (out.size(), MaxDatagramPayloadSafe);
    std::size_t request = capacityCap;

    // If we know the size up-front, enforce strict no-truncation before reading when requested.
    if (probed > 0)
    {
        if (opts.errorOnTruncate && probed > capacityCap)
            throw SocketException("DatagramSocket::readAvailable(span): buffer too small for incoming datagram.");
        request = (std::min) (probed, capacityCap);
    }

    // Perform the single receive via the low-level backbone.
    sockaddr_storage src{};
    auto srcLen = static_cast<socklen_t>(sizeof(src));
    std::size_t datagramSize = 0;
    bool truncated = false;

    const std::size_t n =
        readIntoBuffer(out.data(), request, effectiveMode, opts.recvFlags, &src, &srcLen, &datagramSize, &truncated);

    // If we couldn't fail early (no probe) and strict policy is requested, enforce now.
    if (opts.errorOnTruncate && (truncated || (datagramSize > 0 && datagramSize > out.size())))
        throw SocketException("DatagramSocket::readAvailable(span): datagram truncated into destination buffer.");

    // Bookkeeping for unconnected sockets.
    if (opts.updateLastRemote)
        rememberRemote(src, srcLen);

    return n;
}

std::size_t DatagramSocket::readIntoExact(void* buffer, const std::size_t len) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readIntoExact(void*,size_t): socket is not open.");
    if (buffer == nullptr)
        throw SocketException("DatagramSocket::readIntoExact(void*,size_t): buffer is null.");
    if (len == 0)
        throw SocketException("DatagramSocket::readIntoExact(void*,size_t): len must be > 0.");

    ReadExactOptions ex{};
    ex.requireExact = true;                            // must match exactly
    ex.padIfSmaller = false;                           // do not pad; enforce strict equality
    ex.base.mode = DatagramReceiveMode::PreflightSize; // prefer early, non-destructive failure

    const auto res = readExact(buffer, len, ex); // single datagram; throws on mismatch
    return res.bytes;                            // should equal len on success
}

std::string DatagramSocket::readAtMostWithTimeout(const std::size_t n, const int timeoutMillis) const
{
    if (n == 0)
        return {};
    if (!hasPendingData(timeoutMillis))
        throw SocketTimeoutException();
    return readAtMost(n);
}

void DatagramSocket::discard(const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::discard(): socket is not open.");

    // Minimal-copy drain: 1-byte scratch; kernel drops the remainder for oversized datagrams.
    char scratch;
    sockaddr_storage src{};
    auto srcLen = static_cast<socklen_t>(sizeof(src));
    std::size_t datagramSize = 0;
    bool truncated = false;

    (void) readIntoBuffer(&scratch,
                          /*len=*/1, DatagramReceiveMode::NoPreflight, opts.recvFlags, &src, &srcLen, &datagramSize,
                          &truncated);

    if (opts.updateLastRemote)
        rememberRemote(src, srcLen);
}

void DatagramSocket::discardExact(const std::size_t n, const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::discardExact(size_t): socket is not open.");
    if (n == 0)
        throw SocketException("DatagramSocket::discardExact(size_t): n must be > 0.");

    // Try a non-destructive preflight to check the size first.
    std::size_t probed = 0;
    try
    {
        if (const std::size_t sz = internal::nextDatagramSize(getSocketFd()); sz > 0)
            probed = sz;
    }
    catch (const SocketException&)
    {
        // Fall through: we will verify post-read.
        probed = 0;
    }

    sockaddr_storage src{};
    auto srcLen = static_cast<socklen_t>(sizeof(src));
    std::size_t datagramSize = 0;
    bool truncated = false;

    if (probed > 0)
    {
        if (probed != n)
            throw SocketException("DatagramSocket::discardExact(size_t): next datagram size mismatch.");

        // Size matches; drain with minimal copy.
        char scratch;
        (void) readIntoBuffer(&scratch,
                              /*len=*/1, DatagramReceiveMode::NoPreflight, opts.recvFlags, &src, &srcLen, &datagramSize,
                              &truncated);
    }
    else
    {
        // Unknown size: read up to n bytes and validate outcome strictly.
        // This copies at most n bytes, throws if oversize (truncated) or undersize.
        const std::unique_ptr<char[]> tmp(new char[(std::min) (n, MaxDatagramPayloadSafe)]);
        const std::size_t request = (std::min) (n, MaxDatagramPayloadSafe);

        const std::size_t got = readIntoBuffer(tmp.get(), request, DatagramReceiveMode::NoPreflight, opts.recvFlags,
                                               &src, &srcLen, &datagramSize, &truncated);

        // Strict exact-length checks post-read.
        if (truncated || (datagramSize > 0 && datagramSize > n))
            throw SocketException("DatagramSocket::discardExact(size_t): datagram larger than required size.");
        if (got < n)
            throw SocketException("DatagramSocket::discardExact(size_t): datagram smaller than required size.");
    }

    if (opts.updateLastRemote)
        rememberRemote(src, srcLen);
}

DatagramReadResult DatagramSocket::readv(const std::span<BufferView> buffers, const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readv(): socket is not open.");

    // Sum total capacity and validate pointers
    std::size_t totalCap = 0;
    for (const auto& [data, size] : buffers)
    {
        if (size > 0 && data == nullptr)
            throw SocketException("DatagramSocket::readv(): buffer view with null data and non-zero size.");
        if (size > 0)
        {
            // Avoid overflow (paranoid)
            if (totalCap > (std::numeric_limits<std::size_t>::max)() - size)
                throw SocketException("DatagramSocket::readv(): total buffer capacity overflow.");
            totalCap += size;
        }
    }

    if (totalCap == 0)
    {
        // No writable capacity: do not consume the datagram; mirror other zero-capacity reads.
        return DatagramReadResult{0u, 0u, false};
    }

    totalCap = (std::min) (totalCap, MaxDatagramPayloadSafe);

    // Optional preflight: enforce strict policy early when able.
    std::size_t probed = 0;
    if (opts.mode != DatagramReceiveMode::NoPreflight || opts.errorOnTruncate)
    {
        try
        {
            if (const std::size_t sz = internal::nextDatagramSize(getSocketFd()); sz > 0)
            {
                probed = sz;
                if (opts.errorOnTruncate && probed > totalCap)
                    throw SocketException("DatagramSocket::readv(): datagram larger than scatter capacity.");
            }
        }
        catch (const SocketException&)
        {
            probed = 0; // fall through
        }
    }

    // Build platform iovecs up to the request size
    const std::size_t toRequest = (probed > 0) ? (std::min) (probed, totalCap) : totalCap;

    sockaddr_storage src{};
    auto srcLen = static_cast<socklen_t>(sizeof(src));

    std::size_t copied = 0;
    std::size_t datagramSize = 0;
    bool truncated = false;

#if defined(_WIN32)
    // ---- Windows: WSARecvFrom + WSABUF[] ----
    std::vector<WSABUF> wbufs;
    wbufs.reserve(buffers.size());
    {
        std::size_t remaining = toRequest;
        for (const auto& [data, size] : buffers)
        {
            if (remaining == 0)
                break;
            if (size == 0)
                continue;
            WSABUF wb;
            wb.buf = static_cast<char*>(data);
            wb.len = static_cast<ULONG>((std::min) (size, remaining));
            wbufs.push_back(wb);
            remaining -= wb.len;
        }
    }

    DWORD flags = 0; // opts.recvFlags doesn't map cleanly to Windows; ignore here
    DWORD got = 0;
    int fromLen = sizeof(src);

    const int rc = ::WSARecvFrom(getSocketFd(), wbufs.data(), static_cast<DWORD>(wbufs.size()), &got, &flags,
                                 reinterpret_cast<sockaddr*>(&src), &fromLen, nullptr, nullptr);
    if (rc == SOCKET_ERROR)
    {
        const int err = GetSocketError();
        if (err == WSAEWOULDBLOCK)
            throw SocketTimeoutException(err, SocketErrorMessage(err));
        if (err == WSAETIMEDOUT)
            throw SocketTimeoutException();
        throw SocketException(err, SocketErrorMessage(err));
    }

    copied = static_cast<std::size_t>(got);
    srcLen = fromLen;

    // Best effort size/truncation inference
    if (probed > 0)
    {
        datagramSize = probed;
        truncated = (probed > toRequest);
    }
    else
    {
        datagramSize = copied;             // full size unknown; report copied
        truncated = (copied == toRequest); // heuristic
    }

#else
    // ---- POSIX: recvmsg + iovec[] ----
    std::vector<iovec> iov;
    iov.reserve(buffers.size());
    {
        std::size_t remaining = toRequest;
        for (const auto& [data, size] : buffers)
        {
            if (remaining == 0)
                break;
            if (size == 0)
                continue;
            iovec v{};
            v.iov_base = data;
            v.iov_len = (std::min) (size, remaining);
            iov.push_back(v);
            remaining -= v.iov_len;
        }
    }

    msghdr msg{};
    msg.msg_name = &src;
    msg.msg_namelen = sizeof(src);
    msg.msg_iov = iov.data();
    msg.msg_iovlen = iov.size();

    int flags = opts.recvFlags;
#if defined(__linux__)
    // On Linux, MSG_TRUNC makes the return value be the *full datagram size* even if truncated.
    flags |= MSG_TRUNC;
#endif

    for (;;)
    {
        if (const ssize_t n = ::recvmsg(getSocketFd(), &msg, flags); n >= 0)
        {
            const bool msgTrunc = (msg.msg_flags & MSG_TRUNC) != 0;
#if defined(__linux__)
            datagramSize = static_cast<std::size_t>(n);    // full size (due to MSG_TRUNC)
            copied = (std::min) (datagramSize, toRequest); // bytes actually copied into iovecs
            truncated = msgTrunc && (datagramSize > toRequest);
#else
            copied = static_cast<std::size_t>(n);          // bytes copied
            datagramSize = (probed > 0 ? probed : copied); // full size unknown without probe
            truncated = msgTrunc || (copied == toRequest && probed == 0);
#endif
            srcLen = msg.msg_namelen;
            break;
        }

        const int err = errno;
        if (err == EINTR)
            continue;
        // NOLINTNEXTLINE
        if (err == EAGAIN || err == EWOULDBLOCK)
        {
            throw SocketTimeoutException(err, SocketErrorMessage(err));
        }
        if (err == ETIMEDOUT)
        {
            throw SocketTimeoutException();
        }
        throw SocketException(err, SocketErrorMessage(err));
    }
#endif

    // Enforce strict no-truncation if requested and we didn’t fail early
    if (opts.errorOnTruncate && (truncated || (datagramSize > 0 && datagramSize > toRequest)))
        throw SocketException("DatagramSocket::readv(): datagram truncated into scatter buffers.");

    if (opts.updateLastRemote)
        rememberRemote(src, srcLen);

    DatagramReadResult res{};
    res.bytes = copied;
    res.datagramSize = datagramSize;
    res.truncated = truncated;
    res.src = src;
    res.srcLen = srcLen;
    return res;
}

DatagramReadResult DatagramSocket::readvAll(const std::span<BufferView> buffers, const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readvAll(): socket is not open.");

    // Force strict no-truncation semantics and prefer an early, non-destructive failure.
    DatagramReadOptions strict = opts;
    strict.errorOnTruncate = true;
    if (strict.mode == DatagramReceiveMode::NoPreflight)
        strict.mode = DatagramReceiveMode::PreflightSize;

    // Delegate to the policy-aware scatter-gather receive.
    // readv(...) already enforces errorOnTruncate early (with preflight) or late (post read).
    auto res = readv(buffers, strict);

    // Defensive: ensure result reflects non-truncation on success.
    res.truncated = false;
    return res;
}

DatagramReadResult DatagramSocket::readvAtMostWithTimeout(const std::span<BufferView> buffers, const int timeoutMillis,
                                                          const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readvAtMostWithTimeout(): socket is not open.");
    if (timeoutMillis < 0)
        throw SocketException("DatagramSocket::readvAtMostWithTimeout(): timeoutMillis must be >= 0.");

    // Validate buffers and compute aggregate capacity; mirror readv(...) behavior.
    std::size_t totalCap = 0;
    for (const auto& [data, size] : buffers)
    {
        if (size > 0 && data == nullptr)
            throw SocketException(
                "DatagramSocket::readvAtMostWithTimeout(): buffer view has null data with non-zero size.");
        if (size > 0)
        {
            if (totalCap > (std::numeric_limits<std::size_t>::max)() - size)
                throw SocketException("DatagramSocket::readvAtMostWithTimeout(): total buffer capacity overflow.");
            totalCap += size;
        }
    }

    // Nothing to write into: preserve "no syscalls" rule, don't consume a datagram.
    if (totalCap == 0)
        return DatagramReadResult{0u, 0u, false};

    // Wait for readability with a per-call timeout (does not touch socket-level timeouts).
    if (!hasPendingData(timeoutMillis))
        throw SocketTimeoutException();

    // Best-effort, single-recv semantics: force NoPreflight, accept truncation.
    DatagramReadOptions local = opts;
    local.mode = DatagramReceiveMode::NoPreflight;
    local.errorOnTruncate = false;

    return readv(buffers, local);
}

DatagramReadResult DatagramSocket::readvAllWithTotalTimeout(const std::span<BufferView> buffers,
                                                            const int totalTimeoutMillis,
                                                            const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readvAllWithTotalTimeout(): socket is not open.");
    if (totalTimeoutMillis < 0)
        throw SocketException("DatagramSocket::readvAllWithTotalTimeout(): totalTimeoutMillis must be >= 0.");

    // Fast capacity sanity check (mirrors readv/readvAll validation)
    for (const auto& [data, size] : buffers)
    {
        if (size > 0 && data == nullptr)
            throw SocketException(
                "DatagramSocket::readvAllWithTotalTimeout(): buffer view has null data with non-zero size.");
    }

    // Per-call readiness wait that does not alter socket-level SO_RCVTIMEO.
    if (!hasPendingData(totalTimeoutMillis))
        throw SocketTimeoutException();

    // Enforce strict no-truncation semantics and prefer early, non-destructive failure.
    DatagramReadOptions strict = opts;
    strict.errorOnTruncate = true;
    if (strict.mode == DatagramReceiveMode::NoPreflight)
        strict.mode = DatagramReceiveMode::PreflightSize;

    return readv(buffers, strict);
}

std::string DatagramSocket::getLocalIp(const bool convertIPv4Mapped) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getLocalIp() failed: socket is not open.");

    sockaddr_storage localAddr{};
    socklen_t addrLen = sizeof(localAddr);

    if (::getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&localAddr), &addrLen) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    return ipFromSockaddr(reinterpret_cast<const sockaddr*>(&localAddr), convertIPv4Mapped);
}

Port DatagramSocket::getLocalPort() const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getLocalPort() failed: socket is not open.");

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (::getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessage(error));
    }

    return portFromSockaddr(reinterpret_cast<const sockaddr*>(&addr));
}

std::string DatagramSocket::getLocalSocketAddress(const bool convertIPv4Mapped) const
{
    return getLocalIp(convertIPv4Mapped) + ":" + std::to_string(getLocalPort());
}

std::string DatagramSocket::getRemoteIp(const bool convertIPv4Mapped) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getRemoteIp() failed: socket is not open.");

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (isConnected())
    {
        if (::getpeername(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }
    }
    else
    {
        if (_remoteAddrLen == 0)
            throw SocketException("getRemoteIp() failed: no datagram received yet (unconnected socket).");

        addr = _remoteAddr;
        addrLen = _remoteAddrLen;
    }

    return ipFromSockaddr(reinterpret_cast<const sockaddr*>(&addr), convertIPv4Mapped);
}

Port DatagramSocket::getRemotePort() const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("getRemotePort() failed: socket is not open.");

    sockaddr_storage addr{};
    socklen_t addrLen = sizeof(addr);

    if (isConnected())
    {
        if (::getpeername(getSocketFd(), reinterpret_cast<sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
        {
            const int error = GetSocketError();
            throw SocketException(error, SocketErrorMessage(error));
        }
    }
    else
    {
        if (_remoteAddrLen == 0)
            throw SocketException("getRemotePort() failed: no datagram received yet (unconnected socket).");

        addr = _remoteAddr;
        addrLen = _remoteAddrLen;
    }

    return portFromSockaddr(reinterpret_cast<const sockaddr*>(&addr));
}

std::string DatagramSocket::getRemoteSocketAddress(const bool convertIPv4Mapped) const
{
    return getRemoteIp(convertIPv4Mapped) + ":" + std::to_string(getRemotePort());
}

void DatagramSocket::setInternalBufferSize(const std::size_t newLen)
{
    _internalBuffer.resize(newLen);
    _internalBuffer.shrink_to_fit();
}

DatagramReadResult DatagramSocket::peek(DatagramPacket& packet, const bool allowResize,
                                        const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::peek(): socket is not open.");

    // Ensure some capacity
    if (packet.buffer.empty())
    {
        if (!allowResize)
            throw SocketException("DatagramSocket::peek(): packet buffer is empty and resizing is disabled.");

        // Prefer exact sizing via probe
        std::size_t target = 0;
        try
        {
            if (const std::size_t sz = internal::nextDatagramSize(getSocketFd()); sz > 0)
                target = (std::min) (sz, MaxDatagramPayloadSafe);
        }
        catch (const SocketException&)
        {
            // Fall back to a reasonable default
            target = (std::min) (DefaultDatagramReceiveSize, MaxDatagramPayloadSafe);
        }
        if (target == 0)
            target = 1; // allow zero-length datagram cleanly
        packet.buffer.resize(target);
    }

    // Clamp capacity to safety cap
    const std::size_t request = (std::min) (packet.buffer.size(), MaxDatagramPayloadSafe);

    sockaddr_storage src{};
    auto srcLen = static_cast<socklen_t>(sizeof(src));

    std::size_t copied = 0;
    std::size_t datagramSize = 0;
    bool truncated = false;

#if defined(_WIN32)
    // ---- Windows: recvfrom + MSG_PEEK ----
    int flags = opts.recvFlags | MSG_PEEK;

    for (;;)
    {
        const ssize_t n = ::recvfrom(getSocketFd(), packet.buffer.data(), static_cast<int>(request), flags,
                                     reinterpret_cast<sockaddr*>(&src), &srcLen);
        if (n != SOCKET_ERROR)
        {
            copied = static_cast<std::size_t>(n);
            // Full size unknown on Windows without extra syscalls; best effort:
            datagramSize = copied;           // 0 if you prefer "unknown"; copied is OK here
            truncated = (copied == request); // heuristic
            break;
        }

        const int err = GetSocketError();
        if (err == WSAEWOULDBLOCK)
            throw SocketTimeoutException(err, SocketErrorMessage(err));
        if (err == WSAETIMEDOUT)
            throw SocketTimeoutException();
        if (err == WSAEINTR)
            continue; // rare
        throw SocketException(err, SocketErrorMessage(err));
    }
#else
    // ---- POSIX: recvmsg + MSG_PEEK (+ MSG_TRUNC on Linux for exact size) ----
    msghdr msg{};
    iovec iov{};
    iov.iov_base = packet.buffer.data();
    iov.iov_len = request;

    msg.msg_name = &src;
    msg.msg_namelen = sizeof(src);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    int flags = opts.recvFlags | MSG_PEEK;
#if defined(__linux__)
    flags |= MSG_TRUNC; // return value is full datagram size even if truncated
#endif

    for (;;)
    {
        if (const ssize_t n = ::recvmsg(getSocketFd(), &msg, flags); n >= 0)
        {
            const bool msgTrunc = (msg.msg_flags & MSG_TRUNC) != 0;
#if defined(__linux__)
            datagramSize = static_cast<std::size_t>(n);  // full size with MSG_TRUNC
            copied = (std::min) (datagramSize, request); // bytes copied into user buffer
            truncated = msgTrunc && (datagramSize > request);
#else
            copied = static_cast<std::size_t>(n); // bytes copied
            datagramSize = copied;                // full size unknown without probe
            truncated = msgTrunc || (copied == request);
#endif
            srcLen = msg.msg_namelen;
            break;
        }

        const int err = errno;
        if (err == EINTR)
            continue;
        // NOLINTNEXTLINE
        if (err == EAGAIN || err == EWOULDBLOCK)
        {
            throw SocketTimeoutException(err, SocketErrorMessage(err));
        }
        if (err == ETIMEDOUT)
        {
            throw SocketTimeoutException();
        }
        throw SocketException(err, SocketErrorMessage(err));
    }
#endif

    // Optional numeric resolution (does not update "last remote").
    if (opts.resolveNumeric)
        internal::resolveNumericHostPort(reinterpret_cast<const sockaddr*>(&src), srcLen, packet.address, packet.port);

    // Optionally shrink the user buffer to exactly the bytes we copied (do not shrink if truncated).
    if (allowResize && !truncated && packet.buffer.size() != copied)
        packet.buffer.resize(copied);

    DatagramReadResult res{};
    res.bytes = copied;
    res.datagramSize = datagramSize; // 0 if unknown; here we report copied as best effort
    res.truncated = truncated;
    res.src = src;
    res.srcLen = srcLen;
    return res;
}

bool DatagramSocket::hasPendingData(const int timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::hasPendingData(): socket is not open.");

#if defined(_WIN32)
    WSAPOLLFD pfd{};
    pfd.fd = getSocketFd();
    pfd.events = POLLIN | POLLRDNORM;

    const int rc = ::WSAPoll(&pfd, 1, timeoutMillis);
    if (rc == 0)
        return false; // timed out
    if (rc == SOCKET_ERROR)
    {
        const int err = GetSocketError();
        throw SocketException(err, SocketErrorMessage(err));
    }

    // Treat hard errors as exceptional; don’t silently report “readable”.
    if (pfd.revents & (POLLERR | POLLNVAL))
    {
        // Use a generic error; WSAPoll doesn’t set errno. Keep the call site consistent.
        throw SocketException(WSAENOTSOCK, SocketErrorMessage(WSAENOTSOCK));
    }

    return (pfd.revents & (POLLIN | POLLRDNORM)) != 0;
#else
    pollfd pfd{};
    pfd.fd = getSocketFd();
    pfd.events = POLLIN;

    for (;;)
    {
        const int rc = ::poll(&pfd, 1, timeoutMillis);
        if (rc > 0)
        {
            if (pfd.revents & (POLLERR | POLLNVAL))
            {
                // Translate to a sensible error code.
                const int err = errno ? errno : EBADF;
                throw SocketException(err, SocketErrorMessage(err));
            }
            // POLLHUP on UDP is rare; don’t treat as readable.
            return (pfd.revents & POLLIN) != 0;
        }
        if (rc == 0)
            return false; // timed out

        const int err = errno;
        if (err == EINTR)
            continue; // retry on signal
        throw SocketException(err, SocketErrorMessage(err));
    }
#endif
}

std::optional<int> DatagramSocket::getMTU() const
{
    if (getSocketFd() == INVALID_SOCKET)
    {
        throw SocketException("DatagramSocket::getMTU(): socket is not open.");
    }
#ifdef _WIN32
    const std::string localIp = internal::getBoundLocalIp(getSocketFd());

    // Allocate space for adapter list
    ULONG outBufLen = 15000;
    std::vector<BYTE> buffer(outBufLen);
    auto* adapterAddrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    constexpr ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    constexpr ULONG family = AF_UNSPEC;

    if (const DWORD ret = GetAdaptersAddresses(family, flags, nullptr, adapterAddrs, &outBufLen); ret != NO_ERROR)
    {
        return std::nullopt;
    }

    for (const IP_ADAPTER_ADDRESSES* adapter = adapterAddrs; adapter != nullptr; adapter = adapter->Next)
    {
        for (const IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast != nullptr;
             unicast = unicast->Next)
        {
            char addrBuf[NI_MAXHOST];
            const int result =
                getnameinfo(unicast->Address.lpSockaddr, static_cast<socklen_t>(unicast->Address.iSockaddrLength),
                            addrBuf, sizeof(addrBuf), nullptr, 0, NI_NUMERICHOST);
            if (result != 0)
            {
                continue;
            }

            if (internal::ipAddressesEqual(addrBuf, localIp))
            {
                return static_cast<int>(adapter->Mtu);
            }
        }
    }

    return std::nullopt;
#else
    sockaddr_storage localAddr{};
    socklen_t addrLen = sizeof(localAddr);
    if (getsockname(getSocketFd(), reinterpret_cast<sockaddr*>(&localAddr), &addrLen) != 0)
    {
        const int err = GetSocketError();
        throw SocketException(err, SocketErrorMessage(err));
    }

    // Convert IP to interface name via getnameinfo + getifaddrs
    char host[NI_MAXHOST] = {};
    if (getnameinfo(reinterpret_cast<sockaddr*>(&localAddr), addrLen, host, sizeof(host), nullptr, 0, NI_NUMERICHOST) !=
        0)
    {
        return std::nullopt;
    }

    ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1)
        return std::nullopt;

    std::optional<std::string> interfaceName;
    for (const ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
            continue;

        if (ifa->ifa_addr->sa_family == localAddr.ss_family)
        {
            char ifHost[NI_MAXHOST];
            if (getnameinfo(ifa->ifa_addr, sizeof(sockaddr_storage), ifHost, sizeof(ifHost), nullptr, 0,
                            NI_NUMERICHOST) == 0)
            {
                if (std::string(ifHost) == host)
                {
                    interfaceName = std::string(ifa->ifa_name);
                    break;
                }
            }
        }
    }

    freeifaddrs(ifaddr);

    if (!interfaceName)
        return std::nullopt;

    int fd = getSocketFd();
    ifreq ifr{};
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, interfaceName->c_str(), IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFMTU, &ifr) < 0)
        return std::nullopt;

    return ifr.ifr_mtu;
#endif
}

void DatagramSocket::waitReady(const Direction dir, const int timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
    {
        throw SocketException(0, "DatagramSocket::waitReady(): socket is not open.");
    }

    // Map Direction to platform poll events.
#if defined(_WIN32)
    WSAPOLLFD pfd{};
    pfd.fd = getSocketFd();
    pfd.events = 0;
    if (dir == Direction::Read || dir == Direction::ReadWrite)
        pfd.events |= POLLRDNORM;
    if (dir == Direction::Write || dir == Direction::ReadWrite)
        pfd.events |= POLLWRNORM;

    const int timeout = (timeoutMillis < 0) ? -1 : timeoutMillis;
    const int rc = ::WSAPoll(&pfd, 1, timeout);
#else
    pollfd pfd{};
    pfd.fd = getSocketFd();
    pfd.events = 0;
    if (dir == Direction::Read || dir == Direction::ReadWrite)
        pfd.events |= POLLIN;
    if (dir == Direction::Write || dir == Direction::ReadWrite)
        pfd.events |= POLLOUT;

    const int timeout = (timeoutMillis < 0) ? -1 : timeoutMillis;
    const int rc = ::poll(&pfd, 1, timeout);
#endif

    if (rc == 0)
    {
        // Timed out before desired readiness was signaled.
        throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE, "DatagramSocket::waitReady(): operation timed out.");
    }

    if (rc < 0)
    {
        const int err = GetSocketError();
        throw SocketException(err, SocketErrorMessage(err));
    }

    // rc > 0: some event(s) occurred. Verify that the requested readiness is present,
    // and handle exceptional conditions by surfacing SO_ERROR when available.
#if defined(_WIN32)
    const short wantRead = (dir == Direction::Read || dir == Direction::ReadWrite) ? POLLRDNORM : 0;
    const short wantWrite = (dir == Direction::Write || dir == Direction::ReadWrite) ? POLLWRNORM : 0;
    const short re = pfd.revents;
#else
    const short wantRead = (dir == Direction::Read || dir == Direction::ReadWrite) ? POLLIN : 0;
    const short wantWrite = (dir == Direction::Write || dir == Direction::ReadWrite) ? POLLOUT : 0;
    const short re = pfd.revents;
#endif

    // If an error-ish condition is flagged, promote it to a SocketException with SO_ERROR.
    if (re & (POLLERR | POLLNVAL | POLLHUP))
    {
        int soerr = 0;
#if defined(_WIN32)
        int optlen = static_cast<int>(sizeof(soerr));
        (void) ::getsockopt(getSocketFd(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soerr), &optlen);
#else
        auto optlen = static_cast<socklen_t>(sizeof(soerr));
        (void) ::getsockopt(getSocketFd(), SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&soerr), &optlen);
#endif
        if (soerr == 0)
            soerr = EIO; // fall back if no specific error reported
        throw SocketException(soerr, SocketErrorMessage(soerr));
    }

    // If we didn’t get the event we actually asked for, treat it as not-ready.
    if ((wantRead && !(re & wantRead)) && (wantWrite && !(re & wantWrite)))
    {
        // Very rare, but be strict: behave like a timeout-equivalent for the requested op.
        throw SocketTimeoutException(JSOCKETPP_TIMEOUT_CODE,
                                     "DatagramSocket::waitReady(): not ready for requested operation.");
    }

    // Ready for requested direction; return to caller.
}

void DatagramSocket::sendUnconnectedTo(const std::string_view host, const Port port, const void* data,
                                       const std::size_t len) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException(0, "DatagramSocket::sendUnconnectedTo(): socket is not open.");
    if (len == 0)
        return;

    // Resolve destination(s): AF_UNSPEC + UDP
    const auto addrInfo = internal::resolveAddress(host, port, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP);
    const addrinfo* head = addrInfo.get();

    bool attempted = false;
    int lastErr = 0;

    for (const addrinfo* ai = head; ai; ai = ai->ai_next)
    {
        // Skip families that cannot possibly carry this datagram.
        if (const std::size_t familyCap = (ai->ai_family == AF_INET6) ? MaxUdpPayloadIPv6 : MaxUdpPayloadIPv4;
            len > familyCap)
            continue;

        try
        {
            internal::sendExactTo(getSocketFd(), data, len, ai->ai_addr, ai->ai_addrlen);

            // Cache last destination (without marking the socket "connected").
            if (!_isConnected)
            {
                rememberRemote(*reinterpret_cast<const sockaddr_storage*>(ai->ai_addr), ai->ai_addrlen);
            }
            return; // success
        }
        catch (const SocketException&)
        {
            attempted = true;
            lastErr = GetSocketError();
            // Try next candidate
        }
    }

    // If we never attempted, nothing could possibly carry this payload.
    if (!attempted)
    {
        const bool fitsV6 = (len <= MaxUdpPayloadIPv6);
        const std::string msg = fitsV6 ? "Datagram payload exceeds IPv4 limit and no IPv6 candidates were available."
                                       : "Datagram payload exceeds the theoretical IPv6 UDP limit.";
        throw SocketException(0, msg);
    }

    // We attempted at least one send and failed → surface the last OS error.
    throw SocketException(lastErr, SocketErrorMessage(lastErr));
}
