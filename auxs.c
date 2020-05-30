#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "auxs.h"

ssize_t readln (int fd, char *buffer, size_t size) {

	int resultado = 0, total = 0;

	while ((resultado = read(fd, &buffer[total], 1)) > 0 && total < size) {
		if (buffer[total] == '\n') {
            buffer[total] = '\0';
            break;
        }

		total += resultado;
	}

	return total;
}
