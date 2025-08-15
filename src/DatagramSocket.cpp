#include "jsocketpp/DatagramSocket.hpp"
#include "jsocketpp/internal/ScopedBlockingMode.hpp"
#include "jsocketpp/SocketException.hpp"
#include "jsocketpp/SocketTimeoutException.hpp"

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
    : SocketOptions(INVALID_SOCKET), _internalBuffer(internalBufferSize.value_or(DefaultBufferSize)), _port(localPort)
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

    const auto recvResolved = recvBufferSize.value_or(DefaultBufferSize);
    const auto sendResolved = sendBufferSize.value_or(DefaultBufferSize);
    const auto internalResolved = internalBufferSize.value_or(DefaultBufferSize);

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
    throw SocketException(error, SocketErrorMessageWrap(error));
}

void DatagramSocket::bind(const Port localPort)
{
    bind("0.0.0.0", localPort);
}

void DatagramSocket::bind()
{
    bind("0.0.0.0", 0);
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
            throw SocketException(errorSelect, SocketErrorMessageWrap(errorSelect));
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

void DatagramSocket::write(const DatagramPacket& packet) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write(DatagramPacket): socket is not open.");

    if (packet.buffer.empty())
        return;

    // Case 1: Explicit destination provided in the packet
    if (!packet.address.empty() && packet.port != 0)
    {
        const auto ai = internal::resolveAddress(packet.address, packet.port, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP);

        internal::sendExactTo(getSocketFd(), packet.buffer.data(), packet.buffer.size(), ai->ai_addr,
#ifdef _WIN32
                              static_cast<int>(ai->ai_addrlen)
#else
                              ai->ai_addrlen
#endif
        );

        if (!_isConnected)
        {
            _remoteAddr = *reinterpret_cast<const sockaddr_storage*>(ai->ai_addr);
            _remoteAddrLen = static_cast<socklen_t>(ai->ai_addrlen);
        }
        return;
    }

    // Case 2: No destination — must be connected
    if (!_isConnected)
        throw SocketException(
            "DatagramSocket::write(DatagramPacket): no destination specified and socket is not connected.");

    internal::sendExact(getSocketFd(), packet.buffer.data(), packet.buffer.size());
}

void DatagramSocket::write(const std::string_view message) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write(std::string_view): socket is not open.");

    if (!_isConnected)
        throw SocketException("DatagramSocket::write(std::string_view): socket is not connected.");

    if (message.empty())
        return;

    internal::sendExact(getSocketFd(), message.data(), message.size());
}

void DatagramSocket::write(const std::span<const std::byte> data) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write(std::span<const std::byte>): socket is not open.");

    if (!_isConnected)
        throw SocketException("DatagramSocket::write(std::span<const std::byte>): socket is not connected.");

    if (data.empty())
        return;

    internal::sendExact(getSocketFd(), reinterpret_cast<const char*>(data.data()), data.size());
}

void DatagramSocket::write(const std::string_view message, const std::string_view host, const Port port) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::write(std::string_view, host, port): socket is not open.");

    if (message.empty())
        return;

    const auto addrInfo = internal::resolveAddress(host, port, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP);

    int lastError = 0;
    for (const addrinfo* ai = addrInfo.get(); ai != nullptr; ai = ai->ai_next)
    {
        try
        {
            internal::sendExactTo(getSocketFd(), message.data(), message.size(), ai->ai_addr,
                                  static_cast<socklen_t>(ai->ai_addrlen));

            // Mirror prior behavior: remember remote on success if not connected.
            if (!_isConnected)
            {
                _remoteAddr = *reinterpret_cast<const sockaddr_storage*>(ai->ai_addr);
                _remoteAddrLen = static_cast<socklen_t>(ai->ai_addrlen);
            }
            return; // success
        }
        catch (const SocketException&)
        {
            lastError = GetSocketError(); // keep last OS error; try next candidate
        }
    }

    // All candidates failed.
    throw SocketException(lastError, SocketErrorMessageWrap(lastError));
}

std::size_t DatagramSocket::readIntoBuffer(char* buf, const std::size_t len, const DatagramReceiveMode mode,
                                           const int recvFlags, sockaddr_storage* outSrc, socklen_t* outSrcLen,
                                           std::size_t* outDatagramSz, bool* outTruncated) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readIntoBuffer(): socket is not open."); // message-only OK

    if ((buf == nullptr && len != 0) || (len == 0 && buf != nullptr))
        throw SocketException("DatagramSocket::readIntoBuffer(): invalid buffer/length.");

    // Preflight (safe clamp only — never exceeds len or MaxDatagramPayloadSafe)
    std::size_t request = len;
    std::size_t probed = 0;

    if (mode != DatagramReceiveMode::NoPreflight)
    {
        try
        {
            if (const std::size_t exact = internal::nextDatagramSize(getSocketFd()); exact > 0)
            {
                probed = exact;
                request = (std::min) ((std::min) (probed, MaxDatagramPayloadSafe), request);
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

    // Receive (EINTR-safe)
    for (;;)
    {
        const ssize_t n = internal::recvInto(getSocketFd(), std::span(reinterpret_cast<std::byte*>(buf), request),
                                             recvFlags, outSrc, outSrcLen);

        if (n != SOCKET_ERROR)
        {
            const auto received = static_cast<std::size_t>(n);

            // Truncation signal
            if (outTruncated)
            {
                if (probed > 0)
                {
                    *outTruncated = (probed > len);
                }
                else if (len > 0)
                {
                    // Heuristic when not probed: if we filled the buffer, assume possible truncation.
                    *outTruncated = (received == len);
                }
            }

            if (outDatagramSz && *outDatagramSz == 0)
                *outDatagramSz = received;

            return received;
        }

        // Error path
        const int err = GetSocketError(); // platform-specific last error
#ifndef _WIN32
        if (err == EINTR)
            continue; // retry
        const bool wouldBlock = (err == EAGAIN || err == EWOULDBLOCK);
        const bool timedOut = wouldBlock; // SO_RCVTIMEO maps here on POSIX
#else
        const bool wouldBlock = (err == WSAEWOULDBLOCK);
        const bool timedOut = (err == WSAETIMEDOUT);
#endif

        // Map would-block and timeout to SocketTimeoutException, preserving the actual code when useful.
        if (timedOut)
        {
            // True timeout → standardized code/message (ETIMEDOUT/WSAETIMEDOUT)
            throw SocketTimeoutException();
        }
        if (wouldBlock)
        {
            // Non-blocking “no data yet” → still signal as a timeout-style condition, but keep real code.
            throw SocketTimeoutException(err, SocketErrorMessageWrap(err));
        }

        // Everything else → canonical two-arg throw
        throw SocketException(err, SocketErrorMessageWrap(err));
    }
}

DatagramReadResult DatagramSocket::read(DatagramPacket& packet, const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::read(DatagramPacket&,DatagramReadOptions): socket is not open.");

    DatagramReadResult result{};
    std::size_t capacity = packet.size();

    // Ensure some capacity exists (provision if allowed)
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

    // Optional preflight: only influences capacity and growth; the actual I/O goes through readIntoBuffer()
    if (opts.mode == DatagramReceiveMode::PreflightSize || opts.mode == DatagramReceiveMode::PreflightMax)
    {
        try
        {
            if (const std::size_t exact = internal::nextDatagramSize(getSocketFd()); exact > 0)
            {
                const std::size_t probe = (std::min) (exact, MaxDatagramPayloadSafe);

                if (opts.mode == DatagramReceiveMode::PreflightSize)
                {
                    if (opts.allowGrow && probe > capacity)
                    {
                        packet.resize(probe);
                        capacity = probe;
                    }
                    else
                    {
                        capacity = (std::min) (capacity, probe);
                    }
                }
                else // PreflightMax: never grow
                {
                    capacity = (std::min) (capacity, probe);
                }
            }
        }
        catch (const SocketException&)
        {
            // degrade gracefully
        }
    }

    // Receive via backbone
    sockaddr_storage src{};
    auto srcLen = static_cast<socklen_t>(sizeof(src));
    std::size_t datagramSize = 0;
    bool truncated = false;

    const std::size_t n = readIntoBuffer(packet.buffer.data(), capacity,
                                         DatagramReceiveMode::NoPreflight, // capacity already chosen above
                                         opts.recvFlags, &src, &srcLen, &datagramSize, &truncated);

    result.bytes = n;
    result.datagramSize = datagramSize ? datagramSize : n;
    result.truncated = truncated;
    result.src = src;
    result.srcLen = srcLen;

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

DatagramReadResult DatagramSocket::readInto(void* buffer, const std::size_t len, const DatagramReadOptions& opts) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::readInto(void*,size_t,DatagramReadOptions): socket is not open.");
    if (buffer == nullptr || len == 0)
        throw SocketException(
            "DatagramSocket::readInto(void*,size_t,DatagramReadOptions): destination buffer is empty.");

    DatagramReadResult result{};
    auto* buf = static_cast<char*>(buffer);

    // For raw buffers, there is no growth; preflight will only clamp the request.
    sockaddr_storage src{};
    auto srcLen = static_cast<socklen_t>(sizeof(src));
    std::size_t datagramSize = 0;
    bool truncated = false;

    // If connected and caller won’t need source address, we can skip capturing it:
    sockaddr_storage* outSrc = _isConnected ? nullptr : &src;
    socklen_t* outLen = _isConnected ? nullptr : &srcLen;

    const std::size_t n =
        readIntoBuffer(buf, len, opts.mode, opts.recvFlags, outSrc, outLen, &datagramSize, &truncated);

    result.bytes = n;
    result.datagramSize = datagramSize ? datagramSize : n;
    result.truncated = truncated;

    if (!_isConnected)
    {
        result.src = src;
        result.srcLen = srcLen;

        if (opts.updateLastRemote)
            rememberRemote(src, srcLen);
        // Note: no packet here to populate address/port fields; caller can use result.src.
    }

    return result;
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

    // Compose base options; default to preflighting so we can validate early when possible.
    DatagramReadOptions base = opts.base;
    if (base.mode == DatagramReceiveMode::NoPreflight)
        base.mode = DatagramReceiveMode::PreflightSize;

    // Raw-buffer call; capacity is exactLen (caller promises that much).
    auto res = readInto(buffer, exactLen, base);

    // Prefer probed size for validation when available; fall back to actual bytes + truncation flag.
    const bool knownProbed = (res.datagramSize != 0);
    const std::size_t fullSize = knownProbed ? res.datagramSize : res.bytes;

    // Enforce policy
    if (opts.requireExact)
    {
        // Too small?
        if (fullSize < exactLen)
        {
            if (opts.padIfSmaller)
            {
                // Caller’s buffer is already zero-filled or not; we do not write beyond res.bytes.
                // If they need explicit zeroing, they can memset the tail; we avoid extra work here.
            }
            else
            {
                throwSizeMismatch(exactLen, fullSize, knownProbed);
            }
        }

        // Too big?
        if (fullSize > exactLen)
        {
            if (opts.errorOnTruncate || !res.truncated)
            {
                // If OS didn’t truncate (because buffer was big) we still reject oversize unless allowed
                throwSizeMismatch(exactLen, fullSize, knownProbed);
            }
            // else: OS truncated to exactLen capacity already; allowed by policy
        }
    }
    else
    {
        // Not requiring exact: if the datagram is smaller and padding requested, zero the tail.
        if (opts.padIfSmaller && res.bytes < exactLen)
        {
            std::memset(static_cast<char*>(buffer) + res.bytes, 0, exactLen - res.bytes);
        }
    }

    return res;
}

std::string DatagramSocket::readAtMost(const std::size_t n) const
{
    if (n == 0)
        return {};

    DatagramPacket pkt(n);
    auto got = read(pkt, /*resizeBuffer=*/true, DatagramReceiveMode::NoPreflight);

    return {pkt.buffer.data(), got};
}

std::string DatagramSocket::readAvailable() const
{
    DatagramPacket pkt;
    read(pkt, /*resizeBuffer=*/true, DatagramReceiveMode::PreflightSize);
    return {pkt.buffer.data(), pkt.buffer.size()};
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
        throw SocketException(error, SocketErrorMessageWrap(error));
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
        throw SocketException(error, SocketErrorMessageWrap(error));
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
            throw SocketException(error, SocketErrorMessageWrap(error));
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
            throw SocketException(error, SocketErrorMessageWrap(error));
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

std::size_t DatagramSocket::peek(DatagramPacket& packet, const bool resizeBuffer) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::peek(): socket is not open.");

    // Automatically resize if buffer is empty and resizing is enabled
    if (packet.buffer.empty())
    {
        if (resizeBuffer)
        {
            packet.buffer.resize(_internalBuffer.size()); // mimic default receive buffer size
        }
        else
        {
            throw SocketException("DatagramSocket::peek(): packet buffer is empty and resizing is disabled.");
        }
    }

    sockaddr_storage srcAddr{};
    socklen_t addrLen = sizeof(srcAddr);

    int flags = MSG_PEEK;
#ifndef _WIN32
    flags |= MSG_NOSIGNAL;
#endif

    const ssize_t received = recvfrom(getSocketFd(), packet.buffer.data(),
#ifdef _WIN32
                                      static_cast<int>(packet.buffer.size()),
#else
                                      packet.buffer.size(),
#endif
                                      flags, reinterpret_cast<sockaddr*>(&srcAddr), &addrLen);

    const int error = GetSocketError();
    if (received == SOCKET_ERROR)
        throw SocketException(error, SocketErrorMessageWrap(error));

    if (received == 0)
        throw SocketException("DatagramSocket::peek(): connection closed by remote host.");

    // Resolve sender's address info (this does NOT update _remoteAddr)
    char hostBuf[NI_MAXHOST], portBuf[NI_MAXSERV];
    const int ret = getnameinfo(reinterpret_cast<const sockaddr*>(&srcAddr), addrLen, hostBuf, sizeof(hostBuf), portBuf,
                                sizeof(portBuf), NI_NUMERICHOST | NI_NUMERICSERV);
    if (ret != 0)
    {
#ifdef _WIN32
        throw SocketException(GetSocketError(), SocketErrorMessageWrap(GetSocketError(), true));
#else
        throw SocketException(ret, SocketErrorMessageWrap(ret, true));
#endif
    }

    packet.address = std::string(hostBuf);
    packet.port = static_cast<Port>(std::stoul(portBuf));

    if (resizeBuffer)
        packet.buffer.resize(static_cast<std::size_t>(received));

    return static_cast<std::size_t>(received);
}

bool DatagramSocket::hasPendingData(const int timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::hasPendingData(): socket is not open.");

    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(getSocketFd(), &readFds);

    timeval tv{};
    if (timeoutMillis >= 0)
    {
        tv.tv_sec = timeoutMillis / 1000;
        tv.tv_usec = (timeoutMillis % 1000) * 1000;
    }

    const int result = select(
#ifdef _WIN32
        0, // unused on Windows
#else
        getSocketFd() + 1,
#endif
        &readFds, nullptr, nullptr, timeoutMillis >= 0 ? &tv : nullptr);

    const int error = GetSocketError();
    if (result == SOCKET_ERROR)
        throw SocketException(error, SocketErrorMessageWrap(error));

    return result > 0 && FD_ISSET(getSocketFd(), &readFds);
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
        throw SocketException(err, SocketErrorMessageWrap(err));
    }

    // Convert IP to interface name via getnameinfo + getifaddrs
    char host[NI_MAXHOST] = {};
    if (getnameinfo(reinterpret_cast<sockaddr*>(&localAddr), addrLen, host, sizeof(host), nullptr, 0, NI_NUMERICHOST) !=
        0)
    {
        return std::nullopt;
    }

    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1)
        return std::nullopt;

    std::optional<std::string> interfaceName;
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
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
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, interfaceName->c_str(), IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFMTU, &ifr) < 0)
        return std::nullopt;

    return ifr.ifr_mtu;
#endif
}

bool DatagramSocket::waitReady(const bool forWrite, const int timeoutMillis) const
{
    if (getSocketFd() == INVALID_SOCKET)
        throw SocketException("DatagramSocket::waitReady(): socket is not open.");

    if (getSocketFd() >= FD_SETSIZE)
    {
        throw SocketException("waitReady(): socket descriptor exceeds FD_SETSIZE (" + std::to_string(FD_SETSIZE) + ")");
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(getSocketFd(), &fds);

    timeval tv{0, 0};
    if (timeoutMillis >= 0)
    {
        tv.tv_sec = timeoutMillis / 1000;
        tv.tv_usec = (timeoutMillis % 1000) * 1000;
    }

#ifdef _WIN32
    const int result = select(0, forWrite ? nullptr : &fds, forWrite ? &fds : nullptr, nullptr, &tv);
#else
    int result;
    do
    {
        result = select(getSocketFd() + 1, forWrite ? nullptr : &fds, forWrite ? &fds : nullptr, nullptr, &tv);
    } while (result < 0 && errno == EINTR);
#endif

    if (result < 0)
    {
        const int error = GetSocketError();
        throw SocketException(error, SocketErrorMessageWrap(error));
    }

    return result > 0;
}
