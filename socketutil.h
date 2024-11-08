#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

// for Linux
#ifdef __linux__
#include <arpa/inet.h>
#include <signal.h>
#endif

// for Windows
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#define DEBUG 1

#define PORT 8080
#define BUFFER_SIZE 2048
#define NAME_LEN 32
#define MAX_CLIENTS 10
#define MAX_ROOMS 10

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
    struct room_t *room;
} client_t;

typedef struct room_t {
    char name[32];
    client_t *clients[MAX_CLIENTS];
} room_t;

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_trim_lf(char* arr, int length) {
    for (int i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

#endif // SOCKET_UTIL_H