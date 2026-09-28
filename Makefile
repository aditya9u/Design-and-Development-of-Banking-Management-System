CC = gcc
CFLAGS = -Wall -Wextra -std=c11
APP_TARGET = banking_management_system.exe
SERVER_TARGET = bank_server.exe
CLIENT_TARGET = bank_client.exe

all: $(APP_TARGET) $(SERVER_TARGET) $(CLIENT_TARGET)

$(APP_TARGET): src/application.c src/bank_core.c src/customer.c src/employee.c include/application.h include/bank_core.h include/customer.h include/employee.h
	$(CC) $(CFLAGS) -Iinclude src/application.c src/bank_core.c src/customer.c src/employee.c -o $(APP_TARGET)

$(SERVER_TARGET): src/server.c src/bank_core.c src/customer.c include/server.h include/bank_core.h include/customer.h
	$(CC) $(CFLAGS) -Iinclude src/server.c src/bank_core.c src/customer.c -lws2_32 -o $(SERVER_TARGET)

$(CLIENT_TARGET): src/client.c include/client.h
	$(CC) $(CFLAGS) -Iinclude src/client.c -lws2_32 -o $(CLIENT_TARGET)

clean:
	del /Q $(APP_TARGET) $(SERVER_TARGET) $(CLIENT_TARGET) 2>NUL
