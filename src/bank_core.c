#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "bank_core.h"

#define BANK_DATA_LOCK_FILE ".bank_data.lock"
#define BANK_SESSION_FILE "bank_sessions.dat"
#define BANK_SESSION_GRACE_SECONDS 30

typedef struct {
    int userId;
    pid_t ownerPid;
    uint64_t generation;
    int active;
    uint32_t lastHeartbeat;
} UserSession;

static pthread_mutex_t bankThreadMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sessionHeartbeatMutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local unsigned int bankLockDepth;
static _Thread_local int bankLockFileDescriptor = -1;
static pthread_t sessionHeartbeatThread;
static int sessionHeartbeatRunning;
static int sessionHeartbeatStarted;
static int sessionHeartbeatUserId;
static uint64_t sessionHeartbeatToken;

int beginBankDataTransaction(void) {
    if (bankLockDepth > 0) {
        bankLockDepth++;
        return 1;
    }

    int error = pthread_mutex_lock(&bankThreadMutex);
    if (error != 0) {
        fprintf(stderr, "Could not acquire the in-process bank lock: %s.\n", strerror(error));
        return 0;
    }

    bankLockFileDescriptor = open(BANK_DATA_LOCK_FILE, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (bankLockFileDescriptor == -1) {
        fprintf(stderr, "Could not open bank lock file: %s.\n", strerror(errno));
        pthread_mutex_unlock(&bankThreadMutex);
        return 0;
    }

    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    while (fcntl(bankLockFileDescriptor, F_SETLKW, &lock) == -1) {
        if (errno == EINTR) {
            continue;
        }
        fprintf(stderr, "Could not acquire bank file lock: %s.\n", strerror(errno));
        close(bankLockFileDescriptor);
        bankLockFileDescriptor = -1;
        pthread_mutex_unlock(&bankThreadMutex);
        return 0;
    }

    bankLockDepth = 1;
    return 1;
}

void endBankDataTransaction(void) {
    if (bankLockDepth == 0) {
        fprintf(stderr, "Bank data lock release requested without a matching acquire.\n");
        return;
    }
    bankLockDepth--;
    if (bankLockDepth > 0) {
        return;
    }

    struct flock lock = {0};
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(bankLockFileDescriptor, F_SETLK, &lock) == -1) {
        fprintf(stderr, "Could not release bank file lock: %s.\n", strerror(errno));
    }
    if (close(bankLockFileDescriptor) == -1) {
        fprintf(stderr, "Could not close bank lock file: %s.\n", strerror(errno));
    }
    bankLockFileDescriptor = -1;
    int error = pthread_mutex_unlock(&bankThreadMutex);
    if (error != 0) {
        fprintf(stderr, "Could not release the in-process bank lock: %s.\n", strerror(error));
    }
}

static void getCurrentTimestamp(char buffer[], size_t size) {
    time_t now = time(NULL);
    struct tm *tmInfo = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tmInfo);
}

void initializeSampleData(User users[], int *userCount,
                         Account accounts[], int *accountCount,
                         Transaction transactions[], int *transactionCount,
                         Feedback feedback[], int *feedbackCount) {
    User sampleUsers[] = {
        {1, "alice", "alice123", ROLE_CUSTOMER, 1, 0},
        {2, "manager", "manager123", ROLE_MANAGER, 1, 0},
        {3, "employee", "emp123", ROLE_EMPLOYEE, 1, 0},
        {4, "admin", "admin123", ROLE_ADMIN, 1, 0}
    };

    Account sampleAccounts[] = {
        {1001, 1, 2500.00, 1},
        {1002, 2, 1200.00, 1},
        {1003, 3, 3400.00, 1}
    };

    Transaction sampleTransactions[] = {
        {1, 1001, "DEPOSIT", 2500.00, "Initial deposit", "2024-01-01 10:00:00"},
        {2, 1002, "DEPOSIT", 1200.00, "Initial deposit", "2024-01-01 10:05:00"},
        {3, 1003, "DEPOSIT", 3400.00, "Initial deposit", "2024-01-01 10:10:00"}
    };

    Feedback sampleFeedback[] = {
        {1, 1, "Customer service is good.", "2024-01-02 09:00:00"}
    };

    int sampleUserCount = (int)(sizeof(sampleUsers) / sizeof(sampleUsers[0]));
    int sampleAccountCount = (int)(sizeof(sampleAccounts) / sizeof(sampleAccounts[0]));
    int sampleTransactionCount = (int)(sizeof(sampleTransactions) / sizeof(sampleTransactions[0]));
    int sampleFeedbackCount = (int)(sizeof(sampleFeedback) / sizeof(sampleFeedback[0]));

    for (int i = 0; i < sampleUserCount; i++) {
        users[*userCount + i] = sampleUsers[i];
    }
    *userCount += sampleUserCount;

    for (int i = 0; i < sampleAccountCount; i++) {
        accounts[*accountCount + i] = sampleAccounts[i];
    }
    *accountCount += sampleAccountCount;

    for (int i = 0; i < sampleTransactionCount; i++) {
        transactions[*transactionCount + i] = sampleTransactions[i];
    }
    *transactionCount += sampleTransactionCount;

    for (int i = 0; i < sampleFeedbackCount; i++) {
        feedback[*feedbackCount + i] = sampleFeedback[i];
    }
    *feedbackCount += sampleFeedbackCount;
}

static int saveRecordsToFile(const void *records, size_t recordSize, int count,
                             int maxCount, const char *fileName) {
    if (count < 0 || count > maxCount || !beginBankDataTransaction()) {
        return 0;
    }

    char temporaryName[PATH_MAX];
    int pathLength = snprintf(temporaryName, sizeof(temporaryName), "%s.tmp.%ld",
                              fileName, (long)getpid());
    if (pathLength < 0 || (size_t)pathLength >= sizeof(temporaryName)) {
        endBankDataTransaction();
        return 0;
    }

    FILE *file = fopen(temporaryName, "wb");
    if (file == NULL) {
        endBankDataTransaction();
        return 0;
    }

    if (fwrite(&count, sizeof(int), 1, file) != 1) {
        fclose(file);
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }

    if (count > 0 && fwrite(records, recordSize, (size_t)count, file) != (size_t)count) {
        fclose(file);
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }

    int flushSucceeded = fflush(file) == 0;
    int closeSucceeded = fclose(file) == 0;
    if (!flushSucceeded || !closeSucceeded) {
        fprintf(stderr, "Could not flush data file %s.\n", fileName);
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }
    int syncDescriptor = open(temporaryName, O_RDWR);
    if (syncDescriptor == -1 || fsync(syncDescriptor) == -1) {
        fprintf(stderr, "Could not sync temporary data file %s: %s.\n",
                temporaryName, strerror(errno));
        if (syncDescriptor != -1) {
            close(syncDescriptor);
        }
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }
    close(syncDescriptor);
    if (rename(temporaryName, fileName) == -1) {
        fprintf(stderr, "Could not commit data file %s: %s.\n", fileName, strerror(errno));
        unlink(temporaryName);
        endBankDataTransaction();
        return 0;
    }

    endBankDataTransaction();
    return 1;
}

static int loadRecordsFromFile(void *records, size_t recordSize, int *count,
                               int maxCount, const char *fileName) {
    if (count == NULL || !beginBankDataTransaction()) {
        return 0;
    }

    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        endBankDataTransaction();
        return 0;
    }

    int loadedCount = 0;
    if (fread(&loadedCount, sizeof(int), 1, file) != 1) {
        fclose(file);
        endBankDataTransaction();
        return 0;
    }

    if (loadedCount < 0 || loadedCount > maxCount) {
        fclose(file);
        endBankDataTransaction();
        return 0;
    }

    if (loadedCount > 0 &&
        fread(records, recordSize, (size_t)loadedCount, file) != (size_t)loadedCount) {
        fclose(file);
        endBankDataTransaction();
        return 0;
    }

    *count = loadedCount;
    fclose(file);
    endBankDataTransaction();
    return 1;
}

int saveUsersToFile(const User users[], int count, const char *fileName) {
    return saveRecordsToFile(users, sizeof(User), count, MAX_USERS, fileName);
}

int loadUsersFromFile(User users[], int *count, const char *fileName) {
    return loadRecordsFromFile(users, sizeof(User), count, MAX_USERS, fileName);
}

int saveAccountsToFile(const Account accounts[], int count, const char *fileName) {
    return saveRecordsToFile(accounts, sizeof(Account), count, MAX_ACCOUNTS, fileName);
}

int loadAccountsFromFile(Account accounts[], int *count, const char *fileName) {
    return loadRecordsFromFile(accounts, sizeof(Account), count, MAX_ACCOUNTS, fileName);
}

int saveTransactionsToFile(const Transaction transactions[], int count, const char *fileName) {
    return saveRecordsToFile(transactions, sizeof(Transaction), count, MAX_TRANSACTIONS, fileName);
}

int loadTransactionsFromFile(Transaction transactions[], int *count, const char *fileName) {
    return loadRecordsFromFile(transactions, sizeof(Transaction), count, MAX_TRANSACTIONS, fileName);
}

int saveFeedbackToFile(const Feedback feedback[], int count, const char *fileName) {
    return saveRecordsToFile(feedback, sizeof(Feedback), count, MAX_FEEDBACK, fileName);
}

int loadFeedbackFromFile(Feedback feedback[], int *count, const char *fileName) {
    return loadRecordsFromFile(feedback, sizeof(Feedback), count, MAX_FEEDBACK, fileName);
}

static int loadUserSessions(UserSession sessions[], int *count) {
    FILE *file = fopen(BANK_SESSION_FILE, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            *count = 0;
            return 1;
        }
        fprintf(stderr, "Could not open user session data: %s.\n", strerror(errno));
        return 0;
    }
    fclose(file);
    return loadRecordsFromFile(sessions, sizeof(UserSession), count,
                               MAX_USERS, BANK_SESSION_FILE);
}

static int findUserSession(const UserSession sessions[], int count, int userId) {
    for (int i = 0; i < count; i++) {
        if (sessions[i].userId == userId) {
            return i;
        }
    }
    return -1;
}

int findUserByUsername(const User users[], int count, const char *username) {
    for (int i = 0; i < count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

int findUserById(const User users[], int count, int userId) {
    for (int i = 0; i < count; i++) {
        if (users[i].user_id == userId) {
            return i;
        }
    }
    return -1;
}

int claimUserSession(int userId, uint64_t *sessionToken) {
    if (sessionToken == NULL) {
        return 0;
    }
    if (!restoreStaleUserSessions()) {
        return 0;
    }
    if (!beginBankDataTransaction()) {
        return 0;
    }

    User currentUsers[MAX_USERS];
    int currentCount = 0;
    UserSession sessions[MAX_USERS] = {0};
    int sessionCount = 0;
    if (!loadUsersFromFile(currentUsers, &currentCount, "bank_users.dat") ||
        !loadUserSessions(sessions, &sessionCount)) {
        endBankDataTransaction();
        return 0;
    }

    int userIndex = findUserById(currentUsers, currentCount, userId);
    if (userIndex == -1 || !currentUsers[userIndex].is_active) {
        endBankDataTransaction();
        return 0;
    }

    int sessionIndex = findUserSession(sessions, sessionCount, userId);
    if (sessionIndex == -1) {
        if (sessionCount >= MAX_USERS) {
            endBankDataTransaction();
            return 0;
        }
        sessionIndex = sessionCount++;
        sessions[sessionIndex].userId = userId;
    }
    if (sessions[sessionIndex].active) {
        time_t now = time(NULL);
        if (now < 0 || (uint64_t)now > UINT32_MAX) {
            fprintf(stderr, "System time is outside the supported session timer range.\n");
            endBankDataTransaction();
            return 0;
        }
        int ownerIsAlive = sessions[sessionIndex].ownerPid > 0 &&
                           (kill(sessions[sessionIndex].ownerPid, 0) == 0 ||
                            errno == EPERM);
        int heartbeatIsFresh = sessions[sessionIndex].lastHeartbeat != 0 &&
                               (uint32_t)now >= sessions[sessionIndex].lastHeartbeat &&
                               (uint32_t)now - sessions[sessionIndex].lastHeartbeat <
                                   BANK_SESSION_GRACE_SECONDS;
        if (ownerIsAlive && heartbeatIsFresh) {
            endBankDataTransaction();
            return -2;
        }
        if (heartbeatIsFresh) {
            endBankDataTransaction();
            return -1;
        }
    }
    if (sessions[sessionIndex].generation == UINT64_MAX) {
        endBankDataTransaction();
        return 0;
    }

    UserSession previousSession = sessions[sessionIndex];
    sessions[sessionIndex].generation++;
    sessions[sessionIndex].ownerPid = getpid();
    sessions[sessionIndex].active = 1;
    time_t now = time(NULL);
    if (now < 0 || (uint64_t)now > UINT32_MAX) {
        sessions[sessionIndex] = previousSession;
        endBankDataTransaction();
        return 0;
    }
    sessions[sessionIndex].lastHeartbeat = (uint32_t)now;
    currentUsers[userIndex].is_logged_in = 1;
    uint64_t newToken = sessions[sessionIndex].generation;

    int saved = saveRecordsToFile(sessions, sizeof(UserSession), sessionCount,
                                 MAX_USERS, BANK_SESSION_FILE);
    if (saved && !saveUsersToFile(currentUsers, currentCount, "bank_users.dat")) {
        sessions[sessionIndex] = previousSession;
        if (!saveRecordsToFile(sessions, sizeof(UserSession), sessionCount,
                               MAX_USERS, BANK_SESSION_FILE)) {
            fprintf(stderr, "Could not roll back the user session claim.\n");
        }
        saved = 0;
    }
    if (saved) {
        *sessionToken = newToken;
    }

    endBankDataTransaction();
    return saved;
}

int isUserSessionCurrent(int userId, uint64_t sessionToken) {
    if (!beginBankDataTransaction()) {
        return -1;
    }

    UserSession sessions[MAX_USERS] = {0};
    int sessionCount = 0;
    if (!loadUserSessions(sessions, &sessionCount)) {
        endBankDataTransaction();
        return -1;
    }

    int sessionIndex = findUserSession(sessions, sessionCount, userId);
    int isCurrent = sessionIndex != -1 &&
                    sessions[sessionIndex].active &&
                    sessions[sessionIndex].generation == sessionToken;
    endBankDataTransaction();
    return isCurrent;
}

int updateUserPassword(int userId, uint64_t sessionToken,
                       const char *currentPassword, const char *newPassword) {
    if (currentPassword == NULL || newPassword == NULL ||
        newPassword[0] == '\0' || strlen(newPassword) >= sizeof(((User *)0)->password)) {
        return 0;
    }
    if (!beginBankDataTransaction()) {
        return 0;
    }

    int sessionStatus = isUserSessionCurrent(userId, sessionToken);
    if (sessionStatus != 1) {
        endBankDataTransaction();
        return sessionStatus == 0 ? -2 : 0;
    }

    User currentUsers[MAX_USERS];
    int currentCount = 0;
    if (!loadUsersFromFile(currentUsers, &currentCount, "bank_users.dat")) {
        fprintf(stderr, "Could not load user records for password change.\n");
        endBankDataTransaction();
        return 0;
    }

    int userIndex = findUserById(currentUsers, currentCount, userId);
    if (userIndex == -1) {
        fprintf(stderr, "Could not find the active user for password change.\n");
        endBankDataTransaction();
        return 0;
    }
    if (strcmp(currentUsers[userIndex].password, currentPassword) != 0) {
        endBankDataTransaction();
        return -1;
    }

    snprintf(currentUsers[userIndex].password,
             sizeof(currentUsers[userIndex].password), "%s", newPassword);
    int saved = saveUsersToFile(currentUsers, currentCount, "bank_users.dat");
    if (!saved) {
        fprintf(stderr, "Could not save the updated password.\n");
    }
    endBankDataTransaction();
    return saved ? 1 : 0;
}

int restoreStaleUserSessions(void) {
    if (!beginBankDataTransaction()) {
        return 0;
    }

    UserSession sessions[MAX_USERS] = {0};
    int sessionCount = 0;
    User currentUsers[MAX_USERS];
    int currentCount = 0;
    if (!loadUserSessions(sessions, &sessionCount) ||
        !loadUsersFromFile(currentUsers, &currentCount, "bank_users.dat")) {
        endBankDataTransaction();
        return 0;
    }

    time_t currentTime = time(NULL);
    if (currentTime < 0 || (uint64_t)currentTime > UINT32_MAX) {
        fprintf(stderr, "System time is outside the supported session timer range.\n");
        endBankDataTransaction();
        return 0;
    }
    uint32_t now = (uint32_t)currentTime;
    UserSession previousSessions[MAX_USERS];
    memcpy(previousSessions, sessions, (size_t)sessionCount * sizeof(UserSession));
    int changed = 0;
    for (int i = 0; i < sessionCount; i++) {
        if (!sessions[i].active) {
            continue;
        }
        int ownerIsAlive = sessions[i].ownerPid > 0 &&
                           (kill(sessions[i].ownerPid, 0) == 0 || errno == EPERM);
        int heartbeatIsFresh = sessions[i].lastHeartbeat != 0 &&
                               now >= sessions[i].lastHeartbeat &&
                               now - sessions[i].lastHeartbeat <
                                   BANK_SESSION_GRACE_SECONDS;
        if (ownerIsAlive && heartbeatIsFresh) {
            continue;
        }
        if (heartbeatIsFresh) {
            continue;
        }
        sessions[i].active = 0;
        sessions[i].ownerPid = 0;
        sessions[i].lastHeartbeat = 0;
        int userIndex = findUserById(currentUsers, currentCount, sessions[i].userId);
        if (userIndex != -1) {
            currentUsers[userIndex].is_logged_in = 0;
        }
        changed = 1;
    }

    if (!changed) {
        endBankDataTransaction();
        return 1;
    }

    int saved = saveRecordsToFile(sessions, sizeof(UserSession), sessionCount,
                                  MAX_USERS, BANK_SESSION_FILE);
    if (saved && !saveUsersToFile(currentUsers, currentCount, "bank_users.dat")) {
        fprintf(stderr, "Could not reset stale user login status.\n");
        memcpy(sessions, previousSessions, (size_t)sessionCount * sizeof(UserSession));
        if (!saveRecordsToFile(sessions, sizeof(UserSession), sessionCount,
                               MAX_USERS, BANK_SESSION_FILE)) {
            fprintf(stderr, "Could not restore stale session records.\n");
        }
        saved = 0;
    }
    endBankDataTransaction();
    return saved;
}

static int refreshUserSessionHeartbeat(int userId, uint64_t sessionToken) {
    if (!beginBankDataTransaction()) {
        return 0;
    }

    UserSession sessions[MAX_USERS] = {0};
    int sessionCount = 0;
    if (!loadUserSessions(sessions, &sessionCount)) {
        endBankDataTransaction();
        return 0;
    }

    int sessionIndex = findUserSession(sessions, sessionCount, userId);
    if (sessionIndex == -1 || !sessions[sessionIndex].active ||
        sessions[sessionIndex].generation != sessionToken) {
        endBankDataTransaction();
        return 0;
    }

    time_t currentTime = time(NULL);
    if (currentTime < 0 || (uint64_t)currentTime > UINT32_MAX) {
        fprintf(stderr, "System time is outside the supported session timer range.\n");
        endBankDataTransaction();
        return 0;
    }
    uint32_t now = (uint32_t)currentTime;
    if (sessions[sessionIndex].lastHeartbeat == now) {
        endBankDataTransaction();
        return 1;
    }

    sessions[sessionIndex].lastHeartbeat = now;
    int saved = saveRecordsToFile(sessions, sizeof(UserSession), sessionCount,
                                  MAX_USERS, BANK_SESSION_FILE);
    endBankDataTransaction();
    return saved;
}

static void *sessionHeartbeatWorker(void *unused) {
    (void)unused;
    for (;;) {
        int error = pthread_mutex_lock(&sessionHeartbeatMutex);
        if (error != 0) {
            fprintf(stderr, "Could not read session heartbeat state: %s.\n", strerror(error));
            return NULL;
        }
        int running = sessionHeartbeatRunning;
        int userId = sessionHeartbeatUserId;
        uint64_t token = sessionHeartbeatToken;
        pthread_mutex_unlock(&sessionHeartbeatMutex);
        if (!running) {
            return NULL;
        }

        struct timespec delay = {1, 0};
        int sleepResult;
        do {
            sleepResult = nanosleep(&delay, &delay);
        } while (sleepResult == -1 && errno == EINTR);
        if (sleepResult == -1) {
            fprintf(stderr, "Could not wait for session heartbeat: %s.\n", strerror(errno));
            return NULL;
        }

        if (!refreshUserSessionHeartbeat(userId, token)) {
            return NULL;
        }
    }
}

int startUserSessionHeartbeat(int userId, uint64_t sessionToken) {
    int error = pthread_mutex_lock(&sessionHeartbeatMutex);
    if (error != 0) {
        fprintf(stderr, "Could not lock session heartbeat state: %s.\n", strerror(error));
        return 0;
    }
    if (sessionHeartbeatStarted) {
        pthread_mutex_unlock(&sessionHeartbeatMutex);
        fprintf(stderr, "A session heartbeat is already running.\n");
        return 0;
    }

    sessionHeartbeatUserId = userId;
    sessionHeartbeatToken = sessionToken;
    sessionHeartbeatRunning = 1;
    error = pthread_create(&sessionHeartbeatThread, NULL, sessionHeartbeatWorker, NULL);
    if (error != 0) {
        sessionHeartbeatRunning = 0;
        pthread_mutex_unlock(&sessionHeartbeatMutex);
        fprintf(stderr, "Could not start session heartbeat: %s.\n", strerror(error));
        return 0;
    }
    sessionHeartbeatStarted = 1;
    pthread_mutex_unlock(&sessionHeartbeatMutex);
    return 1;
}

int stopUserSessionHeartbeat(void) {
    int error = pthread_mutex_lock(&sessionHeartbeatMutex);
    if (error != 0) {
        fprintf(stderr, "Could not lock session heartbeat state: %s.\n", strerror(error));
        return 0;
    }
    int started = sessionHeartbeatStarted;
    sessionHeartbeatRunning = 0;
    pthread_t thread = sessionHeartbeatThread;
    pthread_mutex_unlock(&sessionHeartbeatMutex);
    if (!started) {
        return 1;
    }

    error = pthread_join(thread, NULL);
    if (error != 0) {
        fprintf(stderr, "Could not stop session heartbeat: %s.\n", strerror(error));
        return 0;
    }

    error = pthread_mutex_lock(&sessionHeartbeatMutex);
    if (error != 0) {
        fprintf(stderr, "Could not finalize session heartbeat state: %s.\n", strerror(error));
        return 0;
    }
    sessionHeartbeatStarted = 0;
    sessionHeartbeatUserId = 0;
    sessionHeartbeatToken = 0;
    pthread_mutex_unlock(&sessionHeartbeatMutex);
    return 1;
}

int waitForUserSessionInput(int userId, uint64_t sessionToken) {
    for (;;) {
        int sessionStatus = isUserSessionCurrent(userId, sessionToken);
        if (sessionStatus != 1) {
            return sessionStatus;
        }

        fd_set inputDescriptors;
        FD_ZERO(&inputDescriptors);
        FD_SET(STDIN_FILENO, &inputDescriptors);
        struct timeval timeout = {0, 250000};
        int ready = select(STDIN_FILENO + 1, &inputDescriptors, NULL, NULL, &timeout);
        if (ready > 0) {
            return 1;
        }
        if (ready == 0 || errno == EINTR) {
            continue;
        }
        fprintf(stderr, "Could not wait for session input: %s.\n", strerror(errno));
        return -1;
    }
}

int releaseUserSession(int userId, uint64_t sessionToken) {
    if (!beginBankDataTransaction()) {
        return 0;
    }

    UserSession sessions[MAX_USERS] = {0};
    int sessionCount = 0;
    User currentUsers[MAX_USERS];
    int currentCount = 0;
    if (!loadUserSessions(sessions, &sessionCount) ||
        !loadUsersFromFile(currentUsers, &currentCount, "bank_users.dat")) {
        endBankDataTransaction();
        return 0;
    }

    int sessionIndex = findUserSession(sessions, sessionCount, userId);
    int userIndex = findUserById(currentUsers, currentCount, userId);
    if (sessionIndex == -1 || userIndex == -1 ||
        !sessions[sessionIndex].active ||
        sessions[sessionIndex].generation != sessionToken) {
        endBankDataTransaction();
        return 0;
    }

    sessions[sessionIndex].active = 0;
    sessions[sessionIndex].ownerPid = 0;
    sessions[sessionIndex].lastHeartbeat = 0;
    currentUsers[userIndex].is_logged_in = 0;
    if (!saveRecordsToFile(sessions, sizeof(UserSession), sessionCount,
                           MAX_USERS, BANK_SESSION_FILE)) {
        endBankDataTransaction();
        return 0;
    }
    int saved = saveUsersToFile(currentUsers, currentCount, "bank_users.dat");
    if (!saved) {
        sessions[sessionIndex].active = 1;
        if (!saveRecordsToFile(sessions, sizeof(UserSession), sessionCount,
                               MAX_USERS, BANK_SESSION_FILE)) {
            fprintf(stderr, "Could not restore the active user session.\n");
        }
    }

    endBankDataTransaction();
    return saved;
}

int findAccountForUser(const Account accounts[], int count, int userId) {
    for (int i = 0; i < count; i++) {
        if (accounts[i].user_id == userId) {
            return i;
        }
    }
    return -1;
}

int findAccountById(const Account accounts[], int count, int accountId) {
    for (int i = 0; i < count; i++) {
        if (accounts[i].account_id == accountId) {
            return i;
        }
    }
    return -1;
}

void addTransaction(Transaction transactions[], int *transactionCount,
                    int accountId, const char *type, double amount, const char *details) {
    if (*transactionCount >= MAX_TRANSACTIONS) {
        printf("Transaction limit reached.\n");
        return;
    }

    Transaction newTransaction = {0};
    newTransaction.txn_id = *transactionCount + 1;
    newTransaction.account_id = accountId;
    snprintf(newTransaction.type, sizeof(newTransaction.type), "%s", type);
    newTransaction.amount = amount;
    snprintf(newTransaction.details, sizeof(newTransaction.details), "%s", details);
    getCurrentTimestamp(newTransaction.timestamp, sizeof(newTransaction.timestamp));

    transactions[*transactionCount] = newTransaction;
    (*transactionCount)++;
}

void printTransactions(const Transaction transactions[], int transactionCount, int accountId) {
    int found = 0;
    printf("\nTransaction History\n");
    printf("---------------------------------------------------\n");
    printf("ID  Account  Type         Amount   Date\n");
    printf("---------------------------------------------------\n");

    for (int i = 0; i < transactionCount; i++) {
        if (transactions[i].account_id == accountId) {
            printf("%2d  %7d  %-12s %8.2f  %s\n",
                   transactions[i].txn_id,
                   transactions[i].account_id,
                   transactions[i].type,
                   transactions[i].amount,
                   transactions[i].timestamp);
            found = 1;
        }
    }

    if (!found) {
        printf("No transactions available.\n");
    }
}

void printFeedback(const Feedback feedback[], int feedbackCount, int userId) {
    int found = 0;
    printf("\nCustomer Feedback\n");
    printf("----------------------------------------\n");
    for (int i = 0; i < feedbackCount; i++) {
        if (feedback[i].user_id == userId) {
            printf("[%d] %s\n", feedback[i].feedback_id, feedback[i].message);
            printf("Date: %s\n", feedback[i].timestamp);
            found = 1;
        }
    }
    if (!found) {
        printf("No feedback available.\n");
    }
}

void printAllUsers(const User users[], int count) {
    printf("\nUsers\n");
    printf("----------------------------------------\n");
    for (int i = 0; i < count; i++) {
        printf("ID:%d Username:%s Role:%d Active:%d LoggedIn:%d\n",
               users[i].user_id,
               users[i].username,
               users[i].role_id,
               users[i].is_active,
               users[i].is_logged_in);
    }
}

void printAllAccounts(const Account accounts[], int count, const User users[], int userCount) {
    printf("\nAccounts\n");
    printf("----------------------------------------\n");
    for (int i = 0; i < count; i++) {
        const char *ownerName = "Unknown";
        for (int j = 0; j < userCount; j++) {
            if (users[j].user_id == accounts[i].user_id) {
                ownerName = users[j].username;
                break;
            }
        }
        printf("Account:%d Owner:%s Balance:%.2f Status:%d\n",
               accounts[i].account_id,
               ownerName,
               accounts[i].balance,
               accounts[i].status);
    }
}

/* Loan application persistence and helpers */

int saveLoanApplications(const LoanApplication loans[], int count, const char *fileName) {
    return saveRecordsToFile(loans, sizeof(LoanApplication), count,
                             MAX_LOAN_APPLICATIONS, fileName);
}

int loadLoanApplications(LoanApplication loans[], int *count, const char *fileName) {
    return loadRecordsFromFile(loans, sizeof(LoanApplication), count,
                               MAX_LOAN_APPLICATIONS, fileName);
}

int addLoanApplication(LoanApplication loans[], int *count, int accountId, double amount, const char *purpose, int assignedEmployeeId) {
    if (*count >= MAX_LOAN_APPLICATIONS) {
        return 0;
    }
    LoanApplication la = {0};
    la.loan_id = *count + 1;
    la.account_id = accountId;
    la.amount = amount;
    snprintf(la.purpose, sizeof(la.purpose), "%s", purpose);
    snprintf(la.status, sizeof(la.status), "PENDING");
    la.assigned_employee_id = assignedEmployeeId;
    time_t now = time(NULL);
    struct tm *tmInfo = localtime(&now);
    strftime(la.timestamp, sizeof(la.timestamp), "%Y-%m-%d %H:%M:%S", tmInfo);

    loans[*count] = la;
    (*count)++;
    return 1;
}

void printLoanApplications(const LoanApplication loans[], int count, int assignedEmployeeId) {
    printf("\nLoan Applications\n");
    printf("---------------------------------------------------\n");
    printf("ID  Account  Amount    Status     Assigned\n");
    printf("---------------------------------------------------\n");
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (assignedEmployeeId == 0 || loans[i].assigned_employee_id == assignedEmployeeId) {
            printf("%2d  %7d  %-8.2f %-10s %d\n",
                   loans[i].loan_id,
                   loans[i].account_id,
                   loans[i].amount,
                   loans[i].status,
                   loans[i].assigned_employee_id);
            found = 1;
        }
    }
    if (!found) {
        printf("No loan applications found.\n");
    }
}
