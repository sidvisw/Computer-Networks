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
#include <netinet/tcp.h>
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
#include <math.h>

#define MAXLINE 1024
#define MAX_HOPS 64
#define TRIES 5

// function to calculate checksum for icmp header
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

// function to print all the received packets in nice format
void print_packet(char *buff, ssize_t recv_len)
{
    struct iphdr *ip = (struct iphdr *)buff;
    struct icmphdr *icmp = (struct icmphdr *)(buff + sizeof(struct iphdr));

    printf("+------------------------------------------------------------------------------+\n");
    printf("|                                  IP Header                                   |\n");
    printf("+------------------------------------------------------------------------------+\n");
    printf("|  Version: %-2d |  IHL: %-2d |  TOS: %-2d |            Total Length: %-14d |\n", ip->version, ip->ihl, ip->tos, ntohs(ip->tot_len));
    printf("+------------------------------------------------------------------------------+\n");
    printf("|           Identification: %-8d | Flags: %-1d |      Fragment Offset: %-6d |\n", ntohs(ip->id), ip->frag_off >> 13, ip->frag_off & 0x1fff);
    printf("+------------------------------------------------------------------------------+\n");
    printf("|      TTL: %-5d |   Protocol: %-4d |          Header Checksum: %-13d |\n", ip->ttl, ip->protocol, ntohs(ip->check));
    printf("+------------------------------------------------------------------------------+\n");
    printf("|                          Source Address: %-35s |\n", inet_ntoa(*(struct in_addr *)&ip->saddr));
    printf("+------------------------------------------------------------------------------+\n");
    printf("|                       Destination Address: %-33s |\n", inet_ntoa(*(struct in_addr *)&ip->daddr));
    printf("+------------------------------------------------------------------------------+\n");

    printf("|                                  ICMP Header                                 |\n");
    printf("+------------------------------------------------------------------------------+\n");
    printf("|     Type: %-5d |      Code: %-5d |             Checksum: %-17d |\n", icmp->type, icmp->code, ntohs(icmp->checksum));
    printf("+------------------------------------------------------------------------------+\n");
    printf("|          Identifier: %-13d |           Sequence Number: %-12d |\n", ntohs(icmp->un.echo.id), ntohs(icmp->un.echo.sequence));
    printf("+------------------------------------------------------------------------------+\n");

    if (icmp->type != ICMP_ECHOREPLY && icmp->type != ICMP_ECHO && icmp->type != ICMP_TIME_EXCEEDED)
    {
        if (recv_len > sizeof(struct iphdr) + sizeof(struct icmphdr))
        {
            ip = (struct iphdr *)(buff + sizeof(struct iphdr) + sizeof(struct icmphdr));
            if (ip->protocol == IPPROTO_ICMP)
            {
                icmp = (struct icmphdr *)(buff + sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(struct iphdr));
                printf("|                                  ICMP Header                                 |\n");
                printf("+------------------------------------------------------------------------------+\n");
                printf("|     Type: %-5d |      Code: %-5d |             Checksum: %-17d |\n", icmp->type, icmp->code, ntohs(icmp->checksum));
                printf("+------------------------------------------------------------------------------+\n");
                printf("|          Identifier: %-13d |           Sequence Number: %-12d |\n", ntohs(icmp->un.echo.id), ntohs(icmp->un.echo.sequence));
                printf("+------------------------------------------------------------------------------+\n");
            }
            else if (ip->protocol == IPPROTO_UDP)
            {
                struct udphdr *udp = (struct udphdr *)(buff + sizeof(struct iphdr));
                printf("|                                  UDP Header                                  |\n");
                printf("+------------------------------------------------------------------------------+\n");
                printf("|      Source Port: %-6d |   Destination Port: %-6d |            Length: %-14d |\n", ntohs(udp->source), ntohs(udp->dest), ntohs(udp->len));
                printf("+------------------------------------------------------------------------------+\n");
                printf("|            Checksum: %-17d |\n", ntohs(udp->check));
                printf("+------------------------------------------------------------------------------+\n");
            }
            else if (ip->protocol == IPPROTO_TCP)
            {
                struct tcphdr *tcp = (struct tcphdr *)(buff + sizeof(struct iphdr));
                printf("|                                  TCP Header                                  |\n");
                printf("+------------------------------------------------------------------------------+\n");
                printf("|      Source Port: %-6d |   Destination Port: %-6d |            Sequence Number: %-12d |\n", ntohs(tcp->source), ntohs(tcp->dest), ntohl(tcp->seq));
                printf("+------------------------------------------------------------------------------+\n");
                printf("|            Acknowledgement Number: %-12d | Data Offset: %-2d | Reserved: %-2d |\n", ntohl(tcp->ack_seq), tcp->doff, tcp->res1);
                printf("+------------------------------------------------------------------------------+\n");
                printf("|      Flags: %-1d | Window Size: %-6d |            Checksum: %-17d |\n", tcp->fin, ntohs(tcp->window), ntohs(tcp->check));
                printf("+------------------------------------------------------------------------------+\n");
                printf("|      Urgent Pointer: %-6d |\n", ntohs(tcp->urg_ptr));
                printf("+------------------------------------------------------------------------------+\n");
            }
            else
            {
                printf("|                                 Unknown Header                               |\n");
                printf("+------------------------------------------------------------------------------+\n");
            }
        }
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
    gethostname(hostbuffer, sizeof(hostbuffer));
    host_entry = gethostbyname(hostbuffer);
    char prev_ip[16];
    strcpy(prev_ip, inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0])));

    printf("Tracing route to %s [%s] over a maximum of %d hops:\n", argv[1], inet_ntoa(dest_ip), MAX_HOPS);

    int seq_no = 0;
    double cumm_latency = 0, cumm_latency_bw = 0;

    for (int ttl = 1; ttl < MAX_HOPS; ttl++)
    {
        // data structure to store ip of the intermediate nodes
        uint32_t IP[TRIES] = {0};
        int IP_count[TRIES] = {0};
        int IP_itr = 0;
        int seq_start = seq_no + 1;
        for (int _ = 0; _ < TRIES; _++)
        {
            struct iphdr *ip = (struct iphdr *)buffer;
            struct icmphdr *icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

            memset(buffer, 0, MAXLINE);

            ip->ihl = 5;
            ip->version = 4;
            ip->tos = 0;
            ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
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

            print_packet(buffer, sizeof(struct iphdr) + sizeof(struct icmphdr));
            if (sendto(sockfd, buffer, sizeof(struct iphdr) + sizeof(struct icmphdr), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
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
                        if (recv_ip->protocol != IPPROTO_ICMP)
                        {
                            struct timeval end;
                            gettimeofday(&end, 0);
                            dur = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
                            continue;
                        }
                        print_packet(buffer, n);
                        struct icmphdr *recv_icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

                        // check if the recv_packet is an ICMP time exceeded
                        if (recv_icmp->type == ICMP_TIME_EXCEEDED)
                        {
                            struct icmphdr *recv_time_exceeded = (struct icmphdr *)(buffer + 2 * sizeof(struct iphdr) + sizeof(struct icmphdr));
                            // check if the id and sequence number of icmp header matches
                            if (recv_time_exceeded->un.echo.id == htons(getpid() & 0xffff) && recv_time_exceeded->un.echo.sequence >= htons(seq_start))
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
                                    if (IP_itr < TRIES)
                                    {
                                        IP[IP_itr] = recv_ip->saddr;
                                        IP_count[IP_itr]++;
                                        IP_itr++;
                                    }
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
                                    if (IP_itr < TRIES)
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

        if (max_count < TRIES / 2)
        {
            // printf("No response from the destination.\n");
            printf("\n|  Hop: %-2d | Link: %8s -      *      | Estimated Latency:     *     | Estimated Bandwidth:     *     |\n\n", ttl, prev_ip);
            strcpy(prev_ip, "*");
            continue;
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = max_ip;
        addr.sin_port = htons(32164);

        struct timeval *sent_stamps = (struct timeval *)calloc(n, sizeof(struct timeval));
        struct timeval *recv_stamps = (struct timeval *)calloc(n, sizeof(struct timeval));

        struct timeval *sent_stamps_bw = (struct timeval *)calloc(n, sizeof(struct timeval));
        struct timeval *recv_stamps_bw = (struct timeval *)calloc(n, sizeof(struct timeval));
        // int *data_size = (int *)calloc(n, sizeof(int)); use if variable size non zero data packets are to be sent
        // while calculating bandwidth

        seq_start = seq_no + 1;
        for (int _ = 0; _ < n; _++)
        {
            struct iphdr *ip = (struct iphdr *)buffer;
            struct icmphdr *icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

            memset(buffer, 0, MAXLINE);

            ip->ihl = 5;
            ip->version = 4;
            ip->tos = 0;
            ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr));
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

            print_packet(buffer, sizeof(struct iphdr) + sizeof(struct icmphdr));
            gettimeofday(&sent_stamps[_], 0);
            if (sendto(sockfd, buffer, sizeof(struct iphdr) + sizeof(struct icmphdr), 0, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            {
                perror("Unable to send packet.\n");
                exit(EXIT_FAILURE);
            }
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
                        struct timeval recv_time;
                        gettimeofday(&recv_time, 0);
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
                        if (recv_ip->protocol != IPPROTO_ICMP)
                        {
                            struct timeval end;
                            gettimeofday(&end, 0);
                            dur = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
                            continue;
                        }
                        print_packet(buffer, n);

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
                                    if ((ntohs(recv_icmp->un.echo.sequence) - seq_start) % 2 == 0)
                                        recv_stamps[(ntohs(recv_icmp->un.echo.sequence) - seq_start) / 2] = recv_time;
                                    else
                                        recv_stamps_bw[(ntohs(recv_icmp->un.echo.sequence) - seq_start) / 2] = recv_time;
                                }
                            }
                        }
                    }
                    struct timeval end;
                    gettimeofday(&end, 0);
                    dur = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
                }
            }

            // Bandwidth measurement starts here

            ip = (struct iphdr *)buffer;
            icmp = (struct icmphdr *)(buffer + sizeof(struct iphdr));

            memset(buffer, 0, MAXLINE);

            ip->ihl = 5;
            ip->version = 4;
            ip->tos = 0;
            ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct icmphdr) + 500);
            ip->id = htons(rand() % 65535);
            ip->frag_off = 0;
            ip->ttl = ttl;
            ip->protocol = IPPROTO_ICMP;
            ip->check = 0;
            ip->saddr = my_addr.sin_addr.s_addr;
            ip->daddr = max_ip; // dont convert to network byte order as it is already in network byte order

            for (int i = 0; i < 25; i++)
            {
                buffer[sizeof(struct iphdr) + sizeof(struct icmphdr) + i] = 'a' + rand() % 26;
            }

            icmp->type = ICMP_ECHO;
            icmp->code = 0;
            icmp->un.echo.id = htons(getpid() & 0xffff);
            icmp->un.echo.sequence = htons(++seq_no);
            icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr) + 500);

            print_packet(buffer, sizeof(struct iphdr) + sizeof(struct icmphdr) + 500);
            gettimeofday(&sent_stamps_bw[_], 0);
            if (sendto(sockfd, buffer, sizeof(struct iphdr) + sizeof(struct icmphdr) + 500, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            {
                perror("Unable to send packet.\n");
                exit(EXIT_FAILURE);
            }

            dur = 0;
            TOUT = T * 1000;

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
                {
                    break;
                }
                if (ret > 0)
                {
                    if (fdset[0].revents & POLLIN)
                    {
                        struct timeval recv_time;
                        gettimeofday(&recv_time, 0);
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
                        if (recv_ip->protocol != IPPROTO_ICMP)
                        {
                            struct timeval end;
                            gettimeofday(&end, 0);
                            dur = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
                            continue;
                        }
                        print_packet(buffer, n);
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
                                    if ((ntohs(recv_icmp->un.echo.sequence) - seq_start) % 2 == 0)
                                        recv_stamps[(ntohs(recv_icmp->un.echo.sequence) - seq_start) / 2] = recv_time;
                                    else
                                        recv_stamps_bw[(ntohs(recv_icmp->un.echo.sequence) - seq_start) / 2] = recv_time;
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
        double latency = 0, latency_bw = 0;
        int count = 0;
        for (int i = 0; i < n; i++)
        {
            if (recv_stamps[i].tv_sec == 0 && recv_stamps[i].tv_usec == 0)
                continue;
            latency += (recv_stamps[i].tv_sec - sent_stamps[i].tv_sec) * 1000000 + (recv_stamps[i].tv_usec - sent_stamps[i].tv_usec);
            count++;
        }
        latency /= count;
        latency /= 2.0;
        count = 0;
        for (int i = 0; i < n; i++)
        {
            if (recv_stamps_bw[i].tv_sec == 0 && recv_stamps_bw[i].tv_usec == 0)
                continue;
            latency_bw += (recv_stamps_bw[i].tv_sec - sent_stamps_bw[i].tv_sec) * 1000000 + (recv_stamps_bw[i].tv_usec - sent_stamps_bw[i].tv_usec);
            count++;
        }
        latency_bw /= count;
        latency_bw /= 2.0;

        if (latency == 0 || latency_bw == 0)
        {
            printf("\n| Hop: %-2d | Link: %-8s - %-8s | Estimated Latency: Link Unreachable | Estimated Bandwidth: Link Unreachable |\n\n", ttl, prev_ip, inet_ntoa(*(struct in_addr *)&max_ip));
            strcpy(prev_ip, inet_ntoa(*(struct in_addr *)&max_ip));
            // check if the max_ip is the destination ip
            if (max_ip == dest_addr.sin_addr.s_addr)
                break;
            continue;
        }

        latency -= cumm_latency;
        cumm_latency += latency;
        latency_bw -= cumm_latency_bw;
        cumm_latency_bw += latency_bw;

        double bandwidth = 300.0 / (latency_bw - latency);

        if (latency < 0)
            latency = 0.0;
        if (bandwidth < 0)
            bandwidth = INFINITY;
        printf("\n| Hop: %-2d | Link: %-8s - %-8s | Estimated Latency: %5.2lf us | Estimated Bandwidth: %5.2lf MBps |\n\n", ttl, prev_ip, inet_ntoa(*(struct in_addr *)&max_ip), latency, bandwidth);

        strcpy(prev_ip, inet_ntoa(*(struct in_addr *)&max_ip));
        // check if the max_ip is the destination ip
        if (max_ip == dest_addr.sin_addr.s_addr)
            break;
    }

    return 0;
}