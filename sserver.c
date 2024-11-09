#include "socketutil.h"

static unsigned int client_count = 0;
static int uid = 10;
client_t *clients[MAX_CLIENTS];
room_t *rooms[MAX_ROOMS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_message(char *s, client_t *cli, int prepend_name);
void create_room(char *room_name, client_t *cli);
void join_room(client_t *cli, char *room_name);
void leave_room(client_t *cli);
void delete_room(room_t *room);
void list_rooms(int sockfd);
void *handle_client(void *arg);

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

void create_room(char *room_name, client_t *cli) {
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
            rooms[i]->clients[0] = cli;
            cli->room = rooms[i];
            // Broadcast join message
            char join_message[BUFFER_SIZE];
            snprintf(join_message, sizeof(join_message), "%s has created and joined the room %s\n", cli->name, room_name);
            send_message(join_message, cli, 0);
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
                    // Broadcast join message
                    char join_message[BUFFER_SIZE];
                    snprintf(join_message, sizeof(join_message), "%s has joined the room %s\n", cli->name, room_name);
                    send_message(join_message, cli, 0);
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
        printf("Client %s is leaving room %s\n", cli->name, cli->room->name);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (cli->room->clients[i] == cli) {
                cli->room->clients[i] = NULL;
                // Broadcast leave message
                char leave_message[BUFFER_SIZE];
                snprintf(leave_message, sizeof(leave_message), "%s has left the room %s\n", cli->name, cli->room->name);
                send_message(leave_message, cli, 0);
                break;
            }
        }
        // Check if the room is empty and delete it if so
        int empty = 1;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (cli->room->clients[i] != NULL) {
                empty = 0;
                break;
            }
        }
        pthread_mutex_unlock(&rooms_mutex);
        if (empty) {
            delete_room(cli->room);
        }
        cli->room = NULL;
    }
}

void delete_room(room_t *room) {
    printf("Attempting to delete room: %s\n", room->name);
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; ++i) {
        if (rooms[i] == room) {
            printf("Found room: %s at index %d. Deleting...\n", room->name, i);
            free(rooms[i]);
            rooms[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
    printf("Room deleted successfully.\n");
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

void send_private_message(char *target_name, char *message, client_t *cli) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && strcmp(clients[i]->name, target_name) == 0) {
            char private_message[BUFFER_SIZE + NAME_LEN];
            snprintf(private_message, sizeof(private_message), "[Private] %s: %s", cli->name, message);
            if (send(clients[i]->sockfd, private_message, strlen(private_message), 0) < 0) {
                perror("ERROR: send to descriptor failed");
            }
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void list_users(int sockfd) {
    pthread_mutex_lock(&clients_mutex);
    char list_message[BUFFER_SIZE] = "Connected users:\n";
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            strcat(list_message, clients[i]->name);
            strcat(list_message, "\n");
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    send(sockfd, list_message, strlen(list_message), 0);
}

void send_message(char *s, client_t *cli, int prepend_name) {
    pthread_mutex_lock(&clients_mutex);
    if (cli && cli->room) {
        char message[BUFFER_SIZE + NAME_LEN];
        if (prepend_name) {
            snprintf(message, sizeof(message), "%s: %s", cli->name, s);
        } else {
            snprintf(message, sizeof(message), "%s", s);
        }
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (cli->room->clients[i] && cli->room->clients[i]->uid != cli->uid) {
                if (send(cli->room->clients[i]->sockfd, message, strlen(message), 0) < 0) {
                    perror("ERROR: send to descriptor failed");
                    break;
                }
            }
        }
        printf("[%s]-%s -> %s\n", cli->room->name, cli->name, s);
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
        send_message(buff_out, cli, 0);
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
                    leave_room(cli);
                    create_room(room_name, cli);
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
            } else if (strncmp(command, "/msg", 4) == 0) {
                char *target_name = strtok(command + 5, " ");
                char *private_message = strtok(NULL, "\0");
                if (target_name && private_message) {
                    send_private_message(target_name, private_message, cli);
                }
            } else if (strncmp(command, "/users", 6) == 0) {
                list_users(cli->sockfd);
            } else if (cli->room && strlen(command) > 0) {
                send_message(command, cli, 1);
                str_trim_lf(command, strlen(command));
                printf("%s -> %s\n", command, cli->name);
            }
        } else if (receive == 0 || strcmp(buff_out, "exit") == 0) {
            snprintf(buff_out, sizeof(buff_out), "%s has left\n", cli->name);
            printf("%s", buff_out);
            send_message(buff_out, cli, 0);
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