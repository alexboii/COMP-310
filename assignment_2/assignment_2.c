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
    int sem_count;
    int reader_sem_count;
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
#define SEM_RESOURCE_NAME "/abraty_sem_resource"
#define SEM_READ_COUNT_NAME "/abraty_sem_read"

// #4 ERROR MESSAGE CONSTANTS
#define ERROR_SMH_OPEN "Could not perform smh_open.\n"
#define ERROR_MMAP "Could not perform mmap.\n"
#define ERROR_FTRUNCANTE "Could not perform ftruncate.\n"
#define ERROR_CLOSE "Could not close file descriptor.\n"
#define ERROR_OPEN_SEMAPHORE "Could not open semaphore.\n"
#define ERROR_WRONG_COMMAND "You have entered an invalid command. Supported commands: \n"
#define ERROR_SEM_SIGNAL "Could not send signal from semaphore.\n"
#define ERROR_SEM_WAIT "Could not enter critical section.\n"
#define ERROR_SHARE_MEMORY "Could not initialize shared memory. Exiting...\n"
#define ERROR_READING "Could not perform read operation.\n"

// #5 DEBUG STATEMENTS
#define DEBUG_PREFIX "DEBUG: "
#define CREATION_OR_READ_SUCCESS DEBUG_PREFIX "Sucessfully created or opened shared memory.\n"
#define OPENING_EXISTING_MEMORY DEBUG_PREFIX "Memory already created, allocating in O_RWRD mode...\n"
#define CS_ATTEMPT DEBUG_PREFIX "Attempting to enter critical section...\n"
#define CS_ENTERED DEBUG_PREFIX "Entered critical section.\n"
#define CS_SIGNAL DEBUG_PREFIX "Exiting critical section & signaling departure from critical section...\n"
#define CALLBACK DEBUG_PREFIX "Executing callback...\n"
#define WRITER_FLAG DEBUG_PREFIX "Writer.\n"
#define READER_FLAG DEBUG_PREFIX "Reader.\n"

// #6 FUNCTION PROTOTYPES
int getcmd(char *prompt, char *args[]);
int execute_command(char *args[], int args_length);
unsigned long hash(unsigned char *str);
int initialize_tables();
int make_reservation(char *table, char *name);
int read_all();
int initialize_semaphore(char *sem_name, int value, sem_t **semaphore, void (*callback)(sem_t **));
int sem_wait_wrapper(sem_t **semaphore);
int sem_post_wrapper(sem_t **semaphore);
int reader(int (*read_operation)());
int writer(int (*write_operation)());

// #7 GLOBAL VARIABLES
Tables *global_tables;
int DEBUG = 1;              // toggle debug statements
sem_t *sem_resource_access; // access semaphore
sem_t *sem_read_count;      // semaphore for the read counter

// #9 MISC
#define LEN(arr) ((int)(sizeof(arr) / sizeof(arr)[0]))
#define LAMBDA(c_) ({ c_ _; })

#pragma endregion

// QUESTIONS
// 1) Does it have to handle cases where critical section never let go?

int main(void)
{
    char *args[20];

    // TODO: Setup signal handler to unlink shared memory and decrement semaphores whatnot

    if (!initialize_tables())
    {
        perror(ERROR_SHARE_MEMORY);
        return EXIT_FAILURE;
    }
    else
    {
        if (DEBUG)
        {
            printf(CREATION_OR_READ_SUCCESS);
        }
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

    if (argument == EXIT_HASH)
    {
        return 0;
    }

    if (argument != RESERVE_HASH && argument != STATUS_HASH && argument != INIT_HASH)
    {
        perror(ERROR_WRONG_COMMAND);
        return -1;
    }

    switch (argument)
    {
    case RESERVE_HASH:
        // TODO: Create validate reservation command
        writer(LAMBDA(int _() { return make_reservation(args[1], args[2]); }));
        break;
    case STATUS_HASH:
        reader(LAMBDA(int _() { return read_all(); }));
        break;
    default:
        return -1;
    }

    return 1;
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

    // this function doesn't work on some machines, so have to change it
    // // open semaphore that contains the number of processes using the shared memory
    // initialize_semaphore(SEM_COUNT_NAME, 0, &sem_count, LAMBDA(void _(sem_t * *sem) {
    //                          // signal that a new process has been created
    //                          sem_post(*sem);
    //                      }));

    // attach current semaphore
    initialize_semaphore(SEM_RESOURCE_NAME, 1, &sem_resource_access, NULL);

    // attach reader count semaphore
    initialize_semaphore(SEM_READ_COUNT_NAME, 1, &sem_read_count, NULL);

    // TODO: Add resource control in here, maybe call reader?

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

    global_tables->sem_count = global_tables->sem_count + 2; // since we have a semaphore the readcounter and an access resource semaphore

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

    return 1;
}

int initialize_semaphore(char *sem_name, int value, sem_t **semaphore, void (*callback)(sem_t **))
{
    *semaphore = sem_open(sem_name, O_RDWR | O_CREAT, S_IRWXU, value);

    if (*semaphore == SEM_FAILED)
    {
        perror(ERROR_OPEN_SEMAPHORE);
        return 0;
    }

    if (callback != NULL)
    {
        if (DEBUG)
        {
            printf(CALLBACK);
        }

        callback(semaphore);
    }
}

int sem_wait_wrapper(sem_t **semaphore)
{
    if (DEBUG)
    {
        printf(CS_ATTEMPT);
    }

    if (sem_wait(*semaphore) == -1)
    {
        perror(ERROR_SEM_WAIT);
        return 0;
    }

    if (DEBUG)
    {
        printf(CS_ENTERED);
    }

    return 1;
}

int sem_post_wrapper(sem_t **semaphore)
{
    if (sem_post(*semaphore) == -1)
    {
        perror(ERROR_SEM_SIGNAL);
        return 0;
    }

    if (DEBUG)
    {
        printf(CS_SIGNAL);
    }

    return 1;
}

#pragma endregion

#pragma region READER AND WRITER FUNCTIONS

int reader(int (*read_operation)())
{

    if (DEBUG)
    {
        printf(READER_FLAG);
    }

    if (sem_wait_wrapper(&sem_read_count) == 0)
    {
        return 0;
    }

    global_tables->reader_sem_count++;

    if (global_tables->reader_sem_count == 1)
    {
        if (sem_wait_wrapper(&sem_resource_access) == 0)
        {
            return 0;
        }
    }

    if (sem_post_wrapper(&sem_read_count) == 0)
    {
        return 0;
    }

    read_operation();

    if (DEBUG)
    {
        sleep(5);
    }

    if (sem_wait_wrapper(&sem_read_count) == 0)
    {
        return 0;
    }

    global_tables->reader_sem_count--;

    if (global_tables->reader_sem_count == 0)
    {
        if (sem_post_wrapper(&sem_resource_access) == 0)
        {
            return 0;
        }
    }

    if (sem_post_wrapper(&sem_read_count) == 0)
    {
        return 0;
    }

    return 1;
}

int writer(int (*write_operation)())
{
    if (DEBUG)
    {
        printf(WRITER_FLAG);
    }

    if (sem_wait_wrapper(&sem_resource_access) == 0)
    {
        return 0;
    }

    // critical write operation
    write_operation();

    if (DEBUG)
    {
        sleep(5);
    }

    if (sem_post_wrapper(&sem_resource_access) == 0)
    {
        return 0;
    }

    return 1;
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