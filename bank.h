#ifndef BANK_H
#define BANK_H

#define MAX_ACCOUNTS 100

typedef struct {
    int accountNumber;
    char name[50];
    double balance;
} Account;

void displayMenu(void);
void addAccount(Account accounts[], int *count);
void deposit(Account accounts[], int count);
void withdraw(Account accounts[], int count);
void displayAccounts(const Account accounts[], int count);
int findAccount(const Account accounts[], int count, int accountNumber);
void loadSampleData(Account accounts[], int *count);

#endif
