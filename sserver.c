#ifdef _WIN32
shutdown(serverSocketFD, SD_BOTH);
#else
shutdown(serverSocketFD, SHUT_RDWR);
#endif

#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "socketutil.h"

struct AcceptedSocket
{
    int acceptedSocketFD;
    struct sockaddr_in address;
    int error;
    bool acceptedSuccessfully;
};

struct AcceptedSocket * acceptIncomingConnection(int serverSocketFD);
void acceptNewConnectionAndReceiveAndPrintItsData(int serverSocketFD);
void *receiveAndPrintIncomingData(void *arg);

void startAcceptingIncomingConnections(int serverSocketFD);

void receiveAndPrintIncomingDataOnSeparateThread(int socketFD);

void sendReceivedMessageToTheOtherClients(char *buffer,int socketFD);

struct AcceptedSocket acceptedSockets[10] ;
int acceptedSocketsCount = 0;


void startAcceptingIncomingConnections(int serverSocketFD) {

    while(true)
    {
        struct AcceptedSocket* clientSocket  = acceptIncomingConnection(serverSocketFD);
        acceptedSockets[acceptedSocketsCount++] = *clientSocket;

        receiveAndPrintIncomingDataOnSeparateThread(clientSocket);
    }
}



void receiveAndPrintIncomingDataOnSeparateThread(int socketFD) {
    pthread_t id;
    int *pSocketFD = malloc(sizeof(int));
    *pSocketFD = socketFD;
    pthread_create(&id, NULL, receiveAndPrintIncomingData, pSocketFD);
}

void *receiveAndPrintIncomingData(void *arg) {
    int socketFD = *(int *)arg;
    char buffer[1024];

    while (true) {
        ssize_t amountReceived = recv(socketFD, buffer, 1024, 0);

        if (amountReceived > 0) {
            buffer[amountReceived] = 0;
            printf("%s\n", buffer);
        } else {
            break;
        }
    }

    close(socketFD);
    free(arg);
    return NULL;
}

void sendReceivedMessageToTheOtherClients(char *buffer,int socketFD) {

    for(int i = 0 ; i<acceptedSocketsCount ; i++)
        if(acceptedSockets[i].acceptedSocketFD !=socketFD)
        {
            send(acceptedSockets[i].acceptedSocketFD,buffer, strlen(buffer),0);
        }

}

struct AcceptedSocket * acceptIncomingConnection(int serverSocketFD) {
    struct sockaddr_in  clientAddress ;
    int clientAddressSize = sizeof (struct sockaddr_in);
    int clientSocketFD = accept(serverSocketFD, (struct sockaddr *)&clientAddress, &clientAddressSize);
    struct AcceptedSocket* acceptedSocket = malloc(sizeof (struct AcceptedSocket));
    acceptedSocket->address = clientAddress;
    acceptedSocket->acceptedSocketFD = clientSocketFD;
    acceptedSocket->acceptedSuccessfully = clientSocketFD>0;

    if(!acceptedSocket->acceptedSuccessfully)
        acceptedSocket->error = clientSocketFD;



    return acceptedSocket;
}


int main() {

    int serverSocketFD = createTCPIpv4Socket();
    struct sockaddr_in *serverAddress = createIPv4Address("",2000);

    int result = bind(serverSocketFD, (const struct sockaddr *)serverAddress, sizeof(*serverAddress));    if(result == 0)
        printf("socket was bound successfully\n");

    int listenResult = listen(serverSocketFD,10);

    startAcceptingIncomingConnections(serverSocketFD);

    shutdown(serverSocketFD,SHUT_RDWR);

    return 0;
}