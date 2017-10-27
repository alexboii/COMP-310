#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>

#pragma region CONSTANTS AND GLOBAL DEFINTION

// SHARED MEMORY STRUCT FOR HOLDING TABLES
typedef struct
{
    char reservations[20][30];
} Tables;

// #1 HASH CONSTANTS FOR COMMANDS
#define INIT_HASH 6385337657
#define EXIT_HASH 6385204799
#define STATUS_HASH 6954030894409
#define RESERVE_HASH 229481161821089

// #2 LIMIT CONSTANTS
#define MAX_TABLE_SIZE 20
#define MAX_RESERVATION_NAME 2500
int MEMORY_SIZE = 20 * sizeof(Tables);

// #3 NAME CONSTANTS
#define MEMORY_NAME "/abraty"
#define SEM_NAME "/abraty_sem"
#define SEM_COUNT_NAME "/abraty_sem_count"

// #4 ERROR MESSAGE CONSTANTS
#define ERROR_SMH_OPEN "Could not perform smh_open.\n"
#define ERROR_MMAP "Could not perform mmap.\n"
#define ERROR_FTRUNCANTE "Could not perform ftruncate.\n"
#define ERROR_CLOSE "Could not close file descriptor.\n"
#define ERROR_OPEN_SEMAPHORE "Could not open semaphore.\n"

// #5 DEBUG STATEMENTS
#define CREATION_OR_READ_SUCCESS "Sucessfully created or opened shared memory.\n"
#define OPENING_EXISTING_MEMORY "Memory already created, allocating in O_RWRD mode...\n"

// #6 FUNCTION PROTOTYPES
int getcmd(char *prompt, char *args[]);
int execute_command(char *args[], int args_length);
unsigned long hash(unsigned char *str);
int initialize_tables();
int make_reservation(char *table, char *name);
int read_all();

// #7 GLOBAL VARIABLES
Tables *global_tables;
int DEBUG = 1; // toggle debug statements
sem_t *sem_count;
sem_t *sem_proc;

// #9 MISC
#define LEN(arr) ((int)(sizeof(arr) / sizeof(arr)[0]))

#pragma endregion

// QUESTIONS
// 1) Does it have to handle cases where critical section never let go?

int main(void)
{
    char *args[20];

    if (initialize_tables() != 0 && DEBUG)
    {
        printf(CREATION_OR_READ_SUCCESS);
    }

    while (1)
    {
        int cnt = getcmd("\n>> ", args);
        execute_command(args, cnt);
    }

    return EXIT_SUCCESS;
}

#pragma region PROGRAM FLOW CONTROL REGION
/**  
 * @brief  Execute user's command
 * @note   
 * @param  *args[]: 
 * @param  args_length: 
 * @retval 
 */
int execute_command(char *args[], int args_length)
{
    unsigned long argument = hash(args[0]);

    if (DEBUG)
    {
        printf("%s = %lu\n", args[0], argument);
    }

    switch (argument)
    {
    case RESERVE_HASH:
        make_reservation(args[1], args[2]);
        break;
    case STATUS_HASH:
        read_all();
        break;
    default:
        return 0;
        break;
    }
}
#pragma endregion

#pragma region SHARED MEMORY MANIPULATION REGION
/** 
 * @brief  
 * @note   
 * @retval 
 */
int initialize_tables()
{
    int fd = shm_open(MEMORY_NAME, O_CREAT | O_RDWR, S_IRWXU);

    if (fd == -1)
    {
        perror(ERROR_SMH_OPEN);
        return 0;
    }

    sem_count = sem_open(SEM_COUNT_NAME, O_RDWR | O_CREAT, S_IRWXU, 1);

    if (sem_count == SEM_FAILED)
    {
        perror(ERROR_OPEN_SEMAPHORE);
        return 0;
    }

    // signal that a new process has been created
    sem_post(sem_count);

    // attach current semaphore
    sem_proc = sem_open(SEM_COUNT_NAME, O_RDWR | O_CREAT, S_IRWXU, 1);

    if (sem_proc == SEM_FAILED)
    {
        perror(ERROR_OPEN_SEMAPHORE);
        return 0;
    }

    sem_wait(sem_proc);

    global_tables = mmap(0, MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (global_tables == MAP_FAILED)
    {
        perror(ERROR_MMAP);
        return 0;
    }

    if (ftruncate(fd, MEMORY_SIZE) == -1)
    {
        perror(ERROR_FTRUNCANTE);
        return 0;
    }

    if (close(fd) == -1)
    {
        perror(ERROR_CLOSE);
        return 0;
    }

    sem_post(sem_proc);

    return 1;
}

int make_reservation(char *table_no, char *name)
{
    // TODO: Add error checking for table > table size
    int i = atoi(table_no);
    strcpy(global_tables->reservations[i], name);
    memcpy(global_tables, global_tables, sizeof(global_tables));

    return 1;
}

int read_all()
{
    for (int i = 0; i < LEN(global_tables->reservations); i++)
    {
        int len = strlen(global_tables->reservations[i]);

        if (len != 0)
        {
            printf("TABLE %d RESERVED FOR USER %s\n", i, global_tables->reservations[i]);
        }
    }
}

#pragma endregion

#pragma region UTILITY FUNCTIONS REGION
/** 
 * @brief Function provided by Prof. Harmouche
 * @note   
 * @param  *prompt: 
 * @param  *args[]: 
 * @param  *background: 
 * @retval Number of arguments
  */
int getcmd(char *prompt, char *args[])
{
    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;

    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);

    if (length <= 0)
    {
        exit(-1);
    }

    while ((token = strsep(&line, " \t\n")) != NULL)
    {
        for (int j = 0; j < strlen(token); j++)
        {
            if (token[j] <= 32)
            {
                token[j] = '\0';
            }
        }
        if (strlen(token) > 0)
        {
            args[i++] = token;
        }
    }

    free(token);
    free(line);

    return i;
}

/** 
 * @brief  Canonical DJB2 hash function
 * @note   Taken from http://www.cse.yorku.ca/~oz/hash.html
 * @param  *str: 
 * @retval Hash for commands
 */
unsigned long hash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

#pragma endregion