LDLIBS = -lsocket
ifeq ($(OS),Windows_NT)
LDLIBS += -lws2_32 -liphlpapi
endif

all: client.cpp server.cpp
	g++ -std=c++11 -I../src -o server server.cpp -L.. $(LDLIBS) -m32
	g++ -std=c++11 -I../src -o client client.cpp -L.. $(LDLIBS) -m32

clean:
ifeq ($(OS),Windows_NT)
	$(CMD) "del *.o *.a client server" 2>nul
else
	rm -f *.o *.a client server
endif