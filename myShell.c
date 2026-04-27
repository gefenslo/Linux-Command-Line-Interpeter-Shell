#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "lineParser.h"
#include <signal.h>


#define INPUT_SIZE 2048
int debugMode = 0;

void execute(cmdLine *pCmdLine) {
    pid_t pid = fork();
    if (pid == 0) { // Child
        // Handle input redirection- TASK 3
        if (pCmdLine->inputRedirect) {
            FILE *infile = fopen(pCmdLine->inputRedirect, "r");
            if (!infile) {
                perror("Input redirection failed");
                exit(1);
            }
            dup2(fileno(infile), STDIN_FILENO);
            fclose(infile);
        }
        // Handle output redirection
        if (pCmdLine->outputRedirect) {
            FILE *outfile = fopen(pCmdLine->outputRedirect, "w");
            if (!outfile) {
                perror("Output redirection failed");
                exit(1);
            }
            dup2(fileno(outfile), STDOUT_FILENO);
            fclose(outfile);
        }
        if (debugMode) {
            fprintf(stderr, "PID: %d\n", getpid());
            fprintf(stderr, "Executing: %s\n", pCmdLine->arguments[0]);
            fprintf(stderr, "Foreground or background: %s\n", pCmdLine->blocking ? "Foreground" : "Background");
        }
        execvp(pCmdLine->arguments[0], (char * const *)pCmdLine->arguments);
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) { // Parent
        if (debugMode) {
            fprintf(stderr, "PID: %d\n", pid);
            fprintf(stderr, "Executing: %s\n", pCmdLine->arguments[0]);
            fprintf(stderr, "Foreground or background: %s\n", pCmdLine->blocking ? "Foreground" : "Background");
        }
        if (pCmdLine->blocking)
            waitpid(pid, NULL, 0);
    } else {
        perror("fork failed");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    char input[INPUT_SIZE];
    char cwd[PATH_MAX];
    cmdLine *cmd;

    // Check for -d flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0)
            debugMode = 1;
    }

    while (1) {
        if (isatty(STDIN_FILENO)) {
            if (getcwd(cwd, sizeof(cwd)) != NULL)
                printf("%s> ", cwd);
            else
                printf("myshell> ");
            fflush(stdout);
        }
        if (!fgets(input, INPUT_SIZE, stdin))
            break;

        cmd = parseCmdLines(input);
        if (!cmd)
            continue;

        if (strcmp(cmd->arguments[0], "quit") == 0) {
            freeCmdLines(cmd);
            break;
        }

        // Handle 'cd' as a built-in command (1b)
        if (strcmp(cmd->arguments[0], "cd") == 0) {
            if (cmd->argCount < 2) {
                fprintf(stderr, "cd: missing argument\n");
            } else if (chdir(cmd->arguments[1]) != 0) {
                perror("cd failed");
            }
            freeCmdLines(cmd);
            continue;
        }

        // Task 2: Signal management built-in commands
        if (strcmp(cmd->arguments[0], "stop") == 0 ||
            strcmp(cmd->arguments[0], "wakeup") == 0 ||
            strcmp(cmd->arguments[0], "ice") == 0 ||
            strcmp(cmd->arguments[0], "nuke") == 0) {
            if (cmd->argCount < 2) {
                fprintf(stderr, "%s: missing process id\n", cmd->arguments[0]);
            } else {
                int pid = atoi(cmd->arguments[1]);
                int sig = 0;
                if (strcmp(cmd->arguments[0], "stop") == 0)
                    sig = SIGSTOP;
                else if (strcmp(cmd->arguments[0], "wakeup") == 0)
                    sig = SIGCONT;
                else if (strcmp(cmd->arguments[0], "ice") == 0)
                    sig = SIGINT;
                else if (strcmp(cmd->arguments[0], "nuke") == 0) {
                    // For nuke, send SIGKILL to the process group
                    if (kill(-pid, SIGKILL) != 0)
                        perror("nuke failed");
                    else
                        fprintf(stderr, "Sent SIGKILL to process group %d\n", pid);
                    freeCmdLines(cmd);
                    continue;
                }
                if (kill(pid, sig) != 0)
                    perror("kill failed");
                else
                    fprintf(stderr, "Sent signal %d to process %d\n", sig, pid);
            }
            freeCmdLines(cmd);
            continue;
        }

        execute(cmd);
        freeCmdLines(cmd);
    }
    return 0;
}