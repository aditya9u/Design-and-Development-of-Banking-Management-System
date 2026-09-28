#include <stdio.h>
#include "application.h"

int runBankApplication(void) {
    Account accounts[MAX_ACCOUNTS];
    LoanApplication loans[MAX_LOAN_APPLICATIONS];
    int activeCustomerSessions[MAX_ACCOUNTS] = {0};
    int count = 0;
    int loanCount = 0;
    int choice;
    int loggedInIndex = -1;

    if (!loadAccountsFromFile(accounts, &count, "bank_accounts.dat")) {
        loadSampleData(accounts, &count);
        saveAccountsToFile(accounts, count, "bank_accounts.dat");
    }
    loadLoanApplications(loans, &loanCount, "loan_applications.dat");

    do {
        displayMainMenu();
        scanf("%d", &choice);

        switch (choice) {
            case 1: {
                int accountNumber;
                char password[20];

                printf("Enter your account number: ");
                scanf("%d", &accountNumber);
                printf("Enter your password: ");
                scanf("%19s", password);

                loggedInIndex = loginCustomer(accounts, count, accountNumber, password);

                if (loggedInIndex == -1 || activeCustomerSessions[loggedInIndex]) {
                    printf("Invalid account number or password. Please try again.\n");
                } else {
                    activeCustomerSessions[loggedInIndex] = 1;
                    printf("Login successful. Welcome, %s!\n", accounts[loggedInIndex].name);
                    customerDashboard(accounts, count, loggedInIndex, loans, &loanCount);
                    activeCustomerSessions[loggedInIndex] = 0;
                }
                break;
            }
            case 2:
            {
                char employeeId[32];
                char password[32];
                printf("Enter employee ID: ");
                scanf("%31s", employeeId);
                printf("Enter employee password: ");
                scanf("%31s", password);
                if (loginEmployee(employeeId, password)) {
                    printf("Employee login successful.\n");
                    employeeDashboard(accounts, &count, loans, &loanCount, employeeId);
                } else {
                    printf("Invalid employee ID or password.\n");
                }
                break;
            }
            case 3:
                printf("Exiting the banking system. Goodbye!\n");
                break;
            default:
                printf("Invalid option. Please try again.\n");
                break;
        }

    } while (choice != 3);

    saveAccountsToFile(accounts, count, "bank_accounts.dat");
    saveLoanApplications(loans, loanCount, "loan_applications.dat");
    return 0;
}

int main(void) {
    return runBankApplication();
}
