#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "macros.h"
#include "auxs.h"

void simplify_command(char* command, ssize_t size);


int main(int argc, char const** argv)
{
    int fd_fifo;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = 1;

    if ((fd_fifo = open(PIPENAME, O_WRONLY)) == -1)
        perror("Open");
    else
        printf("[DEBUG] opened FIFO for writing\n");

    if (argc > 1) {
        strcpy(buffer, argv[1]);
        for (int i = 2; i < argc; i++) {
            strcat(buffer, " ");
            strcat(buffer, argv[i]);
        }

        write(fd_fifo, buffer, strlen(buffer));
    }
    else {

        write(1, "    $ ", 6);
        while ((bytes_read = readln(0, buffer, BUFFER_SIZE)) > 0 && strcmp(buffer, "exit") != 0) {
            simplify_command(buffer, bytes_read);

            write(fd_fifo, buffer, bytes_read);
            printf("[DEBUG] wrote '%s' to fifo\n", buffer);
            write(1, "    $ ", 6);
        }
    }

    close(fd_fifo);

    return 0;
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
