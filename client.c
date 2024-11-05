#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
// for Linux
// #include <arpa/inet.h>

// for Windows
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define NAME_LEN 32

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[NAME_LEN];

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

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

void caesar_encrypt(char *message, int shift) {
    for (int i = 0; message[i] != '\0'; ++i) {
        char c = message[i];
        if (c >= 'a' && c <= 'z') {
            message[i] = (c - 'a' + shift) % 26 + 'a';
        } else if (c >= 'A' && c <= 'Z') {
            message[i] = (c - 'A' + shift) % 26 + 'A';
        }
    }
}

void caesar_decrypt(char *message, int shift) {
    caesar_encrypt(message, 26 - shift);
}
void send_msg_handler() {
    char message[BUFFER_SIZE] = {};
    char buffer[BUFFER_SIZE + NAME_LEN] = {};
    while (1) {
        str_overwrite_stdout();
        fgets(message, BUFFER_SIZE, stdin);
        str_trim_lf(message, BUFFER_SIZE);

        if (strcmp(message, "exit") == 0) {
            break;
        } else {
            caesar_encrypt(message, 3); // Encrypt message with a shift of 3
            snprintf(buffer, sizeof(buffer), "%s: %s\n", name, message);
            send(sockfd, buffer, strlen(buffer), 0);
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
        if (receive > 0) {
            caesar_decrypt(message, 3); // Decrypt message with a shift of 3
            printf("%s", message);
            str_overwrite_stdout();
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

    // Send name
    send(sockfd, name, NAME_LEN, 0);

    printf("=== WELCOME TO THE CHATROOM ===\n");

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

    close(sockfd);

    // Cleanup Winsock (Windows only)
    #ifdef _WIN32
    WSACleanup();
    #endif

    return EXIT_SUCCESS;
}