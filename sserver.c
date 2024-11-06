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
#define MAX_ROOMS 10
#define BUFFER_SIZE 2048

typedef struct client {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
    struct room *room;
} client_t;

typedef struct room {
    char name[32];
    client_t *clients[MAX_CLIENTS];
} room_t;

pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned int client_count = 0;
static int uid = 10;

client_t *clients[MAX_CLIENTS];
room_t *rooms[MAX_ROOMS];

// Forward declarations
void str_trim_lf(char* arr, int length);
void queue_remove(int uid);

void create_room(char *room_name) {
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (!rooms[i]) {
            rooms[i] = (room_t *)malloc(sizeof(room_t));
            strcpy(rooms[i]->name, room_name);
            for (int j = 0; j < MAX_CLIENTS; ++j) {
                rooms[i]->clients[j] = NULL;
            }
            break;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}

void join_room(client_t *cli, char *room_name) {
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (rooms[i] && strcmp(rooms[i]->name, room_name) == 0) {
            for (int j = 0; j < MAX_CLIENTS; ++j) {
                if (!rooms[i]->clients[j]) {
                    rooms[i]->clients[j] = cli;
                    cli->room = rooms[i];
                    break;
                }
            }
            break;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}

void leave_room(client_t *cli) {
    if (cli->room) {
        pthread_mutex_lock(&rooms_mutex);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (cli->room->clients[i] == cli) {
                cli->room->clients[i] = NULL;
                break;
            }
        }
        cli->room = NULL;
        pthread_mutex_unlock(&rooms_mutex);
    }
}

void send_message(char *s, int uid) {
    pthread_mutex_lock(&clients_mutex);

    char *command = strtok(s, " ");
    if (command && strcmp(command, "/msg") == 0) {
        // Private message
        char *recipient_name = strtok(NULL, " ");
        char *private_message = strtok(NULL, "\0");

        if (recipient_name && private_message) {
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (clients[i] && strcmp(clients[i]->name, recipient_name) == 0) {
                    char formatted_message[BUFFER_SIZE];
                    snprintf(formatted_message, BUFFER_SIZE, "[Private] %s: %s", clients[uid]->name, private_message);
                    if (send(clients[i]->sockfd, formatted_message, strlen(formatted_message), 0) < 0) {
                        perror("ERROR: send to descriptor failed");
                    }
                    break;
                }
            }
        }
    } else if (command && strcmp(command, "/list") == 0) {
        // List all clients in the room
        char list_message[BUFFER_SIZE] = "Connected clients:\n";
        client_t *cli = clients[uid];
        if (cli->room) {
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (cli->room->clients[i]) {
                    strcat(list_message, cli->room->clients[i]->name);
                    strcat(list_message, "\n");
                }
            }
            if (send(cli->sockfd, list_message, strlen(list_message), 0) < 0) {
                perror("ERROR: send to descriptor failed");
            }
        }
    } else if (command && strcmp(command, "/list_rooms") == 0) {
        // List all rooms
        char list_message[BUFFER_SIZE] = "Available rooms:\n";
        for (int i = 0; i < MAX_ROOMS; ++i) {
            if (rooms[i]) {
                strcat(list_message, rooms[i]->name);
                strcat(list_message, "\n");
            }
        }
        if (send(clients[uid]->sockfd, list_message, strlen(list_message), 0) < 0) {
            perror("ERROR: send to descriptor failed");
        }
    } else if (command && strcmp(command, "/join") == 0) {
        // Join a room
        char *room_name = strtok(NULL, " ");
        if (room_name) {
            leave_room(clients[uid]);
            join_room(clients[uid], room_name);
            char join_message[BUFFER_SIZE];
            snprintf(join_message, BUFFER_SIZE, "%s has joined the room %s\n", clients[uid]->name, room_name);
            send_message(join_message, uid);
        }
    } else if (command && strcmp(command, "/create") == 0) {
        // Create a room
        char *room_name = strtok(NULL, " ");
        if (room_name) {
            create_room(room_name);
            char create_message[BUFFER_SIZE];
            snprintf(create_message, BUFFER_SIZE, "Room %s created\n", room_name);
            send_message(create_message, uid);
        }
    } else {
        // Broadcast message to the room
        client_t *cli = clients[uid];
        if (cli->room) {
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (cli->room->clients[i] && cli->room->clients[i]->uid != uid) {
                    if (send(cli->room->clients[i]->sockfd, s, strlen(s), 0) < 0) {
                        perror("ERROR: send to descriptor failed");
                        break;
                    }
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    char buff_out[BUFFER_SIZE];
    char name[32];
    char room_name[32];
    int leave_flag = 0;

    client_count++;
    client_t *cli = (client_t *)arg;

    // Receive name
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1) {
        printf("Didn't enter the name.\n");
        leave_flag = 1;
    } else {
        strcpy(cli->name, name);
        // Receive room name
        if (recv(cli->sockfd, room_name, 32, 0) <= 0 || strlen(room_name) < 2 || strlen(room_name) >= 32 - 1) {
            printf("Didn't enter the room name.\n");
            leave_flag = 1;
        } else {
            join_room(cli, room_name);
            snprintf(buff_out, sizeof(buff_out), "%s has joined the room %s\n", cli->name, room_name);
            printf("%s", buff_out);
            send_message(buff_out, cli->uid);
        }
    }

    memset(buff_out, 0, BUFFER_SIZE);

    while (!leave_flag) {
        int receive = recv(cli->sockfd, buff_out, BUFFER_SIZE, 0);
        if (receive > 0) {
            if (strlen(buff_out) > 0) {
                send_message(buff_out, cli->uid);
                str_trim_lf(buff_out, strlen(buff_out));
                printf("%s -> %s\n", buff_out, cli->name);
            }
        } else if (receive == 0 || strcmp(buff_out, "exit") == 0) {
            snprintf(buff_out, sizeof(buff_out), "%s has left\n", cli->name);
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