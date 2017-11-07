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
// TODO: Read writer-reader's problem 2.5.2
// TODO: Comment code
// TODO: Test
// TODO: Test without debug
// TODO: Test two files at the same time
// TODO: Create edge case files
// TODO: Remove all crappy code

int main(int argc, char *argv[])
{
    char args[MAX_ARGS][MAX_ARG_SIZE];
    char line[LINE_SIZE];
    int cnt;

    if (signal(SIGINT, signal_handler) == SIG_ERR || signal(SIGTSTP, SIG_IGN) == SIG_ERR)
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

    //intialising random number generator
    time_t now;
    srand((unsigned int)(time(&now)));

    D printf(CREATION_OR_READ_SUCCESS);

    if (argc == 2)
    {
        FILE *fp;

        if ((fp = fopen(argv[1], "r")) == NULL)
        {
            perror(ERROR_WRONG_FILE);
        }
        else
        {
            while (fgets(line, LINE_SIZE, fp) != NULL)
            {
                cnt = getcmd(line, args);

                if (cnt == 0)
                {
                    continue;
                }

                if (execute_command(args, cnt) == 0)
                {
                    break;
                }

                memset(line, 0, sizeof(line));
                memset(args, 0, sizeof(args));
            }

            fclose(fp);
        }
    }

    while (1 && argc == 1)
    {
        printf(SHELL_TICKER);
        fgets(line, LINE_SIZE, stdin);
        cnt = getcmd(line, args);

        // D printf("table_no_og = %s\nnew_table = %i\n", args[0], table_no_to_index(args[0], args[1]));

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
int execute_command(char args[MAX_ARGS][MAX_ARG_SIZE], int args_length)
{
    unsigned long argument = hash(args[0]);

    D printf("%s = %lu\n", args[0], argument);

    if (argument == EXIT_HASH)
    {
        return 0;
    }

    if (argument != RESERVE_HASH && argument != STATUS_HASH && argument != INIT_HASH)
    {
        printf(ERROR_WRONG_COMMAND);
        return -1;
    }

    switch (argument)
    {
    case RESERVE_HASH:
        // TODO: Create validate reservation command
        if (validate_reservation_format(args[1], args[2], args[3]))
        {
            writer(LAMBDA(int _() { return make_reservation(args[1], args[2], args[3]); }));
        }
        break;
    case INIT_HASH:
        writer(LAMBDA(int _() { return wipe(); }));
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

            if (ftruncate(fd, sizeof(Tables)) == -1)
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

int make_reservation(char *name, char *section, char *table)
{
    int i = table_no_to_index(table, section);

    // TODO: Add error checking for table > table size
    if (strlen(table) != 0)
    {
        if (strlen(global_tables->reservations[i]) != 0)
        {
            printf(ERROR_TABLE_NOT_AVAILABLE);
        }
        strcpy(global_tables->reservations[i], name);

        return 1;
    }

    int new_index;

    if ((new_index = first_available_from(i)) == -1)
    {
        printf(ERROR_TABLES_NOT_AVAILABLE);
        return 0;
    }

    strcpy(global_tables->reservations[new_index], name);

    return 1;
}

int first_available_from(int table_offset)
{
    for (int i = 0; i < MAX_TABLE_SIZE; i++)
    {
        int index = (i + table_offset) % MAX_TABLE_SIZE;

        if (strlen(global_tables->reservations[index]) == 0)
        {
            return index;
        }
    }

    return -1;
}

int wipe()
{
    memset(global_tables->reservations, 0, sizeof(global_tables->reservations));
    return 1;
}

int read_all()
{
    for (int i = 0; i < MAX_TABLE_SIZE; i++)
    {
        int offset = (i < 10) ? 100 : 200;

        printf("TABLE %d : %s\n", offset + (i % 10), (strlen(global_tables->reservations[i]) == 0) ? FREE_SPOT : global_tables->reservations[i]);
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
    D printf("DEBUG: READERS COUNT BEFORE = %i\n", global_tables->reader_sem_count);

    if (global_tables->reader_sem_count == 1)
    {
        if (sem_wait_wrapper(&sem_resource_access) == 0)
        {
            ret_value = 0;
            return ret_value;
        }
    }

    if (sem_post_wrapper(&sem_read_count) == 0)
    {
        ret_value = 0;
        return ret_value;
    }

    if (read_operation() == 0)
    {
        ret_value = 0;
    }

    sleep(rand() % 10);

    if (sem_wait_wrapper(&sem_read_count) == 0)
    {
        ret_value = 0;
        return ret_value;
    }

    global_tables->reader_sem_count--;

    D printf("DEBUG: READERS COUNT AFTER = %i\n", global_tables->reader_sem_count);

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

    sleep(rand() % 10);

    critical_section_writer = 0;
    if (sem_post_wrapper(&sem_resource_access) == 0)
    {
        ret_value = 0;
    }

    return ret_value;
}

#pragma endregion

#pragma region UTILITY FUNCTIONS REGION

int table_no_to_index(char *table, char *section)
{
    int table_int = 0;
    if (strlen(table) != 0)
    {
        table_int = atoi(table);
    }
    int section_int = get_section_no(section);
    int index = table_int % 10;

    return (section_int == 2) ? 10 + index : index;
}

int validate_reservation_format(char *name, char *section, char *table)
{

    // TODO: Change != NULL validations to strlen()
    if (strlen(name) > MAX_TABLE_SIZE)
    {
        printf(ERROR_MAX_NAME_LIMIT);
        return 0;
    }

    int section_no = get_section_no(section);

    if (!validate_section(section, section_no))
    {
        printf(ERROR_SECTION_NUMBER);
        return 0;
    }

    int table_no = atoi(table);

    if (strlen(table) != 0 & !validate_table_number(section_no, table_no))
    {
        printf(ERROR_TABLE_NUMBER);
        return 0;
    }

    return 1;
}

int validate_section(char *section, int section_no)
{
    return section_no == 1 || section_no == 2;
}

int validate_table_number(int section, int table_no)
{
    return (section == 1 && (SECTION_1_LO <= table_no && table_no <= SECTION_1_UP)) || (section == 2 && (SECTION_2_LO <= table_no && table_no <= SECTION_2_UP));
}

int get_section_no(char *section)
{
    if (strcmp(section, "A") == 0)
    {
        return 1;
    }

    if (strcmp(section, "B") == 0)
    {
        return 2;
    }

    return -1;
}

/** 
 * @brief Function provided by Prof. Harmouche
 * @note   
 * @param  *prompt: 
 * @param  *args[]: 
 * @param  *background: 
 * @retval Number of arguments
  */
int getcmd(char line[LINE_SIZE], char args[MAX_ARGS][MAX_ARG_SIZE])
{
    int count = 0;
    char *token;
    char *delimiters = " \t\n\r\v";

    // split line buffer into separate arguments in array of strings
    token = strtok(line, delimiters);
    while (token != NULL && count < MAX_ARGS)
    {
        strncpy(args[count++], token, strlen(token) + 1);
        token = strtok(NULL, delimiters);
    }

    return count;
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
        sem_post_wrapper(&sem_resource_access);

        // we decrement the number of readers since we interrupted before it was decremented
        writer(LAMBDA(int _() {
            global_tables->reader_sem_count--;
            return 1;
        }));
    }

    end_program();
    exit(EXIT_SUCCESS);
}