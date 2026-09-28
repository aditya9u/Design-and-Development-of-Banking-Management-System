#ifndef CUSTOMER_H
#define CUSTOMER_H

#include "bank_core.h"

void displayCustomerMenu(void);
int loginCustomer(const Account accounts[], int count, int accountNumber, const char *password);
void viewBalance(const Account *account);
int depositToAccount(Account *account, double amount);
int withdrawFromAccount(Account *account, double amount);
void transferFunds(Account accounts[], int count, int senderIndex);
void applyForLoan(const Account *account, LoanApplication loans[], int *loanCount);
void customerDashboard(Account accounts[], int count, int loggedInIndex,
                       LoanApplication loans[], int *loanCount);

#endif