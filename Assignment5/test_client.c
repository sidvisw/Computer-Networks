#include "mysocket.h"
#include <stdio.h>
#include <stdlib.h>

#define BUFF_MAX 5000

int main(int argc, char *argv[]){
    int sockfd;
    struct sockaddr_in serv_addr;

    FILE* fp = fopen("client.log", "w");

    char buff[BUFF_MAX];
    memset(buff, 0, BUFF_MAX);
    if((sockfd = my_socket(AF_INET, SOCK_MyTCP, 0)) < 0){
        printf("Error creating socket");
        exit(EXIT_FAILURE);
    }
    
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if(my_connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        printf("Error connecting to server");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "Connected to server\n");

    int count = 25;
    int recv_len;
    while(count--)
    {   
        memset(buff, 0, BUFF_MAX);
        recv_len=my_recv(sockfd, buff, BUFF_MAX, 0);
        fprintf(fp, "%d: ", 25-count);
        for(int i = 0; i < recv_len; i++){
            fprintf(fp, "%c", buff[i]);
        }
        fprintf(fp, "\n");
    }
    // recv_len=my_recv(sockfd, buff, 50, 0);
    // for(int i = 0; i < 50; i++){
    //     fprintf(fp, "%c", buff[i]);
    // }
    // printf("\n");

    // strcpy(buff, "Hello from client");
    // my_send(sockfd, buff, 50, 0);

    // strcpy(buff, "Hello from client 2");
    // my_send(sockfd, buff, 50, 0);

    // strcpy(buff, "Hello from client 3");
    // my_send(sockfd, buff, 50, 0);
    memset(buff, 0, BUFF_MAX);
    for(int i = 0; i < BUFF_MAX; i++){
        buff[i] = 'a';
    }
    count = 25;
    while(count--)
    {   
        my_send(sockfd, buff, BUFF_MAX, 0);
    }
    fflush(fp);
    my_close(sockfd);

    return 0;
}