#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../includes/macros.h"
#include "../includes/auxs.h"

int execution_mode;
int fd_fifo_client_server;
int fd_fifo_server_client;

void simplify_command(char*, size_t);
void read_output_from_server();


//----------------------------SIGNAL HANDLERS----------------------------//

/**
 * @brief           Signal usado para informar o cliente que uma tarefa terminou a sua execução
 * @param signum    Identificador do signal recebido
 */
void SIGUSR1_handler_client(int signum)
{
    read_output_from_server();

    if (execution_mode == COMMAND_LINE_EXECUTION_MODE)
        _exit(0);
    else
        write(1, "argus$ ", 7);
}


//----------------------------MAIN FUNCTION----------------------------//

int main(int argc, char const** argv)
{
    signal(SIGUSR1, SIGUSR1_handler_client);

    char buffer[BUFFER_SIZE];
    size_t bytes_read = 1;

    // Open pipe for client->server communication
    if ((fd_fifo_client_server = open(CLIENT_SERVER_PIPENAME, O_WRONLY)) == -1)
        perror("Open client->server pipe");
    else if (DEBUG_STATUS)
        printf("[DEBUG] opened FIFO for writing\n");

    // Open pipe for server->client communication
    if ((fd_fifo_server_client = open(SERVER_CLIENT_PIPENAME, O_RDONLY)) == -1)
        perror("Open server->client pipe");
    else if (DEBUG_STATUS)
        printf("[DEBUG] opened FIFO for reading\n");

    // Send client pid to server
    sprintf(buffer, "%d", getpid());
    write(fd_fifo_client_server, buffer, strlen(buffer));

    // Set current execution mode
    execution_mode = argc > 1 ? COMMAND_LINE_EXECUTION_MODE : INTERPRETER_EXECUTION_MODE;

    // Read input from command line arguments and send it to server
    if (execution_mode == COMMAND_LINE_EXECUTION_MODE) {
        // Parsing command from arguments
        strcpy(buffer, argv[1]);
        for (int i = 2; i < argc; i++) {
            strcat(buffer, " ");
            strcat(buffer, argv[i]);
        }

        // Send command to server
        write(fd_fifo_client_server, buffer, strlen(buffer));
        if (DEBUG_STATUS) printf("[DEBUG] wrote '%s' to fifo\n", buffer);

        // Wait for server to finish command execution
        while(1);
    }

    // Run an interpreter an send the information to server
    if (execution_mode == INTERPRETER_EXECUTION_MODE) {
        write(1, "argus$ ", 7);
        while (1) {
            // Read input from user
            bytes_read = readln(0, buffer, BUFFER_SIZE);

            if (strcmp(buffer, "exit") == 0) break;

            // Interprete input and send it to server
            simplify_command(buffer, bytes_read);
            write(fd_fifo_client_server, buffer, bytes_read);
            if (DEBUG_STATUS) printf("[DEBUG] wrote '%s' to fifo\n", buffer);

            if (strncmp(buffer,"-e",2) == 0)
                write(1, "argus$ ", 7);
        }
    }

    // Close file descriptors
    close(fd_fifo_client_server);

    return 0;
}


//----------------------------SECONDARY FUNCTIONS----------------------------//

/**
 * @brief   Função que recebe, do servidor, o output de uma tarefa ou comando
 */
void read_output_from_server()
{
    size_t bytes_read;
    char buffer[BUFFER_SIZE];

    write(1,"\n\n",2);

    while (1) {
        // Read output send from server
        bzero(buffer, BUFFER_SIZE);
        bytes_read = read(fd_fifo_server_client, buffer, BUFFER_SIZE);

        // Write output received to stdout, always checking for EOF
        if (strstr(buffer,PIPE_COMMUNICATION_EOF) != NULL) {
            write(1, buffer, bytes_read - PIPE_COMMUNICATION_EOF_SIZE);
            break;
        }
        else {
            write(1, buffer, bytes_read);
        }
    }

    write(1,"\n",1);
}

/**
 * @brief           Função que recebe uma string com um comando passado pelo stdin(extenso) e tranforma num comando a ser interpretado pelo servidor(simplificado)
 * @param command   Comando a ser simplificado
 * @param size      Tamanho do comando recebido
 */
void simplify_command(char* command, size_t size)
{
    char command_i[] = "tempo-inactividade ";
    char command_m[] = "tempo-execucao ";
    char command_e[] = "executar ";
    char command_l[] = "listar";
    char command_t[] = "terminar ";
    char command_r[] = "historico";
    char command_h[] = "ajuda";
    char command_o[] = "output ";

    if (strncmp(command, command_i, strlen(command_i)) == 0) {
        char* aux = command+strlen(command_i);
        char simplified_command[] = "-i ";
        strcat(simplified_command, aux);

        strcpy(command, simplified_command);
    }
    else if (strncmp(command, command_m, strlen(command_m)) == 0) {
        char* aux = command+strlen(command_m);
        char simplified_command[] = "-m ";
        strcat(simplified_command, aux);

        strcpy(command, simplified_command);
    }
    else if (strncmp(command, command_e, strlen(command_e)) == 0) {
        char* aux = command+strlen(command_e);
        char simplified_command[] = "-e ";
        strcat(simplified_command, aux);

        strcpy(command, simplified_command);
    }
    else if (strncmp(command, command_l, strlen(command_l)) == 0) {
        strcpy(command, "-l");
    }
    else if (strncmp(command, command_t, strlen(command_t)) == 0) {
        char* aux = command+strlen(command_t);
        char simplified_command[] = "-t ";
        strcat(simplified_command, aux);

        strcpy(command, simplified_command);
    }
    else if (strncmp(command, command_r, strlen(command_r)) == 0) {
        strcpy(command, "-r");
    }
    else if (strncmp(command, command_h, strlen(command_h)) == 0) {
        strcpy(command, "-h");
    }
    else if (strncmp(command, command_o, strlen(command_o)) == 0) {
        char* aux = command+strlen(command_o);
        char simplified_command[] = "-o ";
        strcat(simplified_command, aux);

        strcpy(command, simplified_command);
    }
}
