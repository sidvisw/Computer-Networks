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

unsigned short in_cksum(unsigned short *addr, int len){
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while(nleft > 1){
        sum += *w++;
        nleft -= 2;
    }

    if(nleft == 1){
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
    if(argc != 2) {
        printf("Usage: %s <IP address/DNS Name>\n", argv[0]);
        exit(1);
    }

    // convert the DNS name to IP address
    struct hostent *host;
    host = gethostbyname(argv[1]);
    if(host == NULL) {
        printf("Invalid DNS name or IP address\n");
        exit(1);
    }
    struct in_addr dest_ip = *(struct in_addr *)host->h_addr_list[0];

    int sockfd;
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(1);
    }

    struct sockaddr_in dest_addr, src_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    memset(&src_addr, 0, sizeof(src_addr));

    // fill in the source address
    src_addr.sin_family = AF_INET;
    src_addr.sin_addr.s_addr = INADDR_ANY;
    src_addr.sin_port = htons(8080);

    // fill in the destination address
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = dest_ip;
    dest_addr.sin_port = htons(8080);

    // set the socket option
    int optval = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval)) < 0) {
        perror("setsockopt failed");
        exit(1);
    }


    // construct the IP header
    char packet[1024];
    struct iphdr *ip = (struct iphdr *)packet;
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr);
    ip->id = htons(54321);
    ip->frag_off = 0;
    ip->ttl = 1;
    ip->protocol = IPPROTO_ICMP;
    ip->check = 0;
    ip->saddr = src_addr.sin_addr.s_addr;
    ip->daddr = dest_addr.sin_addr.s_addr;

    // construct the ICMP header
    struct icmphdr *icmp = (struct icmphdr *)(packet + sizeof(struct iphdr));
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->un.echo.id = htons(10);
    icmp->un.echo.sequence = htons(10);
    icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr));

    for(int i=1; i<=16; i++)
    {   
        ip->ttl = i;

        // send the packet
        if (sendto(sockfd, packet, ip->tot_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            perror("sendto failed");
            exit(1);
        }

        // receiver address
        struct sockaddr_in recv_addr;
        socklen_t recv_addr_len = sizeof(recv_addr);
        memset(&recv_addr, 0, sizeof(recv_addr));

        // receive the packet
        char recv_packet[1024];
        int n = recvfrom(sockfd, recv_packet, sizeof(recv_packet), 0, (struct sockaddr *)&recv_addr, &recv_addr_len);
        if (n < 0) {
            perror("recvfrom failed");
            exit(1);
        }

        // extract the IP header
        struct iphdr *recv_ip = (struct iphdr *)recv_packet;

        // extract the ICMP header
        struct icmphdr *recv_icmp = (struct icmphdr *)(recv_packet + sizeof(struct iphdr));

        // check if the recv_packet is an ICMP echo reply
        if (recv_icmp->type == ICMP_ECHOREPLY) {
            printf("ICMP echo reply received from %s\n", inet_ntoa(recv_addr.sin_addr));
            if(recv_icmp->un.echo.id == icmp->un.echo.id && recv_icmp->un.echo.sequence == icmp->un.echo.sequence)
            {
                printf("Matched ICMP echo reply received from %s\n", inet_ntoa(recv_addr.sin_addr));
            }
            break;
        }

        // check if the recv_packet is an ICMP time exceeded
        if (recv_icmp->type == ICMP_TIME_EXCEEDED) {
            printf("ICMP time exceeded received from %s\n", inet_ntoa(recv_addr.sin_addr));
        }

        // check if the recv_packet is an ICMP destination unreachable
        if (recv_icmp->type == ICMP_DEST_UNREACH) {
            printf("ICMP destination unreachable received from %s\n", inet_ntoa(recv_addr.sin_addr));
            break;
        }
    }

    
    
    



}