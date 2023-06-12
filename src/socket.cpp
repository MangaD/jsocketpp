/*
 * File: socket.cpp
 *
 * Contains the implementation of 'ServerSocket' and 'Socket' classes that attempt to provide
 * an higher level of abstraction for sockets to ease their use.  Also, compatibility between
 * Windows and Linux operating systems.
 *
 * Author: David Gonçalves (MangaD)
 * Code review: Adrian Bibby Walther (Someone else)
 *
 */

#include "socket.hpp"

#include <exception>//SocketErrorMessageWrap
#include <cstring>//Use memset()

using namespace sock;

#ifdef _WIN32
std::string sock::SocketErrorMessage(int error) {

	if(error == 0)
		return std::string();

	LPSTR buffer = nullptr;
	size_t size;

	if( (size = FormatMessageA(
	        FORMAT_MESSAGE_ALLOCATE_BUFFER | // allocate buffer on local heap for error text
	        FORMAT_MESSAGE_FROM_SYSTEM | // use system message tables to retrieve error text
	        FORMAT_MESSAGE_IGNORE_INSERTS, // Important! will fail otherwise, since we're not
	                                       //(and CANNOT) pass insertion parameters
	        nullptr, // NULL since source is internal message table
	                 // unused with FORMAT_MESSAGE_FROM_SYSTEM
	        error,
	        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),//MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
	        (LPTSTR) &buffer,
	        0, // minimum size for output buffer
	        nullptr ))
	   == 0)
	{
		std::cerr << "Format message failed: " << GetLastError() << std::endl;
		SetLastError(error);
		return std::string();
	}
	std::string errString(buffer, size);
	LocalFree(buffer);
	
	return errString;
}
#endif

std::string sock::SocketErrorMessageWrap(int error) {
	std::string errString{};
	try {
		errString = SocketErrorMessage(error);
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
	return errString;
}

//ServerSocket Constructor - creates a server socket
ServerSocket::ServerSocket (unsigned short port_)
    : port(port_)
{
	struct addrinfo hints;

	//The hints argument points to an addrinfo structure that specifies criteria for
	//selecting the socket address structures returned in the list by getaddrinfo
	memset( &hints, 0, sizeof(hints) );
	hints.ai_family = AF_UNSPEC;//IPv4 (AF_INET) or IPv6 (AF_INET6)
	hints.ai_socktype = SOCK_STREAM;//Used with TCP protocol
	hints.ai_protocol = IPPROTO_TCP;
	/* If the AI_PASSIVE flag is specified in hints.ai_flags, and node is
	 * NULL, then the returned socket addresses will be suitable for
	 * bind(2)ing a socket that will accept(2) connections.  The returned
	 * socket address will contain the "wildcard address" (INADDR_ANY for
	 * IPv4 addresses, IN6ADDR_ANY_INIT for IPv6 address).  The wildcard
	 * address is used by applications (typically servers) that intend to
	 * accept connections on any of the hosts's network addresses.  If node
	 * is not NULL, then the AI_PASSIVE flag is ignored.
	 */
	hints.ai_flags = AI_PASSIVE;

	// Resolve the local address and port to be used by the server
	std::string s = std::to_string(port_);
	char const *port_c = s.c_str();
	if ( getaddrinfo(nullptr, port_c, &hints, &srv_addrinfo) != 0 )
		throw socket_exception ( GetSocketError(), SocketErrorMessage(GetSocketError()) );

	//Create a SOCKET for the server to listen for client connections
	for(struct addrinfo *p = srv_addrinfo; p != nullptr; p = p->ai_next) {
		if(p->ai_family == AF_INET || p->ai_family == AF_INET6){
			if (serverSocket != INVALID_SOCKET) CloseSocket(serverSocket);
			serverSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if(p->ai_family == AF_INET6) {  //Preference of IPv6 over IPv4, creates a IPv6 socket, if possible, with IPV6_V6ONLY disabled
				int OptionValue = 0;		//so that it accepts IPv4 connections too.
				if(setsockopt(serverSocket,IPPROTO_IPV6,IPV6_V6ONLY,(const char*)&OptionValue,sizeof(OptionValue)) == SOCKET_ERROR)
					throw socket_exception ( GetSocketError(), SocketErrorMessage(GetSocketError()) );
				break;
			}
		}
	}
	if (serverSocket == INVALID_SOCKET) {
		int error = GetSocketError();
		freeaddrinfo(srv_addrinfo);
		throw socket_exception ( error, SocketErrorMessage(error) );
	}

	int optval = 1;
#ifdef _WIN32
	//Otherwise, it appears Windows would let the socket bind to a port already in use...
	//https://msdn.microsoft.com/en-us/library/windows/desktop/ms740668%28v=vs.85%29.aspx#WSAEADDRINUSE
	if(setsockopt(serverSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,(const char*) &optval, sizeof optval) == SOCKET_ERROR)
		throw socket_exception ( GetSocketError(), SocketErrorMessage(GetSocketError()) );
#else
	//Let the socket bind to a port that is not active. Otherwise "The reason you can't normally listen on the same port
	//right away is because the socket, though closed, remains in the 2MSL state for some amount of time (generally a few minutes).
	//http://stackoverflow.com/questions/4979425/difference-between-address-in-use-with-bind-in-windows-and-on-linux-errno
	if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,(const char*) &optval, sizeof optval) == SOCKET_ERROR)
		throw socket_exception ( GetSocketError(), SocketErrorMessage(GetSocketError()) );
#endif
}

//ServerSocket Destructor - closes a server socket
ServerSocket::~ServerSocket() {
	try{
		close();
	}
	catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
	}
}

void ServerSocket::close() {
	if(this->serverSocket != INVALID_SOCKET){
		if( CloseSocket(this->serverSocket) )
			throw socket_exception ( GetSocketError(), SocketErrorMessageWrap(GetSocketError()) );
		else
			this->serverSocket = INVALID_SOCKET;
	}
}

//ServerSocket bind function
void ServerSocket::bind() {
	struct addrinfo *addr_in_use = nullptr;
	for(struct addrinfo *p = srv_addrinfo; p != nullptr; p = p->ai_next) {
		if(p->ai_family == AF_INET)
			addr_in_use = p;
		else if(p->ai_family == AF_INET6){//Use IPv6 address if it exists.
			addr_in_use = p;
			break;
		}
	}
	if(addr_in_use == nullptr) throw socket_exception (0,"bind() invalid addrinfo");
	int res = ::bind( serverSocket, addr_in_use->ai_addr, addr_in_use->ai_addrlen);
	if (res == SOCKET_ERROR) {
		throw socket_exception ( GetSocketError(), SocketErrorMessage(GetSocketError()) );
	}
}

//ServerSocket listen function
void ServerSocket::listen() {
	int n = ::listen(serverSocket, SOMAXCONN);//SOMAXCONN - how many clients can be in queue.
	if (n == SOCKET_ERROR)
		throw socket_exception ( GetSocketError(), SocketErrorMessage(GetSocketError()) );
}

//ServerSocket accept function
Socket ServerSocket::accept() {
	//Create structure to hold client address (optional)
	struct sockaddr_storage cli_addr;
	socklen_t clilen;
	clilen = sizeof(cli_addr);

	SOCKET clientSocket = ::accept(serverSocket, (struct sockaddr *)&cli_addr, &clilen);
	if (clientSocket == INVALID_SOCKET)
		throw socket_exception ( GetSocketError(), SocketErrorMessage(GetSocketError()) );
	Socket client(clientSocket, cli_addr, clilen);

	return client;
}

//Socket Constructor - creates a client socket from the accept function of ServerSocket
Socket::Socket(SOCKET client, struct sockaddr_storage addr, socklen_t len)
    : remote_addr(addr), remote_addr_length(len)
{
	clientSocket = client;
}

//Socket Constructor - creates a client socket
Socket::Socket(std::string host, unsigned short port)
    : remote_addr{}, remote_addr_length(0)
{
	struct addrinfo hints;

	memset( &hints, 0, sizeof(hints) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the local address and port to be used by the server
	std::string s = std::to_string(port);
	char const *port_c = s.c_str();
	if ( getaddrinfo(host.c_str(), port_c, &hints, &cli_addrinfo) != 0 )
		throw socket_exception ( GetSocketError(), SocketErrorMessage(GetSocketError()) );

	clientSocket = socket(cli_addrinfo->ai_family, cli_addrinfo->ai_socktype, cli_addrinfo->ai_protocol);
	if (clientSocket == INVALID_SOCKET) {
		int error = GetSocketError();
		freeaddrinfo(cli_addrinfo);
		throw socket_exception ( error, SocketErrorMessage(error) );
	}

}

//Socket - connect to server
void Socket::connect() {
	// Should really try the next address returned by getaddrinfo
	// if the connect call failed
	struct addrinfo *p;
	for(p = cli_addrinfo; p != nullptr; p = p->ai_next) {
		// Connect to server.
		int res = ::connect( clientSocket, p->ai_addr, p->ai_addrlen);
		if (res == SOCKET_ERROR && p->ai_next == nullptr) {
			throw socket_exception ( GetSocketError(), SocketErrorMessage(GetSocketError()) );
		} else if (res == SOCKET_ERROR) continue;
		else break;
	}
}

//Socket Destructor - closes a client socket
Socket::~Socket() {
	try{
		close();
	}
	catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
	}
}

void Socket::close() {
	if(this->clientSocket != INVALID_SOCKET){
		if( CloseSocket(this->clientSocket) )
			throw socket_exception ( GetSocketError(), SocketErrorMessageWrap(GetSocketError()) );
		else
			this->clientSocket = INVALID_SOCKET;
	}
}

void Socket::shutdown() {
	int how;
#ifdef _WIN32
	how = SD_BOTH;
#else
	how = SHUT_RDWR;
#endif
	if (this->clientSocket != INVALID_SOCKET) {
		if( ::shutdown(this->clientSocket, how) )
			throw socket_exception ( GetSocketError(), SocketErrorMessageWrap(GetSocketError()) );
	}
}

//Socket - get IP address of client
//http://www.microhowto.info/howto/convert_an_ip_address_to_a_human_readable_string_in_c.html
std::string Socket::getRemoteSocketAddress() {
	std::string ip_port = "null";
	if(remote_addr_length > 0){
		if (remote_addr.ss_family==AF_INET6) {
			struct sockaddr_in6* addr6=(struct sockaddr_in6*)&remote_addr;
			if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
				struct sockaddr_in addr4;
				memset(&addr4,0,sizeof(addr4));
				addr4.sin_family=AF_INET;
				addr4.sin_port=addr6->sin6_port;
				memcpy(&addr4.sin_addr.s_addr,addr6->sin6_addr.s6_addr+12,sizeof(addr4.sin_addr.s_addr));
				memcpy(&remote_addr,&addr4,sizeof(addr4));
				remote_addr_length=sizeof(addr4);
			}
		}
		char ip_s[INET6_ADDRSTRLEN];
		char port_s[6];
		getnameinfo((struct sockaddr*)&remote_addr,remote_addr_length,ip_s,sizeof(ip_s),port_s,sizeof(port_s),NI_NUMERICHOST | NI_NUMERICSERV);

		ip_port = std::string(ip_s) + ":" + port_s;
	}
	return ip_port;
}

//Socket - write to client
int Socket::write(std::string message) {
	int flags = 0;
#ifndef _WIN32
	flags = MSG_NOSIGNAL;
#endif
	int len = static_cast<int>(send(clientSocket, message.c_str(), message.length(), flags));
	if (len == SOCKET_ERROR) throw socket_exception( GetSocketError(), SocketErrorMessage(GetSocketError()) );
	return len;
}

void Socket::setBufferLength(std::size_t newLen) {
	setBufferSize(newLen);
}

void Socket::setBufferSize(std::size_t newLen) {
	buffer.resize(newLen);
	buffer.shrink_to_fit();
}

#ifdef _WIN32
/**
 * Redefine because not available on Windows XP
 * http://stackoverflow.com/questions/13731243/what-is-the-windows-xp-equivalent-of-inet-pton-or-inetpton
 */
const char * sock::inet_ntop_aux(int af, const void *src, char *dst, socklen_t size)
{
	struct sockaddr_storage ss;
	unsigned long s = size;

	ZeroMemory(&ss, sizeof(ss));
	ss.ss_family = static_cast<short>(af);

	switch(af) {
		case AF_INET:
			((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
			break;
		case AF_INET6:
			((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
			break;
		default:
			return NULL;
	}
	/* cannot direclty use &size because of strict aliasing rules */
	return (WSAAddressToString((struct sockaddr *)&ss, sizeof(ss), nullptr, dst, &s) == 0)?dst : NULL;
}
#endif

std::vector<std::string> sock::getHostAddr() {
	std::vector<std::string> ips;

#ifdef _WIN32
	/* Declare and initialize variables */
	const int MAX_TRIES = 3;
	DWORD dwRetVal = 0;
	unsigned int i = 0;
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;// Set the flags to pass to GetAdaptersAddresses
	ULONG family = AF_UNSPEC;// default to unspecified address family (both)
	PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
	ULONG outBufLen = 15000;// Allocate a 15 KB buffer to start with.
	ULONG Iterations = 0;
	char buff[100];
	DWORD bufflen=100;
	PIP_ADAPTER_ADDRESSES pCurrAddresses = nullptr;
	PIP_ADAPTER_UNICAST_ADDRESS pUnicast = nullptr;
	PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = nullptr;
	PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = nullptr;
	
	do {
		pAddresses = (IP_ADAPTER_ADDRESSES *) HeapAlloc(GetProcessHeap(), 0, (outBufLen));
		if (pAddresses == nullptr) {
			std::cerr << "Memory allocation failed for IP_ADAPTER_ADDRESSES struct." << std::endl;
			return ips;
		}
		dwRetVal = GetAdaptersAddresses(family, flags, nullptr, pAddresses, &outBufLen);
		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			HeapFree(GetProcessHeap(), 0, (pAddresses));
			pAddresses = nullptr;
		} else {
			break;
		}
		Iterations++;
	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));
	
	if (dwRetVal == NO_ERROR) {
		// If successful, output some information from the data we received
		pCurrAddresses = pAddresses;
		while (pCurrAddresses) {
			pUnicast = pCurrAddresses->FirstUnicastAddress;
			if (pUnicast) {
				for (i = 0; pUnicast != nullptr; i++) {
					if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
					{
						sockaddr_in *sa_in = (sockaddr_in *)pUnicast->Address.lpSockaddr;
						ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv4 Address " +
						                 inet_ntop_aux(AF_INET,&(sa_in->sin_addr),buff,bufflen));
					}
					else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6)
					{
						sockaddr_in6 *sa_in6 = (sockaddr_in6 *)pUnicast->Address.lpSockaddr;
						ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv6 Address " +
						                 inet_ntop_aux(AF_INET6,&(sa_in6->sin6_addr),buff,bufflen));
					}
					//else{printf("\tUNSPEC");}
					pUnicast = pUnicast->Next;
				}
			}
	
			pAnycast = pCurrAddresses->FirstAnycastAddress;
			if (pAnycast) {
				for (i = 0; pAnycast != nullptr; i++) {
					if (pAnycast->Address.lpSockaddr->sa_family == AF_INET)
					{
						sockaddr_in *sa_in = (sockaddr_in *)pAnycast->Address.lpSockaddr;
						ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv4 Address " +
						                 inet_ntop_aux(AF_INET,&(sa_in->sin_addr),buff,bufflen));
					}
					else if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6)
					{
						sockaddr_in6 *sa_in6 = (sockaddr_in6 *)pAnycast->Address.lpSockaddr;
						ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv6 Address " +
						                 inet_ntop_aux(AF_INET6,&(sa_in6->sin6_addr),buff,bufflen));
					}
					//else{printf("\tUNSPEC");}
					pAnycast = pAnycast->Next;
				}
			}
				
			pMulticast = pCurrAddresses->FirstMulticastAddress;
			if (pMulticast) {
				for (i = 0; pMulticast != nullptr; i++) {
					if (pMulticast->Address.lpSockaddr->sa_family == AF_INET)
					{
						sockaddr_in *sa_in = (sockaddr_in *)pMulticast->Address.lpSockaddr;
						ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv4 Address " +
						                 inet_ntop_aux(AF_INET,&(sa_in->sin_addr),buff,bufflen));
					}
					else if (pMulticast->Address.lpSockaddr->sa_family == AF_INET6)
					{
						sockaddr_in6 *sa_in6 = (sockaddr_in6 *)pMulticast->Address.lpSockaddr;
						ips.emplace_back(std::string(pCurrAddresses->AdapterName) + " IPv6 Address " +
						                 inet_ntop_aux(AF_INET6,&(sa_in6->sin6_addr),buff,bufflen));
					}
					//else{printf("\tUNSPEC");}
					pMulticast = pMulticast->Next;
				}
			}
			
			pCurrAddresses = pCurrAddresses->Next;
		}
	} else {
		if (pAddresses) {
			HeapFree(GetProcessHeap(), 0, (pAddresses));
		}
		socket_exception( dwRetVal, SocketErrorMessage(dwRetVal) );
	}
	
	if (pAddresses) {
		HeapFree(GetProcessHeap(), 0, (pAddresses));
	}
#else
	struct ifaddrs * ifAddrStruct=nullptr;
	struct ifaddrs * ifa=nullptr;
	void * tmpAddrPtr=nullptr;
	
	if(getifaddrs(&ifAddrStruct)) {
		socket_exception( GetSocketError(), SocketErrorMessage(GetSocketError()) );
	}
	
	for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
		if (!ifa->ifa_addr) {
			continue;
		}
		if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
			// is a valid IP4 Address
			tmpAddrPtr=&(reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr))->sin_addr;
			char addressBuffer[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			ips.emplace_back(std::string(ifa->ifa_name) + " IPv4 Address " + addressBuffer);
		} else if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
			// is a valid IP6 Address
			tmpAddrPtr=&(reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr))->sin6_addr;
			char addressBuffer[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
			ips.emplace_back(std::string(ifa->ifa_name) + " IPv6 Address " + addressBuffer);
		} 
	}
	if (ifAddrStruct != nullptr) freeifaddrs(ifAddrStruct);
#endif
	
	return ips;
}

