#include <stdio.h>
#include <string.h>
#include "customer.h"

#define DATA_FILE "bank_accounts.dat"
#define LOAN_FILE "loan_applications.dat"

void displayCustomerMenu(void) {
    printf("\n===== Customer Dashboard =====\n");
    printf("1. View Balance\n");
    printf("2. Deposit Money\n");
    printf("3. Withdraw Money\n");
    printf("4. Transfer Funds\n");
    printf("5. View Transaction History\n");
    printf("6. Apply for a Loan\n");
    printf("7. Logout\n");
    printf("Select an option: ");
}

int loginCustomer(const Account accounts[], int count, int accountNumber, const char *password) {
    for (int i = 0; i < count; i++) {
        if (accounts[i].accountNumber == accountNumber && strcmp(accounts[i].password, password) == 0) {
            return i;
        }
    }
    return -1;
}

void viewBalance(const Account *account) {
    if (account == NULL) {
        printf("Account not found.\n");
        return;
    }

    printf("\nAccount Number: %d\n", account->accountNumber);
    printf("Customer Name: %s\n", account->name);
    printf("Current Balance: %.2f\n", account->balance);
}

int depositToAccount(Account *account, double amount) {
    if (account == NULL) {
        return 0;
    }

    if (!lockAccount(account)) {
        return 0;
    }

    if (amount <= 0) {
        printf("Deposit amount must be positive.\n");
        unlockAccount(account);
        return 0;
    }

    account->balance += amount;
    recordTransaction(account, "DEPOSIT", amount, "Cash deposit");
    printf("Deposit successful. New balance: %.2f\n", account->balance);
    unlockAccount(account);
    return 1;
}

int withdrawFromAccount(Account *account, double amount) {
    if (account == NULL) {
        return 0;
    }

    if (!lockAccount(account)) {
        return 0;
    }

    if (amount <= 0) {
        printf("Withdrawal amount must be positive.\n");
        unlockAccount(account);
        return 0;
    }

    if (amount > account->balance) {
        printf("Insufficient balance.\n");
        unlockAccount(account);
        return 0;
    }

    account->balance -= amount;
    recordTransaction(account, "WITHDRAW", amount, "Cash withdrawal");
    printf("Withdrawal successful. New balance: %.2f\n", account->balance);
    unlockAccount(account);
    return 1;
}

void transferFunds(Account accounts[], int count, int senderIndex) {
    int receiverAccountNumber;
    double amount;

    if (senderIndex < 0 || senderIndex >= count) {
        printf("Invalid sender account.\n");
        return;
    }

    printf("Enter recipient account number: ");
    scanf("%d", &receiverAccountNumber);
    printf("Enter amount to transfer: ");
    scanf("%lf", &amount);

    int receiverIndex = findAccount(accounts, count, receiverAccountNumber);
    if (receiverIndex == -1) {
        printf("Recipient account not found.\n");
        return;
    }

    if (receiverIndex == senderIndex) {
        printf("You cannot transfer to the same account.\n");
        return;
    }

    if (amount <= 0) {
        printf("Transfer amount must be positive.\n");
        return;
    }

    if (!lockAccount(&accounts[senderIndex]) || !lockAccount(&accounts[receiverIndex])) {
        printf("One of the accounts is busy. Please try again later.\n");
        return;
    }

    if (amount > accounts[senderIndex].balance) {
        printf("Insufficient balance for transfer.\n");
        unlockAccount(&accounts[senderIndex]);
        unlockAccount(&accounts[receiverIndex]);
        return;
    }

    accounts[senderIndex].balance -= amount;
    accounts[receiverIndex].balance += amount;
    recordTransaction(&accounts[senderIndex], "TRANSFER_OUT", amount, "Transfer to account");
    recordTransaction(&accounts[receiverIndex], "TRANSFER_IN", amount, "Received from account");
    printf("Transfer successful.\n");
    printf("Remaining balance: %.2f\n", accounts[senderIndex].balance);
    saveAccountsToFile(accounts, count, DATA_FILE);
    unlockAccount(&accounts[senderIndex]);
    unlockAccount(&accounts[receiverIndex]);
}

void applyForLoan(const Account *account, LoanApplication loans[], int *loanCount) {
    if (account == NULL || *loanCount >= MAX_LOAN_APPLICATIONS) {
        printf("Loan application limit reached.\n");
        return;
    }

    LoanApplication application = {0};
    application.id = 1;
    for (int i = 0; i < *loanCount; i++) {
        if (loans[i].id >= application.id) {
            application.id = loans[i].id + 1;
        }
    }
    application.accountNumber = account->accountNumber;
    snprintf(application.assignedEmployeeId, sizeof(application.assignedEmployeeId),
             "%s", DEFAULT_EMPLOYEE_ID);

    printf("Enter requested loan amount: ");
    if (scanf("%lf", &application.amount) != 1 || application.amount <= 0) {
        printf("Loan amount must be a positive number.\n");
        while (getchar() != '\n') { }
        return;
    }

    int character;
    while ((character = getchar()) != '\n' && character != EOF) { }
    printf("Enter loan purpose: ");
    if (fgets(application.purpose, sizeof(application.purpose), stdin) == NULL) {
        return;
    }
    application.purpose[strcspn(application.purpose, "\n")] = '\0';
    if (application.purpose[0] == '\0') {
        printf("Loan purpose cannot be empty.\n");
        return;
    }

    snprintf(application.status, sizeof(application.status), "PENDING");
    loans[*loanCount] = application;
    (*loanCount)++;
    if (!saveLoanApplications(loans, *loanCount, LOAN_FILE)) {
        printf("Warning: loan applications could not be saved.\n");
    }
    printf("Loan application #%d submitted for employee review.\n", application.id);
}

void customerDashboard(Account accounts[], int count, int loggedInIndex,
                       LoanApplication loans[], int *loanCount) {
    int option = 0;

    do {
        displayCustomerMenu();
        scanf("%d", &option);

        switch (option) {
            case 1:
                viewBalance(&accounts[loggedInIndex]);
                break;
            case 2: {
                double amount;
                printf("Enter deposit amount: ");
                scanf("%lf", &amount);
                if (depositToAccount(&accounts[loggedInIndex], amount)) {
                    saveAccountsToFile(accounts, count, DATA_FILE);
                }
                break;
            }
            case 3: {
                double amount;
                printf("Enter withdrawal amount: ");
                scanf("%lf", &amount);
                if (withdrawFromAccount(&accounts[loggedInIndex], amount)) {
                    saveAccountsToFile(accounts, count, DATA_FILE);
                }
                break;
            }
            case 4:
                transferFunds(accounts, count, loggedInIndex);
                break;
            case 5:
                displayTransactionHistory(&accounts[loggedInIndex]);
                break;
            case 6:
                applyForLoan(&accounts[loggedInIndex], loans, loanCount);
                break;
            case 7:
                printf("Customer logged out successfully.\n");
                break;
            default:
                printf("Invalid option. Please try again.\n");
                break;
        }
    } while (option != 7);
}