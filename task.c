#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "task.h"

TASK initTask(int id, pid_t pid, char* command, int status)
{
    TASK new_task = malloc(sizeof(struct task));

    new_task->id = id;
    new_task->pid = pid;
    new_task->command = strdup(command);
    new_task->status = status;

    return new_task;
}

void freeTask(TASK task)
{
    free(task->command);
    free(task);
}
