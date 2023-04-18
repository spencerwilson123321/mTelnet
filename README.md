# mTelnet 

Note: mTelnet is still in development and is missing features.

mTelnet is a minimal Telnet client implementation written in C and designed for Linux.
It is not compatible with Windows or macOS. It was created for educational purposes
but is a fully working Telnet client program.

By default, all options are refused so that the Telnet connection defaults to a basic NVT (Network Virtual Terminal).
Additional settings for option negotiation may be added in the future.

Currently only implemented with IPv4 support.

# Compile
To compile run the following command:
```shell
gcc -o telnet telnet.c
```

# Usage
```shell
./telnet ip port
```
