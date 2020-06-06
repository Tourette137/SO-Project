#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "../includes/auxs.h"

/**
 * @brief           Função que lê uma linha a partir de um file descriptor
 * @param fd		File descriptor de leitura
 * @param buffer	String onde se guarda o resultado da leitura
 * @param size		Máximo de carateres a ser lidos
 * @return			Número de carateres lidos
 */
size_t readln (int fd, char *buffer, size_t size) {

	int resultado = 0, total = 0;

	while ((resultado = read(fd, &buffer[total], 1)) > 0 && total < size) {
		if (buffer[total] == '\n') {
            buffer[total] = '\0';
            break;
        }

		total += resultado;
	}

	return total;
}

/**
 * @brief           Função que conta o número de ocorrências de um carater numa string
 * @param string	String a ser analizada
 * @param token		Carater a ser procurado
 * @return			Número de ocorrências do carater na string
 */
int strcnt(char* string, char token)
{
	int x = 0;

	for(int i = 0; i < strlen(string); i++) {
		if (string[i] == token)
			x++;
	}

	return x;
}

/**
 * @brief				Função que elimina um ficheiro
 * @param filename		Nome do ficheiro a ser eliminado
 * @return				Valor de retorno da execução do comando "rm filename"
 */
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
