#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "macros.h"

int max_inactivity_time = 10;
int max_running_time = 10;


int main(int argc, char const** argv)
{
    char buffer[MAX_ARG_SIZE];
    int fd_fifo;
    ssize_t bytes_read;

    // Create pipe for communicating with the clients
    if (mkfifo(PIPENAME, 0666) == -1)
        perror("Mkfifo");

    // Run cicle, waiting for input from the client
    while (1) {
        bzero(buffer, MAX_ARG_SIZE);

        if ((fd_fifo = open(PIPENAME, O_RDONLY)) == -1) {
          perror("Open FIFO");
          return -1;
        }
        else {
          printf("[DEBUG] opened FIFO for reading\n");
        }

        while ((bytes_read = read(fd_fifo, buffer, MAX_ARG_SIZE)) > 0) {
            printf("[DUBUG] received %s from client\n", buffer);
            bzero(buffer, MAX_ARG_SIZE);
        };

        close(fd_fifo);
    }

    return 0;
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
void changeMaxRunningTime (int seconds)
{
    max_running_time = seconds;
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
        };
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
int execut_Chained_Commands ()
{
    char buffer [MAX_ARG_SIZE];
    char* commands [MAX_COMMANDS]; //Tentar mudar de MAX_COMMANDS para um realloc que se ia fazendo ao longno do tempo
    char* command;
    char* line;
    int n;
    int p[MAX_COMMANDS-1][2];
    int number_of_commands = 0;
    int status[MAX_COMMANDS];

    n = read(0, buffer, MAX_ARG_SIZE);
    buffer[n-1] = '\0';
    line = strdup(buffer);


    command = strtok(line, "|");
    while(command != NULL) {
      commands[number_of_commands++] = strdup(command);
      command = strtok (NULL, "|");
    }


        if (number_of_commands == 1) {

            switch (fork()) {
                case -1:
                    perror("Fork não foi efetuado");
                    return -1;
                  case 0:
                    exec_command(commands[0]);
                    _exit(0);
              }

          } else {

                for (int i = 0; i < number_of_commands; i++) {

                    if (i == 0) {

                        if (pipe(p[i]) != 0) {
                          perror("Pipe not created.");
                          return -1;
                        }

                        switch(fork()) {
                            case -1:
                                perror("Fork não foi efetuado");
                                return -1;
                              case 0:
                                // codigo do filho 0

                                close(p[i][0]);

                                dup2(p[i][1],1);
                                close(p[i][1]);

                                exec_command(commands[i]);
                                _exit(0);

                              default:
                                close(p[i][1]);
                            }
                    }
                    else if (i == number_of_commands-1) {

                        if (pipe(p[i]) != 0) {
                          perror("Pipe not created.");
                          return -1;
                        }

                        switch(fork()) {
                            case -1:
                                perror("Fork não foi efetuado");
                                return -1;
                              case 0:
                                // codigo do filho n-1
                                //close(p[i-1][1]); //Já está fechado do anterior

                                dup2(p[i-1][0],0);
                                close(p[i-1][0]);

                                exec_command(commands[i]);

                                _exit(0);

                              default:
                                close(p[i-1][0]);
                            }
                    }
                    else {

                        if (pipe(p[i]) != 0) {
                          perror("Pipe not created.");
                          return -1;
                        }

                        switch(fork()) {
                            case -1:
                                perror("Fork não foi efetuado");
                                return -1;
                              case 0:
                                // codigo do filho i

                                //close(p[i-1][1]); //Fechado no anterior
                                close(p[i][0]);

                                dup2(p[i][1],1);
                                close(p[i][1]);

                                dup2(p[i-1][0],0);
                                  close(p[i-1][0]);

                                exec_command(commands[i]);

                                _exit(0);

                              default:
                                close(p[i][1]);
                                close(p[i-1][0]);
                            }
                    }
                }
            }

            for (int w = 0; w < number_of_commands; w++) {
              wait(&status[w]);
            }

            for (int c = 0; c < number_of_commands; c++) {
              free(commands[c]);
            }

            free(line);

    return 0;
}
