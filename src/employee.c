#include <stdio.h>
#include <math.h>
#include <string.h>
#include "employee.h"
#include "user_password.h"

static int chooseCustomer(User users[], int userCount) {
    int selectedUserId = -1;
    int found = 0;

    for (int i = 0; i < userCount; i++) {
        if (users[i].role_id == ROLE_CUSTOMER) {
            printf("User ID: %d Username: %s\n", users[i].user_id, users[i].username);
            found = 1;
        }
    }

    if (!found) {
        printf("No customers found.\n");
        return -1;
    }

    printf("Enter customer user ID: ");
    scanf("%d", &selectedUserId);
    return selectedUserId;
}

void displayEmployeeMenu(void) {
    printf("\n===== Employee Dashboard =====\n");
    printf("1. New customer\n");
    printf("2. Modify customer details\n");
    printf("3. Process loan application\n");
    printf("4. View customer transactions\n");
    printf("5. View assigned loan applications\n");
    printf("6. Change my password\n");
    printf("7. Logout\n");
    printf("8. Exit\n");
    printf("Select an option: ");
}

void employeeDashboard(User users[], int *userCount,
                      Account accounts[], int *accountCount,
                      Transaction transactions[], int *transactionCount,
                      Feedback feedback[], int *feedbackCount,
                      LoanApplication loans[], int *loanCount,
                      int loggedInUserId, uint64_t sessionToken) {
    int option = 0;

    do {
        displayEmployeeMenu();
        int waitingStatus = waitForUserSessionInput(loggedInUserId, sessionToken);
        if (waitingStatus != 1) {
            if (waitingStatus < 0) {
                fprintf(stderr, "Could not verify employee session; session stopped.\n");
            } else {
                printf("You were logged out because this account signed in elsewhere.\n");
            }
            return;
        }
        if (scanf("%d", &option) != 1) {
            if (feof(stdin)) {
                return;
            }
            printf("Invalid input.\n");
            int character;
            while ((character = getchar()) != '\n' && character != EOF) { }
            option = 0;
            continue;
        }
        int sessionStatus = isUserSessionCurrent(loggedInUserId, sessionToken);
        if (sessionStatus != 1) {
            if (sessionStatus < 0) {
                fprintf(stderr, "Could not verify employee session; session stopped.\n");
            } else {
                printf("You were logged out because this account signed in elsewhere.\n");
            }
            return;
        }

        switch (option) {
            case 1: {
                User newUser = {0};
                Account newAccount = {0};
                printf("Enter customer username: ");
                scanf("%19s", newUser.username);
                printf("Enter customer password: ");
                scanf("%19s", newUser.password);
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadUsersFromFile(users, userCount, "bank_users.dat") ||
                    !loadAccountsFromFile(accounts, accountCount, "bank_accounts.dat")) {
                    fprintf(stderr, "Could not reload user/account data; customer creation cancelled.\n");
                    endBankDataTransaction();
                    break;
                }
                if (findUserByUsername(users, *userCount, newUser.username) != -1) {
                    printf("Username already exists.\n");
                    endBankDataTransaction();
                    break;
                }
                if (*userCount >= MAX_USERS || *accountCount >= MAX_ACCOUNTS) {
                    printf("User or account capacity reached.\n");
                    endBankDataTransaction();
                    break;
                }
                int maxUserId = 0;
                for (int i = 0; i < *userCount; i++) {
                    if (users[i].user_id > maxUserId) maxUserId = users[i].user_id;
                }
                newUser.user_id = maxUserId + 1;
                newUser.role_id = ROLE_CUSTOMER;
                newUser.is_active = 1;
                newUser.is_logged_in = 0;

                int maxAccountId = 1000;
                for (int i = 0; i < *accountCount; i++) {
                    if (accounts[i].account_id > maxAccountId) maxAccountId = accounts[i].account_id;
                }
                newAccount.account_id = maxAccountId + 1;
                newAccount.user_id = newUser.user_id;
                newAccount.balance = 0.0;
                newAccount.status = 1;

                users[*userCount] = newUser;
                accounts[*accountCount] = newAccount;
                int newUserCount = *userCount + 1;
                int newAccountCount = *accountCount + 1;
                if (saveUsersToFile(users, newUserCount, "bank_users.dat") &&
                    saveAccountsToFile(accounts, newAccountCount, "bank_accounts.dat")) {
                    *userCount = newUserCount;
                    *accountCount = newAccountCount;
                    printf("New customer created successfully.\n");
                } else {
                    fprintf(stderr, "Could not save new customer records.\n");
                    loadUsersFromFile(users, userCount, "bank_users.dat");
                    loadAccountsFromFile(accounts, accountCount, "bank_accounts.dat");
                }
                endBankDataTransaction();
                break;
            }

            case 2: {
                if (!loadUsersFromFile(users, userCount, "bank_users.dat")) {
                    fprintf(stderr, "Could not refresh customer records.\n");
                    break;
                }
                int customerUserId = chooseCustomer(users, *userCount);
                if (customerUserId == -1) {
                    break;
                }
                char newUsername[20];
                char newPassword[20];
                printf("Enter new username: ");
                scanf("%19s", newUsername);
                printf("Enter new password: ");
                scanf("%19s", newPassword);
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadUsersFromFile(users, userCount, "bank_users.dat")) {
                    fprintf(stderr, "Could not reload user records.\n");
                    endBankDataTransaction();
                    break;
                }
                int userIndex = findUserById(users, *userCount, customerUserId);
                if (userIndex == -1) {
                    printf("Customer no longer exists.\n");
                    endBankDataTransaction();
                    break;
                }
                int duplicateIndex = findUserByUsername(users, *userCount, newUsername);
                if (duplicateIndex != -1 && duplicateIndex != userIndex) {
                    printf("Username already exists.\n");
                    endBankDataTransaction();
                    break;
                }
                snprintf(users[userIndex].username, sizeof(users[userIndex].username), "%s", newUsername);
                snprintf(users[userIndex].password, sizeof(users[userIndex].password), "%s", newPassword);
                if (saveUsersToFile(users, *userCount, "bank_users.dat")) {
                    printf("Customer details updated.\n");
                } else {
                    fprintf(stderr, "Could not save customer details.\n");
                }
                endBankDataTransaction();
                break;
            }

            case 3: {
                if (!loadLoanApplications(loans, loanCount, "bank_loans.dat")) {
                    fprintf(stderr, "Could not load loan applications.\n");
                    break;
                }
                if (*loanCount == 0) {
                    printf("No loan applications available.\n");
                    break;
                }
                printLoanApplications(loans, *loanCount, 0);

                int selectedLoanId;
                printf("Enter loan ID to process: ");
                scanf("%d", &selectedLoanId);

                int loanIndex = -1;
                for (int i = 0; i < *loanCount; i++) {
                    if (loans[i].loan_id == selectedLoanId && strcmp(loans[i].status, "PENDING") == 0) {
                        loanIndex = i;
                        break;
                    }
                }
                if (loanIndex == -1) {
                    printf("Pending loan application not found.\n");
                    break;
                }

                char decision;
                printf("Approve this loan? (y/n): ");
                scanf(" %c", &decision);
                if (decision != 'y' && decision != 'Y' && decision != 'n' && decision != 'N') {
                    printf("Invalid decision; application remains pending.\n");
                    break;
                }
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadLoanApplications(loans, loanCount, "bank_loans.dat") ||
                    !loadAccountsFromFile(accounts, accountCount, "bank_accounts.dat") ||
                    !loadTransactionsFromFile(transactions, transactionCount, "bank_transactions.dat")) {
                    fprintf(stderr, "Could not reload loan records; processing cancelled.\n");
                    endBankDataTransaction();
                    break;
                }
                loanIndex = -1;
                for (int i = 0; i < *loanCount; i++) {
                    if (loans[i].loan_id == selectedLoanId &&
                        strcmp(loans[i].status, "PENDING") == 0 &&
                        (loans[i].assigned_employee_id == 0 ||
                         loans[i].assigned_employee_id == loggedInUserId)) {
                        loanIndex = i;
                        break;
                    }
                }
                if (loanIndex == -1) {
                    printf("Loan is no longer pending or is assigned to another employee.\n");
                    endBankDataTransaction();
                    break;
                }

                int accountIndex = findAccountById(accounts, *accountCount, loans[loanIndex].account_id);
                if ((decision == 'y' || decision == 'Y') && accountIndex == -1) {
                    printf("The customer account no longer exists; application cannot be approved.\n");
                    endBankDataTransaction();
                    break;
                }

                if (decision == 'y' || decision == 'Y') {
                    if (*transactionCount >= MAX_TRANSACTIONS) {
                        printf("Transaction history is full; loan cannot be approved.\n");
                        endBankDataTransaction();
                        break;
                    }
                    if (!accounts[accountIndex].status) {
                        printf("Account is inactive; loan cannot be approved.\n");
                        endBankDataTransaction();
                        break;
                    }
                    if (!isfinite(accounts[accountIndex].balance + loans[loanIndex].amount)) {
                        printf("Loan would exceed the supported account balance.\n");
                        endBankDataTransaction();
                        break;
                    }
                    snprintf(loans[loanIndex].status, sizeof(loans[loanIndex].status), "APPROVED");
                    accounts[accountIndex].balance += loans[loanIndex].amount;
                    addTransaction(transactions, transactionCount, loans[loanIndex].account_id, "LOAN_APPROVED", loans[loanIndex].amount, "Loan approved by employee");
                    if (saveAccountsToFile(accounts, *accountCount, "bank_accounts.dat") &&
                        saveTransactionsToFile(transactions, *transactionCount, "bank_transactions.dat") &&
                        saveLoanApplications(loans, *loanCount, "bank_loans.dat")) {
                        printf("Loan approved and credited to account %d.\n", accounts[accountIndex].account_id);
                    } else {
                        fprintf(stderr, "Could not persist the approved loan across all data files.\n");
                    }
                } else {
                    snprintf(loans[loanIndex].status, sizeof(loans[loanIndex].status), "REJECTED");
                    if (saveLoanApplications(loans, *loanCount, "bank_loans.dat")) {
                        printf("Loan application rejected.\n");
                    } else {
                        fprintf(stderr, "Could not save loan rejection.\n");
                    }
                }
                endBankDataTransaction();
                break;
            }

            case 4: {
                int accountId;
                printf("Enter account ID: ");
                if (scanf("%d", &accountId) != 1) {
                    printf("Invalid account ID.\n");
                    break;
                }
                if (!loadTransactionsFromFile(transactions, transactionCount,
                                              "bank_transactions.dat")) {
                    fprintf(stderr, "Could not refresh transaction history.\n");
                    break;
                }
                printTransactions(transactions, *transactionCount, accountId);
                break;
            }

            case 5: {
                if (!loadLoanApplications(loans, loanCount, "bank_loans.dat")) {
                    fprintf(stderr, "Could not load loan applications.\n");
                } else {
                    printLoanApplications(loans, *loanCount, loggedInUserId);
                }
                break;
            }

            case 6:
                promptUserPasswordChange(loggedInUserId, sessionToken);
                break;

            case 7:
                if (releaseUserSession(loggedInUserId, sessionToken)) {
                    printf("Employee logged out successfully.\n");
                } else {
                    fprintf(stderr, "Could not persist employee logout state.\n");
                }
                return;

            case 8:
                if (!releaseUserSession(loggedInUserId, sessionToken)) {
                    fprintf(stderr, "Could not persist employee logout state.\n");
                }
                printf("Exiting employee dashboard.\n");
                return;

            default:
                printf("Invalid option.\n");
                break;
        }
    } while (option != 8 && option != 7);
}
