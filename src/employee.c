#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "employee.h"

#define DATA_FILE "bank_accounts.dat"
#define LOAN_FILE "loan_applications.dat"

void displayEmployeeMenu(void) {
    printf("\n===== Employee Dashboard =====\n");
    printf("1. List Customer Accounts\n");
    printf("2. Add New Customer\n");
    printf("3. Modify Customer Details\n");
    printf("4. Close Customer Account\n");
    printf("5. View Assigned Loan Applications\n");
    printf("6. Approve/Reject Assigned Loan Application\n");
    printf("7. Logout\n");
    printf("Select an option: ");
}

int loginEmployee(const char *employeeId, const char *password) {
    return employeeId != NULL && password != NULL &&
           strcmp(employeeId, DEFAULT_EMPLOYEE_ID) == 0 && strcmp(password, "staff123") == 0;
}

void addAccount(Account accounts[], int *count) {
    if (*count >= MAX_ACCOUNTS) {
        printf("Account limit reached. Cannot add more accounts.\n");
        return;
    }

    Account newAccount = {0};
    newAccount.accountNumber = 1001;
    for (int i = 0; i < *count; i++) {
        if (accounts[i].accountNumber >= newAccount.accountNumber) {
            if (accounts[i].accountNumber == INT_MAX) {
                printf("Cannot generate another account number.\n");
                return;
            }
            newAccount.accountNumber = accounts[i].accountNumber + 1;
        }
    }

    printf("Generated account number: %d\n", newAccount.accountNumber);
    int character;
    while ((character = getchar()) != '\n' && character != EOF) { }

    printf("Enter account holder name: ");
    fgets(newAccount.name, sizeof(newAccount.name), stdin);
    newAccount.name[strcspn(newAccount.name, "\n")] = '\0';

    printf("Enter password: ");
    scanf("%19s", newAccount.password);

    printf("Enter initial balance: ");
    if (scanf("%lf", &newAccount.balance) != 1 || newAccount.balance < 0) {
        printf("Initial balance cannot be negative.\n");
        while (getchar() != '\n') { }
        return;
    }
    newAccount.isLocked = 0;
    newAccount.transactionCount = 0;

    recordTransaction(&newAccount, "OPEN", newAccount.balance, "Account created");

    accounts[*count] = newAccount;
    (*count)++;
    saveAccountsToFile(accounts, *count, DATA_FILE);
    printf("Account created successfully.\n");
}

void modifyCustomer(Account accounts[], int count) {
    int accountNumber;
    printf("Enter account number to modify: ");
    if (scanf("%d", &accountNumber) != 1) {
        printf("Invalid account number.\n");
        while (getchar() != '\n') { }
        return;
    }

    int index = findAccount(accounts, count, accountNumber);
    if (index == -1) {
        printf("Customer account not found.\n");
        return;
    }

    int option;
    printf("1. Change customer name\n2. Change customer password\nSelect a field: ");
    if (scanf("%d", &option) != 1) {
        printf("Invalid option.\n");
        while (getchar() != '\n') { }
        return;
    }

    if (option == 1) {
        int character;
        while ((character = getchar()) != '\n' && character != EOF) { }
        printf("Enter new customer name: ");
        if (fgets(accounts[index].name, sizeof(accounts[index].name), stdin) == NULL) {
            return;
        }
        accounts[index].name[strcspn(accounts[index].name, "\n")] = '\0';
        if (accounts[index].name[0] == '\0') {
            printf("Customer name cannot be empty.\n");
            return;
        }
    } else if (option == 2) {
        printf("Enter new customer password: ");
        if (scanf("%19s", accounts[index].password) != 1) {
            printf("Password was not changed.\n");
            return;
        }
    } else {
        printf("Invalid option.\n");
        return;
    }

    saveAccountsToFile(accounts, count, DATA_FILE);
    printf("Customer details updated successfully.\n");
}

void closeCustomer(Account accounts[], int *count,
                   const LoanApplication loans[], int loanCount) {
    int accountNumber;
    printf("Enter account number to close: ");
    if (scanf("%d", &accountNumber) != 1) {
        printf("Invalid account number.\n");
        while (getchar() != '\n') { }
        return;
    }

    int index = findAccount(accounts, *count, accountNumber);
    if (index == -1) {
        printf("Customer account not found.\n");
        return;
    }
    if (accounts[index].balance > 0.005) {
        printf("Account cannot be closed until its balance has been withdrawn.\n");
        return;
    }
    for (int i = 0; i < loanCount; i++) {
        if (loans[i].accountNumber == accountNumber && strcmp(loans[i].status, "PENDING") == 0) {
            printf("Account cannot be closed while a loan application is pending.\n");
            return;
        }
    }

    char confirmation;
    printf("Close account %d belonging to %s? (y/n): ", accountNumber, accounts[index].name);
    scanf(" %c", &confirmation);
    if (confirmation != 'y' && confirmation != 'Y') {
        printf("Account closure cancelled.\n");
        return;
    }

    for (int i = index + 1; i < *count; i++) {
        accounts[i - 1] = accounts[i];
    }
    (*count)--;
    saveAccountsToFile(accounts, *count, DATA_FILE);
    printf("Customer account closed successfully.\n");
}

void displayLoanApplications(const LoanApplication loans[], int loanCount,
                             const char *employeeId) {
    printf("\nLoan Applications\n");
    printf("ID   Account   Amount       Status    Purpose\n");
    int assignedCount = 0;
    for (int i = 0; i < loanCount; i++) {
        if (strcmp(loans[i].assignedEmployeeId, employeeId) != 0) {
            continue;
        }
        printf("%-4d %-9d %-12.2f %-9s %s\n", loans[i].id, loans[i].accountNumber,
               loans[i].amount, loans[i].status, loans[i].purpose);
        assignedCount++;
    }
    if (assignedCount == 0) {
        printf("No loan applications are assigned to employee %s.\n", employeeId);
    }
}

void processLoanApplication(Account accounts[], int count,
                            LoanApplication loans[], int loanCount,
                            const char *employeeId) {
    displayLoanApplications(loans, loanCount, employeeId);

    int applicationId;
    printf("Enter pending application ID to process: ");
    if (scanf("%d", &applicationId) != 1) {
        printf("Invalid application ID.\n");
        while (getchar() != '\n') { }
        return;
    }

    int index = -1;
    for (int i = 0; i < loanCount; i++) {
        if (loans[i].id == applicationId && strcmp(loans[i].status, "PENDING") == 0 &&
            strcmp(loans[i].assignedEmployeeId, employeeId) == 0) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        printf("Pending loan application not found.\n");
        return;
    }

    char decision;
    printf("Approve this application? (y/n): ");
    scanf(" %c", &decision);
    int accountIndex = findAccount(accounts, count, loans[index].accountNumber);
    if ((decision == 'y' || decision == 'Y') && accountIndex == -1) {
        printf("The customer account no longer exists; application cannot be approved.\n");
        return;
    }

    if (decision == 'y' || decision == 'Y') {
        snprintf(loans[index].status, sizeof(loans[index].status), "APPROVED");
        accounts[accountIndex].balance += loans[index].amount;
        recordTransaction(&accounts[accountIndex], "LOAN", loans[index].amount,
                          "Loan application approved");
        saveAccountsToFile(accounts, count, DATA_FILE);
        printf("Loan approved and credited to account %d.\n", accounts[accountIndex].accountNumber);
    } else if (decision == 'n' || decision == 'N') {
        snprintf(loans[index].status, sizeof(loans[index].status), "REJECTED");
        printf("Loan application rejected.\n");
    } else {
        printf("Invalid decision; application remains pending.\n");
        return;
    }
    if (!saveLoanApplications(loans, loanCount, LOAN_FILE)) {
        printf("Warning: loan applications could not be saved.\n");
    }
}

void employeeDashboard(Account accounts[], int *count,
                       LoanApplication loans[], int *loanCount,
                       const char *employeeId) {
    int option = 0;
    do {
        displayEmployeeMenu();
        if (scanf("%d", &option) != 1) {
            printf("Invalid option.\n");
            while (getchar() != '\n') { }
            option = 0;
            continue;
        }

        switch (option) {
            case 1:
                displayAccounts(accounts, *count);
                break;
            case 2:
                addAccount(accounts, count);
                break;
            case 3:
                modifyCustomer(accounts, *count);
                break;
            case 4:
                closeCustomer(accounts, count, loans, *loanCount);
                break;
            case 5:
                displayLoanApplications(loans, *loanCount, employeeId);
                break;
            case 6:
                processLoanApplication(accounts, *count, loans, *loanCount, employeeId);
                break;
            case 7:
                printf("Employee logged out successfully.\n");
                break;
            default:
                printf("Invalid option. Please try again.\n");
                break;
        }
    } while (option != 7);
}