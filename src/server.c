#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>

#include "../includes/macros.h"
#include "../includes/auxs.h"
#include "../includes/task.h"
#include "../includes/server_child.h"

//TODO:
//      - Server passar informação ao client, em vez de mandar para o stdout;
//      - Apanhar a puta;

int default_fd_error;

int max_inactivity_time = -1;
int max_execution_time = -1;
TASK* tasks_history = NULL;
int total_tasks_history = 0;
TASK* tasks_running = NULL;
int total_tasks_running = 0;

void launch_task_on_server(char*);
void read_client_command(char*);
void print_server_running_tasks();
void print_server_terminated_tasks();
void print_help_menu();
void add_task_to_server(char*);
void changeMaxInactivityTime(int);
void changeMaxExecutionTime(int);
void end_task_given (int, int, int);


void SIGUSR1_handler(int signum)
{
    int status;
    int pid = wait(&status);

    for(int i = 0; i < total_tasks_running; i++) {
        if(tasks_running[i]->pid == pid) {
            int task_id = tasks_running[i]->id;

            if (WIFEXITED(status)) {
                switch (WEXITSTATUS(status)) {
                    case EXIT_STATUS_TERMINATED:
                        end_task_given(task_id-1, i, TASK_TERMINATED);
                        printf("[DEBUG] TASK #%d done with exit code TASK_TERMINATED\n", task_id);
                        break;
                    case EXIT_STATUS_INACTIVITY:
                        end_task_given(task_id-1, i, TASK_TERMINATED_INACTIVITY);
                        printf("[DEBUG] TASK #%d done with exit code TASK_TERMINATED_INACTIVITY\n", task_id);
                        break;
                    case EXIT_STATUS_EXECUTION_TIME:
                        end_task_given(task_id-1, i, TASK_TERMINATED_EXECUTION_TIME);
                        printf("[DEBUG] TASK #%d done with exit code TASK_TERMINATED_EXECUTION_TIME\n", task_id);
                        break;
                    case EXIT_STATUS_TERMINATED_INTERRUPTED:
                        end_task_given(task_id-1, i, TASK_TERMINATED_INTERRUPTED);
                        printf("[DEBUG] TASK #%d done with exit code TASK_TERMINATED_INTERRUPTED\n", task_id);
                        break;
                }
            }
            break;
        }
    }
}



void SIGINT_handler_server(int signum)
{
    for (int i = 0; i < total_tasks_history; i++)
        freeTask(tasks_history[i]);
    free(tasks_history);

    for (int i = 0; i < total_tasks_running; i++)
        freeTask(tasks_running[i]);
    free(tasks_running);

    close(default_fd_error);

    printf("\n\n\n\tRunning Errors:\n\n");
    execlp("cat", "cat", ERROR_FILENAME, NULL);

    _exit(0);
}


int main(int argc, char const** argv)
{
    signal(SIGUSR1, SIGUSR1_handler);
    signal(SIGINT, SIGINT_handler_server);

    char buffer[BUFFER_SIZE];
    int fd_fifo;
    ssize_t bytes_read;
    default_fd_error = open(ERROR_FILENAME, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (default_fd_error == -1) {
        perror("Open error file");
        return -1;
    }
    dup2(default_fd_error,2);

    // Create pipe for communicating with the clients
    if (mkfifo(PIPENAME, 0666) == -1)
        perror("Mkfifo");

    // Run cicle, waiting for input from the client
    while (1) {

        bzero(buffer, BUFFER_SIZE);

        if ((fd_fifo = open(PIPENAME, O_RDONLY)) == -1) {
            perror("Open FIFO");
            return -1;
        }
        else {
            printf("[DEBUG] opened FIFO for reading\n");
        }

        while ((bytes_read = read(fd_fifo, buffer, BUFFER_SIZE)) > 0) {
            printf("[DEBUG] received '%s' from client\n", buffer);
            read_client_command(buffer);
            bzero(buffer, BUFFER_SIZE);
        }

        close(fd_fifo);
    }

    return 0;
}

/**
 * @brief           Função que recebe um comando(simplificado), interpreta-o e executa-o
 * @param command   Comando a ser interpretado e executado
 */
void read_client_command(char* command)
{
    if (strncmp(command, "-i", 2) == 0) {
        int n = atoi(command+3);
        changeMaxInactivityTime(n);
    }
    else if (strncmp(command, "-m", 2) == 0) {
        int n = atoi(command+3);
        changeMaxExecutionTime(n);
    }
    else if (strncmp(command, "-e", 2) == 0) {
        add_task_to_server(command+3);
        launch_task_on_server(command+3);
    }
    else if (strncmp(command, "-l", 2) == 0) {
        print_server_running_tasks();
    }
    else if (strncmp(command, "-t", 2) == 0) {
        int n = atoi(command+3);

        for (int i = 0; i < total_tasks_running; i++) {
            if (tasks_running[i]->id == n) {
                pid_t server_child_pid = tasks_running[i]->pid;
                kill(server_child_pid, SIGINT);
                break;
            }
        }

    }
    else if (strncmp(command, "-r", 2) == 0) {
        print_server_terminated_tasks();
    }
    else if (strncmp(command, "-h", 2) == 0) {
        print_help_menu();
    }
    else {
        printf("Received invalid input from client\n");
    }
}

void launch_task_on_server(char* command)
{
    int task_id = total_tasks_history;
    pid_t pid = fork();

    switch (pid) {
        case -1:
            perror("Fork");
            return;
        case 0:
            server_child_start(task_id, command);
        default:
            tasks_running[total_tasks_running-1]->pid = pid;
    }
}


/**
 * @brief       Função que imprime as tarefas que estão a correr no servidor
 */
void print_server_running_tasks()
{
    for(int i = 0; i < total_tasks_history; i++) {
        TASK aux_task = tasks_history[i];
        if (aux_task->status == TASK_RUNNING) {
            printf("#%d: ", i+1);
            printf("%s\n", aux_task->command);
        }
    }
}

/**
 * @brief       Função que imprime as tarefas que já terminaram de correr no servidor
 */
void print_server_terminated_tasks()
{
    for(int i = 0; i < total_tasks_history; i++) {
        TASK aux_task = tasks_history[i];
        if (aux_task->status != TASK_RUNNING) {
            printf("#%d, ", i+1);

            int aux_status = aux_task->status;
            if (aux_status == TASK_TERMINATED) printf("concluida: ");
            else if (aux_status == TASK_TERMINATED_INACTIVITY) printf("max inactividade: ");
            else if (aux_status == TASK_TERMINATED_EXECUTION_TIME) printf("max execução: ");
            else if (aux_status == TASK_TERMINATED_INTERRUPTED) printf("interrompida: ");

            printf("%s\n", aux_task->command);
        }
    }
}

/**
 * @brief       Função que imprime o menu de ajuda para o utilizador
 */
void print_help_menu()
{
    printf("tempo-inactividade segs\n");
    printf("tempo-execucao segs\n");
    printf("executar p1 | p2 ... | pn\n");
    printf("listar\n");
    printf("terminar #tarefa\n");
    printf("historico\n");
}

/**
 * @brief           Função que adiciona uma tarefa ao registo de tarefas do servidor
 * @param command   Comando da tarefa a ser adicionada ao servidor
 */
void add_task_to_server(char* command)
{
    total_tasks_history++;
    tasks_history = realloc(tasks_history, total_tasks_history * sizeof(TASK));
    tasks_history[total_tasks_history-1] = initTask(total_tasks_history, 0, command, TASK_RUNNING);

    total_tasks_running++;
    tasks_running = realloc(tasks_running, total_tasks_running * sizeof(TASK));
    tasks_running[total_tasks_running-1] = initTask(total_tasks_history, 0, command, TASK_RUNNING);
}


/**
 * @brief           Função que altera o tempo de Inatividade Máxima de comunicação entre um pipe anónimo
 * @param seconds   Tempo em segundos que queremos que seja o tempo de inatividade máxima
 */
void changeMaxInactivityTime (int seconds)
{
    max_inactivity_time = seconds;
}

/**
 * @brief           Função que altera o tempo de Execução Máxima de uma Tarefa
 * @param seconds   Tempo em segundos que queremos que seja o tempo de execução máxima de uma tarefa
 */
void changeMaxExecutionTime (int seconds)
{
    max_execution_time = seconds;
}

/**
 * @brief           Função que acaba com uma Task que esteja a ser executada
 * @param seconds   Tarefa que queremos terminar de maneira forçada
 */

void end_task_given (int task_ind, int task_running_ind, int terminated_status)
{
    tasks_history[task_ind]->status = terminated_status;

    tasks_running[task_running_ind] = tasks_running[total_tasks_running-1];
    total_tasks_running--;
    tasks_running = realloc(tasks_running, total_tasks_running);
}
