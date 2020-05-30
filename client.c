#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "macros.h"
#include "auxs.h"

int main(int argc, char const** argv)
{
    int fd_fifo;
    char buffer[MAX_ARG_SIZE];
    ssize_t bytes_read;

    if ((fd_fifo = open(PIPENAME, O_WRONLY)) == -1)
        perror("Open");
    else
        printf("[DEBUG] opened FIFO for writing\n");

    while ((bytes_read = readln(0, buffer, MAX_ARG_SIZE)) > 0) {
        write(fd_fifo, buffer, bytes_read);
        printf("[DEBUG] wrote %s to fifo\n", buffer);
    }

    close(fd_fifo);

    return 0;
}
