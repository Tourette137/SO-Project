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

void read_client_command(char*);
void add_task_to_server(char*);
void launch_task_on_server(char*);
void change_max_inactivity_time(int);
void change_max_execution_time(int);
void remove_task_from_server (int, int, int);

void write_task_output_from_log_file_to_client(int);
void write_task_output_to_log_file(int);
void write_task_output_to_client(int);
void write_output_to_client(char*);


//----------------------------SIGNAL HANDLERS----------------------------//

/**
 * @brief           Signal usado para informar o servidor que uma tarefa terminou a sua execução
 * @param signum    Identificador do signal recebido
 */
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
                        remove_task_from_server(task_id, i, TASK_TERMINATED);
                        printf("[DEBUG] TASK #%d done with exit code TASK_TERMINATED\n", task_id);
                        write_task_output_to_client(task_id);
                        write_task_output_to_log_file(task_id);
                        break;
                    case EXIT_STATUS_INACTIVITY:
                        remove_task_from_server(task_id, i, TASK_TERMINATED_INACTIVITY);
                        printf("[DEBUG] TASK #%d done with exit code TASK_TERMINATED_INACTIVITY\n", task_id);
                        break;
                    case EXIT_STATUS_EXECUTION_TIME:
                        remove_task_from_server(task_id, i, TASK_TERMINATED_EXECUTION_TIME);
                        printf("[DEBUG] TASK #%d done with exit code TASK_TERMINATED_EXECUTION_TIME\n", task_id);
                        break;
                    case EXIT_STATUS_TERMINATED_INTERRUPTED:
                        remove_task_from_server(task_id, i, TASK_TERMINATED_INTERRUPTED);
                        printf("[DEBUG] TASK #%d done with exit code TASK_TERMINATED_INTERRUPTED\n", task_id);
                        break;
                }
            }

            // Remove temporary output file created
            char result_output_filename[BUFFER_SIZE];
            sprintf(result_output_filename, "%s%d.txt", TASK_RESULT_OUTPUT_FILENAME, task_id);
            if (remove_file(result_output_filename) == -1)
                perror("Remove output file");

            break;
        }
    }
}

/**
 * @brief           Signal usado para terminar a execução do servidor
 * @param signum    Identificador do signal recebido
 */
void SIGINT_handler_server(int signum)
{
    // Remove all temporary files related to running tasks
    char buffer[BUFFER_SIZE];
    for (int i = 0; i < total_tasks_running; i++) {
        sprintf(buffer, "%s%d.txt", TASK_RESULT_OUTPUT_FILENAME, tasks_running[i]->id);
        remove_file(buffer);
    }

    // Free tasks history alocated memory
    for (int i = 0; i < total_tasks_history; i++)
        freeTask(tasks_history[i]);
    free(tasks_history);

    // Free tasks running alocated memory
    for (int i = 0; i < total_tasks_running; i++)
        freeTask(tasks_running[i]);
    free(tasks_running);

    // Close file descriptors
    close(default_fd_error);
    close(fd_fifo_client_server);
    close(fd_fifo_server_client);

    // Print running errors
    printf("\n\n\n\tRunning Errors:\n\n");
    execlp("cat", "cat", ERROR_FILENAME, NULL);

    _exit(0);
}


//----------------------------MAIN FUNCTION----------------------------//

int main(int argc, char const** argv)
{
    signal(SIGUSR1, SIGUSR1_handler);
    signal(SIGINT, SIGINT_handler_server);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

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

    // Remove previously created log file
    if (remove_file(LOG_FILENAME) == -1)
        perror("Remove log file");

    // Remove previously created log index file
    if (remove_file(LOG_INDEX_FILENAME) == -1)
        perror("Remove log index file");

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


//----------------------------SECONDARY FUNCTIONS----------------------------//

/**
 * @brief           Função que recebe um comando(simplificado), interpreta-o e executa-o
 * @param command   Comando a ser interpretado e executado
 */
void read_client_command(char* command)
{
    if (strncmp(command, "-i", 2) == 0) {
        int n = atoi(command+3);
        change_max_inactivity_time(n);
        write_output_to_client(command);
    }
    else if (strncmp(command, "-m", 2) == 0) {
        int n = atoi(command+3);
        change_max_execution_time(n);
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

        write_output_to_client(command);
    }
    else if (strncmp(command, "-r", 2) == 0) {
        write_output_to_client(command);
    }
    else if (strncmp(command, "-h", 2) == 0) {
        write_output_to_client(command);
    }
    else if (strncmp(command, "-o", 2) == 0) {
        int task_id = atoi(command+3);
        if (task_id <= total_tasks_history && tasks_history[task_id-1]->status == EXIT_STATUS_TERMINATED)
            write_task_output_from_log_file_to_client(task_id);
        else
            write_output_to_client(command);
    }
    else {
        printf("Received invalid input from client\n");
        write_output_to_client(command);
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
 * @brief           Função que inicia a execução de uma tarefa
 * @param command   Comando da tarefa a ser executada
 */
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
 * @brief           Função que altera o tempo de Inatividade Máxima de comunicação entre um pipe anónimo
 * @param seconds   Tempo de inatividade máxima (em segundos)
 */
void change_max_inactivity_time (int seconds)
{
    max_inactivity_time = seconds;
}

/**
 * @brief           Função que altera o tempo de Execução Máxima de uma Tarefa
 * @param seconds   Tempo de execução máxima (em segundos)
 */
void change_max_execution_time (int seconds)
{
    max_execution_time = seconds;
}

/**
 * @brief                       Função que remove uma tarefa dos registos de tarefas do servidor
 * @param task_id               ID da tarefa que queremos remover
 * @param task_running_ind      Índice da tarefa no registo de tarefas a ser executadas do servidor
 * @param terminated_status     Estado de término da tarefa
 */
void remove_task_from_server (int task_id, int task_running_ind, int terminated_status)
{
    tasks_history[task_id-1]->status = terminated_status;

    tasks_running[task_running_ind] = tasks_running[total_tasks_running-1];
    total_tasks_running--;
    tasks_running = realloc(tasks_running, total_tasks_running);
}


//----------------------------WRITE OUTPUT FUNCTIONS----------------------------//

/**
 * @brief           Função que lê o output de uma tarefa no ficheiro de logs e manda a informação para o cliente
 * @param task_id   ID da tarefa correspondente
 */
void write_task_output_from_log_file_to_client(int task_id)
{
    // Open log file
    int fd_log_file = open(LOG_FILENAME, O_RDONLY);

    // Open log index file
    int fd_log_index_file = open(LOG_INDEX_FILENAME, O_RDONLY);

    off_t start_pos;
    int total_bytes = -1;

    // Reading log index file to find task output position in log file
    char buffer[BUFFER_SIZE];
    char* aux_string;
    int aux_task_id;

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        readln(fd_log_index_file, buffer, BUFFER_SIZE);
        aux_string = strtok(buffer, ":");
        aux_task_id = atoi(aux_string);
        if (aux_task_id == task_id) {
            aux_string = strtok(NULL, ":");
            start_pos = atoi(aux_string);
            aux_string = strtok(NULL, ":");
            total_bytes = atoi(aux_string);
            break;
        }
    }

    // Set log file offset to task output start
    lseek(fd_log_file, start_pos, SEEK_SET);

    // Reading log file and writing to client
    kill(client_pid, SIGUSR1);

    size_t bytes_read, total_bytes_read = 0;

    sprintf(buffer, "TASK #%d RESULT\n", task_id);
    write(fd_fifo_server_client, buffer, strlen(buffer));

    while (total_bytes_read < total_bytes) {
        bzero(buffer, BUFFER_SIZE);
        bytes_read = read(fd_log_file, buffer, BUFFER_SIZE);
        total_bytes_read += bytes_read;

        if (total_bytes_read > total_bytes) {
            bytes_read = bytes_read - (total_bytes_read - total_bytes);
        }
        write(fd_fifo_server_client, buffer, bytes_read);
    }

    write(fd_fifo_server_client, PIPE_COMMUNICATION_EOF, PIPE_COMMUNICATION_EOF_SIZE);

    // Close file descriptors
    close(fd_log_file);
    close(fd_log_index_file);
}

/**
 * @brief           Função que lê o output de uma tarefa de um ficheiro temporário e escreve no ficheiro de logs
 * @param task_id   ID da tarefa correspondente
 */
void write_task_output_to_log_file(int task_id)
{
    // Open task output temporary file
    char result_output_filename[BUFFER_SIZE];
    sprintf(result_output_filename, "%s%d.txt", TASK_RESULT_OUTPUT_FILENAME, task_id);
    int fd_result_output = open(result_output_filename, O_RDONLY);

    // Open log file
    int fd_log_file = open(LOG_FILENAME, O_CREAT | O_WRONLY | O_APPEND, 0600);

    // Open log index file
    int fd_log_index_file = open(LOG_INDEX_FILENAME, O_CREAT | O_WRONLY | O_APPEND, 0600);

    // Set variable to write on file index file
    off_t start_pos = lseek(fd_log_file, 0, SEEK_END);
    int total_bytes = 0;

    // Reading from task output file and writing to log file
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // Updating log file
    while ((bytes_read = read(fd_result_output, buffer, BUFFER_SIZE)) > 0) {
        write(fd_log_file, buffer, bytes_read);
        total_bytes += bytes_read;
    }

    // Updating log index file
    bytes_read = sprintf(buffer, "%d:%ld:%d\n", task_id, start_pos, total_bytes);
    write(fd_log_index_file, buffer, bytes_read);

    // Close file descriptors
    close(fd_result_output);
    close(fd_log_file);
    close(fd_log_index_file);
}

/**
 * @brief           Função que lê o output de uma tarefa de um ficheiro temporário e manda a informação para o cliente
 * @param task_id   ID da tarefa correspondente
 */
void write_task_output_to_client(int task_id)
{
    char result_output_filename[BUFFER_SIZE];
    sprintf(result_output_filename, "%s%d.txt", TASK_RESULT_OUTPUT_FILENAME, task_id);
    int fd_result_output = open(result_output_filename, O_RDONLY);

    kill(client_pid, SIGUSR1);

    size_t bytes_read;
    char buffer[BUFFER_SIZE];

    sprintf(buffer, "TASK #%d RESULT\n", task_id);
    write(fd_fifo_server_client, buffer, strlen(buffer));

    while ((bytes_read = read(fd_result_output, buffer, BUFFER_SIZE)) > 0) {
        write(fd_fifo_server_client, buffer, bytes_read);
    }

    write(fd_fifo_server_client, PIPE_COMMUNICATION_EOF, PIPE_COMMUNICATION_EOF_SIZE);

    close(fd_result_output);
}

/**
 * @brief           Função que envia o output de um comando para o cliente
 * @param task_id   Comando correspondente
 */
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
