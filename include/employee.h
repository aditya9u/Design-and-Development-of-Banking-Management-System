#ifndef EMPLOYEE_H
#define EMPLOYEE_H

#include "bank_core.h"

void displayEmployeeMenu(void);
int loginEmployee(const char *employeeId, const char *password);
void addAccount(Account accounts[], int *count);
void modifyCustomer(Account accounts[], int count);
void closeCustomer(Account accounts[], int *count,
                   const LoanApplication loans[], int loanCount);
void displayLoanApplications(const LoanApplication loans[], int loanCount,
                             const char *employeeId);
void processLoanApplication(Account accounts[], int count,
                            LoanApplication loans[], int loanCount,
                            const char *employeeId);
void employeeDashboard(Account accounts[], int *count,
                       LoanApplication loans[], int *loanCount,
                       const char *employeeId);

#endif