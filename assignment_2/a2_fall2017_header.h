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

// #1 HASH CONSTANTS FOR COMMANDS
#define EXIT_HASH 6385204799
#define INIT_HASH 6385337657
#define RESERVE_HASH 229481161821089
#define STATUS_HASH 6954030894409

// #2 LIMIT CONSTANTS
#define MAX_RESERVATION_NAME 50
#define MAX_TABLE_SIZE 20
#define SECTION_1_LO 100
#define SECTION_1_UP 109
#define SECTION_2_LO 200
#define SECTION_2_UP 209

// #3 NAME CONSTANTS
#define MEMORY_NAME "/abraty"
#define SEM_READ_COUNT_NAME "/abraty_sem_read"
#define SEM_RESOURCE_NAME "/abraty_sem_resource"
#define FREE_SPOT "Unreserved."

// #4 ERROR MESSAGE CONSTANTS
#define ERROR_CLOSE "Could not close file descriptor.\n"
#define ERROR_CLOSE_SEM "Could not close active semaphore.\n"
#define ERROR_FTRUNCANTE "Could not perform ftruncate.\n"
#define ERROR_MMAP "Could not perform mmap.\n"
#define ERROR_OPEN_SEMAPHORE "Could not open semaphore.\n"
#define ERROR_READING "Could not perform read operation.\n"
#define ERROR_SEM_SIGNAL "Could not send signal from semaphore.\n"
#define ERROR_SEM_WAIT "Could not enter critical section.\n"
#define ERROR_SHARE_MEMORY "Could not initialize shared memory. Exiting...\n"
#define ERROR_SIGNAL_BINDING "Could not bind signal. Exiting...\n"
#define ERROR_SMH_OPEN "Could not perform smh_open.\n"
#define ERROR_WRONG_COMMAND "You have entered an invalid command. Supported commands: \n"
#define ERROR_MAX_NAME_LIMIT "Reservee's name is too long.\n"
#define ERROR_SECTION_NUMBER "Invalid section number.\n"
#define ERROR_TABLE_NUMBER "Invalid table number.\n"
#define ERROR_TABLE_NOT_AVAILABLE "Specified table is not available.\n"
#define ERROR_TABLES_NOT_AVAILABLE "No tables are currently available.\n"

// #5 DEBUG STATEMENTS
#define CALLBACK DEBUG_PREFIX "Executing callback...\n"
#define CREATION_OR_READ_SUCCESS DEBUG_PREFIX "Sucessfully created or opened shared memory.\n"
#define CS_ATTEMPT DEBUG_PREFIX "Attempting to enter critical section...\n"
#define CS_ENTERED DEBUG_PREFIX "Entered critical section.\n"
#define CS_SIGNAL DEBUG_PREFIX "Exiting critical section & signaling departure from critical section...\n"
#define CS_UNLINK DEBUG_PREFIX "Unlinked semaphore.\n"
#define DEBUG_PREFIX "DEBUG: "
#define OPENING_EXISTING_MEMORY DEBUG_PREFIX "Memory already created, allocating in O_RWRD mode...\n"
#define READER_FLAG DEBUG_PREFIX "Reader.\n"
#define SEMAPHORE_CLOSED DEBUG_PREFIX "Semaphore closed.\n"
#define TERMINATE_ATTEMPT DEBUG_PREFIX "Attempting termination...\n"
#define TERMINATE_SUCCESS DEBUG_PREFIX "Succesfully terminating...\n"
#define WRITER_FLAG DEBUG_PREFIX "Writer.\n"
#define SIGNAL_CAPTURED DEBUG_PREFIX "SIGINT has been captured.\n"

// #6 FUNCTION PROTOTYPES
int execute_command(char *args[], int args_length);
int getcmd(char *prompt, char *args[]);
int initialize_semaphore(char *sem_name, int value, sem_t **semaphore, void (*callback)(sem_t **));
int initialize_tables();
int make_reservation(char *name, char *section, char *table);
int read_all();
int reader(int (*read_operation)());
int sem_close_wrapper(sem_t **semaphore);
int sem_post_wrapper(sem_t **semaphore);
int sem_unlink_wrapper(char *semaphore);
int sem_wait_wrapper(sem_t **semaphore);
int writer(int (*write_operation)());
int validate_reservation_format(char *name, char *section, char *table);
unsigned long hash(unsigned char *str);
void end_program();
void signal_handler(int sig);
int table_no_to_index(char *table, char *section);
int validate_section(char *section, int section_no);
int validate_table_number(int section, int table_no);
int first_available_from(int table_offset);
int wipe();

// #7 MISC
#define LAMBDA(c_) ({ c_ _; })
#define LEN(arr) ((int)(sizeof(arr) / sizeof(arr)[0]))

// SHARED MEMORY STRUCT FOR HOLDING TABLES
typedef struct
{
    char reservations[MAX_TABLE_SIZE][MAX_RESERVATION_NAME];
    int sem_count;
    int reader_sem_count;
} Tables;

int MEMORY_SIZE = 20 * sizeof(Tables);
