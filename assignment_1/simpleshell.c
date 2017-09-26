#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
// QUESTIONS
// 1 - What's considered "other built-in features" for 20% in the grading scheme?
// 2 - Do we have to handle stopped jobs, i.e. jobs that we can resume again afterwards?
// 3 - If I have two jobs running in the background, are both supposed to terminate when I click ctrl + c?
// 4 - If one job is on fg, and the other job is on bg, and we click ctrl + c -- are both jobs going to be suspended?
// 5 - Do we exit shell with ctrl + c? What happens with ctrl + c exactly?

// PRIORITIES
// 1 - Setup output redirection
// 2 - Memory leaks
// 3 - Go through todos
// 4 - Comment code
// 6 - Store foreground in global variable instead of searching for foreground in list of jobs

// PROMPT CONSTANTS
#define CURRENT_DIRECTORY "Current directory: '%s'"
#define ERROR_CD_INVALID_DIRECTORY "Invalid directory."
#define ERROR_CHILD_PROCESS "Error in child process during command execution."
#define ERROR_CWD_INVALID "Invalid current working directory."
#define ERROR_DEFAULT "An error has occurred, exiting shell..."
#define ERROR_FG "Error while bringing process to foreground."
#define ERROR_FORKING "Forking error."
#define ERROR_JOB_CREATION "Error creationg a new job."
#define ERROR_JOB_EXECUTION "Error during job execution."
#define ERROR_NO_ARGS "You haven't entered a command. Please try again."
#define ERROR_SIGNAL_BINDING "Could not bind signal handler for child process. Aborting..."
#define ERROR_WAITPID "Error while awaiting background process."
#define EXIT_MESSAGE "Exiting...\n"
#define JOBS_LIST "Background jobs\n -----------------------\n"
#define PRESENTATION "Alexander Bratyshkin --- 260684228 --- Assignment 1 --- ECSE 427"

// HASH CONSTANTS FOR COMMANDS
#define CD_HASH 5863276
#define EXIT_HASH 2090237503
#define FG_HASH 5863378
#define JOBS_HASH 2090407155
#define LS_HASH 5863588

// MISC CONSTANTS
#define MAX_JOBS 20 // Max number of jobs to be displayed at any point in time.

// ENUM FOR STATUS OF JOB
enum status
{
    RUNNING,
    FINISHED,
    STOPPED,
    FOREGROUND
} status;

// STRUCT DEFINITION FOR JOBS
typedef struct job
{
    struct job *next;
    pid_t process_id;
    char *command;
    enum status status;
} job;

// FUNCTION PROTOTYPES
const char *get_status(enum status status);
int execute_built_in(char *args[], job *head_job);
int execute_cd(char *path);
int execute_fg(char *job_id_string, job *head_job);
int execute_fork(char *args[]);
int execute_non_fork(char *args[]);
int execute_pwd();
int getcmd(char *prompt, char *args[], int *background);
int jobs_length(job *head);
job *add_job(struct job *head, pid_t pid, char *command, enum status status);
job *change_job_status(job *head_job, pid_t pid, enum status status);
job *create_job(char *command, int pid, job *next, enum status status);
unsigned long hash(unsigned char *str);
void execute_job(char *args[], int bg, job **head_job);
void print_jobs(job *job);
void prompt_message(char *message, ...);
void signal_child_handler();
void signal_int_handler(int sig);
job *retrieve_foreground_job(job *head);
job *delete_job_by_pid(pid_t pid, job *head_job);
job *retrieve_job_by_pid(job *head, pid_t pid);

// GLOBAL VARIABLE
struct job *head_job; // sorry for having a global variable, but unfortunately it's needed for signal handling

// TODO: Add verbose global constant to toggle debug statements :)

/** 
 * @brief  Assignment 1
 * @note   Alexander Bratyshkin - 260684228
 * @retval 
 */
int main(void)
{
    char *args[20]; // TODO: Extract this constant
    int bg;
    head_job = NULL;
    prompt_message(PRESENTATION);

    if (signal(SIGCHLD, signal_child_handler) == SIG_ERR || signal(SIGINT, signal_int_handler) == SIG_ERR || signal(SIGTSTP, SIG_IGN) == SIG_ERR)
    {
        prompt_message(ERROR_SIGNAL_BINDING);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        // delocalize memory from command line arguments
        bg = 0;
        int cnt = getcmd("\n>> ", args, &bg);

        for (int i = 0; i < cnt; i++)
        {
            int test = hash(args[i]);
            printf("\nArg[%d] = %s, %i", i, args[i], test);
            printf("\n");
        }

        // WHENEVER & IS THERE, DON'T WAITPID

        // error handling for command input
        if (cnt == -1)
        {
            prompt_message(ERROR_DEFAULT);
            exit(1);
        }

        if (cnt == 0)
        {
            prompt_message(ERROR_NO_ARGS);
            continue;
        }

        // TODO: insert an error if > to one of the commands that are not ls, cat, jobs, etc.

        // we do not need to fork for the following commands, so simply execute them

        // TODO: VERY IMPORTANT => CHANGE THIS IN CASE WE ALLOWED TO EXECUTE OUR OWN BUILT INS
        int result_non_fork = execute_non_fork(args);
        if (result_non_fork == 0 || result_non_fork == 1)
        {
            if (result_non_fork == 0)
            {
                exit(1);
            }

            continue;
        }

        // pass the first null job by reference

        if (execute_built_in(args, head_job) == 2)
        {
            execute_job(args, bg, &head_job);
        }

        memset(args, 0, sizeof(args) * sizeof(char *));
    }
}

// FLOW-CONTROL SECTION

/** 
 * @brief  Execute the functions that do not require forking, i.e. "cd" and "exit"
 * @note   
 * @param  *args[]: Arguments entered by user
 * @retval 1 for success, 0 for error, 2 for command not found
 */
int execute_non_fork(char *args[])
{
    int argument = hash(args[0]);
    switch (argument)
    {
    case CD_HASH:
        execute_cd(args[1]);
        return 1;
        break;
    case EXIT_HASH:
        prompt_message(EXIT_MESSAGE);
        return 0;
        break;
    default:
        return 2;
        break;
    }
}

// BUILT-IN COMMANDS SECTION

/** 
 * @brief  Execute the built-in commands that are forkable
 * @note   
 * @param  *args[]: 
 * @retval 
 */
int execute_built_in(char *args[], job *head_job)
{
    int argument = hash(args[0]);

    switch (argument)
    {
    case JOBS_HASH:
        print_jobs(head_job);
        return 1;
        break;
    case FG_HASH:
        if (execute_fg(args[1], head_job) == 0)
        {
            prompt_message(ERROR_FG);
            return 0;
        }
        return 1;
        break;
    default:
        return 2;
        break;
    }
}

/** 
 * @brief  Execute job
 * @note   
 * @param  *jobs: 
 * @param  pid: 
 * @param  *args: 
 * @retval 
 */
void execute_job(char *args[], int bg, job **head_job)
{
    int pid = fork();

    switch (pid)
    {
    case -1:
        prompt_message(ERROR_FORKING);
        break;
    case 0:
        // TODO: to test jobs, put an await signal or something like that so that you see the command running despite &
        // TODO: HOW TO TRACK ZOMBIE PROCESSES?
        if (bg == 1)
        {
            signal(SIGINT, SIG_IGN); // ignore ctrl + c
        }

        sleep(10);
        execvp(args[0], args);
        prompt_message(ERROR_CHILD_PROCESS);
        *head_job = change_job_status(*head_job, pid, FINISHED);
        break;
    default:
        if (bg == 0)
        {
            int status;
            *head_job = add_job(*head_job, pid, args[0], FOREGROUND);
            if (waitpid(pid, &status, WUNTRACED) < 0)
            {
                prompt_message(ERROR_WAITPID);
                exit(EXIT_FAILURE);
            }

            *head_job = delete_job_by_pid(pid, *head_job);
        }
        else
        {
            *head_job = add_job(*head_job, pid, args[0], RUNNING);
            printf("Job with PID %d put in background\n", pid); // TODO: REPLACE
        }
        break;
    }
}

/** 
 * @brief Add job to end of list of jobs
 * @note   
 * @param  *jobs: List of jobs
 * @param  pid: Process id of job
 * @param  *args: Command
 * @retval 
 */
job *add_job(struct job *head, pid_t pid, char *command, enum status status)
{
    job *buffer = head;
    job *new_job;

    // if we're at the beginning of the linked list, create a new first job
    if (buffer == NULL)
    {
        new_job = create_job(command, pid, NULL, status);
        head = new_job;
        return head;
    }

    // TODO: extract this functionality into its own method (for print as well could be useful, also useful for a stop parameter)
    while (buffer->next != NULL)
    {
        buffer = buffer->next;
    }

    new_job = create_job(command, pid, NULL, status);
    buffer->next = new_job;

    return head;
}

job *delete_job_by_pid(pid_t pid, job *head_job)
{
    if (head_job == NULL)
    {
        // if nothing is in the linked list (should never reach this code with current workflow though)
        return NULL;
    }

    if (head_job->process_id == pid)
    {
        job *buffer = head_job->next;
        free(head_job);

        return buffer;
    }

    head_job->next = delete_job_by_pid(pid, head_job->next);

    return head_job;
}

/** 
 * @brief  Creates a new job "job" object 
 * @note   
 * @param  *command: Job's command
 * @param  pid: Job's PID
 * @param  *next: Next job to point to
 * @retval Newly created job
 */
job *create_job(char *command, int pid, job *next, enum status status)
{
    job *new_job = (job *)malloc(sizeof(job));
    if (new_job == NULL)
    {
        printf(ERROR_JOB_CREATION);
    }

    new_job->command = command;
    new_job->process_id = pid;
    new_job->next = next;
    new_job->status = status; // upon the moment of creation, we assume that the job is running

    return new_job;
}

/** 
 * @brief  Prints the whole list of background jobs
 * @note   
 * @param  *head: Head of the list of jobs
 * @retval None
 */
void print_jobs(job *head)
{
    prompt_message(JOBS_LIST);
    job *buffer = head;
    int job_number = 1;
    while (buffer != NULL)
    {
        printf("\n [%i] %d          %s", job_number, buffer->process_id, get_status(buffer->status));
        job_number++;
        buffer = buffer->next;
    }
}

/** 
 * @brief  Returns the length of the linked list of jobs
 * @note   
 * @param  *head: 
 * @retval Length of linked list of jobs
 */
int jobs_length(job *head)
{
    int length = 0;
    job *buffer = head;
    while (buffer != NULL)
    {
        length++;
        buffer = buffer->next;
    }

    printf("\nLength: %i", length);
    return length;
}

job *retrieve_job_at_index(job *head, int index)
{
    int buffer_index = 1;
    job *buffer = head;
    while (buffer != NULL)
    {
        if (buffer_index == index)
        {
            return buffer;
        }

        buffer_index++;
        buffer = buffer->next;
    }

    return NULL;
}

job *retrieve_job_by_pid(job *head, pid_t pid)
{
    job *buffer = head;
    while (buffer != NULL)
    {
        if (buffer->process_id == pid)
        {
            return buffer;
        }

        buffer = buffer->next;
    }

    return NULL;
}

// TODO: Refactor this?
job *retrieve_first_running_job(job *head)
{
    job *buffer = head;
    while (buffer != NULL)
    {
        if (buffer->status == RUNNING)
        {
            return buffer;
        }

        buffer = buffer->next;
    }

    return NULL;
}

job *retrieve_foreground_job(job *head)
{
    job *buffer = head;
    while (buffer != NULL)
    {
        if (buffer->status == FOREGROUND)
        {
            return buffer;
        }

        buffer = buffer->next;
    }

    return NULL;
}

job *change_job_status(job *head_job, pid_t pid, enum status status)
{
    job *buffer = head_job;
    while (buffer != NULL)
    {
        if (buffer->process_id == pid)
        {
            buffer->status = status;
            break;
        }

        buffer = buffer->next;
    }

    return head_job;
}

// job *update_all_jobs(job *head_job)
// {
//     job *buffer = head_job;
//     while (buffer != NULL)
//     {
//         if (buffer->process_id == pid)
//         {
//             buffer->status = status;
//             break;
//         }

//         buffer = buffer->next;
//     }

//     return head_job;
// }

/** 
 * @brief  
 * @note   
 * @param  *job: 
 * @param  *head_job: 
 * @retval 
 */

int execute_fg(char *job_id_string, job *head_job_local)
{
    int job_id = (job_id_string == NULL) ? 0 : atoi(job_id_string);
    pid_t pid_buffer;

    printf("job_id %i", job_id);

    // TODO: Extract errors to constats

    if (job_id < 0)
    {
        // if user entered a negative number
        printf("Cannot have negative job id. ");
        return 0;
    }
    else if (job_id == 0)
    {
        printf("Am I here? 1");
        job *retrieve_job = retrieve_first_running_job(head_job);

        if (retrieve_job == NULL)
        {
            printf("No jobs are currently running. ");
            return 0;
        }

        // TODO: Extract this into own method
        pid_buffer = retrieve_job->process_id;
        head_job = change_job_status(head_job, pid_buffer, FOREGROUND);
        signal(SIGINT, signal_int_handler);
        // if job is null, execute fg on head node
        if (waitpid(pid_buffer, NULL, 0) < 0)
        {
            return 0;
        }
    }
    else if (job_id <= jobs_length(head_job))
    {
        // if job is less than the length of list of jobs, we assume that the user has entered a job number
        job *retrieve_job = retrieve_job_at_index(head_job, job_id);

        if (retrieve_job == NULL)
        {
            printf("retrieve job null?");
            return 0;
        }

        if (retrieve_job->status == FINISHED)
        {
            exit(EXIT_FAILURE);
            return 0;
        }

        pid_buffer = retrieve_job->process_id;
        head_job = change_job_status(head_job, pid_buffer, FOREGROUND);
        signal(SIGINT, signal_int_handler);

        if (waitpid(pid_buffer, NULL, 0) < 0)
        {
            return 0;
        }
    }
    else
    {
        printf("Am I here in process pid?");
        if (getpgid(pid_buffer = job_id) < 0)
        {
            printf("Process with PID %d does not exist or has already been terminated. ", job_id);
            return 0;
        }

        head_job = change_job_status(head_job, pid_buffer, FOREGROUND);
        signal(SIGINT, signal_int_handler);

        // otherwise, the user wants to bring job to foreground by pid
        if (waitpid((pid_t)job_id, NULL, 0) < 0)
        {
            printf("Wrong process id of job. ");
            return 0;
        }
    }

    head_job = delete_job_by_pid(pid_buffer, head_job); // after job is finished, we change its status

    return 1;
}

/** 
 * @brief  Function to execute "cd" command
 * @note   
 * @param  *path: path to navigate
 * @retval Returns $HOME environment variable if argument empty. If error in system call, return 0
 */
int execute_cd(char *path)
{
    if (path == NULL)
    {
        path = getenv("HOME");
    }

    if (chdir(path) != 0)
    {
        prompt_message(ERROR_CD_INVALID_DIRECTORY);
        return 0;
    }

    if (execute_pwd() == 0)
    {
        return 0;
    }

    return 1;
}

/** 
 * @brief  Function to execute the "pwd" command
 * @note   
 * @retval 
 */
int execute_pwd()
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        prompt_message(ERROR_CWD_INVALID);
        return 0;
    }

    prompt_message(CURRENT_DIRECTORY, cwd);
    return 1;
}

// SIGNAL HANDLERS

void signal_int_handler(int sig)
{
    job *buffer = retrieve_foreground_job(head_job);

    if (buffer == NULL)
    {
        return;
    }

    if (sig == SIGINT)
    {
        kill(buffer->process_id, SIGKILL);
    }
}
/** 
 * @brief  
 * @note   
 * @retval None
 */
void signal_child_handler()
{
    pid_t pid;
    int status;

    // we iterate through all active child pids
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {

        job *buffer = retrieve_job_by_pid(head_job, pid);
        if (buffer != NULL)
        {
            if (buffer->status == FOREGROUND)
            {
                head_job = delete_job_by_pid(pid, head_job);
            }
            else
            {
                printf("When do I reach here?");
                head_job = change_job_status(head_job, pid, FINISHED);
            }

            if (WIFEXITED(status))
            {
                printf("\n>> ");
                fflush(stdout); // TODO: Extract constant
            }
        }
    }
}

// HELPER SECTION

/** 
 * @brief  Get string for the job status enum of the job
 * @note   
 * @param  status: Status of the job in enum representation 
 * @retval String representation of the job's enum
 */

const char *get_status(enum status status)
{
    switch (status)
    {
    case RUNNING:
        return "RUNNING";
    case FINISHED:
        return "FINISHED";
    case STOPPED:
        return "STOPPED";
    case FOREGROUND:
        return "FOREGROUND";
    }
}

/** 
 * @brief Function provided by Prof. Harmouche
 * @note   
 * @param  *prompt: 
 * @param  *args[]: 
 * @param  *background: 
 * @retval Number of arguments
  */
int getcmd(char *prompt, char *args[], int *background)
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
    // Check if background is specified..
    if ((loc = index(line, '&')) != NULL)
    {
        *background = 1;
        *loc = ' ';
    }
    else
    {
        *background = 0;
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

/** 
 * @brief  Displays string message on shell with variable amount of arguments
 * @note   Taken from https://www.gnu.org/software/libc/manual/html_node/Variable-Arguments-Output.html
 * @param  message: Message to display
 * @retval None
 */
void prompt_message(char *message, ...)
{
    va_list ap;
    extern char *program_invocation_short_name;

    va_start(ap, message);
    vfprintf(stdout, message, ap);
    va_end(ap);
}
