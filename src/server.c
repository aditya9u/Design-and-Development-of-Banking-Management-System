#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server.h"
#include "customer.h"

#define DATA_FILE "bank_accounts.dat"
#define DEFAULT_PORT "5050"
#define REQUEST_SIZE 128

typedef struct {
    SOCKET socket;
} ClientContext;

static int sendResponse(SOCKET clientSocket, const char *response) {
    size_t length = strlen(response);
    size_t sent = 0;
    while (sent < length) {
        int result = send(clientSocket, response + sent, (int)(length - sent), 0);
        if (result == SOCKET_ERROR || result == 0) {
            return 0;
        }
        sent += (size_t)result;
    }
    return 1;
}

static int receiveRequest(SOCKET clientSocket, char request[REQUEST_SIZE]) {
    size_t used = 0;
    while (used < REQUEST_SIZE - 1) {
        char character;
        int received = recv(clientSocket, &character, 1, 0);
        if (received <= 0) {
            return 0;
        }
        if (character == '\n') {
            break;
        }
        if (character != '\r') {
            request[used++] = character;
        }
    }
    request[used] = '\0';
    return used > 0 && used < REQUEST_SIZE - 1;
}

static DWORD WINAPI handleClient(LPVOID parameter) {
    ClientContext *context = (ClientContext *)parameter;
    SOCKET clientSocket = context->socket;
    free(context);

    char request[REQUEST_SIZE] = {0};
    if (!receiveRequest(clientSocket, request)) {
        sendResponse(clientSocket, "ERROR Invalid request\n");
        closesocket(clientSocket);
        return 0;
    }

    int accountNumber;
    char password[20];
    char extra;
    if (sscanf(request, "LOGIN %d %19s %c", &accountNumber, password, &extra) != 2) {
        sendResponse(clientSocket, "ERROR Expected LOGIN <account-number> <password>\n");
        closesocket(clientSocket);
        return 0;
    }

    Account accounts[MAX_ACCOUNTS];
    int count = 0;
    if (!loadAccountsFromFile(accounts, &count, DATA_FILE)) {
        sendResponse(clientSocket, "ERROR Account database is unavailable\n");
        closesocket(clientSocket);
        return 0;
    }

    int accountIndex = loginCustomer(accounts, count, accountNumber, password);
    if (accountIndex == -1) {
        sendResponse(clientSocket, "ERROR Invalid account number or password\n");
        closesocket(clientSocket);
        return 0;
    }

    char safeName[sizeof(accounts[accountIndex].name)];
    snprintf(safeName, sizeof(safeName), "%s", accounts[accountIndex].name);
    for (size_t i = 0; safeName[i] != '\0'; i++) {
        if (safeName[i] == '\r' || safeName[i] == '\n' || safeName[i] == '|') {
            safeName[i] = ' ';
        }
    }

    char response[160];
    snprintf(response, sizeof(response), "OK|%d|%.2f|%s\n",
             accounts[accountIndex].accountNumber,
             accounts[accountIndex].balance,
             safeName);
    sendResponse(clientSocket, response);
    closesocket(clientSocket);
    return 0;
}

static int initializeDatabase(void) {
    Account accounts[MAX_ACCOUNTS];
    int count = 0;
    if (loadAccountsFromFile(accounts, &count, DATA_FILE)) {
        return 1;
    }

    if (GetFileAttributesA(DATA_FILE) != INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "Account database exists but could not be loaded.\n");
        return 0;
    }

    loadSampleData(accounts, &count);
    if (!saveAccountsToFile(accounts, count, DATA_FILE)) {
        fprintf(stderr, "Could not create the account database.\n");
        return 0;
    }
    return 1;
}

int runBankServer(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : DEFAULT_PORT;
    WSADATA winsockData;
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
        fprintf(stderr, "Winsock initialization failed.\n");
        return 1;
    }

    if (!initializeDatabase()) {
        WSACleanup();
        return 1;
    }

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *address = NULL;
    if (getaddrinfo(NULL, port, &hints, &address) != 0) {
        fprintf(stderr, "Invalid server port.\n");
        WSACleanup();
        return 1;
    }

    SOCKET listenSocket = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        fprintf(stderr, "Could not create server socket.\n");
        freeaddrinfo(address);
        WSACleanup();
        return 1;
    }

    BOOL reuseAddress = TRUE;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&reuseAddress, sizeof(reuseAddress));
    if (bind(listenSocket, address->ai_addr, (int)address->ai_addrlen) == SOCKET_ERROR ||
        listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "Could not bind or listen on port %s.\n", port);
        closesocket(listenSocket);
        freeaddrinfo(address);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(address);

    printf("Bank server listening on port %s. Press Ctrl+C to stop.\n", port);
    for (;;) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            fprintf(stderr, "Client connection could not be accepted.\n");
            continue;
        }

        ClientContext *context = (ClientContext *)malloc(sizeof(*context));
        if (context == NULL) {
            closesocket(clientSocket);
            continue;
        }
        context->socket = clientSocket;

        HANDLE thread = CreateThread(NULL, 0, handleClient, context, 0, NULL);
        if (thread == NULL) {
            closesocket(clientSocket);
            free(context);
            continue;
        }
        CloseHandle(thread);
    }
}

int main(int argc, char *argv[]) {
    return runBankServer(argc, argv);
}