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

pid_t child_pid;
int fd_result_output;

int max_inactivity_time;
int current_inactivity_time = 0;
int max_execution_time;
int current_execution_time = 0;

int exec_command(char*);
int execute_Chained_Commands(char*, int);


/**
 * @brief           Handler do SIGALRM da tarefa de comandos encadeados por pipes a executar
 * @param singum    Inteiro remetente ao tipo de sinal transmitido
 */
void SIGALRM_handler_server_child(int signum)
{
    current_execution_time++;
    if (current_execution_time > max_execution_time && max_execution_time >= 0) {
        kill(getppid(), SIGUSR1);
        _exit(EXIT_STATUS_EXECUTION_TIME);
    }

    alarm(1);
}

void SIGINT_handler_server_child(int signum)
{
    kill(child_pid, SIGINT);
    wait(NULL);
    kill(getppid(), SIGUSR1);
    _exit(EXIT_STATUS_TERMINATED_INTERRUPTED);
}

/**
 * @brief           Handler do SIGALRM para os processos que executam um comandos da tarefa de comandos encadeados por pipes a executar
 * @param singum    Inteiro remetente ao tipo de sinal transmitido
 */
void SIGALRM_handler_server_child_command(int signum)
{
    current_inactivity_time++;
    if (current_inactivity_time > max_inactivity_time && max_inactivity_time >= 0) {
        _exit(EXIT_STATUS_INACTIVITY);
    }

    alarm(1);
}

void SIGINT_handler_server_child_command(int signum)
{
    kill(child_pid, SIGKILL);
    _exit(0);
}


void server_child_start(int task_id, char* command)
{
    signal(SIGALRM, SIGALRM_handler_server_child);
    signal(SIGINT, SIGINT_handler_server_child);

    char result_output_filename[BUFFER_SIZE];
    sprintf(result_output_filename, "%s%d.txt", RESULT_OUTPUT_FILENAME, task_id);

    fd_result_output = open(result_output_filename, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd_result_output == -1) {
        perror("Open error file");
        fd_result_output = 1;
    }

    if (execute_Chained_Commands(command, task_id) == -1)
        perror("Execute task");

    kill(getppid(), SIGUSR1);
    _exit(EXIT_STATUS_TERMINATED);
}


/**
 * @brief           Função que executa um comando dado
 * @param command   Comando que queremos executar
 * @return          Inteiro que revela se comando pedido executou corretamente ou não
 */
int exec_command (char* command)
{
    int number_of_args = strcnt(command, ' ') + 1;
    char* exec_args[number_of_args];
    char* string;
    int exec_ret = 0, i = 0;

    string = strtok(command, " ");
    while (string != NULL) {
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
    for(int i = 0; i < number_of_commands; i++) {
        line = strsep(&commands,"|");
        commands_array[i] = strdup(line);
    }

    kill(getpid(), SIGALRM);
    // Execução dos comandos
    if (number_of_commands == 1) {

        pid_t fork_pid = fork();

        switch (fork_pid) {
            case -1:
                perror("Fork");
                return -1;
            case 0:
                dup2(fd_result_output, 1);
                exec_command(commands_array[0]);
                _exit(0);
            default:
                child_pid = fork_pid;
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

                pid_t fork_pid = fork();

                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        signal(SIGALRM, SIGALRM_handler_server_child_command);

                        // codigo do filho 0
                        close(p[i][0]);

                        dup2(p[i][1],1);
                        close(p[i][1]);

                        fork_pid = fork();

                        switch (fork_pid) {
                            case -1:
                                perror("Fork");
                                return -1;
                            case 0:
                                exec_command(commands_array[i]);
                            default:
                                child_pid = fork_pid;
                                kill(getpid(), SIGALRM);
                        }

                        wait(NULL);
                        _exit(EXIT_STATUS_TERMINATED);
                    default:
                        child_pid = fork_pid;
                        close(p[i][1]);

                }
            }
            else if (i == number_of_commands-1) {

                pid_t fork_pid = fork();

                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        // codigo do filho n-1
                        //close(p[i-1][1]); //Já está fechado do anterior
                        dup2(p[i-1][0],0);
                        close(p[i-1][0]);

                        dup2(fd_result_output, 1);

                        exec_command(commands_array[i]);
                    default:
                        child_pid = fork_pid;
                        close(p[i-1][0]);

                }
            }
            else {

                if (pipe(p[i]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                pid_t fork_pid = fork();

                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        signal(SIGALRM, SIGALRM_handler_server_child_command);

                        // codigo do filho i
                        //close(p[i-1][1]); //Fechado no anterior
                        close(p[i][0]);

                        dup2(p[i][1],1);
                        close(p[i][1]);

                        dup2(p[i-1][0],0);
                        close(p[i-1][0]);

                        fork_pid = fork();

                        switch (fork_pid) {
                            case -1:
                                perror("Fork");
                                return -1;
                            case 0:
                                exec_command(commands_array[i]);
                            default:
                                child_pid = fork_pid;
                                kill(getpid(), SIGALRM);
                        }

                        wait(NULL);
                        _exit(EXIT_STATUS_TERMINATED);
                    default:
                        child_pid = fork_pid;
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
