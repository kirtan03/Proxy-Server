#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>

void send_int(int number, int socket)
{
	send(socket, &number, sizeof(int), 0);
	return;
}

int rec_int(int socket)
{
	int number = 0;
	recv(socket, &number, sizeof(int), 0);
	return number;
}

void send_string(char *string, int socket)
{
	send_int(strlen(string) + 1, socket);
	send(socket, string, (strlen(string) + 1) * sizeof(char), 0);
	return;
}

char *rec_string(int socket)
{
	int len = rec_int(socket);
	if (len == -1 || len == 0)
		return NULL;
	char *string = (char *) calloc(len, sizeof(char));
	recv(socket, string, len * sizeof(char), 0);
	return string;
}

