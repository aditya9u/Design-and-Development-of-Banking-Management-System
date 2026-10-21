#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "application.h"

static int loginUser(User users[], int userCount, int roleId,
                    const char *username, const char *password, int *matchedIndex) {
    for (int i = 0; i < userCount; i++) {
        if (users[i].role_id == roleId &&
            strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0 &&
            users[i].is_active == 1 &&
            users[i].is_logged_in == 0) {
            *matchedIndex = i;
            return 1;
        }
    }
    return 0;
}

int runBankApplication(void) {
    User users[MAX_USERS] = {0};
    Account accounts[MAX_ACCOUNTS] = {0};
    Transaction transactions[MAX_TRANSACTIONS] = {0};
    Feedback feedback[MAX_FEEDBACK] = {0};
    LoanApplication loans[MAX_LOAN_APPLICATIONS] = {0};
    int userCount = 0;
    int accountCount = 0;
    int transactionCount = 0;
    int feedbackCount = 0;
    int loanCount = 0;
    int selection = 0;

    if (!beginBankDataTransaction()) {
        return 1;
    }
    int loaded = loadUsersFromFile(users, &userCount, "bank_users.dat") &&
                 loadAccountsFromFile(accounts, &accountCount, "bank_accounts.dat") &&
                 loadTransactionsFromFile(transactions, &transactionCount, "bank_transactions.dat") &&
                 loadFeedbackFromFile(feedback, &feedbackCount, "bank_feedback.dat") &&
                 loadLoanApplications(loans, &loanCount, "bank_loans.dat");
    if (!loaded) {
        int anyDataFileExists =
            access("bank_users.dat", F_OK) == 0 ||
            access("bank_accounts.dat", F_OK) == 0 ||
            access("bank_transactions.dat", F_OK) == 0 ||
            access("bank_feedback.dat", F_OK) == 0 ||
            access("bank_loans.dat", F_OK) == 0;
        if (anyDataFileExists) {
            fprintf(stderr, "Existing bank data is incomplete or invalid; refusing to overwrite it.\n");
            endBankDataTransaction();
            return 1;
        }
        initializeSampleData(users, &userCount, accounts, &accountCount, transactions, &transactionCount, feedback, &feedbackCount);
        if (!saveUsersToFile(users, userCount, "bank_users.dat") ||
            !saveAccountsToFile(accounts, accountCount, "bank_accounts.dat") ||
            !saveTransactionsToFile(transactions, transactionCount, "bank_transactions.dat") ||
            !saveFeedbackToFile(feedback, feedbackCount, "bank_feedback.dat") ||
            !saveLoanApplications(loans, loanCount, "bank_loans.dat")) {
            fprintf(stderr, "Could not initialize bank data files.\n");
            endBankDataTransaction();
            return 1;
        }
    }
    endBankDataTransaction();

    if (!startBankTransactionWorker()) {
        fprintf(stderr, "Could not start transaction worker; application cannot continue.\n");
        return 1;
    }

    do {
        printf("\n===== Banking Management System =====\n");
        printf("1. Customer Login\n");
        printf("2. Manager Login\n");
        printf("3. Employee Login\n");
        printf("4. Admin Login\n");
        printf("5. Exit\n");
        printf("Choose an option: ");

        if (scanf("%d", &selection) != 1) {
            printf("Invalid input.\n");
            while (getchar() != '\n') { }
            selection = 0;
            continue;
        }

        char username[20];
        char password[20];
        int loginIndex = -1;

        if (selection >= 1 && selection <= 4) {
            printf("Username: ");
            scanf("%19s", username);
            printf("Password: ");
            scanf("%19s", password);

            int roleId = 0;
            if (selection == 1) roleId = ROLE_CUSTOMER;
            if (selection == 2) roleId = ROLE_MANAGER;
            if (selection == 3) roleId = ROLE_EMPLOYEE;
            if (selection == 4) roleId = ROLE_ADMIN;

            int loggedIn = 0;
            if (!beginBankDataTransaction()) {
                continue;
            }
            if (!loadUsersFromFile(users, &userCount, "bank_users.dat")) {
                fprintf(stderr, "Could not load user records; login cancelled.\n");
            } else if (loginUser(users, userCount, roleId, username, password, &loginIndex)) {
                users[loginIndex].is_logged_in = 1;
                if (saveUsersToFile(users, userCount, "bank_users.dat")) {
                    loggedIn = 1;
                } else {
                    users[loginIndex].is_logged_in = 0;
                    fprintf(stderr, "Could not save login state; login cancelled.\n");
                }
            }
            endBankDataTransaction();

            if (loggedIn) {
                printf("Login successful.\n");
                switch (selection) {
                    case 1:
                        customerDashboard(users, &userCount, accounts, &accountCount, transactions, &transactionCount, feedback, &feedbackCount, loans, &loanCount, users[loginIndex].user_id);
                        break;
                    case 2:
                        managerDashboard(users, &userCount, accounts, &accountCount, transactions, &transactionCount, feedback, &feedbackCount, loans, &loanCount, users[loginIndex].user_id);
                        break;
                    case 3:
                        employeeDashboard(users, &userCount, accounts, &accountCount, transactions, &transactionCount, feedback, &feedbackCount, loans, &loanCount, users[loginIndex].user_id);
                        break;
                    case 4:
                        adminDashboard(users, &userCount, accounts, &accountCount, transactions, &transactionCount, feedback, &feedbackCount, loans, &loanCount, users[loginIndex].user_id);
                        break;
                }
            } else {
                printf("Invalid username, password, or role.\n");
            }
        }
    } while (selection != 5);

    if (!stopBankTransactionWorker()) {
        fprintf(stderr, "Transaction worker did not shut down cleanly.\n");
        return 1;
    }
    printf("Goodbye!\n");
    return 0;
}

int main(void) {
    return runBankApplication();
}
