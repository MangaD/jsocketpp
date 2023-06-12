#include <iostream>
#include <sstream>//stringstream
///Include socket.hpp
#include "socket.hpp"

using namespace std;
using namespace sock;///Use namespace sock

int main() {
	///Get a port from user input
	int port;
	cout << "Type a port to start listening at: ";
	while(true) {
		string input;
		getline(std::cin, input);
		stringstream myStream(input);
		if(myStream >> port && port >= 0 && port <= 65535) {
			break;
		}
		cout << "Error: Invalid port number. Port must be between 0 and 65535." << endl;
	}
	
	/**
	 * Initializing Winsock (for Windows)
	 * Necessary to initialize sockets before using them.
	 * The SocketInitializer object should only be destroyed when no longer using sockets.
	 * It is advised to create it as a local variable in the main function so that it will
	 * only be destroyed at the end of the program (when it gets out of scope).
	 */
	SocketInitializer sockInit;
	
	///Create ServerSocket
	ServerSocket serverSocket(port);
	
	while(true) {
		try {
			///Bind socket
			serverSocket.bind();
			///Listen on socket
			serverSocket.listen();
			break;
		}
		catch (socket_exception &se) {
			cerr << "Error code: " << se.get_error_code() << endl;
			cerr << "Error message: " << se.what() << endl;
			exit(0);
		}
	}

	cout << "Server has been activated. Waiting for client to connect." << endl;
	
	///Wait for client
	Socket conn = serverSocket.accept();
	///Get client address
	cout << "Client has connected from: " << conn.getRemoteSocketAddress() << endl;
	///Read from client
	cout << "Client says: " << conn.read() << endl;
	///Write to client
	conn.write("Hello client!");
	
	return 0;
}