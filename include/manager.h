#ifndef MANAGER_H
#define MANAGER_H

#include "bank_core.h"

void displayManagerMenu(void);
void managerDashboard(User users[], int *userCount,
                     Account accounts[], int *accountCount,
                     Transaction transactions[], int *transactionCount,
                     Feedback feedback[], int *feedbackCount,
                     LoanApplication loans[], int *loanCount,
                     int loggedInUserId, uint64_t sessionToken);

#endif
