#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

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
pthread_t data_thread, input_thread;
int fd;
char read_buffer[BUFFSIZE] = {0};
char write_buffer[BUFFSIZE] = {0};

int fail(const char* msg, int file_descriptor, int exitcode) {
    printf("%s\n", msg);
    if (file_descriptor != 0) {
        close(file_descriptor);
    }
    exit(exitcode);
}

byte negotiate(byte opt_code) {
    if (opt_code == WILL) return DO;
    else if (opt_code == DO) return WONT;
    else if (opt_code == DONT) return WONT;
    else if (opt_code == WONT) return DONT;
    else return 0;
}

enum states {
    initial = 0,
    s1 = 1,
    s2 = 2,
} state;

void sig_handler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        printf("\nCaught signal, closing socket and ending threads...\n");
        pthread_cancel(data_thread);
        pthread_cancel(input_thread);
        pthread_join(input_thread, NULL);
        pthread_join(data_thread, NULL);
        close(fd);
        exit(1);
    }
}

void* receive_handler(void* arg) {

    // Enable thread cancel.
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    state = initial;
    int num_bytes;
    byte opt_code;
    int write_buffer_index = 0;

    while (1) {
        num_bytes = recv(fd, read_buffer, BUFFSIZE, 0);
        if (num_bytes == -1) {
            fail("Error reading from socket.", fd, 1);
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
                    fail("Error in state 1.", fd, 1);
                }
                opt_code = current;
                state = s2;
                continue;
            }
            else if (state == s2) {
                // Write IAC opt_code current to write buffer.
                write_buffer[write_buffer_index++] = IAC;
                write_buffer[write_buffer_index++] = negotiate(opt_code);
                write_buffer[write_buffer_index++] = current;
                state = initial;
                continue;
            }
        }
        if (strlen(write_buffer) > 0) {
            if (send(fd, write_buffer, strlen(write_buffer), 0) == -1) {
                fail("Failed to send", fd, 1);
            }
            memset(write_buffer, '\0', BUFFSIZE);
            write_buffer_index = 0;
        }
        memset(read_buffer, '\0', BUFFSIZE);
    }
    return NULL;
}

void *input_handler(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    while (1) {
        fgets(write_buffer, BUFFSIZE, stdin);
        send(fd, write_buffer, strlen(write_buffer), 0);
        memset(write_buffer, 0, BUFFSIZE);
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

    printf("IP: %s\n", ip);
    printf("PORT: %d\n", port);

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if(inet_pton(AF_INET, ip, &server_address.sin_addr) <= 0) {
        printf("IP address conversion failed.\n");
        exit(1);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("Socket creation failed.\n");
        exit(1);
    }

    if(connect(fd, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        printf("Failed to connect to address.\n");
        exit(1);
    }

    memset(write_buffer, '\0', sizeof(char)*BUFFSIZE);
    memset(read_buffer, '\0', sizeof(char)*BUFFSIZE);

    // Make threads.
    pthread_create(&input_thread, NULL, input_handler, NULL);
    pthread_create(&data_thread, NULL, receive_handler, NULL);

    pthread_join(input_thread, NULL);
    pthread_join(data_thread, NULL);
    close(fd);
    return 0;
};

