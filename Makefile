CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = banking_management_system

all: $(TARGET)

$(TARGET): main.c bank.c
	$(CC) $(CFLAGS) main.c bank.c -o $(TARGET)

clean:
	del /Q $(TARGET).exe 2>NUL
