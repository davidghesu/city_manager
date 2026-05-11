#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

volatile sig_atomic_t end_process = 0;

static void handler(int signum)
{
    char msg_SIGINT[] = "Caught SIGINT. Ending process.\n";
    char msg_SIGUSR1[] = "Caught SIGUSR1. New report has been added.\n";
    if (signum == SIGINT) {
        write(STDOUT_FILENO, msg_SIGINT, strlen(msg_SIGINT));
        end_process=1;
    } else if (signum == SIGUSR1) {
        write(STDOUT_FILENO, msg_SIGUSR1, strlen(msg_SIGUSR1));
    }
}

int main() {

    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    // create .moditor_pid file and write PID to it
    char path[128];
    snprintf(path, sizeof(path), ".monitor_pid");

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        printf("Could not open file for writing");
        exit(1);
    }

    char pid_to_string[10] = {0};
    int len = snprintf(pid_to_string, sizeof(pid_to_string), "%d", getpid());
    if (write(fd, pid_to_string, len) != len) {
        printf("Error writing to file");
        close(fd);
        exit(1);
    }
    close(fd);

    // handle signals
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        printf("Error setting up SIGUSR1");
        unlink(path);
        exit(1);
    }
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        printf("Error setting up SIGINT");
        unlink(path);
        exit(1);
    }

    // wait for signals
    while (!end_process) {
        pause();
    }

    
    if (unlink(path) == -1) {
        printf("Error deleting .monitor_pid");
        exit(1);
    }
    
    return 0;
}