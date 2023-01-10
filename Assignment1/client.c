
/*    THE CLIENT PROCESS */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
    int sockfd;
    struct sockaddr_in serv_addr;

    int i;
    double *buf = (double *)malloc(sizeof(double));

    /* Opening a socket is exactly similar to the server process */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Unable to create socket\n");
        exit(0);
    }

    /* Recall that we specified INADDR_ANY when we specified the server
       address in the server. Since the client can run on a different
       machine, we must specify the IP address of the server.

       In this program, we assume that the server is running on the
       same machine as the client. 127.0.0.1 is a special address
       for "localhost" (this machine)

    /* IF YOUR SERVER RUNS ON SOME OTHER MACHINE, YOU MUST CHANGE
           THE IP ADDRESS SPECIFIED BELOW TO THE IP ADDRESS OF THE
           MACHINE WHERE YOU ARE RUNNING THE SERVER.
        */

    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(20000);

    /* With the information specified in serv_addr, the connect()
       system call establishes a connection with the server process.
    */
    if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
    {
        perror("Unable to connect to server\n");
        exit(0);
    }

    char *expr = (char *)calloc(1, sizeof(char));
    printf("Enter the expression: ");
    char ch;
    int len = 0;
    while ((ch = getchar()) != '\n')
    {
        expr = (char *)realloc(expr, (len + 1) * sizeof(char));
        expr[len++] = ch;
        expr[len] = '\0';
    }
    while (strcmp(expr, "-1"))
    {
        send(sockfd, expr, strlen(expr) + 1, 0);
        recv(sockfd, buf, sizeof(double), 0);
        printf("Result: %lf\n", *buf);
        printf("Enter the expression: ");
        len = 0;
        while ((ch = getchar()) != '\n')
        {
            expr = (char *)realloc(expr, (len + 1) * sizeof(char));
            expr[len++] = ch;
            expr[len] = '\0';
        }
    }

    send(sockfd, expr, strlen(expr) + 1, 0);
    free(expr);

    close(sockfd);
    return 0;
}
