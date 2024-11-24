#include "socketutil.h"
#include "cipherutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//client public/private keys
int e, d, n;
int e_server, n_server;


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
    char buffer[BUFFER_SIZE];

    while (1) {
        str_overwrite_stdout_with_room();
        fgets(message, BUFFER_SIZE, stdin);
        str_trim_lf(message, BUFFER_SIZE);

        if (strcmp(message, "exit") == 0) {
            break;
        } else if (strncmp(message, "/msg", 4) == 0 || strncmp(message, "/users", 6) == 0) {
            snprintf(buffer, sizeof(buffer), "%s", message);
        } else {
            snprintf(buffer, sizeof(buffer), "%s: %s", name, message);
        }

        if (DEBUG) printf("[DEBUG]Message before encryption: %s\n", buffer);

        // RSA Encryption
        int len = strlen(buffer);           // Length of the message
        int encrypted_message[BUFFER_SIZE]; // Array to hold encrypted message

        // Encrypt the entire message using RSA
        encrypt_message(buffer, encrypted_message, len, e_server, n_server);

        if (DEBUG) {
            printf("[DEBUG]Encrypted message: ");
            for (int i = 0; i < len; i++) {
                printf("%d ", encrypted_message[i]);
            }
            printf("\n");
        }

        // Prepare the encrypted message as a space-separated string
        char encrypted_buffer[BUFFER_SIZE] = {};
        int encrypted_len = 0;
        for (int i = 0; i < len; i++) {
            encrypted_len += snprintf(encrypted_buffer + encrypted_len, sizeof(encrypted_buffer) - encrypted_len, "%d ", encrypted_message[i]);
        }

        // Send the encrypted message
        if (send(sockfd, encrypted_buffer, strlen(encrypted_buffer), 0) == -1) {
            perror("ERROR: send message failed");
        }

        memset(message, 0, sizeof(message));
        memset(buffer, 0, sizeof(buffer));
        memset(encrypted_buffer, 0, sizeof(encrypted_buffer));
    }
    catch_ctrl_c_and_exit(2);
}



void recv_msg_handler() {
    int encrypted_array[BUFFER_SIZE];  // Array to hold encrypted message
    while (1) {
        // Receive the encrypted message as raw bytes, cast to char* for recv
        int receive = recv(sockfd, (char *)encrypted_array, sizeof(encrypted_array), 0);

        if (receive == -1) {
            perror("ERROR: recv message failed");
            break;
        }

        if (receive > 0) {
            if (DEBUG) {
                printf("[DEBUG]Received encrypted message: ");
                // Print the received message as integers
                for (int i = 0; i < receive / sizeof(int); i++) {
                    printf("%d ", encrypted_array[i]);
                }
                printf("\n");
            }

            // Handle specific messages, first check if it's a room-related message
            if (strstr((char *)encrypted_array, "created and joined")) {
                // In case the encrypted message contains the room creation info
                sscanf((char *)encrypted_array, "Room %s created and joined", current_room);
                printf("Room %s created and joined\n", current_room);
            } else if (strstr((char *)encrypted_array, "Joined room")) {
                // If the encrypted message contains the room joining info
                sscanf((char *)encrypted_array, "Joined room %s", current_room);
                printf("You joined room %s\n", current_room);
            } else {
                // Decrypt the entire message using RSA
                char decrypted_message[BUFFER_SIZE];
                int len = receive / sizeof(int);  // Number of integers in the array

                decrypt_message(encrypted_array, decrypted_message, len, d, n);  // Use client private key

                if (DEBUG) {
                    printf("[DEBUG] Decrypted message: %s\n", decrypted_message);
                }

                printf("%s\n", decrypted_message);  // Display the decrypted message
            }

            // Overwrite the prompt after printing the message
            str_overwrite_stdout_with_room();
        } else if (receive == 0) {
            break;
        }

        memset(encrypted_array, 0, sizeof(encrypted_array));  // Clear the encrypted message array
    }
}


int main() {
    signal(SIGINT, catch_ctrl_c_and_exit);

    // RSA Key generation (client's keys)
    int p = 7, q = 11;
    generate_keys(p, q, &e, &d, &n); // Generate client's public and private keys


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

    // Step 1: Send client's public key to the server
    int client_public_key[2] = {e, n};
    if (send(sockfd, (char *)client_public_key, sizeof(client_public_key), 0) == -1) {
        perror("ERROR: send public key failed");
        return EXIT_FAILURE;
    }

    // Step 2: Receive server's public key
    int server_public_key[2];
    if (recv(sockfd, (char *)server_public_key, sizeof(server_public_key), 0) == -1) {
        perror("ERROR: receive server public key failed");
        return EXIT_FAILURE;
    }
    e_server = server_public_key[0];
    n_server = server_public_key[1];

    // Now we have the server's public key (e_server, n_server)
    // The server will use this public key to encrypt messages for the client
    if (DEBUG) printf("[DEBUG]Received server's public key: (e = %d, n = %d)\n", e_server, n_server);

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