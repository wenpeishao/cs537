#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_INPUT_SIZE 1024
#define MAX_ARG_SIZE 100

char *directories[] = {"/bin", "/usr/bin", NULL}; // Add more as needed

char *find_executable(const char *command)
{
    char *full_path = malloc(1024); // Large enough buffer, you should manage this better

    for (int i = 0; directories[i] != NULL; ++i)
    {
        snprintf(full_path, 1024, "%s/%s", directories[i], command);

        if (access(full_path, X_OK) == 0)
        {
            return full_path; // Found an executable
        }
    }

    free(full_path); // Free memory if executable is not found
    return NULL;     // Executable not found in any of the directories
}

void sigchld_handler(int signo)
{
    (void)signo;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

// Data structure to keep track of background jobs
typedef struct
{
    pid_t pid;
    char *command;
} Job;

Job jobs[100]; // Assume max 100 jobs
int nextJobId = 1;

void handle_builtin(char **args, int num_args)
{
    if (strcmp(args[0], "exit") == 0)
    {
        exit(0);
    }
    else if (strcmp(args[0], "cd") == 0)
    {
        if (num_args != 2)
        {
            printf("cd: wrong number of arguments\n");
        }
        else
        {
            if (chdir(args[1]) != 0)
            {
                perror("cd");
            }
        }
    }
    else if (strcmp(args[0], "jobs") == 0)
    {
        // List jobs
        for (int i = 0; i < nextJobId; ++i)
        {
            if (jobs[i].pid != 0)
            {
                printf("%d: %s\n", i, jobs[i].command);
            }
        }
    }
    else if (strcmp(args[0], "fg") == 0 || strcmp(args[0], "bg") == 0)
    {
        int jobId;
        if (num_args == 2)
        {
            jobId = atoi(args[1]);
        }
        else if (num_args == 1)
        {
            jobId = nextJobId - 1;
        }
        else
        {
            printf("fg/bg: wrong number of arguments\n");
            return;
        }

        if (jobId >= nextJobId || jobs[jobId].pid == 0)
        {
            printf("fg/bg: invalid job id\n");
            return;
        }

        if (strcmp(args[0], "fg") == 0)
        {
            // Bring the job to the foreground
            tcsetpgrp(STDIN_FILENO, jobs[jobId].pid);
            kill(jobs[jobId].pid, SIGCONT);
            int status;
            waitpid(jobs[jobId].pid, &status, WUNTRACED);
            tcsetpgrp(STDIN_FILENO, getpgrp());
        }
        else
        {
            // Run the job in the background
            kill(jobs[jobId].pid, SIGCONT);
        }
    }
}

/**
 * @brief Takes a line and splits it into args similar to how argc and argv work in main.
 * @param line The line being split up. Will be mangled after completion of the function.
 * @param args A ptr to char** that will be filled and allocated with the args from the line.
 * @param num_args A ptr to an int for the number of arguments in args.
 * @return returns 0 on success, and -1 on failure
 */
int lexer(char *line, char ***args, int *num_args)
{
    *num_args = 0;
    // count number of args
    char *l = strdup(line);
    if (l == NULL)
    {
        return -1;
    }
    char *token = strtok(l, " \t\n");
    while (token != NULL)
    {
        (*num_args)++;
        token = strtok(NULL, " \t\n");
    }
    free(l);
    // split line into args
    *args = malloc(sizeof(char **) * (*num_args + 1));
    *num_args = 0;
    token = strtok(line, " \t\n");
    while (token != NULL)
    {
        char *token_copy = strdup(token);
        if (token_copy == NULL)
        {
            return -1;
        }
        (*args)[(*num_args)++] = token_copy;
        token = strtok(NULL, " \t\n");
    }
    (*args)[(*num_args)] = NULL;
    return 0;
}

void prase_and_excute(char *input)
{
    char args[MAX_ARG_SIZE];
    char ***token;
    int *num_args;
    int i = 0, background = 0;

    lexer(input, token, num_args);

    // while (token != NULL)
    // {
    //     if (strcmp(token, "&") == 0)
    //     {
    //         background = 1;
    //     }
    // }
}

int main(int argc, char **argv)
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    char *line = NULL;
    size_t len = 0;
    char **args;
    int num_args;

    while (1)
    {
        printf("wsh> ");
        getline(&line, &len, stdin);

        if (lexer(line, &args, &num_args) == -1)
        {
            printf("Error tokenizing input\n");
            continue;
        }

        if (num_args > 0)
        {
            int num_cmds = 1;
            for (int i = 0; i < num_args; i++)
            {
                if (strcmp(args[i], "|") == 0)
                    num_cmds++;
            }

            int pipefds[2 * num_cmds];

            for (int i = 0; i < num_cmds; i++)
            {
                if (pipe(pipefds + i * 2) < 0)
                {
                    perror("pipe");
                    exit(1);
                }
            }

            int cmd_start = 0;
            for (int i = 0; i < num_cmds; i++)
            {
                pid_t pid = fork();
                if (pid == 0)
                {
                    // First command
                    if (i != 0)
                    {
                        dup2(pipefds[(i - 1) * 2], 0);
                    }
                    // Not last command
                    if (i != num_cmds - 1)
                    {
                        dup2(pipefds[i * 2 + 1], 1);
                    }
                    for (int j = 0; j < 2 * num_cmds; j++)
                    {
                        close(pipefds[j]);
                    }
                    char *cmd_args[20];
                    int arg_idx = 0;
                    for (int j = cmd_start; j < num_args; j++)
                    {
                        if (strcmp(args[j], "|") == 0)
                        {
                            cmd_start = j + 1;
                            break;
                        }
                        cmd_args[arg_idx++] = args[j];
                    }
                    cmd_args[arg_idx] = NULL;
                    execvp(cmd_args[0], cmd_args);
                    perror("execvp");
                    exit(1);
                }
                else if (pid < 0)
                {
                    perror("fork");
                    exit(1);
                }
            }

            for (int i = 0; i < 2 * num_cmds; i++)
            {
                close(pipefds[i]);
            }
            for (int i = 0; i < num_cmds; i++)
            {
                wait(NULL);
            }
        }

        // Free allocated memory
        for (int i = 0; i < num_args; ++i)
        {
            free(args[i]);
        }
        free(args);
    }

    if (line)
    {
        free(line);
    }

    return 0;
}