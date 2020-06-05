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
//      - Apanhar a puta;

pid_t client_pid;
int default_fd_error;
int fd_fifo_client_server;
int fd_fifo_server_client;

int max_inactivity_time = -1;
int max_execution_time = -1;
TASK* tasks_history = NULL;
int total_tasks_history = 0;
TASK* tasks_running = NULL;
int total_tasks_running = 0;

void write_task_output_to_client(int);
int remove_file(char*);
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
                        write_task_output_to_client(task_id);
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
    ssize_t bytes_read;

    // Open file for replacing stderror
    default_fd_error = open(ERROR_FILENAME, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (default_fd_error == -1) {
        perror("Open error file");
        return -1;
    }
    dup2(default_fd_error,2);

    // Create pipe for client->server communication
    if (mkfifo(CLIENT_SERVER_PIPENAME, 0666) == -1)
        perror("Mkfifo client->server");

    // Create pipe for server->client communication
    if (mkfifo(SERVER_CLIENT_PIPENAME, 0666) == -1)
        perror("Mkfifo server->client");


    // Run cicle, waiting for input from the client
    while (1) {

        bzero(buffer, BUFFER_SIZE);

        // Open pipe for client->server communication
        if ((fd_fifo_client_server = open(CLIENT_SERVER_PIPENAME, O_RDONLY)) == -1) {
            perror("Open client->server pipe");
            return -1;
        }
        else {
            printf("[DEBUG] opened FIFO for reading\n");
        }

        // Open pipe for server->client communication
        if ((fd_fifo_server_client = open(SERVER_CLIENT_PIPENAME, O_WRONLY)) == -1) {
            perror("Open server->client pipe");
            return -1;
        }
        else
            printf("[DEBUG] opened FIFO for writing\n");

        // Get client PID
        read(fd_fifo_client_server, buffer, BUFFER_SIZE);
        client_pid = atoi(buffer);

        while ((bytes_read = read(fd_fifo_client_server, buffer, BUFFER_SIZE)) > 0) {
            printf("[DEBUG] received '%s' from client\n", buffer);
            read_client_command(buffer);
            bzero(buffer, BUFFER_SIZE);
        }

        close(fd_fifo_client_server);
        close(fd_fifo_server_client);
    }

    return 0;
}

void write_task_output_to_client(int task_id)
{
    char result_output_filename[BUFFER_SIZE];
    sprintf(result_output_filename, "%s%d.txt", RESULT_OUTPUT_FILENAME, task_id);
    int fd_result_output = open(result_output_filename, O_RDONLY);

    kill(client_pid, SIGUSR1);

    ssize_t bytes_read;
    char buffer[BUFFER_SIZE];

    sprintf(buffer, "TASK #%d RESULT\n", task_id);
    write(fd_fifo_server_client, buffer, strlen(buffer));

    while ((bytes_read = read(fd_result_output, buffer, BUFFER_SIZE)) > 0) {
        write(fd_fifo_server_client, buffer, bytes_read);
    }

    write(fd_fifo_server_client, PIPE_COMMUNICATION_EOF, PIPE_COMMUNICATION_EOF_SIZE);

    close(fd_result_output);

    if (remove_file(result_output_filename) == -1)
        perror("Remove output file");
}

int remove_file(char* filename)
{
    int status;

    switch (fork()) {
        case -1:
            perror("Fork");
            return -1;
        case 0:
            execlp("rm", "rm", filename, NULL);
        default:
            wait(&status);
    }

    return status;
}

void write_output_to_client(char* command)
{
    char buffer[BUFFER_SIZE];
    int written_bytes;

    kill(client_pid, SIGUSR1);

    if (strncmp(command,"-l",2) == 0) {
        for(int i = 0; i < total_tasks_history; i++) {
            TASK aux_task = tasks_history[i];
            if (aux_task->status == TASK_RUNNING) {
                bzero(buffer, BUFFER_SIZE);
                written_bytes = sprintf(buffer, "#%d: %s\n", i+1, aux_task->command);
                write(fd_fifo_server_client, buffer, written_bytes);
            }
        }
    }
    else if (strncmp(command,"-r",2) == 0) {
        for(int i = 0; i < total_tasks_history; i++) {
            TASK aux_task = tasks_history[i];
            if (aux_task->status != TASK_RUNNING) {
                bzero(buffer, BUFFER_SIZE);
                written_bytes = sprintf(buffer, "#%d, ", i+1);

                int aux_status = aux_task->status;
                if (aux_status == TASK_TERMINATED) written_bytes += sprintf(buffer+written_bytes, "concluida: ");
                else if (aux_status == TASK_TERMINATED_INACTIVITY) written_bytes += sprintf(buffer+written_bytes, "max inactividade: ");
                else if (aux_status == TASK_TERMINATED_EXECUTION_TIME) written_bytes += sprintf(buffer+written_bytes, "max execução: ");
                else if (aux_status == TASK_TERMINATED_INTERRUPTED) written_bytes += sprintf(buffer+written_bytes, "interrompida: ");

                written_bytes += sprintf(buffer+written_bytes, "%s\n", aux_task->command);

                write(fd_fifo_server_client, buffer, written_bytes);
            }
        }
    }
    else if (strncmp(command,"-h",2) == 0) {
        bzero(buffer, BUFFER_SIZE);
        written_bytes = 0;
        written_bytes += sprintf(buffer+written_bytes, "tempo-inactividade segs\n");
        written_bytes += sprintf(buffer+written_bytes, "tempo-execucao segs\n");
        written_bytes += sprintf(buffer+written_bytes, "executar p1 | p2 ... | pn\n");
        written_bytes += sprintf(buffer+written_bytes, "listar\n");
        written_bytes += sprintf(buffer+written_bytes, "terminar #tarefa\n");
        written_bytes += sprintf(buffer+written_bytes, "historico\n");

        write(fd_fifo_server_client, buffer, written_bytes);
    }
    else {
        write(fd_fifo_server_client, " ", 1);
    }

    write(fd_fifo_server_client, PIPE_COMMUNICATION_EOF, PIPE_COMMUNICATION_EOF_SIZE);
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
        write_output_to_client(command);
    }
    else if (strncmp(command, "-m", 2) == 0) {
        int n = atoi(command+3);
        changeMaxExecutionTime(n);
        write_output_to_client(command);
    }
    else if (strncmp(command, "-e", 2) == 0) {
        add_task_to_server(command+3);
        launch_task_on_server(command+3);
    }
    else if (strncmp(command, "-l", 2) == 0) {
        write_output_to_client(command);
    }
    else if (strncmp(command, "-t", 2) == 0) {
        int task_id = atoi(command+3);

        for (int i = 0; i < total_tasks_running; i++) {
            if (tasks_running[i]->id == task_id) {
                pid_t server_child_pid = tasks_running[i]->pid;
                kill(server_child_pid, SIGINT);
                break;
            }
        }

        char result_output_filename[BUFFER_SIZE];
        sprintf(result_output_filename, "%s%d.txt", RESULT_OUTPUT_FILENAME, task_id);
        if (remove_file(result_output_filename) == -1)
            perror("Remove output file");

        write_output_to_client(command);
    }
    else if (strncmp(command, "-r", 2) == 0) {
        write_output_to_client(command);
    }
    else if (strncmp(command, "-h", 2) == 0) {
        write_output_to_client(command);
    }
    else {
        printf("Received invalid input from client\n");
        write_output_to_client(command);
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
