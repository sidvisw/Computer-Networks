#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX 1024

unsigned short in_cksum(unsigned short *addr, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}

int main(int argc, char *argv[])
{
    srand(time(NULL));

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

    struct sockaddr_in my_addr, dest_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));

    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(20000);

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = dest_ip;
    dest_addr.sin_port = htons(32164);

    char buffer[MAX];

    for (int ttl = 1; ttl < 17; ttl++)
    {
        struct iphdr *ip = (struct iphdr *)buffer;
        struct icmphdr *icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

        char payload[64];
        memset(payload, 0, 64);
        for (int i = 0; i < 64; i++)
        {
            payload[i] = 'a' + (rand() % 26);
        }

        memset(buffer, 0, MAX);

        ip->ihl = 5;
        ip->version = 4;
        ip->tos = 0;
        ip->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr);
        ip->id = htons(rand() % 65535);
        ip->frag_off = 0;
        ip->ttl = ttl;
        ip->protocol = IPPROTO_ICMP;
        ip->check = 0;
        ip->saddr = my_addr.sin_addr.s_addr;
        ip->daddr = dest_addr.sin_addr.s_addr;
        memcpy(buffer + sizeof(struct iphdr) + sizeof(struct icmphdr), payload, 64);

        icmp->type = ICMP_ECHO;
        icmp->code = 0;
        icmp->un.echo.id = htons(0);
        icmp->un.echo.sequence = htons(0);
        icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr));

        if (sendto(sockfd, buffer, ip->tot_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
        {
            printf("Unable to send packet.\n");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);

        int n = recvfrom(sockfd, buffer, MAX, 0, (struct sockaddr *)&recv_addr, &recv_addr_len);
        if (n < 0)
        {
            printf("Unable to receive packet.\n");
            exit(EXIT_FAILURE);
        }

        printf("%d\n", n);
        struct iphdr *recv_ip = (struct iphdr *)buffer;
        struct icmphdr *recv_icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

        if (recv_icmp->type == ICMP_TIME_EXCEEDED)
        {
            struct iphdr *recv_ip = (struct iphdr *)(buffer + sizeof(struct iphdr) + sizeof(struct icmphdr));
            struct icmphdr *recv_icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(struct iphdr));

            if (recv_icmp->type == ICMP_ECHOREPLY)
            {
                printf("%d\t", ttl);
                printf("%s (%s) ", inet_ntoa(*(struct in_addr *)&recv_ip->saddr), argv[1]);
                printf("%d ms ", 0);
            }

            else
            {
                printf("%d\t", ttl);
                printf("*\t");
                printf("*\t");
            }

            printf("randi mehta\n");
            printf("%s (%s) ", inet_ntoa(*(struct in_addr *)&recv_ip->saddr), argv[1]);
        }

        else if (recv_icmp->type == ICMP_ECHOREPLY)
        {
            printf("%d\t", ttl);
            printf("%s (%s) ", inet_ntoa(*(struct in_addr *)&recv_ip->saddr), argv[1]);
            printf("%d ms ", 0);
        }

        else
        {
            printf("%d\t", ttl);
            printf("*\t");
            printf("*\t");
        }
    }

    return 0;
}