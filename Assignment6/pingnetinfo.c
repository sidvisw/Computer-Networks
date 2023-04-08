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
#include <poll.h>

#define MAXLINE 1024
#define MAX_HOPS 30
#define TRIES 5

#define BYTE_TO_BINARY_PATTERN " %c %c %c %c %c %c %c %c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0')

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

// ssize_t my_recvfrom(int sockfd, void*buf, size_t len, int flags,struct sockaddr *recv_addr, socklen_t *addrlen, int timeout, int seq_num)
// {

// }

void print_packet(char *buff, ssize_t recv_len)
{
    struct iphdr *ip = (struct iphdr *)buff;
    struct icmphdr *icmp = (struct icmphdr *)(buff + sizeof(struct iphdr));

    printf("+------------------------------------------------------------------------------+\n");
    printf("|                                  IP Header                                   |\n");
    printf("+------------------------------------------------------------------------------+\n");
    printf("|  Version: %-2d |  IHL: %-2d |  TOS: %-2d |           Total Length: %-14d |\n", ip->version, ip->ihl, ip->tos, ntohs(ip->tot_len));
    printf("+------------------------------------------------------------------------------+\n");
    printf("|           Identification: %-8d | Flags: %-1d |      Fragment Offset: %-6d |\n", ntohs(ip->id), ip->frag_off >> 13, ip->frag_off & 0x1fff);
    printf("+------------------------------------------------------------------------------+\n");
    printf("|      TTL: %-5d |   Protocol: %-4d |          Header Checksum: %-13d |\n", ip->ttl, ip->protocol, ntohs(ip->check));
    printf("+------------------------------------------------------------------------------+\n");
    printf("|                          Source Address: %-35s |\n", inet_ntoa(*(struct in_addr *)&ip->saddr));
    printf("+------------------------------------------------------------------------------+\n");
    printf("|                       Destination Address: %-33s |\n", inet_ntoa(*(struct in_addr *)&ip->daddr));
    printf("+------------------------------------------------------------------------------+\n");
    if (ip->protocol == IPPROTO_ICMP)
    {
        printf("|                                  ICMP Header                                 |\n");
        printf("+------------------------------------------------------------------------------+\n");
        printf("|     Type: %-5d |      Code: %-5d |             Checksum: %-17d |\n", icmp->type, icmp->code, ntohs(icmp->checksum));
        printf("+------------------------------------------------------------------------------+\n");
        printf("|          Identifier: %-13d |           Sequence Number: %-12d |\n", ntohs(icmp->un.echo.id), ntohs(icmp->un.echo.sequence));
        printf("+------------------------------------------------------------------------------+\n");
    }
    if (recv_len > sizeof(struct iphdr) + sizeof(struct icmphdr))
    {
        printf("|      ");
        for (int i = 0; i < 4; i++)
        {
            printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY((unsigned char)buff[i + sizeof(struct iphdr) + sizeof(struct icmphdr)]));
        }
        printf("        |\n");
        printf("+------------------------------------------------------------------------------+\n");
        printf("|      ");
        for (int i = 4; i < 8; i++)
        {
            printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY((unsigned char)buff[i + sizeof(struct iphdr) + sizeof(struct icmphdr)]));
        }
        printf("        |\n");
        printf("+------------------------------------------------------------------------------+\n");
    }
}

int main(int argc, char *argv[])
{
    srand(time(NULL));

    if (getuid())
    {
        printf("You are not root. Please run as root.\n");
        exit(EXIT_FAILURE);
    }

    if (argc != 4)
    {
        printf("Usage: %s <IP Address/Site Address> <Number of probes> <Time difference between probes>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct hostent *host = gethostbyname(argv[1]);
    if (host == NULL)
    {
        perror("Unable to resolve the hostname.\n");
        exit(EXIT_FAILURE);
    }

    const int n = atoi(argv[2]);
    const int T = atoi(argv[3]);

    struct in_addr dest_ip = *(struct in_addr *)host->h_addr_list[0];

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
    {
        perror("Unable to create socket.\n");
        exit(EXIT_FAILURE);
    }

    int one = 1;
    setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

    struct sockaddr_in my_addr, dest_addr;

    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(20000);

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = dest_ip;
    dest_addr.sin_port = htons(32164);

    char buffer[MAXLINE];

    char hostbuffer[256];
    struct hostent *host_entry;
    int hostname;
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    host_entry = gethostbyname(hostbuffer);
    char prev_ip[16];
    strcpy(prev_ip, inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])));

    int seq_no = 0;
    double prev_latency = 0;
    
    for (int ttl = 1; ttl < MAX_HOPS; ttl++)
    {   
        // data structure to store ip of the intermediate nodes
        uint32_t IP[TRIES] = {0};
        int IP_count[TRIES] = {0};
        int IP_itr = 0;
        for (int _ = 0; _ < TRIES; _++)
        {
            struct iphdr *ip = (struct iphdr *)buffer;
            struct icmphdr *icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

            memset(buffer, 0, MAXLINE);

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

            icmp->type = ICMP_ECHO;
            icmp->code = 0;
            icmp->un.echo.id = htons(getpid() & 0xffff);
            icmp->un.echo.sequence = htons(++seq_no);
            icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr));

            if (sendto(sockfd, buffer, ip->tot_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
            {
                perror("Unable to send packet.\n");
                exit(EXIT_FAILURE);
            }

            struct sockaddr_in recv_addr;
            socklen_t recv_addr_len = sizeof(recv_addr);

            struct timeval start;
            int dur = 0, TOUT = 1000, ret;

            nfds_t sz = 1;
            struct pollfd fdset[1];
            fdset[0].fd = sockfd;
            fdset[0].events = POLLIN;

            while (1)
            {   
                if (dur >= TOUT)
                    break;
                gettimeofday(&start, 0);
                TOUT -= dur;
                // printf("Waiting for reply: %d\n", TOUT);
                ret = poll(fdset, sz, TOUT);
                if (ret < 0)
                {
                    // poll call failed
                    perror("poll");
                    exit(EXIT_FAILURE);
                }
                if (ret == 0)
                    break;
                if (ret > 0)
                {
                    if (fdset[0].revents == POLLIN)
                    {
                        memset(buffer, 0, MAXLINE);
                        memset(&recv_addr, 0, sizeof(recv_addr));
                        int n = recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len);
                        if (n < 0)
                        {
                            // recvfrom call failed
                            perror("recvfrom");
                            exit(EXIT_FAILURE);
                        }

                        // print_packet(buffer, n);

                        struct iphdr *recv_ip = (struct iphdr *)buffer;
                        struct icmphdr *recv_icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

                        // check if the recv_packet is an ICMP time exceeded
                        if (recv_icmp->type == ICMP_TIME_EXCEEDED)
                        {
                            // check if the sender ip is already presnet in IP
                            int found = 0;
                            for (int i = 0; i < IP_itr; i++)
                            {
                                if (IP[i] == recv_ip->saddr)
                                {
                                    found = 1;
                                    IP_count[i]++;
                                    break;
                                }
                            }
                            if (!found)
                            {   
                                if(IP_itr < TRIES)
                                {
                                    IP[IP_itr] = recv_ip->saddr;
                                    IP_count[IP_itr]++;
                                    IP_itr++;
                                }
                            }
                        }
                        else if (recv_icmp->type == ICMP_ECHOREPLY)
                        {
                            // check if sender ip equals destination ip
                            if (recv_ip->saddr == dest_addr.sin_addr.s_addr)
                            {
                                // check if the sender ip is already presnet in IP
                                int found = 0;
                                for (int i = 0; i < IP_itr; i++)
                                {
                                    if (IP[i] == recv_ip->saddr)
                                    {
                                        found = 1;
                                        IP_count[i]++;
                                        break;
                                    }
                                }
                                if (!found)
                                {
                                    if(IP_itr < TRIES)
                                    {
                                        IP[IP_itr] = recv_ip->saddr;
                                        IP_count[IP_itr]++;
                                        IP_itr++;
                                    }
                                }
                            }
                        }
                    }
                    struct timeval end;
                    gettimeofday(&end, 0);
                    dur = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
                }
            }
        }

        uint32_t max_ip = 0;
        int max_count = 0;
        for (int i = 0; i < IP_itr; i++)
        {
            if (IP_count[i] > max_count)
            {
                max_count = IP_count[i];
                max_ip = IP[i];
            }
        }

        if(max_count == 0)
        {
            printf("No response from the destination.\n");
            continue;
        }

        struct timeval *sent_stamps = (struct timeval *)calloc(n, sizeof(struct timeval));
        struct timeval *recv_stamps = (struct timeval *)calloc(n, sizeof(struct timeval));
        int seq_start = seq_no + 1;
        for (int _ = 0; _ < n; _++)
        {
            struct iphdr *ip = (struct iphdr *)buffer;
            struct icmphdr *icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

            memset(buffer, 0, MAXLINE);

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
            ip->daddr = max_ip; // dont convert to network byte order as it is already in network byte order

            icmp->type = ICMP_ECHO;
            icmp->code = 0;
            icmp->un.echo.id = htons(getpid() & 0xffff);
            icmp->un.echo.sequence = htons(++seq_no);
            icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr));

            if (sendto(sockfd, buffer, ip->tot_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
            {
                perror("Unable to send packet.\n");
                exit(EXIT_FAILURE);
            }
            gettimeofday(&sent_stamps[_], 0);
            struct sockaddr_in recv_addr;
            socklen_t recv_addr_len = sizeof(recv_addr);

            struct timeval start;
            int dur = 0, TOUT = T * 1000, ret;

            nfds_t sz = 1;
            struct pollfd fdset[1];
            fdset[0].fd = sockfd;
            fdset[0].events = POLLIN;

            while (1)
            {
                if (dur >= TOUT)
                    break;
                gettimeofday(&start, 0);
                TOUT -= dur;
                ret = poll(fdset, sz, TOUT);
                if (ret < 0)
                {
                    // poll call failed
                    perror("poll");
                    exit(EXIT_FAILURE);
                }
                if (ret == 0)
                    break;
                if (ret > 0)
                {
                    if (fdset[0].revents & POLLIN)
                    {
                        memset(buffer, 0, MAXLINE);
                        memset(&recv_addr, 0, sizeof(recv_addr));
                        int n = recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len);
                        struct timeval recv_time;
                        gettimeofday(&recv_time, 0);
                        if (n < 0)
                        {
                            // recvfrom call failed
                            perror("recvfrom");
                            exit(EXIT_FAILURE);
                        }

                        // print_packet(buffer, n);

                        struct iphdr *recv_ip = (struct iphdr *)buffer;
                        struct icmphdr *recv_icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

                        // check if the recv_packet is an ICMP echo reply
                        if (recv_icmp->type == ICMP_ECHOREPLY)
                        {
                            // check for valid identifier
                            if (recv_icmp->un.echo.id == htons(getpid() & 0xffff))
                            {
                                // check for valid sequence number
                                if (ntohs(recv_icmp->un.echo.sequence) >= seq_start)
                                {
                                    // add the timestamp to recv_stamps
                                    recv_stamps[ntohs(recv_icmp->un.echo.sequence) - seq_start] = recv_time;
                                }
                            }
                        }
                    }
                    struct timeval end;
                    gettimeofday(&end, 0);
                    dur = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
                }
            }
        }
        // calculate average RTT from sent_stamps and recv_stamps
        double latency = 0;
        int count = 0;
        for (int i = 0; i < n; i++)
        {
            if (recv_stamps[i].tv_sec == 0 && recv_stamps[i].tv_usec == 0)
                continue;
            latency += (recv_stamps[i].tv_sec - sent_stamps[i].tv_sec) * 10000000 + (recv_stamps[i].tv_usec - sent_stamps[i].tv_usec);
            count++;
        }
        latency /= count;
        latency /= 2.0;
        latency -= prev_latency;
        printf("Latency of link %s to %s is %lf us\n", prev_ip, inet_ntoa(*(struct in_addr *)&max_ip), latency);
        prev_latency = latency;
        strcpy(prev_ip, inet_ntoa(*(struct in_addr *)&max_ip));
        // check if the max_ip is the destination ip
        if (max_ip == dest_addr.sin_addr.s_addr)
            break;
    }

    return 0;
}