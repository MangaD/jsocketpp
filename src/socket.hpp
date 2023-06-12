/*
 * File: socket.hpp
 *
 * Contains declarations for the 'ServerSocket' and 'Socket' classes that attempt to provide
 * an higher level of abstraction for sockets to ease their use. Also, compatibility between
 * Windows and Linux operating systems.
 *
 * Author: David Gonçalves (MangaD)
 * Code review: Adrian Bibby Walther (Someone else)
 *
 */

#pragma once

#ifdef __GNUC__
	#define QUOTE(s) #s
	#define DIAGNOSTIC_PUSH() _Pragma("GCC diagnostic push")
	#define DIAGNOSTIC_IGNORE(warning) _Pragma(QUOTE(GCC diagnostic ignored warning))
	#define DIAGNOSTIC_POP() _Pragma("GCC diagnostic pop")
#else
	#define DIAGNOSTIC_PUSH()
	#define DIAGNOSTIC_IGNORE(warning)
	#define DIAGNOSTIC_POP()
#endif

#include <iostream>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32

	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>//GetAdaptersInfo

	//https://msdn.microsoft.com/en-us/library/6sehtctf.aspx
	#if !defined(WINVER) || (WINVER < 0x0501)
		#error WINVER must be defined to something equal or above to 0x0501 // Win XP
	#endif // WINVER
	#if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0501)
		#error _WIN32_WINNT must be defined to something equal or above to 0x0501 // Win XP
	#endif // _WIN32_WINNT

	/* If compiled with Visual C++
	 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms737629%28v=vs.85%29.aspx
	 * http://stackoverflow.com/questions/3484434/what-does-pragma-comment-mean
	 * http://stackoverflow.com/questions/70013/how-to-detect-if-im-compiling-code-with-visual-studio-2008
	 */
	#ifdef _MSC_VER
		#pragma comment(lib, "Ws2_32.lib")
		#pragma comment(lib, "iphlpapi.lib")////GetAdaptersInfo
	#endif

#else

	//Assuming Linux
	#include <unistd.h>
	#include <errno.h>//errno
	#include <cstring>//strerror
	#include <sys/ioctl.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <netinet/in.h>
	#include <netdb.h>//for struct addrinfo
	#include <arpa/inet.h>//for inet_ntoa
	#include <ifaddrs.h>//getifaddrs

#endif

namespace sock{

#ifdef _WIN32

	inline int InitSockets(){ WSADATA WSAData; return WSAStartup(MAKEWORD(2,2),&WSAData); }
	inline int CleanupSockets(){ return WSACleanup(); }
	inline int GetSocketError(){ return WSAGetLastError(); }
	inline int CloseSocket(SOCKET fd) { return closesocket(fd); }
	
	const char *inet_ntop_aux(int af, const void *src, char *dst, socklen_t size);

#else

	typedef int SOCKET;
	constexpr SOCKET INVALID_SOCKET = -1;
	constexpr SOCKET SOCKET_ERROR = -1;

	constexpr int InitSockets(){ return 0; }
	constexpr int CleanupSockets(){ return 0; }
	inline int GetSocketError(){ return errno; }
	inline int CloseSocket(SOCKET fd){ return close(fd); }

	inline int ioctlsocket(SOCKET fd,long cmd,u_long *argp){ return ioctl(fd,static_cast<unsigned long>(cmd),argp); }

#endif

	std::string SocketErrorMessage(int error);
	std::string SocketErrorMessageWrap(int error);

	class socket_exception : public std::exception {
		public:
			explicit socket_exception(int code, std::string message = "socket_exception")
				: std::exception(), error_code(code), error_message(message) {
				error_message += " (" + std::to_string(code) + ")";
			}
			const char *what() const noexcept
			{
				return error_message.c_str();
			}
			int get_error_code() const noexcept
			{
				return error_code;
			}

		private:
			int error_code = 0;
			std::string error_message;
	};

	class SocketInitializer {
	public:
		inline SocketInitializer(){
			if(InitSockets() != 0) throw socket_exception( GetSocketError(), SocketErrorMessage(GetSocketError()) );
		}
		inline ~SocketInitializer() noexcept {
			if(CleanupSockets() != 0) std::cerr << "Socket cleanup failed: " << SocketErrorMessageWrap(GetSocketError()) << ": " << GetSocketError() << std::endl;
		}

		SocketInitializer(const SocketInitializer &rhs)=delete;
		SocketInitializer &operator=(const SocketInitializer &rhs)=delete;
	};

	class Socket {
		friend class ServerSocket;
		private:
			SOCKET clientSocket = INVALID_SOCKET;
			struct sockaddr_storage remote_addr;//sockaddr_in for IPv4; sockaddr_in6 for IPv6; sockaddr_storage for both (portability)
			socklen_t remote_addr_length;
			struct addrinfo *cli_addrinfo = nullptr;
			std::vector<char> buffer = std::vector<char>(512);
		protected:
			Socket(SOCKET client, struct sockaddr_storage addr, socklen_t len);
		public:
			explicit Socket(std::string host, unsigned short port);
			//move constructor
			Socket(const Socket& rhs)=delete; //-Weffc++
			Socket(Socket&& rhs) noexcept : //rhs = right hand side
				clientSocket (rhs.clientSocket),
				remote_addr (rhs.remote_addr),
				remote_addr_length (rhs.remote_addr_length),
				cli_addrinfo (rhs.cli_addrinfo),
				buffer (std::move(rhs.buffer))
			{
				rhs.clientSocket = INVALID_SOCKET;
			}
			//move assignment function
			Socket &operator=(const Socket& rhs)=delete; //-Weffc++
			Socket &operator=(Socket&& rhs) noexcept {
				clientSocket = rhs.clientSocket;
				remote_addr = rhs.remote_addr;
				remote_addr_length = rhs.remote_addr_length;
				cli_addrinfo = rhs.cli_addrinfo;
				buffer = std::move(rhs.buffer);
				if(this->clientSocket != INVALID_SOCKET){
					if( CloseSocket(this->clientSocket) )
						std::cerr << "closesocket() failed: " << SocketErrorMessage(GetSocketError()) << ": " << GetSocketError() << std::endl;
				}
				clientSocket = rhs.clientSocket;
				rhs.clientSocket = INVALID_SOCKET;
				return *this;
			}
			~Socket();
			std::string getRemoteSocketAddress();
			void connect();
			template <typename T = std::string>
			T read(){
				T r;
				int len = recv(this->clientSocket,reinterpret_cast<char*>(&r),sizeof(T),0);
				if (len == SOCKET_ERROR) throw socket_exception( GetSocketError(), SocketErrorMessage(GetSocketError()) );
				if (len == 0) throw socket_exception(0, "Connection closed by remote host.");
				return r;
			}
			void close();
			void shutdown();
			int write(std::string message);
			void setBufferSize(std::size_t newLen);
			void setBufferLength(std::size_t newLen);
			inline bool isValid() { return this->clientSocket != INVALID_SOCKET; }
	};

	class ServerSocket {
		private:
			SOCKET serverSocket = INVALID_SOCKET;
			struct addrinfo *srv_addrinfo = nullptr;
			unsigned short port;
		public:
			explicit ServerSocket(unsigned short port);
			//move constructor
			ServerSocket(const ServerSocket& rhs)=delete; //-Weffc++
			ServerSocket(ServerSocket&& rhs) noexcept : //rhs = right hand side
				serverSocket (rhs.serverSocket),
				srv_addrinfo (rhs.srv_addrinfo),
				port (rhs.port)
			{
				rhs.serverSocket = INVALID_SOCKET;
			}
			ServerSocket &operator=(const ServerSocket& rhs)=delete; //-Weffc++
			~ServerSocket();
			void bind();
			void listen();
			Socket accept();
			void close();
			void shutdown();
			inline bool isValid() { return this->serverSocket != INVALID_SOCKET; }
	};

	template<>
	inline std::string Socket::read(){
		DIAGNOSTIC_PUSH()
		DIAGNOSTIC_IGNORE("-Wuseless-cast")
		int len = static_cast<int>(recv(clientSocket, buffer.data(), buffer.size(), 0));
		DIAGNOSTIC_POP()
		if (len == SOCKET_ERROR) throw socket_exception( GetSocketError(), SocketErrorMessage(GetSocketError()) );
		if (len == 0) throw socket_exception(0, "Connection closed by remote host.");
		return std::string(buffer.data(), static_cast<size_t>(len));
	}
	
	std::vector<std::string> getHostAddr();

}
