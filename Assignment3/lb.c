#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <time.h>

/* THE LOAD BALANCER PROCESS */

int main(int argc, char *argv[])
{
    // Check for command line arguments
    if (argc != 4)
    {
        printf("Usage: ./lb <port> <server1_port> <server2_port>\n");
        exit(EXIT_FAILURE);
    }

    time_t T;                             // Clock variable to keep trach of timeout of 5 sec
    int timeout, load_tracker[2] = {0, 0}; // Timeout variable and load tracker array
    int sockfd, newsockfd;                 // Socket descriptors
    int clilen;
    struct sockaddr_in lb_addr, cli_addr, serv_addr[2];

    char buf[50]; // Buffer for communicating through server and client

    // System call to open a socket for communication through client
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Cannot create socket\n");
        exit(0);
    }

    // Assign the values for sin_family, sin_addr, and sin_port according to the TCP communication
    lb_addr.sin_family = AF_INET;
    lb_addr.sin_addr.s_addr = INADDR_ANY;
    lb_addr.sin_port = htons(atoi(argv[1]));

    for (int i = 0; i < 2; i++)
    {
        serv_addr[i].sin_family = AF_INET;
        inet_aton("127.0.0.1", &serv_addr[i].sin_addr);
        serv_addr[i].sin_port = htons(atoi(argv[i + 2]));
    }

    // Associate the load balancer with its port using the bind() system call
    if (bind(sockfd, (struct sockaddr *)&lb_addr, sizeof(lb_addr)) < 0)
    {
        perror("Unable to bind local address\n");
        exit(0);
    }

    // Specifies that up to 5 concurrent client requests will be queued up
    listen(sockfd, 5);

    // Initialize the timeout variable and the clock variable
    timeout = 5000;
    T = time(0);

    // Looping structure for the concurrent TCP server used for load balancer
    // fork() function generates a new child process and the parent process comes back and waits at the accept call
    while (1)
    {
        // Declarations and definitions for the polling construct
        struct pollfd fd_set[1];
        fd_set[0].fd = sockfd;
        fd_set[0].events = POLLIN;

        int ret_val;

        // Polling construct for the concurrent server
        if ((ret_val = poll(fd_set, 1, timeout)) < 0)
        {
            perror("Poll error\n");
            exit(0);
        }

        if (ret_val > 0)
        {
            // The accept() system call accepts a client connection.
            clilen = sizeof(cli_addr);
            newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

            if (newsockfd < 0)
            {
                perror("Accept error\n");
                exit(0);
            }

            // The child process handles the client request and the parent goes back to accept the next client request
            if (fork() == 0)
            {
                // Close the old socket in the child since all communications will be through the new socket.
                close(sockfd);

                // Determine the server with the least load
                int serv_idx = (load_tracker[0] < load_tracker[1]) ? 0 : 1;

                // Create a socket for communication with the server with the least load
                int serv_sockfd;
                if ((serv_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    perror("Cannot create socket\n");
                    exit(0);
                }

                // Connect to the server with the least load
                if (connect(serv_sockfd, (struct sockaddr *)&serv_addr[serv_idx], sizeof(serv_addr[serv_idx])) < 0)
                {
                    perror("Unable to connect to server\n");
                    exit(0);
                }

                // Ask the server to send the time for the client
                strcpy(buf, "Send Time");
                send(serv_sockfd, buf, strlen(buf) + 1, 0);

                printf("Sending client request to IP: %s Port: %d\n", inet_ntoa(serv_addr[serv_idx].sin_addr), ntohs(serv_addr[serv_idx].sin_port));

                // Receive the time from the server
                int recv_len = recv(serv_sockfd, buf, 50, 0);
                while (buf[recv_len - 1])
                    recv_len += recv(serv_sockfd, buf + recv_len, 50 - recv_len, 0);

                // Close the socket for communication with the server
                close(serv_sockfd);

                // Send the time to the client
                send(newsockfd, buf, strlen(buf) + 1, 0);

                // Close the new socket and exit from the child process
                close(newsockfd);
                exit(0);
            }

            // Close the new socket in the parent since all communications will be through the old socket.
            close(newsockfd);

            // Update the timeout and the clock variable
            timeout = (5 - (time(0)-T)) * 1000;
        }
        else
        {
            // Update the load tracker array
            // Loop for the two servers to get their load
            for (int i = 0; i < 2; i++)
            {
                // Create a socket for communication with the server
                int serv_sockfd;
                if ((serv_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    perror("Cannot create socket\n");
                    exit(0);
                }

                // Connect to the server
                if (connect(serv_sockfd, (struct sockaddr *)&serv_addr[i], sizeof(serv_addr[i])) < 0)
                {
                    perror("Unable to connect to server\n");
                    exit(0);
                }

                // Ask the server to send the load
                strcpy(buf, "Send Load");
                send(serv_sockfd, buf, strlen(buf) + 1, 0);

                // Receive the load from the server
                int recv_len = recv(serv_sockfd, buf, 50, 0);
                while (buf[recv_len - 1])
                    recv_len += recv(serv_sockfd, buf + recv_len, 50 - recv_len, 0);

                // Update the load tracker array
                load_tracker[i] = atoi(buf);

                // Close the socket for communication with the server
                close(serv_sockfd);

                printf("Load recieved from IP: %s Port: %d %d\n", inet_ntoa(serv_addr[i].sin_addr), ntohs(serv_addr[i].sin_port), load_tracker[i]);
            }

            // Update the timeout and the clock variable
            timeout = 5000;
            T = time(0);
        }
    }
    return 0;
}
