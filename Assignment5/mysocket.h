#ifndef _MYSOCKET_H_
#define _MYSOCKET_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SOCK_MyTCP 1
#define TABLE_MAX 10
#define MAX_MSG 5000
#define SEND_MAX 1000
#define T 1

int my_socket(int, int, int);
int my_bind(int, const struct sockaddr *, socklen_t);
int my_listen(int, int);
int my_accept(int, struct sockaddr *, socklen_t *);
int my_connect(int, const struct sockaddr *, socklen_t);
ssize_t my_send(int, const void *, size_t, int);
ssize_t my_recv(int, void *, size_t, int);
int my_close(int);
int min(int, int);

void *S(void *);
void *R(void *);

typedef struct _MyTable
{
    int *sizes;
    char **table;
    int in, out;
    int count;
    int connected;
} MyTable;

#endif