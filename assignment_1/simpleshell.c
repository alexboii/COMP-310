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
// QUESTIONS
// 1 - What's considered "other built-in features" for 20% in the grading scheme?
// 2 - Do we have to handle stopped jobs, i.e. jobs that we can resume again afterwards?
// 3 - If I have two jobs running in the background, are both supposed to terminate when I click ctrl + c?
// 4 - If one job is on fg, and the other job is on bg, and we click ctrl + c -- are both jobs going to be suspended?
// 5 - Do we exit shell with ctrl + c? What happens with ctrl + c exactly?
// IMPORTANT:
// - Do jobs, fg, cd et al have to be supported as bg processes?
// - 20% for additional features

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
#define ERROR_JOB_FINISHED "Job has already been finished.\n"
#define ERROR_JOB_NOT_FOUND "Cannot find job with given id.\n"
#define ERROR_NEGATIVE_JOB_ID "Cannot have negative job id.\n"
#define ERROR_NO_ARGS "You haven't entered a command. Please try again."
#define ERROR_NO_RUNNING_JOBS "No jobs are currently running.\n"
#define ERROR_SIGNAL_BINDING "Could not bind signal handler for child process. Aborting..."
#define ERROR_WAITPID "Error while awaiting background process."
#define EXIT_MESSAGE "Exiting...\n"
#define JOBS_LIST "Background jobs\n -----------------------\n"
#define JOB_IN_BG "Job with PID %d put in background.\n"
#define PRESENTATION "Alexander Bratyshkin --- 260684228 --- Assignment 1 --- ECSE 427"

// HASH CONSTANTS FOR COMMANDS
#define CD_HASH 5863276
#define EXIT_HASH 6385204799
#define FG_HASH 5863378
#define JOBS_HASH 6385374451
#define LS_HASH 5863588

// MISC CONSTANTS
#define MAX_ARGS 20 // Max number of jobs to be displayed at any point in time.

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
int execute_non_fork(char *args[], int bg);
int execute_pwd();
int getcmd(char *prompt, char *args[], int *background, int *redirect);
int is_exit(char *first_arg);
int jobs_length(job *head);
job *add_job(struct job *head, pid_t pid, char *command, enum status status);
job *change_job_status(job *head_job, pid_t pid, enum status status);
job *create_job(char *command, int pid, job *next, enum status status);
job *delete_job_by_pid(pid_t pid, job *head_job);
job *retrieve_first_job_by_status(job *head, enum status status_comparator);
job *retrieve_job_by_pid(job *head, pid_t pid);
unsigned long hash(unsigned char *str);
void close_output_redirection(int *std_out, int *redirect_directory);
void execute_job(char *args[], int bg, int redirect, job **head_job);
void open_output_redirection(char *args[], int *std_out, int *redirect_directory, int cnt);
void print_jobs(job *job);
void prompt_message(char *message, ...);
void random_sleep();
void signal_child_handler();
void signal_int_handler(int sig);

// GLOBAL VARIABLE
struct job *head_job; // sorry for having a global variable, but unfortunately it's needed for signal handling
                      // please also note that I kept parameter "head_job" in many methods for several reasons,
                      // mostly because if I ever figured out how to do this exercise without a global variable
                      // for the signal handlers which don't accept parameters, I could easily go back to the
                      // clearner way of passing parameters & pointers to my functions

char *HOME;    // this kind of acts like a constant, so it's okay to keep it as a global variable in my opinion
int debug = 0; // global debugger variable

/** 
 * @brief  Assignment 1
 * @note   Alexander Bratyshkin - 260684228
 * @retval Hopefully a good grade ;] 
 */
int main(void)
{
    char *args[MAX_ARGS];
    int bg, redirect, std_out, redirect_directory;
    head_job = NULL;
    HOME = getenv("HOME");

    time_t now;
    srand((unsigned int)(time(&now)));

    if (signal(SIGCHLD, signal_child_handler) == SIG_ERR || signal(SIGINT, signal_int_handler) == SIG_ERR || signal(SIGTSTP, SIG_IGN) == SIG_ERR)
    {
        prompt_message(ERROR_SIGNAL_BINDING);
        exit(EXIT_FAILURE);
    }

    prompt_message(PRESENTATION);

    while (1)
    {
        fflush(stdout);

        bg = 0, redirect = 0;

        int cnt = getcmd("\n>> ", args, &bg, &redirect);

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

        // handle exit before fork by killing all processes spawned by the program & the program itself
        if (is_exit(args[0]) == 1)
        {
            kill(0, SIGKILL);
        }

        // we redirect stdout to a file
        if (redirect == 1)
        {
            open_output_redirection(args, &std_out, &redirect_directory, cnt);
        }

        // we do not need to fork fg & we want to ensure that whenever cd is ran on foreground,
        // the parent's process directory switches, which is a different behavior from cd ran
        // in the background
        if (execute_non_fork(args, bg) == 0)
        {
            execute_job(args, bg, redirect, &head_job);
        }

        // rewire stdout
        if (redirect == 1)
        {
            close_output_redirection(&std_out, &redirect_directory);
        }

        // nullify function, for some reason *args = NULL doesn't work
        memset(args, 0, sizeof(args) * sizeof(char *));

        fflush(stdout);
    }

    return EXIT_SUCCESS;
}

// FLOW-CONTROL SECTION

/** 
 * @brief  Execute the functions that do not require forking & cannot run in background, i.e. "cd" on foreground and "fg"
 * @note   
 * @param  *args[]: List of arguments
 * @param  bg: Background boolean
 * @retval 1 if non-forkable command has been executed (regardless of error), 0 otherwise
 */
int execute_non_fork(char *args[], int bg)
{
    unsigned long argument = hash(args[0]);

    switch (argument)
    {
    case CD_HASH:
        if (bg == 1)
        {
            // if it's a background process, we don't want the main shell's process to be change directory
            // we want the child's process to change directory, and therefore should be executed inside the child's fork
            return 0;
        }
        execute_cd(args[1]);
        return 1;
        break;
    case FG_HASH:
        execute_fg(args[1], head_job);
        return 1;
        break;
    default:
        return 0;
        break;
    }
}

// BUILT-IN COMMANDS SECTION

/** 
 * @brief  Execute the built-in commands that are forkable and can run in background
 * @note   
 * @param  *args[]: List of arguments
 * @param  *head_job: Head of the list of jobs
 * @retval 
 */
int execute_built_in(char *args[], job *head_job)
{
    unsigned long argument = hash(args[0]);

    switch (argument)
    {
    case JOBS_HASH:
        print_jobs(head_job);
        return 1;
    case CD_HASH:
        execute_cd(args[1]);
        return 1;
    default:
        return 0;
    }
}

/** 
 * @brief  Execution of child and parent processes 
 * @note   
 * @param  *args[]: List of arguments   
 * @param  bg: 1 if process has to run in background, 0 otherwise
 * @param  redirect: 1 if stdout redirected, 0 otherwise
 * @param  **head_job: Pointer to the head of the list of jobs
 * @retval None
 */
void execute_job(char *args[], int bg, int redirect, job **head_job)
{
    int pid = fork();

    switch (pid)
    {
    case -1:
        prompt_message(ERROR_FORKING);
        break;
    case 0:
        if (bg == 1)
        {
            signal(SIGINT, SIG_IGN); // ignore ctrl + c in case of background to prevent killing of background process
        }

        random_sleep();

        if (execute_built_in(args, *head_job) == 0)
        {
            execvp(args[0], args);
            prompt_message(ERROR_CHILD_PROCESS);
            *head_job = change_job_status(*head_job, pid, FINISHED);
        }
        else
        {
            // this alllows us to run the "jobs" command in the background, as well as "cd" due to the termination
            // of the child processes running in the background
            // during exit, we send a SIGINT signal, which marks background jobs as finished
            exit(EXIT_SUCCESS);
        }
        break;
    default:
        if (bg == 0)
        {
            int status;
            // mark job as foreground job
            // alternatively, could've had a global "current_process" variable
            // but global variables hurt my eyes
            *head_job = add_job(*head_job, pid, args[0], FOREGROUND);
            if (waitpid(pid, &status, WUNTRACED) < 0)
            {
                prompt_message(ERROR_WAITPID);
                exit(EXIT_FAILURE);
            }

            // delete it as soon as it's finished
            *head_job = delete_job_by_pid(pid, *head_job);
        }
        else
        {
            *head_job = add_job(*head_job, pid, args[0], RUNNING);

            // we don't want to print this in STDOUT for background processes that have output redirection enabled
            if (redirect == 0)
            {
                prompt_message(JOB_IN_BG, pid);
            }
        }

        fflush(stdout);
        break;
    }
}

/** 
 * @brief  Add job to end of list of jobs
 * @note   
 * @param  *head: Head of the list of jobs
 * @param  pid: Job process ID 
 * @param  *command: Job's associated command
 * @param  status: Job's status 
 * @retval Modified head of list of jobs
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

    while (buffer->next != NULL)
    {
        buffer = buffer->next;
    }

    new_job = create_job(command, pid, NULL, status);
    buffer->next = new_job;

    return head;
}

/** 
 * @brief  Delete a job from the list of jobs by given pid
 * @note   
 * @param  pid: Process you wish to delete from list of jobs
 * @param  *head_job: Head of list of jobs
 * @retval Modified head of list of jobs
 */
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
 * @param  status: Status of job 
 * @retval Newly created job
 */
job *create_job(char *command, int pid, job *next, enum status status)
{
    job *new_job = (job *)malloc(sizeof(job));
    if (new_job == NULL)
    {
        prompt_message(ERROR_JOB_CREATION);
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
    job *buffer = head_job;
    int job_number = 1;
    int status = 0;
    while (buffer != NULL)
    {
        fprintf(stdout, "\n [%i] %-30s  %d          %s", job_number, buffer->command, buffer->process_id, get_status(buffer->status));
        fflush(stdout);
        job_number++;
        buffer = buffer->next;
    }

    printf("\n");
}

/** 
 * @brief  Returns the length of the linked list of jobs
 * @note   
 * @param  *head: Head of list of jobs
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

    return length;
}

/** 
 * @brief  Retrieve job from linked list of jobs at a given position in the list
 * @note   
 * @param  *head: Head of list of jobs
 * @param  index: Position of linked list 
 * @retval Job at given index
 */
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

/** 
 * @brief  Retrieve job from linked list of jobs with a given pid
 * @note   
 * @param  *head: Head of list of jobs
 * @param  pid: PID of job
 * @retval Job with given PID
 */
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

/** 
 * @brief  Retrieve first job in the list with a specified status 
 * @note   
 * @param  *head: Head of list of jobs
 * @param  status_comparator: RUNNING or FOREGROUND, usually
 * @retval First job with specified status
 */
job *retrieve_first_job_by_status(job *head, enum status status_comparator)
{
    job *buffer = head;
    while (buffer != NULL)
    {
        if (buffer->status == status_comparator)
        {
            return buffer;
        }

        buffer = buffer->next;
    }

    return NULL;
}

/** 
 * @brief  Change the status of a job with a specified process id 
 * @note   
 * @param  *head_job: Head of list of jobs
 * @param  pid: Identifier for job's process
 * @param  status: Status to which you wish to change the job 
 * @retval Head job with modified node
 */
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

/** 
 * @brief  Bring a process to the background
 * @note   
 * @param  *job_id_string: ID of job, PID, or no argument
 * @param  *head_job_local: Head of list of jobs
 * @retval 1 if successful, 0 if error
 */
int execute_fg(char *job_id_string, job *head_job_local)
{
    int job_id = (job_id_string == NULL) ? 0 : atoi(job_id_string);
    pid_t pid_buffer;

    if (job_id < 0)
    {
        prompt_message(ERROR_NEGATIVE_JOB_ID);
        return 0;
    }
    else if (job_id == 0)
    {
        job *retrieve_job = retrieve_first_job_by_status(head_job, RUNNING);

        if (retrieve_job == NULL)
        {
            prompt_message(ERROR_NO_RUNNING_JOBS);
            return 0;
        }

        if (retrieve_job->status == FINISHED)
        {
            prompt_message(ERROR_JOB_FINISHED);
            return 0;
        }

        pid_buffer = retrieve_job->process_id;
    }
    else if (job_id <= jobs_length(head_job))
    {
        // if job is less than the length of list of jobs, we assume that the user has entered a job number
        job *retrieve_job = retrieve_job_at_index(head_job, job_id);

        if (retrieve_job == NULL)
        {
            prompt_message(ERROR_JOB_NOT_FOUND);
            return 0;
        }

        if (retrieve_job->status == FINISHED)
        {
            prompt_message(ERROR_JOB_FINISHED);
            exit(EXIT_FAILURE);
            return 0;
        }

        pid_buffer = retrieve_job->process_id;
    }
    else
    {
        // otherwise, user is trying to bring job to foreground by pid
        if (getpgid(pid_buffer = job_id) < 0)
        {
            prompt_message(ERROR_JOB_NOT_FOUND);
            return 0;
        }
    }

    head_job = change_job_status(head_job, pid_buffer, FOREGROUND);
    signal(SIGINT, signal_int_handler);

    if (waitpid(pid_buffer, NULL, 0) < 0)
    {
        prompt_message(ERROR_FG);
        return 0;
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
    int is_path_null = (path == NULL) ? 1 : 0;

    if (chdir((is_path_null == 1) ? HOME : path) != 0)
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

// OUTPUT REDIRECTION
/** 
 * @brief  Rewires the stdout to a specified file
 * @note   
 * @param  *args[]: List of arguments
 * @param  *std_out: Pointer to std_out variable in main function
 * @param  *redirect_directory: Pointer to redirection directory in main function
 * @param  cnt: Number of arguments
 * @retval None
 */
void open_output_redirection(char *args[], int *std_out, int *redirect_directory, int cnt)
{
    *std_out = dup(STDOUT_FILENO);
    close(STDOUT_FILENO);
    *redirect_directory = open(args[cnt - 1], O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    args[cnt - 1] = NULL;
}

/** 
 * @brief  Rewires stdout back to shell process
 * @note   
 * @param  *std_out: 
 * @param  *redirect_directory: 
 * @retval None
 */
void close_output_redirection(int *std_out, int *redirect_directory)
{
    close(*redirect_directory);
    dup(*std_out);
    close(*std_out);
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

/** 
 * @brief  Check if input is the exit command 
 * @note   
 * @param  *first_arg: First argument
 * @retval 1 if first argument is exit, 0 if it's not 
 */
int is_exit(char *first_arg)
{
    return strcmp(first_arg, "exit") == 0;
}

// SIGNAL HANDLERS

/** 
 * @brief  Signal handler for SIGINT 
 * @note   
 * @param  sig: 
 * @retval None
 */
void signal_int_handler(int sig)
{
    job *buffer = retrieve_first_job_by_status(head_job, FOREGROUND);

    if (buffer == NULL)
    {
        return;
    }
    if (sig == SIGINT)
    {
        kill(buffer->process_id, SIGKILL);
    }
    else
    {
        fflush(stdout);
    }
}
/** 
 * @brief  Signal handler for the child
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
                head_job = change_job_status(head_job, pid, FINISHED);
            }

            if (WIFEXITED(status))
            {
                printf("\n>>");
                fflush(stdout);
            }
        }
    }
}

// HELPER SECTION

/** 
 * @brief  Function to sleep process
 * @note   Provided by TA Aakash Nandi & Prof. Rola Harmouche
 * @retval None
 */
void random_sleep()
{
    int w, rem;
    w = rand() % 10;
    rem = sleep(w); //handles interruption by signal
    while (rem != 0)
    {
        rem = sleep(rem);
    }
}

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
int getcmd(char *prompt, char *args[], int *background, int *redirect)
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

    if ((loc = index(line, '>')) != NULL)
    {
        *redirect = 1;
        *loc = ' ';
    }
    else
    {
        *redirect = 0;
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
    fflush(stdout);
    va_end(ap);
}
