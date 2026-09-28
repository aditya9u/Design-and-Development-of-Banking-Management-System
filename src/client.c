#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT "5050"

static int sendRequest(SOCKET socketHandle, const char *request) {
    size_t length = strlen(request);
    size_t sent = 0;
    while (sent < length) {
        int result = send(socketHandle, request + sent, (int)(length - sent), 0);
        if (result == SOCKET_ERROR || result == 0) {
            return 0;
        }
        sent += (size_t)result;
    }
    return 1;
}

int runBankClient(int argc, char *argv[]) {
    const char *host = argc > 1 ? argv[1] : DEFAULT_HOST;
    const char *port = argc > 2 ? argv[2] : DEFAULT_PORT;
    int accountNumber;
    char password[20];

    printf("Account number: ");
    if (scanf("%d", &accountNumber) != 1) {
        fprintf(stderr, "Invalid account number.\n");
        return 1;
    }
    printf("Password: ");
    if (scanf("%19s", password) != 1) {
        fprintf(stderr, "Could not read password.\n");
        return 1;
    }

    WSADATA winsockData;
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
        fprintf(stderr, "Winsock initialization failed.\n");
        return 1;
    }

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo *addresses = NULL;
    if (getaddrinfo(host, port, &hints, &addresses) != 0) {
        fprintf(stderr, "Could not resolve server address.\n");
        WSACleanup();
        return 1;
    }

    SOCKET serverSocket = INVALID_SOCKET;
    for (struct addrinfo *address = addresses; address != NULL; address = address->ai_next) {
        serverSocket = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (serverSocket != INVALID_SOCKET &&
            connect(serverSocket, address->ai_addr, (int)address->ai_addrlen) == 0) {
            break;
        }
        if (serverSocket != INVALID_SOCKET) {
            closesocket(serverSocket);
            serverSocket = INVALID_SOCKET;
        }
    }
    freeaddrinfo(addresses);

    if (serverSocket == INVALID_SOCKET) {
        fprintf(stderr, "Could not connect to %s:%s.\n", host, port);
        WSACleanup();
        return 1;
    }

    char request[64];
    snprintf(request, sizeof(request), "LOGIN %d %s\n", accountNumber, password);
    if (!sendRequest(serverSocket, request)) {
        fprintf(stderr, "Could not send login request.\n");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    char response[256];
    int received;
    while ((received = recv(serverSocket, response, sizeof(response) - 1, 0)) > 0) {
        response[received] = '\0';
        fputs(response, stdout);
    }

    closesocket(serverSocket);
    WSACleanup();
    return received == SOCKET_ERROR ? 1 : 0;
}

int main(int argc, char *argv[]) {
    return runBankClient(argc, argv);
}