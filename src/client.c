#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "../includes/macros.h"
#include "../includes/auxs.h"

void simplify_command(char*, ssize_t);
void read_task_output_from_server();

int main(int argc, char const** argv)
{
    int fd_fifo_client_server;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = 1;

    // Open pipe for client->server communication
    if ((fd_fifo_client_server = open(CLIENT_SERVER_PIPENAME, O_WRONLY)) == -1)
        perror("Open client->server pipe");
    else
        printf("[DEBUG] opened FIFO for writing\n");

    if (argc > 1) {
        strcpy(buffer, argv[1]);
        for (int i = 2; i < argc; i++) {
            strcat(buffer, " ");
            strcat(buffer, argv[i]);
        }

        write(fd_fifo_client_server, buffer, strlen(buffer));
        printf("[DEBUG] wrote '%s' to fifo\n", buffer);

        read_task_output_from_server();
    }
    else {

        write(1, "    $ ", 6);
        while ((bytes_read = readln(0, buffer, BUFFER_SIZE)) > 0 && strcmp(buffer, "exit") != 0) {
            simplify_command(buffer, bytes_read);

            write(fd_fifo_client_server, buffer, bytes_read);
            printf("[DEBUG] wrote '%s' to fifo\n", buffer);

            read_task_output_from_server();

            write(1, "    $ ", 6);
        }
    }

    close(fd_fifo_client_server);

    return 0;
}

void read_task_output_from_server()
{
    ssize_t bytes_read;
    char buffer[BUFFER_SIZE];
    int fd_fifo_server_client;

    // Open pipe for server->client communication
    if ((fd_fifo_server_client = open(SERVER_CLIENT_PIPENAME, O_RDONLY)) == -1)
        perror("Open server->client pipe");
    else
        printf("[DEBUG] opened FIFO for reading\n");

    while ((bytes_read = read(fd_fifo_server_client, buffer, BUFFER_SIZE)) > 0) {
        //printf("[DEBUG] received '%s' from server\n", buffer);

        write(1, buffer, bytes_read);

        bzero(buffer, BUFFER_SIZE);
    }

    close(fd_fifo_server_client);
}

/**
 * @brief           Função que recebe uma string com um comando passado pelo stdin(extenso) e tranforma num comando a ser interpretado pelo server(simplificado)
 * @param command   Comando a ser simplificado
 * @param size      Tamanho do comando recebido
 */
void simplify_command(char* command, ssize_t size)
{
    char command_i[] = "tempo-inactividade ";
    char command_m[] = "tempo-execucao ";
    char command_e[] = "executar ";
    char command_l[] = "listar";
    char command_t[] = "terminar ";
    char command_r[] = "historico";
    char command_h[] = "ajuda";

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
}
