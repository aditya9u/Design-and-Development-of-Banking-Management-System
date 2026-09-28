#include <stdio.h>
#include <string.h>
#include <time.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "bank_core.h"

#define PERSISTENCE_MUTEX_NAME "Local\\BankingManagementSystemPersistence"

typedef struct {
    int id;
    int accountNumber;
    double amount;
    char purpose[80];
    char status[12];
} LegacyLoanApplication;

static HANDLE lockPersistence(void) {
    HANDLE mutex = CreateMutexA(NULL, FALSE, PERSISTENCE_MUTEX_NAME);
    if (mutex == NULL) {
        return NULL;
    }

    DWORD result = WaitForSingleObject(mutex, INFINITE);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
        CloseHandle(mutex);
        return NULL;
    }
    return mutex;
}

static void unlockPersistence(HANDLE mutex) {
    if (mutex != NULL) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
}

void displayMainMenu(void) {
    printf("\n===== Banking Management System =====\n");
    printf("1. Customer Login\n");
    printf("2. Employee Login\n");
    printf("3. Exit\n");
    printf("Select an option: ");
}

int lockAccount(Account *account) {
    if (account == NULL) {
        return 0;
    }

    if (account->isLocked) {
        printf("This account is currently busy. Please try again later.\n");
        return 0;
    }

    account->isLocked = 1;
    return 1;
}

void unlockAccount(Account *account) {
    if (account != NULL) {
        account->isLocked = 0;
    }
}

int findAccount(const Account accounts[], int count, int accountNumber) {
    for (int i = 0; i < count; i++) {
        if (accounts[i].accountNumber == accountNumber) {
            return i;
        }
    }
    return -1;
}

int saveLoanApplications(const LoanApplication loans[], int count, const char *fileName) {
    HANDLE mutex = lockPersistence();
    if (mutex == NULL) {
        return 0;
    }
    FILE *file = fopen(fileName, "wb");
    if (file == NULL) {
        unlockPersistence(mutex);
        return 0;
    }

    if (fwrite(&count, sizeof(int), 1, file) != 1 ||
        (count > 0 && fwrite(loans, sizeof(LoanApplication), count, file) != (size_t)count)) {
        fclose(file);
        unlockPersistence(mutex);
        return 0;
    }

    fclose(file);
    unlockPersistence(mutex);
    return 1;
}

int loadLoanApplications(LoanApplication loans[], int *count, const char *fileName) {
    HANDLE mutex = lockPersistence();
    if (mutex == NULL) {
        return 0;
    }
    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        unlockPersistence(mutex);
        return 0;
    }

    int loadedCount = 0;
    if (fread(&loadedCount, sizeof(int), 1, file) != 1 ||
        loadedCount < 0 || loadedCount > MAX_LOAN_APPLICATIONS ||
        fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        unlockPersistence(mutex);
        return 0;
    }

    long fileSize = ftell(file);
    long legacySize = (long)sizeof(int) + (long)loadedCount * (long)sizeof(LegacyLoanApplication);
    long currentSize = (long)sizeof(int) + (long)loadedCount * (long)sizeof(LoanApplication);
    if (fileSize != legacySize && fileSize != currentSize) {
        fclose(file);
        unlockPersistence(mutex);
        return 0;
    }

    if (fseek(file, (long)sizeof(int), SEEK_SET) != 0) {
        fclose(file);
        unlockPersistence(mutex);
        return 0;
    }

    if (fileSize == legacySize) {
        for (int i = 0; i < loadedCount; i++) {
            LegacyLoanApplication oldApplication;
            if (fread(&oldApplication, sizeof(oldApplication), 1, file) != 1) {
                fclose(file);
                unlockPersistence(mutex);
                return 0;
            }
            loans[i].id = oldApplication.id;
            loans[i].accountNumber = oldApplication.accountNumber;
            loans[i].amount = oldApplication.amount;
            memcpy(loans[i].purpose, oldApplication.purpose, sizeof(loans[i].purpose));
            memcpy(loans[i].status, oldApplication.status, sizeof(loans[i].status));
            snprintf(loans[i].assignedEmployeeId, sizeof(loans[i].assignedEmployeeId),
                     "%s", DEFAULT_EMPLOYEE_ID);
        }
    } else if (loadedCount > 0 &&
               fread(loans, sizeof(LoanApplication), loadedCount, file) != (size_t)loadedCount) {
        fclose(file);
         unlockPersistence(mutex);
        return 0;
    }

    *count = loadedCount;
    fclose(file);
    unlockPersistence(mutex);
    return 1;
}

void recordTransaction(Account *account, const char *type, double amount, const char *description) {
    if (account == NULL || type == NULL || description == NULL) {
        return;
    }

    if (account->transactionCount >= MAX_TRANSACTIONS) {
        for (int i = 1; i < MAX_TRANSACTIONS; i++) {
            account->transactions[i - 1] = account->transactions[i];
        }
        account->transactionCount = MAX_TRANSACTIONS - 1;
    }

    Transaction *transaction = &account->transactions[account->transactionCount];
    transaction->id = account->transactionCount + 1;
    snprintf(transaction->type, sizeof(transaction->type), "%s", type);
    transaction->amount = amount;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(transaction->date, sizeof(transaction->date), "%Y-%m-%d", tm_info);
    snprintf(transaction->description, sizeof(transaction->description), "%s", description);
    account->transactionCount++;
}

void displayTransactionHistory(const Account *account) {
    if (account == NULL || account->transactionCount == 0) {
        printf("No transaction history available.\n");
        return;
    }

    printf("\nTransaction History\n");
    printf("---------------------------------------------------\n");
    printf("ID   Type       Amount      Date        Description\n");
    printf("---------------------------------------------------\n");

    for (int i = 0; i < account->transactionCount; i++) {
        printf("%2d   %-10s %-10.2f %-10s %s\n",
               account->transactions[i].id,
               account->transactions[i].type,
               account->transactions[i].amount,
               account->transactions[i].date,
               account->transactions[i].description);
    }
}

int saveAccountsToFile(const Account accounts[], int count, const char *fileName) {
    HANDLE mutex = lockPersistence();
    if (mutex == NULL) {
        return 0;
    }
    FILE *file = fopen(fileName, "wb");
    if (file == NULL) {
        unlockPersistence(mutex);
        return 0;
    }

    if (fwrite(&count, sizeof(int), 1, file) != 1) {
        fclose(file);
        unlockPersistence(mutex);
        return 0;
    }

    if (count > 0 && fwrite(accounts, sizeof(Account), count, file) != (size_t)count) {
        fclose(file);
        unlockPersistence(mutex);
        return 0;
    }

    fclose(file);
    unlockPersistence(mutex);
    return 1;
}

int loadAccountsFromFile(Account accounts[], int *count, const char *fileName) {
    HANDLE mutex = lockPersistence();
    if (mutex == NULL) {
        return 0;
    }
    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        unlockPersistence(mutex);
        return 0;
    }

    int loadedCount = 0;
    if (fread(&loadedCount, sizeof(int), 1, file) != 1) {
        fclose(file);
        unlockPersistence(mutex);
        return 0;
    }

    if (loadedCount < 0 || loadedCount > MAX_ACCOUNTS) {
        fclose(file);
        unlockPersistence(mutex);
        return 0;
    }

    if (loadedCount > 0 && fread(accounts, sizeof(Account), loadedCount, file) != (size_t)loadedCount) {
        fclose(file);
        unlockPersistence(mutex);
        return 0;
    }

    *count = loadedCount;
    fclose(file);
    unlockPersistence(mutex);
    return 1;
}

void loadSampleData(Account accounts[], int *count) {
    Account sample[] = {
        {1001, "Alice Johnson", "pass1001", 2500.00, 0, 0, {{0}}},
        {1002, "Bob Smith", "pass1002", 5200.50, 0, 0, {{0}}},
        {1003, "Charlie Brown", "pass1003", 4100.75, 0, 0, {{0}}}
    };

    int sampleCount = sizeof(sample) / sizeof(sample[0]);
    for (int i = 0; i < sampleCount; i++) {
        accounts[*count] = sample[i];
        (*count)++;
    }
}

void displayAccounts(const Account accounts[], int count) {
    if (count == 0) {
        printf("No accounts available.\n");
        return;
    }

    printf("\nAccount List\n");
    printf("---------------------------------------------------\n");
    printf("Acc No.   Name                 Balance\n");
    printf("---------------------------------------------------\n");

    for (int i = 0; i < count; i++) {
        printf("%7d   %-20s %.2f\n",
               accounts[i].accountNumber,
               accounts[i].name,
               accounts[i].balance);
    }
}