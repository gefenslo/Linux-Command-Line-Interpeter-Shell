#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "lineParser.h"
#include <signal.h>
#include <ctype.h>
#include <errno.h>


#define INPUT_SIZE 2048
#define HISTLEN 10
int debugMode = 0;

typedef struct historyEntry {
    int cmd_num;
    char *cmd_line;
} historyEntry;

historyEntry *history[HISTLEN] = {0};
int hist_start = 0;
int hist_count = 0;
int next_cmd_num = 1;

static int isBlankLine(const char *s) {
    while (*s) {
        if (!isspace((unsigned char)*s))
            return 0;
        s++;
    }
    return 1;
}

static void addHistoryEntry(const char *line) {
    int idx = (hist_start + hist_count) % HISTLEN;
    if (hist_count == HISTLEN) {
        idx = hist_start;
        free(history[idx]->cmd_line);
        free(history[idx]);
        hist_start = (hist_start + 1) % HISTLEN;
    } else {
        hist_count++;
    }

    history[idx] = (historyEntry *)malloc(sizeof(historyEntry));
    history[idx]->cmd_num = next_cmd_num++;
    history[idx]->cmd_line = strdup(line);
}

static void printHistoryList(void) {
    for (int i = 0; i < hist_count; i++) {
        int idx = (hist_start + i) % HISTLEN;
        fprintf(stdout, "%d %s\n", history[idx]->cmd_num, history[idx]->cmd_line);
    }
}

static const char *getLastHistoryEntry(void) {
    if (hist_count == 0)
        return NULL;
    return history[(hist_start + hist_count - 1) % HISTLEN]->cmd_line;
}

static const char *getHistoryEntryByNumber(int cmd_num) {
    for (int i = 0; i < hist_count; i++) {
        int idx = (hist_start + i) % HISTLEN;
        if (history[idx]->cmd_num == cmd_num)
            return history[idx]->cmd_line;
    }
    return NULL;
}

static void freeHistoryList(void) {
    for (int i = 0; i < hist_count; i++) {
        int idx = (hist_start + i) % HISTLEN;
        free(history[idx]->cmd_line);
        free(history[idx]);
        history[idx] = NULL;
    }
    hist_start = 0;
    hist_count = 0;
}

/* Process manager structures and helpers */
#define TERMINATED  -1
#define RUNNING 1
#define SUSPENDED 0

typedef struct process{
    cmdLine* cmd; 
    pid_t pid;
    int status; /* TERMINATED/RUNNING/SUSPENDED */
    struct process *next;
} process;

process *process_list = NULL;

static cmdLine *cloneCmdLine(const cmdLine *src) {
    if (!src) return NULL;
    cmdLine *c = (cmdLine*)malloc(sizeof(cmdLine));
    memset(c, 0, sizeof(cmdLine));
    c->argCount = src->argCount;
    c->blocking = src->blocking;
    c->idx = src->idx;
    if (src->inputRedirect)
        c->inputRedirect = strdup(src->inputRedirect);
    if (src->outputRedirect)
        c->outputRedirect = strdup(src->outputRedirect);
    for (int i=0;i<src->argCount;i++) {
        ((char**)c->arguments)[i] = strdup(src->arguments[i]);
    }
    c->next = NULL;
    return c;
}

void addProcess(process** process_list, cmdLine* cmd, pid_t pid) {
    if (!process_list) return;
    process *p = (process*)malloc(sizeof(process));
    p->cmd = cloneCmdLine(cmd);
    p->pid = pid;
    p->status = RUNNING;
    p->next = *process_list;
    *process_list = p;
}

void updateProcessStatus(process** process_list, int pid, int status) {
    process *iter = *process_list;
    while (iter) {
        if (iter->pid == pid) {
            iter->status = status;
            return;
        }
        iter = iter->next;
    }
}

void updateProcessList(process **process_list) {
    process *iter = process_list ? *process_list : NULL;
    while (iter) {
        int status;
        pid_t result = waitpid(iter->pid, &status, WNOHANG | WUNTRACED);/* non-blocking check */
        if (result == iter->pid) {
            if (WIFEXITED(status) || WIFSIGNALED(status))
                iter->status = TERMINATED;
            else if (WIFSTOPPED(status))
                iter->status = SUSPENDED;
        }
        iter = iter->next;
    }
}

static void removeTerminatedProcesses(process **process_list_p) {
    process **cur = process_list_p;
    while (cur && *cur) {
        process *node = *cur;
        if (node->status == TERMINATED) {
            *cur = node->next;
            if (node->cmd)
                freeCmdLines(node->cmd);
            free(node);
        } else {
            cur = &node->next;
        }
    }
}

void printProcessList(process** process_list) {
    updateProcessList(process_list);
    process *iter = *process_list;
    fprintf(stdout, "PID\tSTATUS\tCommand\n");
    while (iter) {
        char *status_str = "";
        if (iter->status == TERMINATED) status_str = "Terminated";
        else if (iter->status == RUNNING) status_str = "Running";
        else if (iter->status == SUSPENDED) status_str = "Suspended";

        /* reconstruct command */
        fprintf(stdout, "%d\t%s\t", iter->pid, status_str);
        if (iter->cmd) {
            for (int i=0;i<iter->cmd->argCount;i++) {
                fprintf(stdout, "%s%s", iter->cmd->arguments[i], (i+1<iter->cmd->argCount)?" ":"");
            }
        }
        fprintf(stdout, "\n");
        iter = iter->next;
    }

    removeTerminatedProcesses(process_list);
}

void freeProcessList(process *process_list) {
    process *iter = process_list;
    while (iter) {
        process *next = iter->next;
        if (iter->cmd)
            freeCmdLines(iter->cmd);
        free(iter);
        iter = next;
    }
}

void execute(cmdLine *pCmdLine) {
    /* If there's no pipeline, run a single command as before */
    if (pCmdLine->next == NULL) {
        pid_t pid = fork();
        if (pid == 0) { // Child
            /* Handle input redirection */
            if (pCmdLine->inputRedirect) {
                FILE *infile = fopen(pCmdLine->inputRedirect, "r");
                if (!infile) {
                    perror("Input redirection failed");
                    exit(1);
                }
                dup2(fileno(infile), STDIN_FILENO);
                fclose(infile);
            }
            /* Handle output redirection */
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
            /* add to process list (clone cmd inside addProcess) */
            addProcess(&process_list, pCmdLine, pid);
            if (debugMode) {
                fprintf(stderr, "PID: %d\n", pid);
                fprintf(stderr, "Executing: %s\n", pCmdLine->arguments[0]);
                fprintf(stderr, "Foreground or background: %s\n", pCmdLine->blocking ? "Foreground" : "Background");
            }
            if (pCmdLine->blocking) {
                waitpid(pid, NULL, 0);
                updateProcessStatus(&process_list, pid, TERMINATED);
            }
        } else {
            perror("fork failed");
            exit(1);
        }
        return;
    }

    /* Pipeline case: exactly two commands linked in pCmdLine and pCmdLine->next */
    cmdLine *left = pCmdLine;
    cmdLine *right = pCmdLine->next;

    /* Validate that redirections don't conflict with piping */
    if (left->outputRedirect) {
        fprintf(stderr, "Error: output redirection on left-hand side of pipe is not allowed\n");
        return;
    }
    if (right->inputRedirect) {
        fprintf(stderr, "Error: input redirection on right-hand side of pipe is not allowed\n");
        return;
    }

    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        return;
    }

    pid_t c1 = fork();
    if (c1 < 0) {
        perror("fork");
        /* cleanup */
        close(fd[0]); close(fd[1]);
        return;
    }
    if (c1 == 0) {
        /* child1: left command; write to pipe */
        if (left->inputRedirect) {
            FILE *infile = fopen(left->inputRedirect, "r");
            if (!infile) {
                perror("Input redirection failed");
                exit(1);
            }
            dup2(fileno(infile), STDIN_FILENO);
            fclose(infile);
        }
        /* redirect stdout to pipe write end */
        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        close(fd[0]); close(fd[1]);
        if (debugMode) {
            fprintf(stderr, "PID: %d\n", getpid());
            fprintf(stderr, "Executing (left): %s\n", left->arguments[0]);
        }
        execvp(left->arguments[0], (char * const *)left->arguments);
        perror("execvp failed");
        exit(1);
    }

    /* parent */
    /* add left child to process list */
    addProcess(&process_list, left, c1);
    if (debugMode) fprintf(stderr, "PID: %d\n", c1);
    /* close write end in parent so child2 can see EOF */
    close(fd[1]);
    pid_t c2 = fork();
    if (c2 < 0) {
        perror("fork");
        close(fd[0]);
        return;
    }
    if (c2 == 0) {
        /* child2: right command; read from pipe */
        if (right->outputRedirect) {
            FILE *outfile = fopen(right->outputRedirect, "w");
            if (!outfile) {
                perror("Output redirection failed");
                exit(1);
            }
            dup2(fileno(outfile), STDOUT_FILENO);
            fclose(outfile);
        }
        if (dup2(fd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            exit(1);
        }
        close(fd[0]);
        if (debugMode) {
            fprintf(stderr, "PID: %d\n", getpid());
            fprintf(stderr, "Executing (right): %s\n", right->arguments[0]);
        }
        execvp(right->arguments[0], (char * const *)right->arguments);
        perror("execvp failed");
        exit(1);
    }

    /* parent */
    /* add right child to process list */
    addProcess(&process_list, right, c2);
    if (debugMode) fprintf(stderr, "PID: %d\n", c2);
    close(fd[0]);

    /* wait for children if blocking */
    if (pCmdLine->blocking) {
        waitpid(c1, NULL, 0);
        waitpid(c2, NULL, 0);
        updateProcessStatus(&process_list, c1, TERMINATED);
        updateProcessStatus(&process_list, c2, TERMINATED);
    }
}

int main(int argc, char *argv[]) {
    char input[INPUT_SIZE];
    char cmdToExec[INPUT_SIZE];
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

        input[strcspn(input, "\n")] = '\0';
        if (isBlankLine(input))
            continue;

        /* Refresh process statuses before handling the next command */
        updateProcessList(&process_list);
        /*print history- task 4 */
        if (strcmp(input, "history") == 0) {
            printHistoryList();
            continue;
        }

        if (strcmp(input, "!!") == 0) {
            const char *last = getLastHistoryEntry();
            if (!last) {
                fprintf(stdout, "No commands in history\n");
                continue;
            }
            strncpy(cmdToExec, last, sizeof(cmdToExec) - 1);
            cmdToExec[sizeof(cmdToExec) - 1] = '\0';
            addHistoryEntry(cmdToExec);
        } else if (input[0] == '!') {
            char *endptr;
            long n;

            errno = 0;
            n = strtol(input + 1, &endptr, 10);
            if (errno != 0 || endptr == input + 1 || *endptr != '\0' || n <= 0) {
                fprintf(stdout, "Invalid history command\n");
                continue;
            }

            const char *entry = getHistoryEntryByNumber((int)n);
            if (!entry) {
                fprintf(stdout, "No such command in history\n");
                continue;
            }

            strncpy(cmdToExec, entry, sizeof(cmdToExec) - 1);
            cmdToExec[sizeof(cmdToExec) - 1] = '\0';
            addHistoryEntry(cmdToExec);
        } else {
            strncpy(cmdToExec, input, sizeof(cmdToExec) - 1);
            cmdToExec[sizeof(cmdToExec) - 1] = '\0';
            addHistoryEntry(cmdToExec);
        }

        cmd = parseCmdLines(cmdToExec);
        if (!cmd)
            continue;

        /* procs builtin */
        if (strcmp(cmd->arguments[0], "procs") == 0) {
            printProcessList(&process_list);
            freeCmdLines(cmd);
            continue;
        }

        if (strcmp(cmd->arguments[0], "quit") == 0) {
            freeCmdLines(cmd);
            freeProcessList(process_list);
            process_list = NULL;
            freeHistoryList();
            break;
        }

        // Handle 'cd' 
        if (strcmp(cmd->arguments[0], "cd") == 0) {
            if (cmd->argCount < 2) {
                fprintf(stderr, "cd: missing argument\n");
            } else if (chdir(cmd->arguments[1]) != 0) {
                perror("cd failed");
            }
            freeCmdLines(cmd);
            continue;
        }

        //  Signal management built-in commands
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
                else {
                    fprintf(stderr, "Sent signal %d to process %d\n", sig, pid);
                    /* update status for stop/wakeup/ice */
                    if (sig == SIGSTOP)
                        updateProcessStatus(&process_list, pid, SUSPENDED);
                    else if (sig == SIGCONT)
                        updateProcessStatus(&process_list, pid, RUNNING);
                    else if (sig == SIGINT)
                        updateProcessStatus(&process_list, pid, TERMINATED);
                }
            }
            freeCmdLines(cmd);
            continue;
        }

        execute(cmd);
        freeCmdLines(cmd);
    }

    freeProcessList(process_list);
    process_list = NULL;
    freeHistoryList();

    return 0;
}