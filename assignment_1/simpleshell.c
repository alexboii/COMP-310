#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
//// This code is given for illustration purposes. You need not include or follow this
// strictly. Feel free to writer better or bug free code. This example code block does not
// worry about deallocating memory. You need to ensure memory is allocated and deallocated
// properly so that your shell works without leaking memory.
//
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
            if (strlen(token) > 0)
            {
                args[i++] = token;
            }
        }
    }

    return i;
}

int main(void)
{
    char *args[20];
    int bg;
    while (1)
    {
        bg = 0;
        int cnt = getcmd("\n>> ", args, &bg);
        printf("%c", cnt);
        /* the steps can be..:(1) fork a child process using fork()(2) the child process will invoke execvp()(3) if background is not specified, the parent will wait,otherwise parent starts the next command... */
    }
}