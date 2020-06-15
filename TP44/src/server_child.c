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

int fd_result_output;

int max_inactivity_time;
int max_execution_time;
int current_execution_time = 0;
pid_t current_execution_pid = -1;
pid_t current_execution_control_pid = -1;
pid_t next_execution_pid = -1;

void kill_active_childs();
int exec_command(char*);
int exec_chained_commands(char*);


//----------------------------PARENT SIGNAL HANDLERS----------------------------//

/**
 * @brief           Signal usado para medir o tempo de execução de uma tarefa
 * @param signum    Identificador do signal recebido
 */
void SIGALRM_handler_execution_time(int signum)
{
    kill_active_childs();
    kill(getppid(), SIGUSR1);
    _exit(EXIT_STATUS_EXECUTION_TIME);
}

/**
 * @brief           Signal usado para indicar a interrupção de uma tarefa
 * @param signum    Identificador do signal recebido
 */
void SIGINT_handler_server_child(int signum)
{
    kill_active_childs();
    kill(getppid(), SIGUSR1);
    _exit(EXIT_STATUS_TERMINATED_INTERRUPTED);
}

/**
 * @brief           Signal usado para indicar a interrupção de uma tarefa por tempo de inatividade
 * @param signum    Identificador do signal recebido
 */
void SIGUSR1_handler_server_child(int signum)
{
    kill_active_childs();
    kill(getppid(), SIGUSR1);
    _exit(EXIT_STATUS_INACTIVITY);
}


//----------------------------CHILD SIGNAL HANDLERS----------------------------//

/**
 * @brief           Signal usado para medir o tempo de inatividade num pipe anónimo (apenas usado pelos filhos)
 * @param signum    Identificador do signal recebido
 */
void SIGALRM_handler_inactivity_time(int signum)
{
    kill(getppid(), SIGUSR1);
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
    signal(SIGALRM, SIGALRM_handler_execution_time);
    signal(SIGINT, SIGINT_handler_server_child);
    signal(SIGUSR1, SIGUSR1_handler_server_child);

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

    // Wait for all the active childs to complete their tasks
    while (current_execution_pid != -1 || current_execution_control_pid != -1 || next_execution_pid != -1) {
        pid_t pid = wait(NULL);
        if (pid == current_execution_pid)
            current_execution_pid = -1;
        else if (pid == current_execution_control_pid)
            current_execution_control_pid = -1;
        else if (pid == next_execution_pid)
            next_execution_pid = -1;
    }

    // Warn server that task has been terminated
    kill(getppid(), SIGUSR1);

    // Exit with task terminated status
    _exit(EXIT_STATUS_TERMINATED);
}


//----------------------------SECONDARY FUNCTIONS----------------------------//

void kill_active_childs()
{
    if (current_execution_pid != -1) kill(current_execution_pid, SIGKILL);
    if (current_execution_control_pid != -1) kill(current_execution_control_pid, SIGKILL);
    if (next_execution_pid != -1) kill(next_execution_pid, SIGKILL);
}

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
    int p_inac[number_of_commands-1][2];
    pid_t fork_pid;

    // Parse commands from a string
    for(int i = 0; i < number_of_commands; i++) {
        line = strsep(&commands,"|");
        commands_array[i] = strdup(line);
    }

    // Start a timer to control task execution time
    if (max_execution_time > 0)
        alarm(max_execution_time);

    // Command(s) execution
    if (number_of_commands == 1) {

        // Create a child to run the command
        fork_pid = fork();
        switch (fork_pid) {
            case -1:
                perror("Fork");
                return -1;
            case 0:
                dup2(fd_result_output, 1);
                exec_command(commands_array[0]);
                _exit(0);
        }

        current_execution_pid = fork_pid;
    }
    else {
        for (int i = 0; i < number_of_commands; i++) {

            // First command execution
            if (i == 0) {
                if (pipe(p_inac[0]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                // Create a child to run the command
                fork_pid = fork();
                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        close(p_inac[0][0]);

                        dup2(p_inac[0][1],1);
                        close(p_inac[0][1]);

                        // Run the command
                        exec_command(commands_array[0]);
                        _exit(EXIT_STATUS_TERMINATED);
                    default:
                        current_execution_pid = fork_pid;
                        close(p_inac[0][1]);
                }


                // Launch a child that receives input from the previous command and sends it to the next, messuring inactivity time
                if (pipe(p[0]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                fork_pid = fork();
                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        if (max_inactivity_time > 0) {
                            signal(SIGALRM, SIGALRM_handler_inactivity_time);
                            alarm(max_inactivity_time);
                        }

                        close(p[0][0]);

                        size_t bytes_read;
                        char buffer[BUFFER_SIZE];
                        while ((bytes_read = read(p_inac[0][0], buffer, BUFFER_SIZE)) > 0)
                            write(p[0][1], buffer, bytes_read);

                        close(p[0][1]);
                        close(p_inac[0][0]);

                        _exit(0);
                    default:
                        current_execution_control_pid = fork_pid;
                        close(p_inac[0][0]);
                        close(p[0][1]);
                }
            }
            // Last command execution
            else if (i == number_of_commands-1) {

                // Create a child to run the command
                fork_pid = fork();
                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        dup2(p[i-1][0],0);
                        close(p[i-1][0]);

                        dup2(fd_result_output, 1);

                        exec_command(commands_array[i]);
                        _exit(EXIT_STATUS_TERMINATED);
                    default:
                        next_execution_pid = fork_pid;
                        close(p[i-1][0]);
                }
            }
            // Intermediate command(s) execution
            else {

                if (pipe(p_inac[i]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                // Create a child to run the command
                fork_pid = fork();
                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        close(p_inac[i][0]);

                        dup2(p_inac[i][1],1);
                        close(p_inac[i][1]);

                        dup2(p[i-1][0],0);
                        close(p[i-1][0]);

                        exec_command(commands_array[i]);
                        _exit(EXIT_STATUS_TERMINATED);
                    default:
                        next_execution_pid = fork_pid;
                        close(p[i-1][0]);
                        close(p_inac[i][1]);
                }


                // Wait for previous child to terminate execution to launch next childs
                waitpid(current_execution_pid, NULL, 0);
                waitpid(current_execution_control_pid, NULL, 0);
                current_execution_pid = next_execution_pid;


                // Launch a child that receives input from the previous command and sends it to the next, messuring inactivity time
                if (pipe(p[i]) != 0) {
                    perror("Pipe");
                    return -1;
                }

                fork_pid = fork();
                switch(fork_pid) {
                    case -1:
                        perror("Fork");
                        return -1;
                    case 0:
                        if (max_inactivity_time > 0) {
                            signal(SIGALRM, SIGALRM_handler_inactivity_time);
                            alarm(max_inactivity_time);
                        }

                        close(p[i][0]);

                        size_t bytes_read;
                        char buffer[BUFFER_SIZE];
                        while ((bytes_read = read(p_inac[i][0], buffer, BUFFER_SIZE)) > 0)
                            write(p[i][1], buffer, bytes_read);

                        close(p[i][1]);
                        close(p_inac[i][0]);

                        _exit(0);
                    default:
                        current_execution_control_pid = fork_pid;
                        close(p_inac[i][0]);
                        close(p[i][1]);
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
