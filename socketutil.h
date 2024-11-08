#ifndef SOCKETUTIL_SOCKETUTIL_H
#define SOCKETUTIL_SOCKETUTIL_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <string.h>
#include <malloc.h>



struct sockaddr_in* createIPv4Address(char *ip, int port);

int createTCPIpv4Socket();


#endif //SOCKETUTIL_SOCKETUTIL_H