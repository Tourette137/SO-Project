#include <stdlib.h>
#include <string.h>

#include "../argus.h"
#include "../includes/task.h"

/**
 * @brief           Função que cria e inicializa uma estrutura task
 * @param id        ID da tarefa
 * @param pid       PID que executa a tarefa
 * @param command   Comando(s) associado à tarefa
 * @param status    Estado da tarefa
 */
TASK initTask(int id, pid_t pid, char* command, int status)
{
    TASK new_task = malloc(sizeof(struct task));

    new_task->id = id;
    new_task->pid = pid;
    new_task->command = strdup(command);
    new_task->status = status;

    return new_task;
}

/**
 * @brief           Função que liberta o espaço alocado para uma estrutura task
 * @param task      Apontador para uma estrutura task
 */
void freeTask(TASK task)
{
    free(task->command);
    free(task);
}
