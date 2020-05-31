#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "auxs.h"

/**
 * @brief           Função que lê uma linha do file descriptor 'fd'
 * @param fd		File descriptor de leitura
 * @param buffer	String onde se guarda o resultado da leitura
 * @param size		Máximo de carateres a ser lidos
 * @return			Número de carateres lidos
 */
ssize_t readln (int fd, char *buffer, size_t size) {

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
 * @return			Número de ocorrências do carater 'token' na string 'string'
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
