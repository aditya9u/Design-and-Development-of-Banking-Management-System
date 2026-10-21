#ifndef EMPLOYEE_H
#define EMPLOYEE_H

#include "bank_core.h"

void displayEmployeeMenu(void);
void employeeDashboard(User users[], int *userCount,
                      Account accounts[], int *accountCount,
                      Transaction transactions[], int *transactionCount,
                      Feedback feedback[], int *feedbackCount,
                      LoanApplication loans[], int *loanCount,
                      int loggedInUserId);

#endif