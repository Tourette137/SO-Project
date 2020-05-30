#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "macros.h"

int max_inactivity_time = 10;

int main(int argc, char const** argv)
{
    char buffer[MAX_ARG_SIZE];
    int fd_fifo;
    ssize_t bytes_read;

    // Create pipe for communicating with the clients
    if (mkfifo(PIPENAME, 0666) == -1)
        perror("Mkfifo");

    // Run cicle, waiting for input from the client
    while (1) {
        bzero(buffer, MAX_ARG_SIZE);

        if ((fd_fifo = open(PIPENAME, O_RDONLY)) == -1) {
          perror("Open FIFO");
          return -1;
        }
        else {
          printf("[DEBUG] opened FIFO for reading\n");
        }

        while (bytes_read = read(fd_fifo, buffer, MAX_ARG_SIZE) > 0) {
            printf("[DUBUG] received %s from client\n", buffer);
            bzero(buffer, MAX_ARG_SIZE);
        };

        close(fd_fifo);
    }

    return 0;
}
