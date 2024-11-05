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

#define PORT 8080
#define MAX_CLIENTS 5
#define BUFFER_SIZE 2048

static unsigned int client_count = 0;
static int uid = 10;

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void print_client_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d",
           addr.sin_addr.s_addr & 0xff,
           (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16,
           (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

void queue_add(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int uid) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->uid == uid) {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void send_message(char *s, int uid) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->uid != uid) {
                if (send(clients[i]->sockfd, s, strlen(s), 0) < 0) {
                    perror("ERROR: send to descriptor failed");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    char buff_out[BUFFER_SIZE];
    char name[32];
    int leave_flag = 0;

    client_count++;
    client_t *cli = (client_t *)arg;

    // Name
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1) {
        printf("Didn't enter the name.\n");
        leave_flag = 1;
    } else {
        strcpy(cli->name, name);
        sprintf(buff_out, "%s has joined\n", cli->name);
        printf("%s", buff_out);
        send_message(buff_out, cli->uid);
    }

    memset(buff_out, 0, BUFFER_SIZE);

    while (1) {
        if (leave_flag) {
            break;
        }

        int receive = recv(cli->sockfd, buff_out, BUFFER_SIZE, 0);
        if (receive > 0) {
            if (strlen(buff_out) > 0) {
                send_message(buff_out, cli->uid);

                str_trim_lf(buff_out, strlen(buff_out));
                printf("%s -> %s\n", buff_out, cli->name);
            }
        } else if (receive == 0 || strcmp(buff_out, "exit") == 0) {
            sprintf(buff_out, "%s has left\n", cli->name);
            printf("%s", buff_out);
            send_message(buff_out, cli->uid);
            leave_flag = 1;
        } else {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        memset(buff_out, 0, BUFFER_SIZE);
    }

    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    client_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main() {
    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    // Initialize Winsock (Windows only)
    #ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code : %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }
    #endif

    // Socket settings
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("ERROR: socket creation failed");
        return EXIT_FAILURE;
    }
    printf("Socket created successfully.\n");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // Ignore pipe signals (Linux only)
    #ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
    #endif

    // Set socket options
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option)) < 0) {
        #ifdef _WIN32
        printf("ERROR: setsockopt failed: %d\n", WSAGetLastError());
        #else
        perror("ERROR: setsockopt failed");
        #endif
        return EXIT_FAILURE;
    }
    printf("Socket options set successfully.\n");

    // Bind
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }
    printf("Socket bound successfully.\n");

    // Listen
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }
    printf("Listening on port %d.\n", PORT);

    printf("=== WELCOME TO THE CHATROOM ===\n");

    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        // Check if max clients is reached
        if ((client_count + 1) == MAX_CLIENTS) {
            printf("Max clients reached. Rejected: ");
            print_client_addr(cli_addr);
            printf(":%d\n", cli_addr.sin_port);
            close(connfd);
            continue;
        }

        // Client settings
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        // Add client to the queue and fork thread
        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void*)cli);

        // Reduce CPU usage
        sleep(1);
    }

    // Cleanup Winsock (Windows only)
    #ifdef _WIN32
    WSACleanup();
    #endif

    return EXIT_SUCCESS;
}