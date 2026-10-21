#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "bank_core.h"

#define BANK_DATA_LOCK_FILE ".bank_data.lock"

static pthread_mutex_t bankThreadMutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local unsigned int bankLockDepth;
static _Thread_local int bankLockFileDescriptor = -1;

int beginBankDataTransaction(void) {
    if (bankLockDepth > 0) {
        bankLockDepth++;
        return 1;
    }

    int error = pthread_mutex_lock(&bankThreadMutex);
    if (error != 0) {
        fprintf(stderr, "Could not acquire the in-process bank lock: %s.\n", strerror(error));
        return 0;
    }

    bankLockFileDescriptor = open(BANK_DATA_LOCK_FILE, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (bankLockFileDescriptor == -1) {
        fprintf(stderr, "Could not open bank lock file: %s.\n", strerror(errno));
        pthread_mutex_unlock(&bankThreadMutex);
        return 0;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    while (fcntl(bankLockFileDescriptor, F_SETLKW, &lock) == -1) {
        if (errno == EINTR) {
            continue;
        }
        fprintf(stderr, "Could not acquire bank file lock: %s.\n", strerror(errno));
        close(bankLockFileDescriptor);
        bankLockFileDescriptor = -1;
        pthread_mutex_unlock(&bankThreadMutex);
        return 0;
    }

    bankLockDepth = 1;
    return 1;
}

void endBankDataTransaction(void) {
    if (bankLockDepth == 0) {
        fprintf(stderr, "Bank data lock release requested without a matching acquire.\n");
        return;
    }
    bankLockDepth--;
    if (bankLockDepth > 0) {
        return;
    }

    struct flock lock = {0};
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(bankLockFileDescriptor, F_SETLK, &lock) == -1) {
        fprintf(stderr, "Could not release bank file lock: %s.\n", strerror(errno));
    }
    if (close(bankLockFileDescriptor) == -1) {
        fprintf(stderr, "Could not close bank lock file: %s.\n", strerror(errno));
    }
    bankLockFileDescriptor = -1;
    int error = pthread_mutex_unlock(&bankThreadMutex);
    if (error != 0) {
        fprintf(stderr, "Could not release the in-process bank lock: %s.\n", strerror(error));
    }
}

static void getCurrentTimestamp(char buffer[], size_t size) {
    time_t now = time(NULL);
    struct tm *tmInfo = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tmInfo);
}

void initializeSampleData(User users[], int *userCount,
                         Account accounts[], int *accountCount,
                         Transaction transactions[], int *transactionCount,
                         Feedback feedback[], int *feedbackCount) {
    User sampleUsers[] = {
        {1, "alice", "alice123", ROLE_CUSTOMER, 1, 0},
        {2, "manager", "manager123", ROLE_MANAGER, 1, 0},
        {3, "employee", "emp123", ROLE_EMPLOYEE, 1, 0},
        {4, "admin", "admin123", ROLE_ADMIN, 1, 0}
    };

    Account sampleAccounts[] = {
        {1001, 1, 2500.00, 1},
        {1002, 2, 1200.00, 1},
        {1003, 3, 3400.00, 1}
    };

    Transaction sampleTransactions[] = {
        {1, 1001, "DEPOSIT", 2500.00, "Initial deposit", "2024-01-01 10:00:00"},
        {2, 1002, "DEPOSIT", 1200.00, "Initial deposit", "2024-01-01 10:05:00"},
        {3, 1003, "DEPOSIT", 3400.00, "Initial deposit", "2024-01-01 10:10:00"}
    };

    Feedback sampleFeedback[] = {
        {1, 1, "Customer service is good.", "2024-01-02 09:00:00"}
    };

    int sampleUserCount = (int)(sizeof(sampleUsers) / sizeof(sampleUsers[0]));
    int sampleAccountCount = (int)(sizeof(sampleAccounts) / sizeof(sampleAccounts[0]));
    int sampleTransactionCount = (int)(sizeof(sampleTransactions) / sizeof(sampleTransactions[0]));
    int sampleFeedbackCount = (int)(sizeof(sampleFeedback) / sizeof(sampleFeedback[0]));

    for (int i = 0; i < sampleUserCount; i++) {
        users[*userCount + i] = sampleUsers[i];
    }
    *userCount += sampleUserCount;

    for (int i = 0; i < sampleAccountCount; i++) {
        accounts[*accountCount + i] = sampleAccounts[i];
    }
    *accountCount += sampleAccountCount;

    for (int i = 0; i < sampleTransactionCount; i++) {
        transactions[*transactionCount + i] = sampleTransactions[i];
    }
    *transactionCount += sampleTransactionCount;

    for (int i = 0; i < sampleFeedbackCount; i++) {
        feedback[*feedbackCount + i] = sampleFeedback[i];
    }
    *feedbackCount += sampleFeedbackCount;
}

static int saveRecordsToFile(const void *records, size_t recordSize, int count,
                             int maxCount, const char *fileName) {
    if (count < 0 || count > maxCount || !beginBankDataTransaction()) {
        return 0;
    }

    char temporaryName[PATH_MAX];
    int pathLength = snprintf(temporaryName, sizeof(temporaryName), "%s.tmp.%ld",
                              fileName, (long)getpid());
    if (pathLength < 0 || (size_t)pathLength >= sizeof(temporaryName)) {
        endBankDataTransaction();
        return 0;
    }

    FILE *file = fopen(temporaryName, "wb");
    if (file == NULL) {
        endBankDataTransaction();
        return 0;
    }

    if (fwrite(&count, sizeof(int), 1, file) != 1) {
        fclose(file);
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }

    if (count > 0 && fwrite(records, recordSize, (size_t)count, file) != (size_t)count) {
        fclose(file);
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }

    int flushSucceeded = fflush(file) == 0;
    int closeSucceeded = fclose(file) == 0;
    if (!flushSucceeded || !closeSucceeded) {
        fprintf(stderr, "Could not flush data file %s.\n", fileName);
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }
    int syncDescriptor = open(temporaryName, O_RDONLY);
    if (syncDescriptor == -1 || fsync(syncDescriptor) == -1) {
        fprintf(stderr, "Could not sync temporary data file %s: %s.\n",
                temporaryName, strerror(errno));
        if (syncDescriptor != -1) {
            close(syncDescriptor);
        }
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }
    close(syncDescriptor);
    if (rename(temporaryName, fileName) == -1) {
        fprintf(stderr, "Could not commit data file %s: %s.\n", fileName, strerror(errno));
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }

    endBankDataTransaction();
    return 1;
}

static int loadRecordsFromFile(void *records, size_t recordSize, int *count,
                               int maxCount, const char *fileName) {
    if (count == NULL || !beginBankDataTransaction()) {
        return 0;
    }

    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        endBankDataTransaction();
        return 0;
    }

    int loadedCount = 0;
    if (fread(&loadedCount, sizeof(int), 1, file) != 1) {
        fclose(file);
        endBankDataTransaction();
        return 0;
    }

    if (loadedCount < 0 || loadedCount > maxCount) {
        fclose(file);
        endBankDataTransaction();
        return 0;
    }

    if (loadedCount > 0 &&
        fread(records, recordSize, (size_t)loadedCount, file) != (size_t)loadedCount) {
        fclose(file);
        endBankDataTransaction();
        return 0;
    }

    *count = loadedCount;
    fclose(file);
    endBankDataTransaction();
    return 1;
}

int saveUsersToFile(const User users[], int count, const char *fileName) {
    return saveRecordsToFile(users, sizeof(User), count, MAX_USERS, fileName);
}

int loadUsersFromFile(User users[], int *count, const char *fileName) {
    return loadRecordsFromFile(users, sizeof(User), count, MAX_USERS, fileName);
}

int saveAccountsToFile(const Account accounts[], int count, const char *fileName) {
    return saveRecordsToFile(accounts, sizeof(Account), count, MAX_ACCOUNTS, fileName);
}

int loadAccountsFromFile(Account accounts[], int *count, const char *fileName) {
    return loadRecordsFromFile(accounts, sizeof(Account), count, MAX_ACCOUNTS, fileName);
}

int saveTransactionsToFile(const Transaction transactions[], int count, const char *fileName) {
    return saveRecordsToFile(transactions, sizeof(Transaction), count, MAX_TRANSACTIONS, fileName);
}

int loadTransactionsFromFile(Transaction transactions[], int *count, const char *fileName) {
    return loadRecordsFromFile(transactions, sizeof(Transaction), count, MAX_TRANSACTIONS, fileName);
}

int saveFeedbackToFile(const Feedback feedback[], int count, const char *fileName) {
    return saveRecordsToFile(feedback, sizeof(Feedback), count, MAX_FEEDBACK, fileName);
}

int loadFeedbackFromFile(Feedback feedback[], int *count, const char *fileName) {
    return loadRecordsFromFile(feedback, sizeof(Feedback), count, MAX_FEEDBACK, fileName);
}

int findUserByUsername(const User users[], int count, const char *username) {
    for (int i = 0; i < count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

int findUserById(const User users[], int count, int userId) {
    for (int i = 0; i < count; i++) {
        if (users[i].user_id == userId) {
            return i;
        }
    }
    return -1;
}

int setUserSessionState(User users[], int *count, int userId, int isLoggedIn) {
    if (count == NULL || !beginBankDataTransaction()) {
        return 0;
    }

    User currentUsers[MAX_USERS];
    int currentCount = 0;
    if (!loadUsersFromFile(currentUsers, &currentCount, "bank_users.dat")) {
        endBankDataTransaction();
        return 0;
    }

    int userIndex = findUserById(currentUsers, currentCount, userId);
    if (userIndex == -1) {
        endBankDataTransaction();
        return 0;
    }

    currentUsers[userIndex].is_logged_in = isLoggedIn != 0;
    int saved = saveUsersToFile(currentUsers, currentCount, "bank_users.dat");
    if (saved) {
        memcpy(users, currentUsers, (size_t)currentCount * sizeof(User));
        *count = currentCount;
    }
    endBankDataTransaction();
    return saved;
}

int findAccountForUser(const Account accounts[], int count, int userId) {
    for (int i = 0; i < count; i++) {
        if (accounts[i].user_id == userId) {
            return i;
        }
    }
    return -1;
}

int findAccountById(const Account accounts[], int count, int accountId) {
    for (int i = 0; i < count; i++) {
        if (accounts[i].account_id == accountId) {
            return i;
        }
    }
    return -1;
}

void addTransaction(Transaction transactions[], int *transactionCount,
                    int accountId, const char *type, double amount, const char *details) {
    if (*transactionCount >= MAX_TRANSACTIONS) {
        printf("Transaction limit reached.\n");
        return;
    }

    Transaction newTransaction = {0};
    newTransaction.txn_id = *transactionCount + 1;
    newTransaction.account_id = accountId;
    snprintf(newTransaction.type, sizeof(newTransaction.type), "%s", type);
    newTransaction.amount = amount;
    snprintf(newTransaction.details, sizeof(newTransaction.details), "%s", details);
    getCurrentTimestamp(newTransaction.timestamp, sizeof(newTransaction.timestamp));

    transactions[*transactionCount] = newTransaction;
    (*transactionCount)++;
}

void printTransactions(const Transaction transactions[], int transactionCount, int accountId) {
    int found = 0;
    printf("\nTransaction History\n");
    printf("---------------------------------------------------\n");
    printf("ID  Account  Type         Amount   Date\n");
    printf("---------------------------------------------------\n");

    for (int i = 0; i < transactionCount; i++) {
        if (transactions[i].account_id == accountId) {
            printf("%2d  %7d  %-12s %8.2f  %s\n",
                   transactions[i].txn_id,
                   transactions[i].account_id,
                   transactions[i].type,
                   transactions[i].amount,
                   transactions[i].timestamp);
            found = 1;
        }
    }

    if (!found) {
        printf("No transactions available.\n");
    }
}

void printFeedback(const Feedback feedback[], int feedbackCount, int userId) {
    int found = 0;
    printf("\nCustomer Feedback\n");
    printf("----------------------------------------\n");
    for (int i = 0; i < feedbackCount; i++) {
        if (feedback[i].user_id == userId) {
            printf("[%d] %s\n", feedback[i].feedback_id, feedback[i].message);
            printf("Date: %s\n", feedback[i].timestamp);
            found = 1;
        }
    }
    if (!found) {
        printf("No feedback available.\n");
    }
}

void printAllUsers(const User users[], int count) {
    printf("\nUsers\n");
    printf("----------------------------------------\n");
    for (int i = 0; i < count; i++) {
        printf("ID:%d Username:%s Role:%d Active:%d LoggedIn:%d\n",
               users[i].user_id,
               users[i].username,
               users[i].role_id,
               users[i].is_active,
               users[i].is_logged_in);
    }
}

void printAllAccounts(const Account accounts[], int count, const User users[], int userCount) {
    printf("\nAccounts\n");
    printf("----------------------------------------\n");
    for (int i = 0; i < count; i++) {
        const char *ownerName = "Unknown";
        for (int j = 0; j < userCount; j++) {
            if (users[j].user_id == accounts[i].user_id) {
                ownerName = users[j].username;
                break;
            }
        }
        printf("Account:%d Owner:%s Balance:%.2f Status:%d\n",
               accounts[i].account_id,
               ownerName,
               accounts[i].balance,
               accounts[i].status);
    }
}

/* Loan application persistence and helpers */

int saveLoanApplications(const LoanApplication loans[], int count, const char *fileName) {
    return saveRecordsToFile(loans, sizeof(LoanApplication), count,
                             MAX_LOAN_APPLICATIONS, fileName);
}

int loadLoanApplications(LoanApplication loans[], int *count, const char *fileName) {
    return loadRecordsFromFile(loans, sizeof(LoanApplication), count,
                               MAX_LOAN_APPLICATIONS, fileName);
}

int addLoanApplication(LoanApplication loans[], int *count, int accountId, double amount, const char *purpose, int assignedEmployeeId) {
    if (*count >= MAX_LOAN_APPLICATIONS) {
        return 0;
    }
    LoanApplication la = {0};
    la.loan_id = *count + 1;
    la.account_id = accountId;
    la.amount = amount;
    snprintf(la.purpose, sizeof(la.purpose), "%s", purpose);
    snprintf(la.status, sizeof(la.status), "PENDING");
    la.assigned_employee_id = assignedEmployeeId;
    time_t now = time(NULL);
    struct tm *tmInfo = localtime(&now);
    strftime(la.timestamp, sizeof(la.timestamp), "%Y-%m-%d %H:%M:%S", tmInfo);

    loans[*count] = la;
    (*count)++;
    return 1;
}

void printLoanApplications(const LoanApplication loans[], int count, int assignedEmployeeId) {
    printf("\nLoan Applications\n");
    printf("---------------------------------------------------\n");
    printf("ID  Account  Amount    Status     Assigned\n");
    printf("---------------------------------------------------\n");
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (assignedEmployeeId == 0 || loans[i].assigned_employee_id == assignedEmployeeId) {
            printf("%2d  %7d  %-8.2f %-10s %d\n",
                   loans[i].loan_id,
                   loans[i].account_id,
                   loans[i].amount,
                   loans[i].status,
                   loans[i].assigned_employee_id);
            found = 1;
        }
    }
    if (!found) {
        printf("No loan applications found.\n");
    }
}
