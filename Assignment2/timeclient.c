// A Simple Client Implementation
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/poll.h>

#define MAXLEN 50

int main()
{
    int sockfd;
    struct sockaddr_in servaddr;

    // Creating socket file descriptor
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8181);
    inet_aton("127.0.0.1", &servaddr.sin_addr);

    char buffer[MAXLEN];

    for (int i = 0; i < 5; i++)
    {
        // Message for the server to give the client's address
        char *connection_mssg = "CLIENT : CONNECTED";
        sendto(sockfd, (const char *)connection_mssg, strlen(connection_mssg) + 1, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        // Poll structure for maintaing a 3 sec timeout for the client
        struct pollfd fd_set[1];
        fd_set[0].fd = sockfd;
        fd_set[0].events = POLLIN;

        int ret_val = poll(fd_set, 1, 3000);

        // Error check for poll() function
        if (ret_val < 0)
        {
            perror("Unable to Poll");
            exit(EXIT_FAILURE);
        }
        // If data has arrived on the socket then do the recvfrom() operation
        else if (ret_val > 0)
        {
            if (fd_set[0].revents == POLLIN)
            {
                struct sockaddr_in serv_addr;
                memset(&serv_addr, 0, sizeof(serv_addr));
                socklen_t serv_len = sizeof(serv_addr);
                recvfrom(sockfd, (char *)buffer, MAXLEN, 0, (struct sockaddr *)&serv_addr, &serv_len);
                printf("%s\n", buffer);
                break;
            }
        }
        // Try again for timeout
        else
        {
            printf("Timeout 3 sec: Trying Again...\n");
        }
    }

    close(sockfd);
    return 0;
}