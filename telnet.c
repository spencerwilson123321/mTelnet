#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <termios.h>
#include <netdb.h>

// typedefs
typedef unsigned char byte;

#define BUFFSIZE 1024

// OPTION Codes
byte WILL = 251;
byte WONT = 252;
byte DO = 253;
byte DONT = 254;
byte IAC = 255;

// GLOBALS
pthread_t input_thread;
int fd;
char read_buffer[BUFFSIZE] = {0};
char write_buffer[BUFFSIZE] = {0};
struct termios oldt, newt;


byte deny(byte opt_code) {
    if (opt_code == WONT) return DONT;
    else if (opt_code == DONT) return WONT;
    else if (opt_code == DO) return WONT;
    else if (opt_code == WILL) return DONT;
    else return 0;
}

enum states {
    initial = 0,
    s1 = 1,
    s2 = 2,
} state;

void shutdown_program() {
    printf("\nCaught signal, shutting down...\n");
    pthread_cancel(input_thread);
    pthread_join(input_thread, NULL);
    // Restore old terminal settings.
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    close(fd);
    exit(1);
}

void sig_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        shutdown_program();
    }
}

void *input_handler(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    // Saving old terminal state,
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Disable canonical mode and echo.
    // Allows character-at-a-time processing.
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    char c;
    while (1) {
        c = getchar();
        send(fd, &c, 1, 0);
    }
    return NULL;
}

int main(int argc, char** argv) {

    signal(SIGINT, sig_handler);

    if (argc != 3) {
        printf("Invalid number of arguments\n");
        exit(1);
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    if (port == 0) {
        printf("Port number integer conversion failed.\n");
        exit(1);
    }

    struct addrinfo hints;
    struct addrinfo *res;

    // Support both IPv4 and IPv6 addresses over TCP.
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Perform DNS hostname resolution.
    if (getaddrinfo(ip, "telnet", &hints, &res) != 0) {
        perror("DNS resolution failed.");
        exit(1);
    }

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        perror("Socket creation failed.");
        exit(1);
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("Failed to connect.");
        exit(1);
    }

    // We can free this memory now.
    freeaddrinfo(res);

    memset(write_buffer, '\0', sizeof(char)*BUFFSIZE);
    memset(read_buffer, '\0', sizeof(char)*BUFFSIZE);

    // Make threads.
    pthread_create(&input_thread, NULL, input_handler, NULL);

    // ---- Main data processing loop ----
    state = initial;
    int num_bytes;
    byte opt_code;
    int write_buffer_index = 0;

    while (1) {
        num_bytes = recv(fd, read_buffer, BUFFSIZE, 0);
        if (num_bytes == -1) {
            perror("Error reading from socket.");
            close(fd);
            exit(1);
        }
        if (num_bytes == 0) {
            break;
        }
        for (int i = 0; i < num_bytes; i++) {
            byte current = (byte) read_buffer[i];
            if (state == initial) {
                if (current != IAC) {
                    printf("%c", current);
                    fflush(stdout);
                    continue;
                }
                state = s1;
                continue;
            }
            else if (state == s1) {
                if (current != WILL && current != DO && current != DONT && current != WONT) {
                    perror("Error in state 1.");
                    close(fd);
                    exit(1);
                }
                opt_code = current;
                state = s2;
                continue;
            }
            else if (state == s2) {
                // Write IAC opt_code current to write buffer.
                // By default, I am denying all DOs and denying all WILLs.
                if (opt_code == WILL && current == 1) {
                    state = initial;
                    continue;
                }
                write_buffer[write_buffer_index++] = IAC;
                write_buffer[write_buffer_index++] = deny(opt_code);
                write_buffer[write_buffer_index++] = current;
                state = initial;
                continue;
            }
        }
        if (strlen(write_buffer) > 0) {
            if (send(fd, write_buffer, strlen(write_buffer), 0) == -1) {
                perror("Failed to send.");
                close(fd);
                exit(1);
            }
            memset(write_buffer, '\0', BUFFSIZE);
            write_buffer_index = 0;
        }
        memset(read_buffer, '\0', BUFFSIZE);
    }

    // Server closed connection, shutdown threads, restore terminal,
    // and close socket descriptor.
    pthread_cancel(input_thread);
    pthread_join(input_thread, NULL);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    close(fd);
    return 0;
};
