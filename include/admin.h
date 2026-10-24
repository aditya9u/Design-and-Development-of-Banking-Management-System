#ifndef ADMIN_H
#define ADMIN_H

#include "bank_core.h"

void displayAdminMenu(void);
void adminDashboard(User users[], int *userCount,
                    Account accounts[], int *accountCount,
                    Transaction transactions[], int *transactionCount,
                    Feedback feedback[], int *feedbackCount,
                    LoanApplication loans[], int *loanCount,
                    int loggedInUserId, uint64_t sessionToken);

#endif
