#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// CONSTANTS 
#define PRESENTATION "Alexander Bratyshkin --- 260684228 --- Assignment 1 --- ECSE 427 "
#define ERROR_MESSAGE_DEFAULT "An error has occurred, exiting shell..."
#define ERROR_MESSAGE_NO_ARGS "You haven't entered a command. Please try again."

// FUNCTION PROTOTYPES 
void prompt_message(char* message);


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
            printf("\nArg[%d] = %s", i, args[i]);

        // error handling for command input    
        if(cnt == -1)
        {
            prompt_message(ERROR_MESSAGE_DEFAULT);
        }
        
        if(cnt == 0)
        {
            prompt_message(ERROR_MESSAGE_NO_ARGS);
            continue;
        }
    }
}

// HELPER SECTION

/** 
 * @brief  Displays string message on shell
 * @note   
 * @param  message: Message to display
 * @retval None
 */
void prompt_message(char* message){
    printf("%s", message);
}

