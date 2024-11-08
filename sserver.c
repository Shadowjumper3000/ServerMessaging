#include "socketutil.h"

static unsigned int client_count = 0;
static int uid = 10;
client_t *clients[MAX_CLIENTS];
room_t *rooms[MAX_ROOMS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void create_room(char *room_name) {
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (!rooms[i]) {
            rooms[i] = (room_t *)malloc(sizeof(room_t));
            if (rooms[i] == NULL) {
                perror("ERROR: malloc failed");
                pthread_mutex_unlock(&rooms_mutex);
                return;
            }
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

void list_rooms(int sockfd) {
    pthread_mutex_lock(&rooms_mutex);
    char list_message[BUFFER_SIZE] = "Available rooms:\n";
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (rooms[i]) {
            strcat(list_message, rooms[i]->name);
            strcat(list_message, "\n");
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
    send(sockfd, list_message, strlen(list_message), 0);
}

void send_message(char *s, int uid) {
    pthread_mutex_lock(&clients_mutex);
    client_t *cli = clients[uid];
    if (cli && cli->room) {
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (cli->room->clients[i] && cli->room->clients[i]->uid != uid) {
                if (send(cli->room->clients[i]->sockfd, s, strlen(s), 0) < 0) {
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

    // Receive name
    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1) {
        printf("Didn't enter the name.\n");
        leave_flag = 1;
    } else {
        strcpy(cli->name, name);
        snprintf(buff_out, sizeof(buff_out), "%s has joined the server\n", cli->name);
        printf("%s", buff_out);
        send_message(buff_out, cli->uid);
    }

    memset(buff_out, 0, BUFFER_SIZE);

    while (!leave_flag) {
        int receive = recv(cli->sockfd, buff_out, BUFFER_SIZE, 0);
        if (receive > 0) {
            printf("Received command: %s\n", buff_out); // Log the command

            // Strip the client's name from the command
            char *command = strchr(buff_out, ':');
            if (command) {
                command += 2; // Skip ": "
            } else {
                command = buff_out;
            }

            if (strncmp(command, "/create", 7) == 0) {
                char *room_name = strtok(command + 8, " ");
                if (room_name) {
                    create_room(room_name);
                    join_room(cli, room_name);
                    snprintf(buff_out, sizeof(buff_out), "Room %s created and joined\n", room_name);
                    send(cli->sockfd, buff_out, strlen(buff_out), 0);
                }
            } else if (strncmp(command, "/join", 5) == 0) {
                char *room_name = strtok(command + 6, " ");
                if (room_name) {
                    leave_room(cli);
                    join_room(cli, room_name);
                    snprintf(buff_out, sizeof(buff_out), "Joined room %s\n", room_name);
                    send(cli->sockfd, buff_out, strlen(buff_out), 0);
                }
            } else if (strncmp(command, "/list", 5) == 0) {
                list_rooms(cli->sockfd);
            } else if (cli->room && strlen(command) > 0) {
                send_message(command, cli->uid);
                str_trim_lf(command, strlen(command));
                printf("%s -> %s\n", command, cli->name);
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
    leave_room(cli);
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
        if (cli == NULL) {
            perror("ERROR: malloc failed");
            close(connfd);
            continue;
        }
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;
        cli->room = NULL;

        // Add client to the queue and fork thread
        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void*)cli);

        // Reduce CPU usage
        usleep(100000); // Sleep for 100 milliseconds
    }

    // Cleanup Winsock (Windows only)
    #ifdef _WIN32
    WSACleanup();
    #endif

    return EXIT_SUCCESS;
}