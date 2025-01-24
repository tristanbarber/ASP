// Author: Tristan Barber

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main () {
    int mapper_pid, reducer_pid, status;
    int pfd[2];

    // Create pipe
    if (pipe(pfd) != 0) {
        perror("Failed to open pipe");
        exit(EXIT_FAILURE);
    }

    mapper_pid = fork();
    if(mapper_pid == -1) {
        perror("failed to create mapper process");
        exit(EXIT_FAILURE);
    } else if (mapper_pid == 0) {
        // Redirect stdout to pipe write, close unused read end
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);

        // execute mapper - execl should not return unless error
        execl("mapper", "mapper", NULL);
        perror("failed to execute mapper");
        exit(EXIT_FAILURE);
    }

    reducer_pid = fork();
    if(reducer_pid == -1) {
        perror("failed to create reducer process");
        exit(EXIT_FAILURE);
    } else if (reducer_pid == 0) {
        // Redirect stdin to pipe read, close unused write end
        close(pfd[1]);
        dup2(pfd[0], STDIN_FILENO);

        // execute reducer - execl should not return unless error
        execl("reducer", "reducer", NULL);
        perror("failed to execute reducer");
        exit(EXIT_FAILURE);
    }

    // clean up
    close(pfd[0]);
    close(pfd[1]);

    // wait for child processes to finish
    waitpid(mapper_pid, &status, 0);
    waitpid(reducer_pid, &status, 0);

    return 0;
}
