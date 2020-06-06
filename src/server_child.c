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
int exec_chained_commands(char*);


//----------------------------PARENT SIGNAL HANDLERS----------------------------//

/**
 * @brief           Signal usado para medir o tempo de execução de uma tarefa
 * @param signum    Identificador do signal recebido
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

/**
 * @brief           Signal usado para indicar a interrupção de uma tarefa
 * @param signum    Identificador do signal recebido
 */
void SIGINT_handler_server_child(int signum)
{
    kill(child_pid, SIGINT);
    wait(NULL);
    kill(getppid(), SIGUSR1);
    _exit(EXIT_STATUS_TERMINATED_INTERRUPTED);
}


//----------------------------CHILD SIGNAL HANDLERS----------------------------//

/**
 * @brief           Signal usado para medir o tempo de inatividade num pipe anónimo (apenas usado pelos filhos)
 * @param signum    Identificador do signal recebido
 */
void SIGALRM_handler_server_child_command(int signum)
{
    current_inactivity_time++;
    if (current_inactivity_time > max_inactivity_time && max_inactivity_time >= 0) {
        _exit(EXIT_STATUS_INACTIVITY);
    }

    alarm(1);
}

/**
 * @brief           Signal usado para indicar a interrupção de uma tarefa (apenas usado pelos filhos)
 * @param signum    Identificador do signal recebido
 */
void SIGINT_handler_server_child_command(int signum)
{
    kill(child_pid, SIGKILL);
    _exit(0);
}


//----------------------------MAIN FUNCTION----------------------------//

/**
 * @brief           Função que inicializa a execução de uma tarefa
 * @param task_id   ID da tarefa a ser inicializada
 * @param commands  Comandos a ser executados
 */
void server_child_start(int task_id, char* commands)
{
    signal(SIGALRM, SIGALRM_handler_server_child);
    signal(SIGINT, SIGINT_handler_server_child);

    // Open task temporary output file
    char result_output_filename[BUFFER_SIZE];
    sprintf(result_output_filename, "%s%d.txt", TASK_RESULT_OUTPUT_FILENAME, task_id);
    fd_result_output = open(result_output_filename, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd_result_output == -1) {
        perror("Open error file");
        fd_result_output = 1;
    }

    // Execute task command(s)
    if (exec_chained_commands(commands) == -1)
        perror("Execute task");

    // Warn server that task has been terminated
    kill(getppid(), SIGUSR1);

    // Exit with task terminated status
    _exit(EXIT_STATUS_TERMINATED);
}


//----------------------------SECONDARY FUNCTIONS----------------------------//

/**
 * @brief           Função que executa um comando
 * @param command   Comando a ser executado
 * @return		    Valor de retorno da execução do comando
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
 * @param commands  Lista de comandos a ser executados
 * @return          Inteiro que revela se execução de comandos correu bem
 */
int exec_chained_commands (char* commands)
{
    char* line;
    int number_of_commands = strcnt(commands, '|') + 1;
    char* commands_array[number_of_commands];
    int p[number_of_commands-1][2];
    int status [number_of_commands];

    // Parse commands from a string
    for(int i = 0; i < number_of_commands; i++) {
        line = strsep(&commands,"|");
        commands_array[i] = strdup(line);
    }

    // Start a timer to control task execution time
    kill(getpid(), SIGALRM);

    // Command(s) execution
    if (number_of_commands == 1) {

        // Create a child to run the command
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

            // First command execution
            if (i == 0) {

                if (pipe(p[i]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                // Create a child to run the command
                pid_t fork_pid = fork();
                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        signal(SIGALRM, SIGALRM_handler_server_child_command);

                        close(p[i][0]);

                        dup2(p[i][1],1);
                        close(p[i][1]);

                        // Create a child to run the command and start a timer to control pipe inactivity time
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
            // Last command execution
            else if (i == number_of_commands-1) {

                // Create a child to run the command
                pid_t fork_pid = fork();
                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        dup2(p[i-1][0],0);
                        close(p[i-1][0]);

                        dup2(fd_result_output, 1);

                        exec_command(commands_array[i]);
                    default:
                        child_pid = fork_pid;
                        close(p[i-1][0]);

                }
            }
            // Intermediate command(s) execution
            else {

                if (pipe(p[i]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                // Create a child to run the command
                pid_t fork_pid = fork();
                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        signal(SIGALRM, SIGALRM_handler_server_child_command);

                        close(p[i][0]);

                        dup2(p[i][1],1);
                        close(p[i][1]);

                        dup2(p[i-1][0],0);
                        close(p[i-1][0]);

                        // Create a child to run the command and start a timer to control pipe inactivity time
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

            // Wait for child to execute a command and interprete its exit status
            wait(&status[i]);
            if (WIFEXITED(status[i])) {
                if (WEXITSTATUS(status[i]) == EXIT_STATUS_INACTIVITY) {
                    kill(getppid(), SIGUSR1);
                    _exit(EXIT_STATUS_INACTIVITY);
                }
            }

        }
    }

    // Free memory allocated for storing the commands
    for (int c = 0; c < number_of_commands; c++) {
        free(commands_array[c]);
    }

    return 0;
}
