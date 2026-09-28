#ifndef BANK_CORE_H
#define BANK_CORE_H

#define MAX_ACCOUNTS 100
#define MAX_TRANSACTIONS 50
#define MAX_LOAN_APPLICATIONS 100
#define DEFAULT_EMPLOYEE_ID "employee"

typedef struct {
    int id;
    char type[20];
    double amount;
    char date[11];
    char description[80];
} Transaction;

typedef struct {
    int accountNumber;
    char name[50];
    char password[20];
    double balance;
    int isLocked;
    int transactionCount;
    Transaction transactions[MAX_TRANSACTIONS];
} Account;

typedef struct {
    int id;
    int accountNumber;
    double amount;
    char purpose[80];
    char status[12];
    char assignedEmployeeId[32];
} LoanApplication;

void displayMainMenu(void);
int findAccount(const Account accounts[], int count, int accountNumber);
void loadSampleData(Account accounts[], int *count);
int lockAccount(Account *account);
void unlockAccount(Account *account);
void recordTransaction(Account *account, const char *type, double amount, const char *description);
void displayTransactionHistory(const Account *account);
void displayAccounts(const Account accounts[], int count);
int saveAccountsToFile(const Account accounts[], int count, const char *fileName);
int loadAccountsFromFile(Account accounts[], int *count, const char *fileName);
int saveLoanApplications(const LoanApplication loans[], int count, const char *fileName);
int loadLoanApplications(LoanApplication loans[], int *count, const char *fileName);

#endif

