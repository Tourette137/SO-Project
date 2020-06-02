#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>

#include "macros.h"
#include "auxs.h"
#include "task.h"

//TODO:
//      - Server passar informação ao client, em vez de mandar para o stdout;
//      - Usar o max_execution_time e max_inactivity_time (bota SIGALRM nisto); ESTÁ FEITO, MAS VERIFICAR
//      - Apanhar a puta;

int server_pid;
int execution_seconds_passed = 0;
int inactivity_seconds_passed = 0;
int max_inactivity_time = 10;
int max_execution_time = 10;
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
int exec_command(char*);
int execute_Chained_Commands(char*, int);

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
                        break;
                    case EXIT_STATUS_INACTIVITY:
                        end_task_given(task_id-1, i, TASK_TERMINATED_INACTIVITY);
                        break;
                    case EXIT_STATUS_EXECUTION_TIME:
                        end_task_given(task_id-1, i, TASK_TERMINATED_EXECUTION_TIME);
                        break;
                }
            }

        }
    }
}


/**
 * @brief           Handler do SIGALRM da tarefa de comandos encadeados por pipes a executar
 * @param singum    Inteiro remetente ao tipo de sinal transmitido
 */
void SIGALRM_handler_server_child(int signum)
{
    execution_seconds_passed++;
    if (execution_seconds_passed == max_execution_time) {
        kill(getppid(), SIGUSR1);
        _exit(EXIT_STATUS_EXECUTION_TIME);
    }

    alarm(1);
}

/**
 * @brief           Handler do SIGALRM para os processos que executam um comandos da tarefa de comandos encadeados por pipes a executar
 * @param singum    Inteiro remetente ao tipo de sinal transmitido
 */
void SIGALRM_handler_server_child_command(int signum)
{
    inactivity_seconds_passed++;
    if (inactivity_seconds_passed >= max_inactivity_time) {
        _exit(EXIT_STATUS_INACTIVITY);
    }

    alarm(1);
}


int main(int argc, char const** argv)
{
    signal(SIGUSR1, SIGUSR1_handler);
    //signal(SIGALRM, SIGALRM_handler);

    char buffer[BUFFER_SIZE];
    int fd_fifo;
    ssize_t bytes_read;

    // Store server PID for later use
    server_pid = getpid();

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

    for (int i = 0; i < total_tasks_history; i++) {
        freeTask(tasks_history[i]);
    }
    free(tasks_history);

    for (int i = 0; i < total_tasks_running; i++)
        freeTask(tasks_running[i]);
    free(tasks_running);

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
                kill(tasks_running[i]->pid, SIGKILL);
                end_task_given(n-1, i, TASK_TERMINATED_INTERRUPTED);
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
            signal(SIGALRM, SIGALRM_handler_server_child);
            if (execute_Chained_Commands(command, task_id) == -1)
                perror("Execute task");
            printf("[DEBUG] job done...\n");
            kill(getppid(), SIGUSR1);
            _exit(EXIT_STATUS_TERMINATED);
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


/**
 * @brief           Função que executa um comando dado
 * @param command   Comando que queremos executar
 * @return          Inteiro que revela se comando pedido executou corretamente ou não
 */
int exec_command (char* command)
{
    char** exec_args = malloc(sizeof(char*)); //Aqui usamos assim para comandos terem a extensão que bem entenderem
    char* string;
    int exec_ret=0, i=0;

    string = strtok(command, " ");
    while (string != NULL) {
        if (realloc(exec_args, (i+1)*(sizeof(char*))) == NULL) { //Necessidade de fazer um realloc para aumentar o numero de argumentos de um mesmo comando
            perror("Realloc não executado");
        }
        exec_args[i] = string;
        string = strtok(NULL, " ");
        i++;
    }
    exec_args[i] = NULL;

    exec_ret = execvp(exec_args[0], exec_args);

    return exec_ret;
}


/**
 * @brief           Função que executa uma lista de comandos em execução encadeada através de pipes
 * @return          Inteiro que revela se execução de comandos correu bem
 */
int execute_Chained_Commands (char* commands, int id)
{
    char* line;
    int number_of_commands = strcnt(commands, '|') + 1;
    char* commands_array[number_of_commands];
    int p[number_of_commands-1][2];
    int status [number_of_commands];

    // Parsing da string com os comandos para um array com os comandos
    line = strtok(commands, "|");
    for(int i = 0; i < number_of_commands; i++) {
        commands_array[i] = strdup(line);
        line = strtok (NULL, "|");
    }

    alarm(1);
    // Execução dos comandos
    if (number_of_commands == 1) {

        switch (fork()) {
            case -1:
                perror("Fork");
                return -1;
            case 0:
                exec_command(commands_array[0]);
                _exit(0);
            default:
                wait(&status[0]);
        }
    }
    else {

        for (int i = 0; i < number_of_commands; i++) {

            if (i == 0) {

                if (pipe(p[i]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                switch(fork()) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        signal(SIGALRM, SIGALRM_handler_server_child_command);
                        alarm(1);

                        // codigo do filho 0
                        close(p[i][0]);

                        dup2(p[i][1],1);
                        close(p[i][1]);

                        exec_command(commands_array[i]);
                        _exit(0);
                    default:
                        close(p[i][1]);

                }
            }
            else if (i == number_of_commands-1) {

                if (pipe(p[i]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                switch(fork()) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        signal(SIGALRM, SIGALRM_handler_server_child_command);
                        alarm(1);
                        // codigo do filho n-1
                        //close(p[i-1][1]); //Já está fechado do anterior
                        dup2(p[i-1][0],0);
                        close(p[i-1][0]);

                        exec_command(commands_array[i]);
                        _exit(0);
                    default:
                        close(p[i-1][0]);

                }
            }
            else {

                if (pipe(p[i]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                switch(fork()) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        signal(SIGALRM, SIGALRM_handler_server_child_command);
                        alarm(1);
                        // codigo do filho i
                        //close(p[i-1][1]); //Fechado no anterior
                        close(p[i][0]);

                        dup2(p[i][1],1);
                        close(p[i][1]);

                        dup2(p[i-1][0],0);
                        close(p[i-1][0]);

                        exec_command(commands_array[i]);
                        _exit(0);
                    default:
                        close(p[i][1]);
                        close(p[i-1][0]);

                }
            }

            wait(&status[i]);
            if (WIFEXITED(status[i])) {
                if (WEXITSTATUS(status[i]) == EXIT_STATUS_INACTIVITY) {
                    kill(getppid(), SIGUSR1);
                    _exit(EXIT_STATUS_INACTIVITY);
                }
            }

        }
    }

    for (int c = 0; c < number_of_commands; c++) {
        free(commands_array[c]);
    }

    return 0;
}
