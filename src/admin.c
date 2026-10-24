#include <stdio.h>
#include <string.h>
#include "admin.h"
#include "user_password.h"

void displayAdminMenu(void) {
    printf("\n===== Admin Dashboard =====\n");
    printf("1. Add new employee\n");
    printf("2. Modify customer or employee details\n");
    printf("3. Manage user roles\n");
    printf("4. Reset another user's password\n");
    printf("5. Change my password\n");
    printf("6. Logout\n");
    printf("7. Exit\n");
    printf("Select an option: ");
}

void adminDashboard(User users[], int *userCount,
                    Account accounts[], int *accountCount,
                    Transaction transactions[], int *transactionCount,
                    Feedback feedback[], int *feedbackCount,
                    LoanApplication loans[], int *loanCount,
                    int loggedInUserId, uint64_t sessionToken) {
    int option = 0;

    do {
        displayAdminMenu();
        int waitingStatus = waitForUserSessionInput(loggedInUserId, sessionToken);
        if (waitingStatus != 1) {
            if (waitingStatus < 0) {
                fprintf(stderr, "Could not verify admin session; session stopped.\n");
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
                fprintf(stderr, "Could not verify admin session; session stopped.\n");
            } else {
                printf("You were logged out because this account signed in elsewhere.\n");
            }
            return;
        }

        switch (option) {
            case 1: {
                User newUser = {0};
                printf("Enter employee username: ");
                scanf("%19s", newUser.username);
                printf("Enter employee password: ");
                scanf("%19s", newUser.password);
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadUsersFromFile(users, userCount, "bank_users.dat")) {
                    fprintf(stderr, "Could not reload users; employee creation cancelled.\n");
                    endBankDataTransaction();
                    break;
                }
                if (*userCount >= MAX_USERS ||
                    findUserByUsername(users, *userCount, newUser.username) != -1) {
                    printf("User capacity reached or username already exists.\n");
                    endBankDataTransaction();
                    break;
                }
                int maxUserId = 0;
                for (int i = 0; i < *userCount; i++) {
                    if (users[i].user_id > maxUserId) maxUserId = users[i].user_id;
                }
                newUser.user_id = maxUserId + 1;
                newUser.role_id = ROLE_EMPLOYEE;
                newUser.is_active = 1;
                newUser.is_logged_in = 0;
                users[*userCount] = newUser;
                if (saveUsersToFile(users, *userCount + 1, "bank_users.dat")) {
                    (*userCount)++;
                    printf("Employee added successfully.\n");
                } else {
                    fprintf(stderr, "Could not save employee.\n");
                }
                endBankDataTransaction();
                break;
            }

            case 2: {
                int userId;
                printf("Enter user ID to modify: ");
                scanf("%d", &userId);
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
                    fprintf(stderr, "Could not reload users.\n");
                    endBankDataTransaction();
                    break;
                }
                int userIndex = findUserById(users, *userCount, userId);
                int duplicateIndex = findUserByUsername(users, *userCount, newUsername);
                if (userIndex == -1 || (duplicateIndex != -1 && duplicateIndex != userIndex)) {
                    printf("User no longer exists or username already exists.\n");
                    endBankDataTransaction();
                    break;
                }
                snprintf(users[userIndex].username, sizeof(users[userIndex].username), "%s", newUsername);
                snprintf(users[userIndex].password, sizeof(users[userIndex].password), "%s", newPassword);
                if (saveUsersToFile(users, *userCount, "bank_users.dat")) {
                    printf("User details updated.\n");
                } else {
                    fprintf(stderr, "Could not save user details.\n");
                }
                endBankDataTransaction();
                break;
            }

            case 3: {
                if (!loadUsersFromFile(users, userCount, "bank_users.dat")) {
                    fprintf(stderr, "Could not load user records.\n");
                    break;
                }
                printAllUsers(users, *userCount);
                int userId;
                printf("Enter user ID to change role: ");
                scanf("%d", &userId);
                printf("Choose new role (1=Customer, 2=Manager, 3=Employee, 4=Admin): ");
                int newRole;
                scanf("%d", &newRole);
                if (newRole < 1 || newRole > 4) {
                    printf("Invalid role.\n");
                    break;
                }
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadUsersFromFile(users, userCount, "bank_users.dat")) {
                    fprintf(stderr, "Could not reload users.\n");
                    endBankDataTransaction();
                    break;
                }
                int userIndex = findUserById(users, *userCount, userId);
                if (userIndex == -1) {
                    printf("User no longer exists.\n");
                    endBankDataTransaction();
                    break;
                }
                users[userIndex].role_id = newRole;
                if (saveUsersToFile(users, *userCount, "bank_users.dat")) {
                    printf("User role updated.\n");
                } else {
                    fprintf(stderr, "Could not save user role.\n");
                }
                endBankDataTransaction();
                break;
            }

            case 4: {
                int userId;
                printf("Enter user ID to change password: ");
                scanf("%d", &userId);
                char newPassword[20];
                printf("Enter new password: ");
                scanf("%19s", newPassword);
                if (!beginBankDataTransaction()) {
                    break;
                }
                if (!loadUsersFromFile(users, userCount, "bank_users.dat")) {
                    fprintf(stderr, "Could not reload users.\n");
                    endBankDataTransaction();
                    break;
                }
                int userIndex = findUserById(users, *userCount, userId);
                if (userIndex == -1) {
                    printf("User no longer exists.\n");
                    endBankDataTransaction();
                    break;
                }
                snprintf(users[userIndex].password, sizeof(users[userIndex].password), "%s", newPassword);
                if (saveUsersToFile(users, *userCount, "bank_users.dat")) {
                    printf("Password updated.\n");
                } else {
                    fprintf(stderr, "Could not save password.\n");
                }
                endBankDataTransaction();
                break;
            }

            case 5:
                promptUserPasswordChange(loggedInUserId, sessionToken);
                break;

            case 6:
                if (releaseUserSession(loggedInUserId, sessionToken)) {
                    printf("Admin logged out successfully.\n");
                } else {
                    fprintf(stderr, "Could not persist admin logout state.\n");
                }
                return;

            case 7:
                if (!releaseUserSession(loggedInUserId, sessionToken)) {
                    fprintf(stderr, "Could not persist admin logout state.\n");
                }
                printf("Exiting admin dashboard.\n");
                return;

            default:
                printf("Invalid option.\n");
                break;
        }
    } while (option != 7 && option != 6);
}
