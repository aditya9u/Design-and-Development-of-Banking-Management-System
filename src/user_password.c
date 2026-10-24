#include <stdio.h>
#include <string.h>
#include "bank_core.h"
#include "user_password.h"

int promptUserPasswordChange(int userId, uint64_t sessionToken) {
    char currentPassword[20];
    char newPassword[20];
    char confirmedPassword[20];

    printf("Current password: ");
    if (scanf("%19s", currentPassword) != 1) {
        if (!feof(stdin)) {
            fprintf(stderr, "Could not read current password.\n");
        }
        return 0;
    }

    printf("New password: ");
    if (scanf("%19s", newPassword) != 1) {
        if (!feof(stdin)) {
            fprintf(stderr, "Could not read new password.\n");
        }
        return 0;
    }

    printf("Confirm new password: ");
    if (scanf("%19s", confirmedPassword) != 1) {
        if (!feof(stdin)) {
            fprintf(stderr, "Could not read password confirmation.\n");
        }
        return 0;
    }

    if (strcmp(newPassword, confirmedPassword) != 0) {
        printf("New passwords do not match.\n");
        return 0;
    }

    if (strcmp(currentPassword, newPassword) == 0) {
        printf("New password must be different from the current password.\n");
        return 0;
    }

    int result = updateUserPassword(userId, sessionToken,
                                    currentPassword, newPassword);
    if (result == 1) {
        printf("Your password has been changed.\n");
    } else if (result == -1) {
        printf("Current password is incorrect.\n");
    } else if (result == -2) {
        printf("Your session is no longer active; password was not changed.\n");
    } else {
        fprintf(stderr, "Could not change password; please try again.\n");
    }
    return result == 1;
}
