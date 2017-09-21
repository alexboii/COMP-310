#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

// CONSTANTS
#define PRESENTATION "Alexander Bratyshkin --- 260684228 --- Assignment 1 --- ECSE 427"
#define ERROR_DEFAULT "An error has occurred, exiting shell..."
#define ERROR_NO_ARGS "You haven't entered a command. Please try again."
#define ERROR_CD_INVALID_DIRECTORY "Invalid directory."
#define ERROR_CWD_INVALID "Invalid current working directory."
#define CURRENT_DIRECTORY "Current directory %s"
#define EXIT_MESSAGE "Exiting..."

// HASH CONSTANTS FOR COMMANDS
#define CD_HASH 5863276
#define EXIT_HASH 2090237503

// FUNCTION PROTOTYPES
void prompt_message(char *message, ...);
int execute_non_fork(char *args[]);
int execute_cd(char *path);
unsigned long hash(unsigned char *str);

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

int main(void)
{
    char *args[20];
    int bg;
    prompt_message(PRESENTATION);
    while (1)
    {
        bg = 0;
        int cnt = getcmd("\n>> ", args, &bg);

        for (int i = 0; i < cnt; i++)
        {
            int test = hash(args[i]);
            printf("\nArg[%d] = %s, %i", i, args[i], test);
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

        int result_non_fork = execute_non_fork(args);
        if (result_non_fork == 0 || result_non_fork == 1)
        {
            if (result_non_fork == 0)
            {
                exit(1);
            }

            continue;
        }

        // we do not need to for for the following commands, so simply execute them
    }
}

// NAVIGATION SECTION

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

// COMMANDS SECTION

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

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        prompt_message(ERROR_CWD_INVALID);
    }

    prompt_message(CURRENT_DIRECTORY, cwd);
    return 1;
}

// HELPER SECTION

/** 
 * @brief  Canonical DJB2 hash function
 * @note   Borrowed from http://www.cse.yorku.ca/~oz/hash.html
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
