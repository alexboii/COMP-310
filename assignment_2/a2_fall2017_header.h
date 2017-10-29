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
#define ERROR_CLOSE_SEM "Could not close active semaphore.\n"

// #5 DEBUG STATEMENTS
#define DEBUG_PREFIX "DEBUG: "
#define CREATION_OR_READ_SUCCESS DEBUG_PREFIX "Sucessfully created or opened shared memory.\n"
#define OPENING_EXISTING_MEMORY DEBUG_PREFIX "Memory already created, allocating in O_RWRD mode...\n"
#define CS_ATTEMPT DEBUG_PREFIX "Attempting to enter critical section...\n"
#define CS_ENTERED DEBUG_PREFIX "Entered critical section.\n"
#define CS_SIGNAL DEBUG_PREFIX "Exiting critical section & signaling departure from critical section...\n"
#define CS_UNLINK DEBUG_PREFIX "Unlinked semaphore.\n"
#define CALLBACK DEBUG_PREFIX "Executing callback...\n"
#define WRITER_FLAG DEBUG_PREFIX "Writer.\n"
#define READER_FLAG DEBUG_PREFIX "Reader.\n"
#define TERMINATE_ATTEMPT DEBUG_PREFIX "Attempting termination...\n"
#define TERMINATE_SUCCESS DEBUG_PREFIX "Succesfully terminating...\n"

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
int sem_unlink_wrapper(char *semaphore);
int reader(int (*read_operation)());
int writer(int (*write_operation)());
void end_program();

// #7 MISC
#define LEN(arr) ((int)(sizeof(arr) / sizeof(arr)[0]))
#define LAMBDA(c_) ({ c_ _; })