#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "bank_core.h"

#define ACCOUNT_FILE "bank_accounts.dat"
#define TRANSACTION_FILE "bank_transactions.dat"

typedef struct {
    int type;
    int sourceAccountId;
    int targetAccountId;
    double amount;
} TransactionRequest;

typedef struct {
    int succeeded;
    double sourceBalanceAfter;
    char message[128];
} TransactionResponse;

static int requestPipe[2] = {-1, -1};
static int responsePipe[2] = {-1, -1};
static pthread_t workerThread;
static pthread_mutex_t requestMutex = PTHREAD_MUTEX_INITIALIZER;
static int workerStarted;

static int readAll(int descriptor, void *buffer, size_t size) {
    unsigned char *bytes = buffer;
    size_t received = 0;

    while (received < size) {
        ssize_t result = read(descriptor, bytes + received, size - received);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return 0;
        }
        received += (size_t)result;
    }
    return 1;
}

static int writeAll(int descriptor, const void *buffer, size_t size) {
    const unsigned char *bytes = buffer;
    size_t sent = 0;

    while (sent < size) {
        ssize_t result = write(descriptor, bytes + sent, size - sent);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return 0;
        }
        sent += (size_t)result;
    }
    return 1;
}

static void setResponse(TransactionResponse *response, const char *message) {
    snprintf(response->message, sizeof(response->message), "%s", message);
}

static void processTransaction(const TransactionRequest *request,
                               TransactionResponse *response) {
    Account accounts[MAX_ACCOUNTS] = {0};
    Transaction transactions[MAX_TRANSACTIONS] = {0};
    int accountCount = 0;
    int transactionCount = 0;

    if (!isfinite(request->amount) || request->amount <= 0) {
        setResponse(response, "Transaction amount must be a finite positive number.");
        return;
    }
    if (!beginBankDataTransaction()) {
        setResponse(response, "Could not acquire bank data lock.");
        return;
    }

    if (!loadAccountsFromFile(accounts, &accountCount, ACCOUNT_FILE) ||
        !loadTransactionsFromFile(transactions, &transactionCount, TRANSACTION_FILE)) {
        endBankDataTransaction();
        setResponse(response, "Could not load current account records.");
        return;
    }

    int sourceIndex = findAccountById(accounts, accountCount, request->sourceAccountId);
    if (sourceIndex == -1 || !accounts[sourceIndex].status) {
        endBankDataTransaction();
        setResponse(response, "Source account is unavailable or inactive.");
        return;
    }

    if (request->type == BANK_TXN_TRANSFER) {
        int targetIndex = findAccountById(accounts, accountCount, request->targetAccountId);
        if (targetIndex == -1 || !accounts[targetIndex].status) {
            endBankDataTransaction();
            setResponse(response, "Recipient account is unavailable or inactive.");
            return;
        }
        if (targetIndex == sourceIndex) {
            endBankDataTransaction();
            setResponse(response, "Cannot transfer to the same account.");
            return;
        }
        if (accounts[sourceIndex].balance < request->amount) {
            endBankDataTransaction();
            setResponse(response, "Insufficient balance for transfer.");
            return;
        }
        if (transactionCount > MAX_TRANSACTIONS - 2 ||
            !isfinite(accounts[targetIndex].balance + request->amount)) {
            endBankDataTransaction();
            setResponse(response, "Transfer cannot be recorded safely.");
            return;
        }

        accounts[sourceIndex].balance -= request->amount;
        accounts[targetIndex].balance += request->amount;
        addTransaction(transactions, &transactionCount, request->sourceAccountId,
                       "TRANSFER_OUT", request->amount, "Funds transferred");
        addTransaction(transactions, &transactionCount, request->targetAccountId,
                       "TRANSFER_IN", request->amount, "Funds received");
    } else if (request->type == BANK_TXN_DEPOSIT) {
        if (transactionCount >= MAX_TRANSACTIONS ||
            !isfinite(accounts[sourceIndex].balance + request->amount)) {
            endBankDataTransaction();
            setResponse(response, "Deposit cannot be recorded safely.");
            return;
        }
        accounts[sourceIndex].balance += request->amount;
        addTransaction(transactions, &transactionCount, request->sourceAccountId,
                       "DEPOSIT", request->amount, "Cash deposit");
    } else if (request->type == BANK_TXN_WITHDRAW) {
        if (accounts[sourceIndex].balance < request->amount) {
            endBankDataTransaction();
            setResponse(response, "Insufficient balance.");
            return;
        }
        if (transactionCount >= MAX_TRANSACTIONS) {
            endBankDataTransaction();
            setResponse(response, "Transaction history is full.");
            return;
        }
        accounts[sourceIndex].balance -= request->amount;
        addTransaction(transactions, &transactionCount, request->sourceAccountId,
                       "WITHDRAW", request->amount, "Cash withdrawal");
    } else {
        endBankDataTransaction();
        setResponse(response, "Unknown transaction type.");
        return;
    }

    if (!saveAccountsToFile(accounts, accountCount, ACCOUNT_FILE) ||
        !saveTransactionsToFile(transactions, transactionCount, TRANSACTION_FILE)) {
        endBankDataTransaction();
        setResponse(response, "Transaction data could not be fully saved; inspect the data files.");
        return;
    }

    response->succeeded = 1;
    response->sourceBalanceAfter = accounts[sourceIndex].balance;
    setResponse(response, "Transaction completed.");
    endBankDataTransaction();
}

static void *transactionWorker(void *unused) {
    (void)unused;
    for (;;) {
        TransactionRequest request;
        TransactionResponse response = {0};

        if (!readAll(requestPipe[0], &request, sizeof(request))) {
            return NULL;
        }
        if (request.type == 0) {
            response.succeeded = 1;
            setResponse(&response, "Worker stopped.");
            if (!writeAll(responsePipe[1], &response, sizeof(response))) {
                return NULL;
            }
            return NULL;
        }

        processTransaction(&request, &response);
        if (!writeAll(responsePipe[1], &response, sizeof(response))) {
            return NULL;
        }
    }
}

int startBankTransactionWorker(void) {
    if (workerStarted) {
        return 1;
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "Could not configure transaction worker pipe handling.\n");
        return 0;
    }
    if (pipe(requestPipe) == -1 || pipe(responsePipe) == -1) {
        fprintf(stderr, "Could not create transaction worker pipes: %s.\n", strerror(errno));
        if (requestPipe[0] != -1) close(requestPipe[0]);
        if (requestPipe[1] != -1) close(requestPipe[1]);
        if (responsePipe[0] != -1) close(responsePipe[0]);
        if (responsePipe[1] != -1) close(responsePipe[1]);
        requestPipe[0] = requestPipe[1] = -1;
        responsePipe[0] = responsePipe[1] = -1;
        return 0;
    }
    int error = pthread_create(&workerThread, NULL, transactionWorker, NULL);
    if (error != 0) {
        fprintf(stderr, "Could not start transaction worker thread: %s.\n", strerror(error));
        close(requestPipe[0]);
        close(requestPipe[1]);
        close(responsePipe[0]);
        close(responsePipe[1]);
        requestPipe[0] = requestPipe[1] = -1;
        responsePipe[0] = responsePipe[1] = -1;
        return 0;
    }
    workerStarted = 1;
    return 1;
}

int stopBankTransactionWorker(void) {
    if (!workerStarted) {
        return 1;
    }

    TransactionRequest request = {0};
    TransactionResponse response;
    int error = pthread_mutex_lock(&requestMutex);
    if (error != 0) {
        fprintf(stderr, "Could not lock transaction worker queue: %s.\n", strerror(error));
        return 0;
    }
    int sent = writeAll(requestPipe[1], &request, sizeof(request));
    int received = sent && readAll(responsePipe[0], &response, sizeof(response));
    pthread_mutex_unlock(&requestMutex);

    error = pthread_join(workerThread, NULL);
    close(requestPipe[0]);
    close(requestPipe[1]);
    close(responsePipe[0]);
    close(responsePipe[1]);
    requestPipe[0] = requestPipe[1] = -1;
    responsePipe[0] = responsePipe[1] = -1;
    workerStarted = 0;

    if (error != 0 || !received) {
        fprintf(stderr, "Could not stop transaction worker cleanly.\n");
        return 0;
    }
    return 1;
}

int submitBankTransaction(BankTransactionType type, int sourceAccountId,
                          int targetAccountId, double amount,
                          double *sourceBalanceAfter) {
    if (!workerStarted) {
        fprintf(stderr, "Transaction worker is not running.\n");
        return 0;
    }

    TransactionRequest request = {
        (int)type, sourceAccountId, targetAccountId, amount
    };
    TransactionResponse response = {0};

    int error = pthread_mutex_lock(&requestMutex);
    if (error != 0) {
        fprintf(stderr, "Could not lock transaction worker queue: %s.\n", strerror(error));
        return 0;
    }
    int succeeded = writeAll(requestPipe[1], &request, sizeof(request)) &&
                   readAll(responsePipe[0], &response, sizeof(response));
    pthread_mutex_unlock(&requestMutex);

    if (!succeeded) {
        fprintf(stderr, "Transaction worker communication failed.\n");
        return 0;
    }
    if (!response.succeeded) {
        printf("%s\n", response.message);
        return 0;
    }
    if (sourceBalanceAfter != NULL) {
        *sourceBalanceAfter = response.sourceBalanceAfter;
    }
    return 1;
}
