CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread
APP_TARGET = banking_management_system

all: $(APP_TARGET)

$(APP_TARGET): src/application.c src/bank_core.c src/transaction_worker.c src/customer.c src/employee.c src/manager.c src/admin.c include/application.h include/bank_core.h include/customer.h include/employee.h include/manager.h include/admin.h
	$(CC) $(CFLAGS) -Iinclude src/application.c src/bank_core.c src/transaction_worker.c src/customer.c src/employee.c src/manager.c src/admin.c -o $(APP_TARGET)

clean:
	rm -f $(APP_TARGET)
