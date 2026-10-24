CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread
APP_TARGET = banking_management_system
WINDOWS_TARGET = banking_management_system.exe
WINDOWS_RUNTIME = /usr/bin/msys-2.0.dll /usr/bin/msys-gcc_s-seh-1.dll
SOURCES = src/application.c src/bank_core.c src/transaction_worker.c src/customer.c src/employee.c src/manager.c src/admin.c src/user_password.c
HEADERS = include/application.h include/bank_core.h include/customer.h include/employee.h include/manager.h include/admin.h include/user_password.h

.PHONY: all windows clean

all: $(APP_TARGET)

$(APP_TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -Iinclude $(SOURCES) -o $@

windows: $(WINDOWS_TARGET)

$(WINDOWS_TARGET): $(SOURCES) $(HEADERS) $(WINDOWS_RUNTIME)
	$(CC) $(CFLAGS) -D_GNU_SOURCE -Iinclude $(SOURCES) -o $@
	cp $(WINDOWS_RUNTIME) .

clean:
	rm -f $(APP_TARGET) $(WINDOWS_TARGET) msys-2.0.dll msys-gcc_s-seh-1.dll
