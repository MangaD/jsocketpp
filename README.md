# C++ Socket library

Cross-platform socket library for making sockets usage easy and clean. Only supports TCP and only the most important methods.

## Building libsocket

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ make install

## How to use?

Compiling:

- link against libsocket.a
- On Windows, link against ws2_32.lib as well

See the tests to learn how to use.