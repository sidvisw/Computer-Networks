#include "mysocket.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFF_MAX 5000

int main(int argc, char *argv[]){
   	int			sockfd, newsockfd ; // Socket descriptors
	socklen_t	clilen; // client length
	struct sockaddr_in	cli_addr, serv_addr;
    char buffer[BUFF_MAX];
	memset(buffer, 0, BUFF_MAX);
	memset(&serv_addr, 0, sizeof(serv_addr));
	memset(&cli_addr, 0, sizeof(cli_addr));

    if ((sockfd = my_socket(AF_INET, SOCK_MyTCP, 0)) < 0) {
        perror("Cannot create socket\n");
        exit(EXIT_FAILURE);
    }

    FILE* fp = fopen("server.log", "w");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (my_bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // listen for connections
    my_listen(sockfd, 10);

    clilen = sizeof(cli_addr);
    newsockfd = my_accept(sockfd, (struct sockaddr *) &cli_addr, &clilen) ;

    if (newsockfd < 0) {
        perror("Accept error\n");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "Connection accepted\n");
    for(int i = 0; i<BUFF_MAX; i++){
        buffer[i] = 'a';
    }
    // my_send(newsockfd, buffer, BUFF_MAX, 0);
    // my_send(newsockfd, "Hello Hello", 12, 0);
    // my_send(newsockfd, "Hello Hello Hello", 18, 0);
    // int n = my_recv(newsockfd, buffer, BUFF_MAX, 0);
    // fprintf(fp, "%s\n", buffer);
    // n = my_recv(newsockfd, buffer, BUFF_MAX, 0);
    // fprintf(fp, "%s\n", buffer);
    // n = my_recv(newsockfd, buffer, BUFF_MAX, 0);
    // fprintf(fp, "%s\n", buffer);
    int count = 25;
    while(count--){
        my_send(newsockfd, buffer, BUFF_MAX, 0);
    }
    count = 25;
    int recvLen;
    while(count--){
        memset(buffer, 0, BUFF_MAX);
        recvLen = my_recv(newsockfd, buffer, BUFF_MAX, 0);
        fprintf(fp, "recvLen: %d\n", recvLen);
        fprintf(fp, "%d: ", 25-count);
        for(int i = 0; i < recvLen; i++){
            fprintf(fp, "%c", buffer[i]);
        }
        fprintf(fp, "\n");
    }
    fflush(fp);
    my_close(newsockfd);
    my_close(sockfd);

}