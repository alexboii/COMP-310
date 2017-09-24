#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
// QUESTIONS

// 1 - Do we need to implement "pwd?"
// 2 - What's considered "other built-in features" for 20% in the grading scheme?

// PROMPT CONSTANTS
#define PRESENTATION "Alexander Bratyshkin --- 260684228 --- Assignment 1 --- ECSE 427"
#define ERROR_DEFAULT "An error has occurred, exiting shell..."
#define ERROR_NO_ARGS "You haven't entered a command. Please try again."
#define ERROR_CD_INVALID_DIRECTORY "Invalid directory."
#define ERROR_CWD_INVALID "Invalid current working directory."
#define ERROR_FORKING "Forking error."
#define ERROR_WAITPID "Error while awaiting background process."
#define ERROR_JOB_EXECUTION "Error during job execution."
#define ERROR_JOB_CREATION "Error creationg a new job."
#define CURRENT_DIRECTORY "Current directory: '%s'"
#define EXIT_MESSAGE "Exiting..."

// HASH CONSTANTS FOR COMMANDS
#define CD_HASH 5863276
#define EXIT_HASH 2090237503
#define LS_HASH 5863588

// MISC CONSTANTS
#define MAX_JOBS 20 // Max number of jobs to be displayed at any point in time.

// STRUCT DEFINITION FOR JOBS
typedef struct job
{
    struct job *next;
    pid_t process_id;
    char *command;
} job;

// FUNCTION PROTOTYPES
job *add_job(struct job *head, pid_t pid, char *command);
job *create_job(char *command, int pid, job *next);
void print_jobs(job *job);
void execute_job(char *args[], int bg, job **head_job);
int execute_non_fork(char *args[]);
int execute_fork(char *args[]);
int execute_cd(char *path);
int exec_pwd();
unsigned long hash(unsigned char *str);
void prompt_message(char *message, ...);
int getcmd(char *prompt, char *args[], int *background);

// TODO: Add verbose global constant to toggle debug statements :)

/** 
 * @brief  Assignment 1
 * @note   Alexander Bratyshkin - 260684228
 * @retval 
 */
int main(void)
{
    int bg;
    struct job *head_job = NULL;
    prompt_message(PRESENTATION);

    while (1)
    {
        // delocalize memory from command line arguments
        char *args[20] = {NULL};
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
        }

        if (cnt == 0)
        {
            prompt_message(ERROR_NO_ARGS);
            continue;
        }

        // TODO: insert an error if > to one of the commands that are not ls, cat, jobs, etc.

        // we do not need to fork for the following commands, so simply execute them
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
        execute_job(args, bg, &head_job);
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
    }
}

// BUILT-IN COMMANDS SECTION

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

    if (pid < 0)
    {
        prompt_message(ERROR_FORKING);
    }
    else if (pid == 0)
    {
        // TODO: to test jobs, put an await signal or something like that so that you see the command running despite &
        execvp(args[0], args);
    }
    else
    {
        if (bg == 0)
        {
            int status;
            if (waitpid(pid, &status, WUNTRACED) < 0)
            {
                prompt_message(ERROR_WAITPID);
            }
        }
        else
        {
            *head_job = add_job(*head_job, pid, args[0]);
            print_jobs(*head_job);
        }
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
job *add_job(struct job *head, pid_t pid, char *command)
{
    job *buffer = head;
    job *new_job;

    if (buffer == NULL)
    {
        new_job = create_job(command, pid, NULL);
        head = new_job;
        return head;
    }

    // TODO: extract this functionality into its own method (for print as well could be useful, also useful for a stop parameter)
    while (buffer->next != NULL)
    {
        buffer = buffer->next;
    }

    new_job = create_job(command, pid, NULL);
    buffer->next = new_job;

    printf("I'm here");

    return head;
}

/** 
 * @brief  Creates a new job "job" object 
 * @note   
 * @param  *command: 
 * @param  pid: 
 * @param  *next: 
 * @retval Newly created job
 */
job *create_job(char *command, int pid, job *next)
{
    job *new_job = (job *)malloc(sizeof(job));
    if (new_job == NULL)
    {
        printf(ERROR_JOB_CREATION);
    }

    new_job->command = command;
    new_job->process_id = pid;
    new_job->next = next;

    return new_job;
}

void print_jobs(job *head)
{
    job *buffer = head;
    while (buffer != NULL)
    {
        printf("\n %d", buffer->process_id);
        buffer = buffer->next;
    }
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

    if (exec_pwd() == 0)
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
int exec_pwd()
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

// HELPER SECTION

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
