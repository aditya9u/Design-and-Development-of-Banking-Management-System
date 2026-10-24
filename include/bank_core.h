#ifndef BANK_CORE_H
#define BANK_CORE_H

#include <stdint.h>

#define MAX_USERS 100
#define MAX_ACCOUNTS 100
#define MAX_TRANSACTIONS 200
#define MAX_FEEDBACK 200
#define MAX_LOAN_APPLICATIONS 200

#define ROLE_CUSTOMER 1
#define ROLE_MANAGER 2
#define ROLE_EMPLOYEE 3
#define ROLE_ADMIN 4

int beginBankDataTransaction(void);
void endBankDataTransaction(void);

typedef struct {
    int user_id;
    char username[20];
    char password[20];
    int role_id;
    int is_active;
    int is_logged_in;
} User;

typedef struct {
    int account_id;
    int user_id;
    double balance;
    int status;
} Account;

typedef struct {
    int txn_id;
    int account_id;
    char type[20];
    double amount;
    char details[80];
    char timestamp[20];
} Transaction;

typedef struct {
    int feedback_id;
    int user_id;
    char message[200];
    char timestamp[20];
} Feedback;

typedef struct {
    int loan_id;
    int account_id;
    double amount;
    char purpose[200];
    char status[12]; /* PENDING, APPROVED, REJECTED */
    int assigned_employee_id; /* user_id of assigned employee or 0 */
    char timestamp[20];
} LoanApplication;

typedef enum {
    BANK_TXN_DEPOSIT = 1,
    BANK_TXN_WITHDRAW = 2,
    BANK_TXN_TRANSFER = 3
} BankTransactionType;

int startBankTransactionWorker(void);
int stopBankTransactionWorker(void);
int submitBankTransaction(BankTransactionType type, int sourceAccountId,
                          int targetAccountId, double amount,
                          double *sourceBalanceAfter);

void initializeSampleData(User users[], int *userCount,
                         Account accounts[], int *accountCount,
                         Transaction transactions[], int *transactionCount,
                         Feedback feedback[], int *feedbackCount);

int saveLoanApplications(const LoanApplication loans[], int count, const char *fileName);
int loadLoanApplications(LoanApplication loans[], int *count, const char *fileName);
int addLoanApplication(LoanApplication loans[], int *count, int accountId, double amount, const char *purpose, int assignedEmployeeId);
void printLoanApplications(const LoanApplication loans[], int count, int assignedEmployeeId);

int saveUsersToFile(const User users[], int count, const char *fileName);
int loadUsersFromFile(User users[], int *count, const char *fileName);

int saveAccountsToFile(const Account accounts[], int count, const char *fileName);
int loadAccountsFromFile(Account accounts[], int *count, const char *fileName);

int saveTransactionsToFile(const Transaction transactions[], int count, const char *fileName);
int loadTransactionsFromFile(Transaction transactions[], int *count, const char *fileName);

int saveFeedbackToFile(const Feedback feedback[], int count, const char *fileName);
int loadFeedbackFromFile(Feedback feedback[], int *count, const char *fileName);

int findUserByUsername(const User users[], int count, const char *username);
int findUserById(const User users[], int count, int userId);
int claimUserSession(int userId, uint64_t *sessionToken);
int isUserSessionCurrent(int userId, uint64_t sessionToken);
int updateUserPassword(int userId, uint64_t sessionToken,
                       const char *currentPassword, const char *newPassword);
int restoreStaleUserSessions(void);
int startUserSessionHeartbeat(int userId, uint64_t sessionToken);
int stopUserSessionHeartbeat(void);
int waitForUserSessionInput(int userId, uint64_t sessionToken);
int releaseUserSession(int userId, uint64_t sessionToken);
int findAccountForUser(const Account accounts[], int count, int userId);
int findAccountById(const Account accounts[], int count, int accountId);

void addTransaction(Transaction transactions[], int *transactionCount,
                    int accountId, const char *type, double amount, const char *details);

void printTransactions(const Transaction transactions[], int transactionCount, int accountId);
void printFeedback(const Feedback feedback[], int feedbackCount, int userId);
void printAllUsers(const User users[], int count);
void printAllAccounts(const Account accounts[], int count, const User users[], int userCount);

#endif
