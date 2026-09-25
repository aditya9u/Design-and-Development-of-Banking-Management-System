#include <stdio.h>
#include <string.h>
#include "bank.h"

void displayMenu(void) {
    printf("\n===== Banking Management System =====\n");
    printf("1. Create Account\n");
    printf("2. Deposit Money\n");
    printf("3. Withdraw Money\n");
    printf("4. Display All Accounts\n");
    printf("5. Exit\n");
    printf("Select an option: ");
}

int findAccount(const Account accounts[], int count, int accountNumber) {
    for (int i = 0; i < count; i++) {
        if (accounts[i].accountNumber == accountNumber) {
            return i;
        }
    }
    return -1;
}

void loadSampleData(Account accounts[], int *count) {
    Account sample[] = {
        {1001, "Alice Johnson", 2500.00},
        {1002, "Bob Smith", 5200.50},
        {1003, "Charlie Brown", 4100.75}
    };

    int sampleCount = sizeof(sample) / sizeof(sample[0]);
    for (int i = 0; i < sampleCount; i++) {
        accounts[*count] = sample[i];
        (*count)++;
    }
}

void addAccount(Account accounts[], int *count) {
    if (*count >= MAX_ACCOUNTS) {
        printf("Account limit reached. Cannot add more accounts.\n");
        return;
    }

    Account newAccount;
    printf("Enter account number: ");
    scanf("%d", &newAccount.accountNumber);
    getchar();

    printf("Enter account holder name: ");
    fgets(newAccount.name, sizeof(newAccount.name), stdin);
    newAccount.name[strcspn(newAccount.name, "\n")] = '\0';

    printf("Enter initial balance: ");
    scanf("%lf", &newAccount.balance);

    accounts[*count] = newAccount;
    (*count)++;
    printf("Account created successfully.\n");
}

void deposit(Account accounts[], int count) {
    int accountNumber;
    double amount;

    printf("Enter account number: ");
    scanf("%d", &accountNumber);
    printf("Enter deposit amount: ");
    scanf("%lf", &amount);

    int index = findAccount(accounts, count, accountNumber);
    if (index == -1) {
        printf("Account not found.\n");
        return;
    }

    if (amount > 0) {
        accounts[index].balance += amount;
        printf("Deposit successful. New balance: %.2f\n", accounts[index].balance);
    } else {
        printf("Deposit amount must be positive.\n");
    }
}

void withdraw(Account accounts[], int count) {
    int accountNumber;
    double amount;

    printf("Enter account number: ");
    scanf("%d", &accountNumber);
    printf("Enter withdrawal amount: ");
    scanf("%lf", &amount);

    int index = findAccount(accounts, count, accountNumber);
    if (index == -1) {
        printf("Account not found.\n");
        return;
    }

    if (amount <= 0) {
        printf("Withdrawal amount must be positive.\n");
        return;
    }

    if (amount > accounts[index].balance) {
        printf("Insufficient balance.\n");
        return;
    }

    accounts[index].balance -= amount;
    printf("Withdrawal successful. New balance: %.2f\n", accounts[index].balance);
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
