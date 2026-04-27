#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    int fd[2];
    pid_t pid;
    char buffer[1024];

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <message>\n", argv[0]);
        exit(1);
    }

    if (pipe(fd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid) { // Parent
        close(fd[0]); // Close read end
        write(fd[1], argv[1], strlen(argv[1]) + 1); // +1 for null terminator
        close(fd[1]); // Close write end
        // Optionally wait for child
        wait(NULL);
    } else { // Child
        close(fd[1]); // Close write end
        read(fd[0], buffer, sizeof(buffer));
        printf("Child received: %s\n", buffer);
        close(fd[0]); // Close read end
    }

    return 0;
}