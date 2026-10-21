#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "customer.h"

static int getLoggedInAccount(const User users[], int userCount,
                             const Account accounts[], int accountCount,
                             int loggedInUserId) {
    (void)users;
    (void)userCount;

    for (int i = 0; i < accountCount; i++) {
        if (accounts[i].user_id == loggedInUserId) {
            return i;
        }
    }
    return -1;
}

void displayCustomerMenu(void) {
    printf("\n===== Customer Dashboard =====\n");
    printf("1. View balance\n");
    printf("2. Deposit\n");
    printf("3. Withdraw\n");
    printf("4. Apply loan\n");
    printf("5. Check transactions\n");
    printf("6. Add feedback\n");
    printf("7. Transfer funds\n");
    printf("8. Logout\n");
    printf("9. Exit\n");
    printf("Select an option: ");
}

void customerDashboard(User users[], int *userCount,
                       Account accounts[], int *accountCount,
                       Transaction transactions[], int *transactionCount,
                       Feedback feedback[], int *feedbackCount,
                       LoanApplication loans[], int *loanCount,
                       int loggedInUserId) {
    int option = 0;

    do {
        displayCustomerMenu();
        if (scanf("%d", &option) != 1) {
            printf("Invalid input.\n");
            while (getchar() != '\n') { }
            option = 0;
            continue;
        }

        if (!loadAccountsFromFile(accounts, accountCount, "bank_accounts.dat")) {
            fprintf(stderr, "Could not refresh account data; customer session stopped.\n");
            return;
        }
        int accountIndex = getLoggedInAccount(users, *userCount, accounts, *accountCount, loggedInUserId);

        switch (option) {
            case 1:
                if (accountIndex == -1) {
                    printf("No account found for this customer.\n");
                } else {
                    printf("\nAccount Number: %d\nBalance: %.2f\n", accounts[accountIndex].account_id, accounts[accountIndex].balance);
                }
                break;

            case 2: {
                double amount;
                if (accountIndex == -1) {
                    printf("No account found for this customer.\n");
                    break;
                }
                printf("Enter deposit amount: ");
                if (scanf("%lf", &amount) != 1 || !isfinite(amount) || amount <= 0) {
                    printf("Deposit amount must be a finite positive number.\n");
                    break;
                }
                double balanceAfter;
                if (submitBankTransaction(BANK_TXN_DEPOSIT,
                                          accounts[accountIndex].account_id,
                                          0, amount, &balanceAfter)) {
                    accounts[accountIndex].balance = balanceAfter;
                    printf("Deposit successful. New balance: %.2f\n", balanceAfter);
                }
                break;
            }

            case 3: {
                double amount;
                if (accountIndex == -1) {
                    printf("No account found for this customer.\n");
                    break;
                }
                printf("Enter withdrawal amount: ");
                if (scanf("%lf", &amount) != 1 || !isfinite(amount) || amount <= 0) {
                    printf("Withdrawal amount must be positive.\n");
                    break;
                }
                double balanceAfter;
                if (submitBankTransaction(BANK_TXN_WITHDRAW,
                                          accounts[accountIndex].account_id,
                                          0, amount, &balanceAfter)) {
                    accounts[accountIndex].balance = balanceAfter;
                    printf("Withdrawal successful. New balance: %.2f\n", balanceAfter);
                }
                break;
            }

            case 4: {
                double amount;
                if (accountIndex == -1) {
                    printf("No account found for this customer.\n");
                    break;
                }
                printf("Enter loan amount: ");
                if (scanf("%lf", &amount) != 1 || !isfinite(amount) || amount <= 0) {
                    printf("Loan amount must be positive.\n");
                    break;
                }
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadAccountsFromFile(accounts, accountCount, "bank_accounts.dat") ||
                    !loadLoanApplications(loans, loanCount, "bank_loans.dat")) {
                    fprintf(stderr, "Could not reload loan data; application cancelled.\n");
                    endBankDataTransaction();
                    break;
                }
                accountIndex = findAccountForUser(accounts, *accountCount, loggedInUserId);
                if (accountIndex == -1 || !accounts[accountIndex].status) {
                    printf("Account is unavailable or inactive.\n");
                    endBankDataTransaction();
                    break;
                }
                if (!addLoanApplication(loans, loanCount, accounts[accountIndex].account_id, amount, "Customer requested loan", 3)) {
                    printf("Loan application could not be created.\n");
                } else {
                    if (saveLoanApplications(loans, *loanCount, "bank_loans.dat")) {
                        printf("Loan request submitted.\n");
                    } else {
                        fprintf(stderr, "Could not save the loan application.\n");
                    }
                }
                endBankDataTransaction();
                break;
            }

            case 5:
                if (accountIndex == -1) {
                    printf("No account found for this customer.\n");
                } else if (!loadTransactionsFromFile(transactions, transactionCount, "bank_transactions.dat")) {
                    fprintf(stderr, "Could not refresh transaction history.\n");
                } else {
                    printTransactions(transactions, *transactionCount, accounts[accountIndex].account_id);
                }
                break;

            case 6: {
                char message[200];
                printf("Enter feedback: ");
                while (getchar() != '\n') { }
                fgets(message, sizeof(message), stdin);
                message[strcspn(message, "\n")] = '\0';
                if (message[0] == '\0') {
                    printf("Feedback cannot be empty.\n");
                    break;
                }
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadFeedbackFromFile(feedback, feedbackCount, "bank_feedback.dat")) {
                    fprintf(stderr, "Could not reload feedback data; submission cancelled.\n");
                    endBankDataTransaction();
                    break;
                }
                if (*feedbackCount >= MAX_FEEDBACK) {
                    printf("Feedback limit reached.\n");
                    endBankDataTransaction();
                    break;
                }
                feedback[*feedbackCount].feedback_id = *feedbackCount + 1;
                feedback[*feedbackCount].user_id = loggedInUserId;
                snprintf(feedback[*feedbackCount].message, sizeof(feedback[*feedbackCount].message), "%s", message);
                time_t now = time(NULL);
                struct tm *tmInfo = localtime(&now);
                strftime(feedback[*feedbackCount].timestamp, sizeof(feedback[*feedbackCount].timestamp), "%Y-%m-%d %H:%M:%S", tmInfo);
                (*feedbackCount)++;
                if (saveFeedbackToFile(feedback, *feedbackCount, "bank_feedback.dat")) {
                    printf("Feedback saved successfully.\n");
                } else {
                    fprintf(stderr, "Could not save feedback.\n");
                }
                endBankDataTransaction();
                break;
            }

            case 7: {
                int targetAccountId;
                double amount;
                if (accountIndex == -1) {
                    printf("No account found for this customer.\n");
                    break;
                }
                printf("Enter target account ID: ");
                scanf("%d", &targetAccountId);
                printf("Enter transfer amount: ");
                if (scanf("%lf", &amount) != 1 || !isfinite(amount)) {
                    printf("Transfer amount must be a finite number.\n");
                    break;
                }

                int receiverIndex = findAccountById(accounts, *accountCount, targetAccountId);
                if (receiverIndex == -1) {
                    printf("Recipient account not found.\n");
                    break;
                }
                if (accounts[receiverIndex].user_id == loggedInUserId) {
                    printf("You cannot transfer to your own account.\n");
                    break;
                }

                double balanceAfter;
                if (submitBankTransaction(BANK_TXN_TRANSFER,
                                          accounts[accountIndex].account_id,
                                          targetAccountId, amount, &balanceAfter)) {
                    if (loadAccountsFromFile(accounts, accountCount, "bank_accounts.dat")) {
                        accountIndex = findAccountForUser(accounts, *accountCount, loggedInUserId);
                    }
                    printf("Transfer successful. Remaining balance: %.2f\n", balanceAfter);
                }
                break;
            }

            case 8:
                if (setUserSessionState(users, userCount, loggedInUserId, 0)) {
                    printf("Customer logged out successfully.\n");
                } else {
                    fprintf(stderr, "Could not persist customer logout state.\n");
                }
                return;

            case 9:
                if (!setUserSessionState(users, userCount, loggedInUserId, 0)) {
                    fprintf(stderr, "Could not persist customer logout state.\n");
                }
                printf("Exiting customer dashboard.\n");
                return;

            default:
                printf("Invalid option. Please try again.\n");
                break;
        }
    } while (option != 9 && option != 8);
}
