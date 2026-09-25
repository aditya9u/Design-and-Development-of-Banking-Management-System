# Design and Development of Banking Management System

This project is a simplified banking management system developed in C. It simulates key banking operations such as creating accounts, depositing money, withdrawing funds, and viewing account records through a console-based interface.

## Project Overview

The system includes:
- Customer account creation
- Deposit and withdrawal operations
- Balance checking
- Account lookup by account number
- Display of all account details
- Menu-driven C application flow

## Essential Tools Installed

The following development tools were installed so the project can run correctly in this Windows environment:

- MinGW GNU Compiler Collection (GCC)
- WinLibs POSIX UCRT toolchain package
- Standard command-line build support for C programs

Installation command used:

```powershell
winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e
```

This provides the `gcc` compiler required to build and run the project.

## Verified Compiler Check

The compiler was confirmed working with:

```powershell
gcc --version
```

## Project Files

```text
Design-and-Development-of-Banking-Management-System/
├── README.md
├── main.c
├── bank.h
├── bank.c
├── Makefile
└── banking_management_system.exe
```

## Build Instructions

Open PowerShell in the project folder and run:

```powershell
gcc main.c bank.c -o banking_management_system.exe
```

Then execute the program:

```powershell
.\banking_management_system.exe
```

You can also use the Makefile:

```powershell
mingw32-make
.\banking_management_system.exe
```

## Features Included in the Current Version

- Create a new bank account
- Deposit money into a selected account
- Withdraw money from a selected account
- View all stored accounts
- Exit the application safely

## Notes

This is a beginner-friendly C project that helps with structured programming, arrays, function design, and menu-based console application development. It can be expanded later with features such as login management, loan processing, transaction history, and admin panels.
