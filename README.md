# mTelnet 

mTelnet is a minimal Telnet client implementation written in C and designed for Linux.

By default, all options are refused so that the Telnet connection defaults to a basic 
NVT (Network Virtual Terminal). Additional settings for option negotiation may be added 
in the future.

# Compile
To compile run the following command:
```shell
gcc -o telnet telnet.c
```

# Usage
```shell
./telnet hostname port
```
