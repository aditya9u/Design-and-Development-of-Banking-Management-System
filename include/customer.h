#ifndef CUSTOMER_H
#define CUSTOMER_H

#include "bank_core.h"

void displayCustomerMenu(void);
void customerDashboard(User users[], int *userCount,
                       Account accounts[], int *accountCount,
                       Transaction transactions[], int *transactionCount,
                       Feedback feedback[], int *feedbackCount,
                       LoanApplication loans[], int *loanCount,
                       int loggedInUserId, uint64_t sessionToken);

#endif