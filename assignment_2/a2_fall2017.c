#include "a2_fall2017_header.h"

#pragma region GLOBAL VARIABLES

#define DEBUG // toggle debug statements

Tables *global_tables;
sem_t *sem_resource_access;  // access semaphore
sem_t *sem_read_count;       // semaphore for the read count
int critical_section_writer; // checks if current process is currently ahold of the critical section
                             // we use this to handle interruptions
int critical_section_reader;

#ifdef DEBUG
#define D
#else
#define D for (; 0;)
#endif

#pragma endregion

// QUESTIONS
// 1) Does it have to handle cases where critical section never let go?

// TODO: Maybe add function to unlink all semaphores if timeout?

int main(void)
{
    char *args[20];

    if (signal(SIGINT, signal_handler) == SIG_ERR)
    {
        perror(ERROR_SIGNAL_BINDING);
        return EXIT_FAILURE;
    }

    // TODO: Setup signal handler to unlink shared memory and decrement semaphores whatnot
    if (!initialize_tables())
    {
        perror(ERROR_SHARE_MEMORY);

        sem_close_wrapper(&sem_read_count);
        sem_close_wrapper(&sem_resource_access);

        return EXIT_FAILURE;
    }

    D printf(CREATION_OR_READ_SUCCESS);

    while (1)
    {
        int cnt = getcmd("\n>> ", args);
        if (execute_command(args, cnt) == 0)
        {
            break;
        }
    }

    end_program();
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

    D printf("%s = %lu\n", args[0], argument);

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

    if (writer(LAMBDA(int _() {
            // attach memory
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
        })) == 0)
    {
        return 0;
    }

    return 1;
}

int make_reservation(char *table_no, char *name)
{
    // TODO: Add error checking for table > table size
    int i = atoi(table_no);
    strcpy(global_tables->reservations[i], name);
    // memcpy(global_tables, global_tables, sizeof(global_tables));

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

    D printf("DEBUG: SEMAPHORE COUNT: %i\n", global_tables->sem_count);

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
        D printf(CALLBACK);

        callback(semaphore);
    }
}

int sem_wait_wrapper(sem_t **semaphore)
{
    D printf(CS_ATTEMPT);

    if (sem_wait(*semaphore) == -1)
    {
        perror(ERROR_SEM_WAIT);
        return 0;
    }

    D printf(CS_ENTERED);

    return 1;
}

int sem_post_wrapper(sem_t **semaphore)
{
    if (sem_post(*semaphore) == -1)
    {
        perror(ERROR_SEM_SIGNAL);
        return 0;
    }

    D printf(CS_SIGNAL);

    return 1;
}

int sem_unlink_wrapper(char *semaphore)
{
    if (sem_unlink(semaphore) == -1)
    {
        perror(ERROR_SEM_SIGNAL);
        return 0;
    }

    D printf(CS_UNLINK);

    return 1;
}

int sem_close_wrapper(sem_t **semaphore)
{
    if (sem_close(*semaphore) == -1)
    {
        perror(ERROR_CLOSE_SEM);
        return 0;
    }

    D printf(SEMAPHORE_CLOSED);

    return 1;
}

void end_program()
{
    // TODO: Test safety of this

    D printf(TERMINATE_ATTEMPT);

    // TODO: Avoid deadlock caused by writing to shared memory
    writer(LAMBDA(int _() {
        global_tables->sem_count = global_tables->sem_count - 2;
        // odds are the next call is never gonna fail
        // so we can decrement the count by 2, because otherwise if we close the resource access semaphore then I don't
        // know how we could safely write to it other than creating another shared memory object for the semaphore count and adding a specific
        // semaphore just to handle this very unlikely edge case
        return 1;
    }));

    sem_close_wrapper(&sem_read_count);
    sem_close_wrapper(&sem_resource_access);

    if (global_tables->sem_count <= 0)
    {
        // shm_unlink(MEMORY_NAME); // not sure if we need to persist the memory forever or if we remove it every time
        sem_unlink_wrapper(SEM_READ_COUNT_NAME);
        sem_unlink_wrapper(SEM_RESOURCE_NAME);
    }

    munmap(global_tables, MEMORY_SIZE);

    D printf(TERMINATE_SUCCESS);
}

#pragma endregion

#pragma region READER AND WRITER FUNCTIONS REGION

int reader(int (*read_operation)())
{
    int ret_value = 1; // specify this value so that we don't preemtively exit critical section

    D printf(READER_FLAG);

    if (sem_wait_wrapper(&sem_read_count) == 0)
    {
        ret_value = 0;
        return ret_value;
    }

    critical_section_reader = 1;

    global_tables->reader_sem_count++;

    if (global_tables->reader_sem_count == 1)
    {
        if (sem_wait_wrapper(&sem_resource_access) == 0)
        {
            ret_value = 0;
            return ret_value;
        }
    }

    critical_section_reader = 0;
    if (sem_post_wrapper(&sem_read_count) == 0)
    {
        ret_value = 0;
        return ret_value;
    }

    if (read_operation() == 0)
    {
        ret_value = 0;
    }

    D sleep(5);

    if (sem_wait_wrapper(&sem_read_count) == 0)
    {
        ret_value = 0;
        return ret_value;
    }
    critical_section_reader = 1;

    global_tables->reader_sem_count--;

    if (global_tables->reader_sem_count == 0)
    {
        if (sem_post_wrapper(&sem_resource_access) == 0)
        {
            ret_value = 0;
            return ret_value;
        }
    }

    critical_section_reader = 0;
    if (sem_post_wrapper(&sem_read_count) == 0)
    {
        ret_value = 0;
    }

    return ret_value;
}

int writer(int (*write_operation)())
{

    int ret_value = 1;

    D printf(WRITER_FLAG);

    if (sem_wait_wrapper(&sem_resource_access) == 0)
    {
        ret_value = 0;
        return ret_value;
    }
    critical_section_writer = 1;

    // critical write operation
    if (write_operation() == 0)
    {
        ret_value = 0;
    }

    D sleep(5);

    critical_section_writer = 0;
    if (sem_post_wrapper(&sem_resource_access) == 0)
    {
        ret_value = 0;
    }

    return ret_value;
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

#pragma SIGNAL HANDLER REGION

void signal_handler(int sig)
{

    D printf(SIGNAL_CAPTURED);

    if (critical_section_writer)
    {
        // to prevent deadlock, we need to release the current process from critical section
        sem_post_wrapper(&sem_resource_access);
    }

    if (critical_section_reader)
    {
        // to prevent race conditions, we release the current process from reader's section
        sem_post_wrapper(&sem_read_count);

        // we decrement the number of readers since we interrupted before it was decremented
        writer(LAMBDA(int _() {
            global_tables->reader_sem_count--;
            return 1;
        }));
    }

    end_program();
    exit(EXIT_SUCCESS);
}