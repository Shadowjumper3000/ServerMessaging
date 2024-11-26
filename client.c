#include "socketutil.h"
#include "cipherutil.h"

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[NAME_LEN];
char current_room[NAME_LEN] = "None";

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

void str_overwrite_stdout_with_room() {
    printf("\r[%s]-", current_room);
    fflush(stdout);
}

void send_msg_handler() {
    char message[BUFFER_SIZE] = {};
    char buffer[BUFFER_SIZE + NAME_LEN] = {};
    while (1) {
        str_overwrite_stdout_with_room();
        fgets(message, BUFFER_SIZE, stdin);
        str_trim_lf(message, BUFFER_SIZE);

        if (DEBUG) printf("[DEBUG] Message entered: %s\n", message);

        if (strcmp(message, "exit") == 0) {
            break;
        } else if (strncmp(message, "/msg", 4) == 0 || strncmp(message, "/users", 6) == 0) {
            snprintf(buffer, sizeof(buffer), "%s", message);
        } else {
            snprintf(buffer, sizeof(buffer), "%s: %s", name, message);
        }

        caesar_encrypt(buffer, SHIFT); // Encrypt the message
        if (DEBUG) printf("[DEBUG] Encrypted message: %s\n", buffer);

        if (send(sockfd, buffer, strlen(buffer), 0) == -1) {
            perror("ERROR: send message failed");
        }

        memset(message, 0, BUFFER_SIZE);
        memset(buffer, 0, BUFFER_SIZE + NAME_LEN);
    }
    catch_ctrl_c_and_exit(2);
}


void recv_msg_handler() {
    char message[BUFFER_SIZE] = {};
    while (1) {
        int receive = recv(sockfd, message, BUFFER_SIZE, 0);
        if (receive == -1) {
            perror("ERROR: recv message failed");
            break;
        }

        if (DEBUG) printf("[DEBUG] Message received (encrypted): %s\n", message);

        caesar_decrypt(message, SHIFT); // Decrypt the message
        if (DEBUG) printf("[DEBUG] Message decrypted: %s\n", message);

        if (receive > 0) {
            if (strstr(message, "created and joined")) {
                sscanf(message, "Room %s created and joined", current_room);
                printf("Room %s created and joined\n", current_room);
            } else if (strstr(message, "Joined room")) {
                sscanf(message, "Joined room %s", current_room);
                printf("You joined room %s\n", current_room);
            } else if (strstr(message, "has left the room")) {
                printf("%s\n", message);
            } else if (strstr(message, "has joined the room")) {
                printf("%s\n", message);
            } else if (strstr(message, "Available rooms")) {
                printf("%s\n", message);
            } else {
                printf("%s\n", message);
            }
            str_overwrite_stdout_with_room();
        } else if (receive == 0) {
            break;
        }
        memset(message, 0, sizeof(message));
    }
}


int main() {
    signal(SIGINT, catch_ctrl_c_and_exit);

    printf("Enter your name: ");
    fgets(name, NAME_LEN, stdin);
    str_trim_lf(name, strlen(name));

    if (strlen(name) > NAME_LEN - 1 || strlen(name) < 2) {
        printf("Name must be less than %d and more than 2 characters.\n", NAME_LEN);
        return EXIT_FAILURE;
    }

    char server_ip[16];
    printf("Enter server IP address: ");
    fgets(server_ip, 16, stdin);
    str_trim_lf(server_ip, strlen(server_ip));

    struct sockaddr_in server_addr;

    // Initialize Winsock (Windows only)
    #ifdef _WIN32
    if (DEBUG) printf("[DEBUG] Initializing Winsock\n");
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Failed. Error Code : %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }
    #endif

    // Socket settings
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        #ifdef _WIN32
        printf("ERROR: socket creation failed with error: %d\n", WSAGetLastError());
        #else
        perror("ERROR: socket creation failed");
        #endif
        return EXIT_FAILURE;
    }
    if (DEBUG) printf("[DEBUG] Socket created\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(PORT);

    // Connect to Server
    int err = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (err == -1) {
        #ifdef _WIN32
        printf("ERROR: connect failed with error: %d\n", WSAGetLastError());
        #else
        perror("ERROR: connect");
        #endif
        return EXIT_FAILURE;
    }
    if (DEBUG) printf("[DEBUG] Connected to server\n");

    // Send name
    if (send(sockfd, name, NAME_LEN, 0) == -1) {
        perror("ERROR: send name failed");
        return EXIT_FAILURE;
    }
    if (DEBUG) printf("[DEBUG] Sent name: %s\n", name);

    printf("=== WELCOME TO THE CHATROOM ===\n");
    printf("Use /create <room_name> to create a room, /join <room_name> to join a room, or /list to list rooms.\n");

    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void*)send_msg_handler, NULL) != 0) {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void*)recv_msg_handler, NULL) != 0) {
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    while (1) {
        if (flag) {
            printf("\nBye\n");
            break;
        }
    }

    // Cleanup
    close(sockfd);
    #ifdef _WIN32
    WSACleanup();
    #endif

    return EXIT_SUCCESS;
}