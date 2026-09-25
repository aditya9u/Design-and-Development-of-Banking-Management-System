#include <stdio.h>
#include "bank.h"

int main(void) {
    Account accounts[MAX_ACCOUNTS];
    int count = 0;
    int choice;

    loadSampleData(accounts, &count);

    do {
        displayMenu();
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                addAccount(accounts, &count);
                break;
            case 2:
                deposit(accounts, count);
                break;
            case 3:
                withdraw(accounts, count);
                break;
            case 4:
                displayAccounts(accounts, count);
                break;
            case 5:
                printf("Exiting the banking system. Goodbye!\n");
                break;
            default:
                printf("Invalid option. Please try again.\n");
                break;
        }

    } while (choice != 5);

    return 0;
}
