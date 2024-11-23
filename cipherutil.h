#ifndef CIPHER_UTIL_H
#define CIPHER_UTIL_H

#define SHIFT 3


#include <stdio.h>
#include <stdlib.h>
#include <math.h>


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


// RSA encryption

// Compute gcd of two numbers
int gcd(int a, int b) {
    while(b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

// Modular exponentiation: (base^exp) % mod
int mod_exp(int base, int exp, int mod) {
    int result = 1;
    base = base % mod;  // In case base is larger than mod
    while(exp > 0) {
        if(exp % 2 == 1) {  // If exp is odd
            result = (result * base) % mod;
        }
        exp = exp >> 1;  // exp = exp // 2
        base = (base * base) % mod;  // base = base^2 % mod
    }
    return result;
}

int mod_inverse(int e, int phi) {
    int t = 0, new_t = 1;
    int r = phi, new_r = e;

    while (new_r != 0) {
        int quotient = r / new_r;

        int temp_t = t;
        t = new_t;
        new_t = temp_t - quotient * new_t;

        int temp_r = r;
        r = new_r;
        new_r = temp_r - quotient * new_r;
    }

    if (r > 1) {
        return -1;  // No modular inverse exists
    }

    if (t < 0) {
        t = t + phi;
    }

    return t;
}


// Generate RSA public and private keys
void generate_keys(int p, int q, int* e, int* d, int* n) {
    *n = p * q;
    int phi = (p - 1) * (q - 1);

    // Choose e such that 1 < e < phi and gcd(e, phi) == 1
    *e = 3;
    while(gcd(*e, phi) != 1) {
        (*e)++;
    }

    // Calculate d such that (d * e) % phi = 1
    *d = mod_inverse(*e, phi);
    if (*d == -1) {
        printf("Error: No modular inverse found for e\n");
        exit(EXIT_FAILURE);
    }
}

// Encrypt a character using RSA
int encrypt_char(char ch, int e, int n) {
    return mod_exp((int)ch, e, n);
}

// Decrypt a character using RSA
char decrypt_char(int encrypted_char, int d, int n) {
    return (char) mod_exp(encrypted_char, d, n);
}

// Decrypt a message (array of encrypted characters)
void decrypt_message(const int* encrypted_message, char* decrypted_message, int length, int d, int n) {
    for(int i = 0; i < length; i++) {
        decrypted_message[i] = decrypt_char(encrypted_message[i], d, n);
    }
    decrypted_message[length] = '\0';  // Null-terminate the decrypted string
}

// Encrypt a message (array of characters)
void encrypt_message(const char* message, int* encrypted_message, int length, int e, int n) {
    for (int i = 0; i < length; i++) {
        encrypted_message[i] = encrypt_char(message[i], e, n);
    }
}

#endif // CIPHER_UTIL_H