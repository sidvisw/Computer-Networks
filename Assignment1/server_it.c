/*
            NETWORK PROGRAMMING WITH SOCKETS

In this program we illustrate the use of Berkeley sockets for interprocess
communication across the network. We show the communication between a server
process and a client process.


*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

double evaluate(const char *const expr)
{
    double result = 0.0;
    char op = '+';
    double value = 0.0;
    int i = 0;
    while (expr[i] != '\0')
    {
        if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/')
        {
            op = expr[i];
            i++;
        }
        else if (expr[i] == '(')
        {
            char *sub_expr;
            int j = i + 1;
            int count = 1;
            while (count != 0)
            {
                if (expr[j] == '(')
                {
                    count++;
                }
                else if (expr[j] == ')')
                {
                    count--;
                }
                j++;
            }
            sub_expr = (char *)malloc(sizeof(char) * (j - i - 1));
            int k = 0;
            for (int l = i + 1; l < j - 1; l++)
            {
                sub_expr[k++] = expr[l];
            }
            sub_expr[k] = '\0';
            value = evaluate(sub_expr);
            free(sub_expr);
            switch (op)
            {
            case '+':
                result = result + value;
                break;
            case '-':
                result = result - value;
                break;
            case '*':
                result = result * value;
                break;
            case '/':
                result = result / value;
                break;
            }
            i = j;
        }
        else if (expr[i] >= '0' && expr[i] <= '9')
        {
            value = 0.0;
            while (expr[i] >= '0' && expr[i] <= '9')
            {
                value = value * 10 + (expr[i] - '0');
                i++;
            }
            if (expr[i] == '.')
            {
                double factor = 0.1;
                i++;
                while (expr[i] >= '0' && expr[i] <= '9')
                {
                    value = value + (expr[i] - '0') * factor;
                    factor = factor / 10;
                    i++;
                }
            }
            switch (op)
            {
            case '+':
                result = result + value;
                break;
            case '-':
                result = result - value;
                break;
            case '*':
                result = result * value;
                break;
            case '/':
                result = result / value;
                break;
            }
        }
        else
        {
            i++;
        }
    }
    return result;
}

/* THE SERVER PROCESS */

int main()
{
    int sockfd, newsockfd; /* Socket descriptors */
    int clilen;
    struct sockaddr_in cli_addr, serv_addr;

    int i;
    char buf[100]; /* We will use this buffer for communication */

    /* The following system call opens a socket. The first parameter
       indicates the family of the protocol to be followed. For internet
       protocols we use AF_INET. For TCP sockets the second parameter
       is SOCK_STREAM. The third parameter is set to 0 for user
       applications.
    */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Cannot create socket\n");
        exit(0);
    }

    /* The structure "sockaddr_in" is defined in <netinet/in.h> for the
       internet family of protocols. This has three main fields. The
       field "sin_family" specifies the family and is therefore AF_INET
       for the internet family. The field "sin_addr" specifies the
       internet address of the server. This field is set to INADDR_ANY
       for machines having a single IP address. The field "sin_port"
       specifies the port number of the server.
    */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(20000);

    /* With the information provided in serv_addr, we associate the server
       with its port using the bind() system call.
    */
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Unable to bind local address\n");
        exit(0);
    }

    listen(sockfd, 5); /* This specifies that up to 5 concurrent client
                  requests will be queued up while the system is
                  executing the "accept" system call below.
               */

    /* In this program we are illustrating an iterative server -- one
       which handles client connections one by one.i.e., no concurrency.
       The accept() system call returns a new socket descriptor
       which is used for communication with the server. After the
       communication is over, the process comes back to wait again on
       the original socket descriptor.
    */
    while (1)
    {

        /* The accept() system call accepts a client connection.
           It blocks the server until a client request comes.

           The accept() system call fills up the client's details
           in a struct sockaddr which is passed as a parameter.
           The length of the structure is noted in clilen. Note
           that the new socket descriptor returned by the accept()
           system call is stored in "newsockfd".
        */
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd < 0)
        {
            perror("Accept error\n");
            exit(0);
        }

        /* We now receive a message from the client. For this example
           we make an assumption that the entire message sent from the
           client will come together. In general, this need not be true
           for TCP sockets (unlike UDPi sockets), and this program may not
           always work (for this example, the chance is very low as the
           message is very short. But in general, there has to be some
           mechanism for the receiving side to know when the entire message
          is received. Look up the return value of recv() to see how you
          can do this.
        */
        recv(newsockfd, buf, 100, 0);
        while (strcmp(buf, "-1"))
        {
            char *expr=(char*)calloc(100,sizeof(char));
            int cur_len=100;
            strcpy(expr, buf);
            int recieved=!expr[cur_len-1];
            while(!recieved){
                expr=(char*)realloc(expr,(cur_len+100)*sizeof(char));
                cur_len+=100;
                recv(newsockfd, expr+cur_len-100, 100, 0);
                for(int i=0;i<cur_len;i++){
                    if(!expr[i])recieved=1;
                }
            }
            double result=evaluate(expr);
            free(expr);
            send(newsockfd, &result, sizeof(result), 0);
            recv(newsockfd, buf, 100, 0);
        }

        close(newsockfd);
    }
    return 0;
}
