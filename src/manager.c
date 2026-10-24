#include <stdio.h>
#include <string.h>
#include "manager.h"
#include "user_password.h"

void displayManagerMenu(void) {
    printf("\n===== Manager Dashboard =====\n");
    printf("1. Activate/Deactivate accounts\n");
    printf("2. Assign loans\n");
    printf("3. Review customer feedback\n");
    printf("4. Change my password\n");
    printf("5. Logout\n");
    printf("6. Exit\n");
    printf("Select an option: ");
}

void managerDashboard(User users[], int *userCount,
                     Account accounts[], int *accountCount,
                     Transaction transactions[], int *transactionCount,
                     Feedback feedback[], int *feedbackCount,
                     LoanApplication loans[], int *loanCount,
                     int loggedInUserId, uint64_t sessionToken) {
    int option = 0;

    do {
        displayManagerMenu();
        int waitingStatus = waitForUserSessionInput(loggedInUserId, sessionToken);
        if (waitingStatus != 1) {
            if (waitingStatus < 0) {
                fprintf(stderr, "Could not verify manager session; session stopped.\n");
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
                fprintf(stderr, "Could not verify manager session; session stopped.\n");
            } else {
                printf("You were logged out because this account signed in elsewhere.\n");
            }
            return;
        }

        switch (option) {
            case 1: {
                int accountId;
                printf("Enter account ID: ");
                scanf("%d", &accountId);
                int newStatus;
                printf("Set account active (1) or inactive (0): ");
                scanf("%d", &newStatus);
                if (newStatus != 0 && newStatus != 1) {
                    printf("Status must be 0 or 1.\n");
                    break;
                }
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadAccountsFromFile(accounts, accountCount, "bank_accounts.dat")) {
                    fprintf(stderr, "Could not reload account data.\n");
                    endBankDataTransaction();
                    break;
                }
                int index = findAccountById(accounts, *accountCount, accountId);
                if (index == -1) {
                    printf("Account not found.\n");
                    endBankDataTransaction();
                    break;
                }
                accounts[index].status = newStatus;
                if (saveAccountsToFile(accounts, *accountCount, "bank_accounts.dat")) {
                    printf("Account status updated.\n");
                } else {
                    fprintf(stderr, "Could not save account status.\n");
                }
                endBankDataTransaction();
                break;
            }

            case 2: {
                if (!loadLoanApplications(loans, loanCount, "bank_loans.dat") ||
                    !loadUsersFromFile(users, userCount, "bank_users.dat")) {
                    fprintf(stderr, "Could not load loan assignments or employee records.\n");
                    break;
                }
                if (*loanCount == 0) {
                    printf("No loan applications available to assign.\n");
                    break;
                }
                printLoanApplications(loans, *loanCount, 0);
                int loanId;
                printf("Enter loan ID to assign: ");
                scanf("%d", &loanId);
                int loanIndex = -1;
                for (int i = 0; i < *loanCount; i++) {
                    if (loans[i].loan_id == loanId) {
                        loanIndex = i;
                        break;
                    }
                }
                if (loanIndex == -1) {
                    printf("Loan not found.\n");
                    break;
                }
                printf("Available employees:\n");
                int found = 0;
                for (int i = 0; i < *userCount; i++) {
                    if (users[i].role_id == ROLE_EMPLOYEE) {
                        printf("User ID: %d Username: %s\n", users[i].user_id, users[i].username);
                        found = 1;
                    }
                }
                if (!found) {
                    printf("No employees available to assign.\n");
                    break;
                }
                int empId;
                printf("Enter employee user ID to assign: ");
                scanf("%d", &empId);
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadLoanApplications(loans, loanCount, "bank_loans.dat") ||
                    !loadUsersFromFile(users, userCount, "bank_users.dat")) {
                    fprintf(stderr, "Could not reload loan assignments or employee records.\n");
                    endBankDataTransaction();
                    break;
                }
                loanIndex = -1;
                for (int i = 0; i < *loanCount; i++) {
                    if (loans[i].loan_id == loanId && strcmp(loans[i].status, "PENDING") == 0) {
                        loanIndex = i;
                        break;
                    }
                }
                if (loanIndex == -1) {
                    printf("Loan is no longer pending.\n");
                    endBankDataTransaction();
                    break;
                }
                int empIndex = findUserById(users, *userCount, empId);
                if (empIndex == -1 || users[empIndex].role_id != ROLE_EMPLOYEE) {
                    printf("Invalid employee selected.\n");
                    endBankDataTransaction();
                    break;
                }
                loans[loanIndex].assigned_employee_id = empId;
                if (saveLoanApplications(loans, *loanCount, "bank_loans.dat")) {
                    printf("Loan assigned to employee %d.\n", empId);
                } else {
                    fprintf(stderr, "Could not save loan assignment.\n");
                }
                endBankDataTransaction();
                break;
            }

            case 3:
                if (!loadFeedbackFromFile(feedback, feedbackCount, "bank_feedback.dat")) {
                    fprintf(stderr, "Could not refresh customer feedback.\n");
                    break;
                }
                if (!loadUsersFromFile(users, userCount, "bank_users.dat")) {
                    fprintf(stderr, "Could not refresh user records for feedback display.\n");
                    break;
                }
                for (int i = 0; i < *feedbackCount; i++) {
                    int userIndex = findUserById(users, *userCount, feedback[i].user_id);
                    printf("Feedback #%d from %s: %s\n",
                           feedback[i].feedback_id,
                           userIndex != -1 ? users[userIndex].username : "unknown",
                           feedback[i].message);
                    printf("Date: %s\n", feedback[i].timestamp);
                }
                if (*feedbackCount == 0) {
                    printf("No customer feedback available.\n");
                }
                break;

            case 4:
                promptUserPasswordChange(loggedInUserId, sessionToken);
                break;

            case 5:
                if (releaseUserSession(loggedInUserId, sessionToken)) {
                    printf("Manager logged out successfully.\n");
                } else {
                    fprintf(stderr, "Could not persist manager logout state.\n");
                }
                return;

            case 6:
                if (!releaseUserSession(loggedInUserId, sessionToken)) {
                    fprintf(stderr, "Could not persist manager logout state.\n");
                }
                printf("Exiting manager dashboard.\n");
                return;

            default:
                printf("Invalid option.\n");
                break;
        }
    } while (option != 6 && option != 5);
}
