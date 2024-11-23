#ifndef CIPHER_UTIL_H
#define CIPHER_UTIL_H

#define SHIFT 3


#include <stdio.h>
#include <stdlib.h>
#include <math.h


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


//RSA encryption

int gcd(int a, int b){
    while(b != 0 ){

        int temp = b;
        b = a%b;
        a = temp;

    }
    return a;
}

int mod_exp(int base, int exp, int mod){
    int result = 1;
    base = base%mod;
    while(exp>0){
        if(exp%2 == 1){
            result = (result*base)%mod;
        }

        exp = exp >> 1;
        base = (base*base)%mod;

    }
    return result;
}

void generate_keys(int p, int q, int*e, int* d, int* n){
    *n = p * q;
    int phi = (p-1)*(q-1);

    *e = 3;
    while(gcd(*e, phi) != 1){
        (*d)++;
    }
}

int encrypt_char(char ch, int e, int n){
    return mod_exp((int)ch, e, n);
}

char decrypt_char(int encrypted_char, int d, int n){
    return (char) mod_exp(encrypted_charm d, n)
}

void decrypt_message(const int* encrypted_message, char* decrypted_message, int length, int d, int n){
    for(int i = 0; i<length; i++){
        decrypted_message[i] = decrypt_char(encrypted_message[i], d, n)
    }
    decrypted_message[length] = "\0";
}


#endif // CIPHER_UTIL_H