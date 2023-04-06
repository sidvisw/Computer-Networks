#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <IP Address / Site Address> <Number of probes> <Time difference between probes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct hostent *host = gethostbyname(argv[1]);
    if (host == NULL)
    {
        printf("Unable to resolve the host name.\n");
        exit(EXIT_FAILURE);
    }

    struct in_addr dest_ip = *(struct in_addr *)host->h_addr_list[0];

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
    {
        printf("Unable to create socket.\n");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    return 0;
}