/*
    Execution Instructions:
    gcc mytraceroute_19CS30008.c -o mytraceroute 
    sudo ./mytraceroute www.example.com
*/

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

#define ERROR(msg, ...) printf("\033[1;31m[ERROR] " msg " \033[0m\n", ##__VA_ARGS__);
#define SUCCESS(msg, ...) printf("\033[1;36m[SUCCESS] " msg " \033[0m\n", ##__VA_ARGS__);
#define INFO(msg, ...) printf("\033[1;34m[INFO] " msg " \033[0m\n", ##__VA_ARGS__);
#define DEBUG(msg, ...) printf("\033[1;32m[DEBUG] " msg "\033[0m", ##__VA_ARGS__);

#define PAYLOAD_SIZE 52
#define DEST_PORT 32164
#define LOCAL_PORT 20000
#define MAX_SIZE 100
#define MAX_RUN_TIME 1000000  // microseconds

uint16_t in_cksum(uint16_t *addr, int len) {
    int nleft = len;
    uint32_t sum = 0;
    uint16_t *w = addr;
    uint16_t answer = 0;
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }
    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;
    return (answer);
}

void set_timeout(struct timeval *tv, int rem_time) {
    tv->tv_sec = 0;
    tv->tv_usec = rem_time;
    return;
}

int get_time_in_ms(struct timeval *tv) {
    return (int)tv->tv_sec * 1000 + (int)tv->tv_usec / 1000;
}

void print(int ttl, struct sockaddr_in *addr, struct timeval *send, struct timeval *recv) {
    char timeout[] = "       *";
    if (ttl == 1) {
        printf("Hop_Count(TTL)\t\tIP address\t  Response_Time\n\n");
    }
    if (addr == NULL) {
        printf("\t%-10d\t%-15s\t%-15s\n", ttl, timeout, timeout);
        return;
    } else {
        struct timeval diff;
        timersub(recv, send, &diff);
        printf("\t%-10d\t%-15s\t%8d ms\n", ttl, inet_ntoa(addr->sin_addr), get_time_in_ms(&diff));
    }
    return;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        ERROR("Domain name not specified");
        exit(1);
    }
    struct hostent *h = gethostbyname(argv[1]);
    // if host not found
    if (h == NULL) {
        ERROR("Could not resolve host name");
        exit(1);
    }
    // get IP address of the host
    struct in_addr dest_ip = *(struct in_addr *)h->h_addr_list[0];

    // print destination ip
    printf("Destination IP for %s: %s\n\n", argv[1], inet_ntoa(dest_ip));

    int sockfd_udp, sockfd_icmp;
    struct sockaddr_in local_addr, dest_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    memset(&dest_addr, 0, sizeof(dest_addr));

    // set local address
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(LOCAL_PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;

    // create socket for udp
    if ((sockfd_udp = socket(AF_INET, SOCK_RAW, IPPROTO_UDP)) < 0) {
        perror("Could not create udp socket");
        exit(1);
    }
    // set IP_HDRINCL to true to tell the kernel that headers are included in the packet
    int opt = 1;
    if (setsockopt(sockfd_udp, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)) < 0) {
        perror("Could not set socket option for udp socket");
        exit(1);
    }
    // bind the udp socket to local address
    if (bind(sockfd_udp, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Could not bind udp socket");
        exit(1);
    }

    // create socket for icmp
    if ((sockfd_icmp = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        perror("Could not create icmp socket");
        exit(1);
    }
    // bind the icmp socket to local address
    if (bind(sockfd_icmp, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Could not bind icmp socket");
        exit(1);
    }

    // set destination address
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    dest_addr.sin_addr = dest_ip;

    int ttl = 1;
    // iterate on Incremental Max Hop Values to check the Route of the Packet
    while (ttl <= 16) {
        srand(time(NULL));
        int time_out = 0, done = 0;
        for (int i = 0; i < 3; i++) {
            time_out = 0;
            struct timeval start, stop, curr;
            gettimeofday(&start, NULL);
            char send_buf[MAX_SIZE];
            memset(send_buf, 0, sizeof(send_buf));
            // create udp packet
            struct iphdr *ip = (struct iphdr *)send_buf;
            struct udphdr *udp = (struct udphdr *)(send_buf + sizeof(struct iphdr));
            char payload[PAYLOAD_SIZE];
            memset(payload, 0, sizeof(payload));
            // generate random payload
            for (int j = 0; j < PAYLOAD_SIZE; j++) {
                payload[j] = rand() % 26 + 'a';
            }

            // fill the udp header
            udp->source = htons(LOCAL_PORT);
            udp->dest = htons(DEST_PORT);
            udp->len = htons(sizeof(struct udphdr) + PAYLOAD_SIZE);
            udp->check = 0;

            // fill the ip header
            ip->ihl = 5;
            ip->version = 4;
            ip->tos = 0;
            ip->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + PAYLOAD_SIZE);
            ip->id = htons(rand());
            ip->frag_off = 0;
            ip->ttl = ttl;
            ip->protocol = IPPROTO_UDP;
            ip->check = in_cksum((unsigned short *)ip, sizeof(struct iphdr));
            ip->saddr = local_addr.sin_addr.s_addr;
            ip->daddr = dest_addr.sin_addr.s_addr;

            // fill the payload
            memcpy(send_buf + sizeof(struct iphdr) + sizeof(struct udphdr), payload, PAYLOAD_SIZE);

            // send the udp packet to the destination
            if (sendto(sockfd_udp, send_buf, sizeof(struct iphdr) + sizeof(struct udphdr) + PAYLOAD_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
                perror("Could not send packet");
                exit(1);
            }

            // set the timeout for receiving the icmp packet
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = MAX_RUN_TIME;

            while (1) {
                fd_set fd;
                FD_ZERO(&fd);
                FD_SET(sockfd_icmp, &fd);

                // select on the raw icmp socket and wait for the icmp packet or a timeout
                int ret = select(sockfd_icmp + 1, &fd, NULL, NULL, &tv);
                // if a packet is received
                if (FD_ISSET(sockfd_icmp, &fd)) {
                    char recv_buf[MAX_SIZE];
                    memset(recv_buf, 0, sizeof(recv_buf));
                    struct sockaddr_in from_addr;
                    socklen_t from_addr_len = sizeof(from_addr);
                    int n = recvfrom(sockfd_icmp, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&from_addr, &from_addr_len);
                    if (n < 0) {
                        perror("Could not receive packet");
                        exit(1);
                    }
                    struct iphdr *recvd_ip = (struct iphdr *)recv_buf;
                    // if the packet is an icmp packet
                    if (recvd_ip->protocol == IPPROTO_ICMP) {
                        struct icmphdr *recvd_icmp = (struct icmphdr *)(recv_buf + sizeof(struct iphdr));
                        // if the icmp packet is for the destination unreachable
                        if (recvd_icmp->type == ICMP_DEST_UNREACH && recvd_icmp->code == ICMP_PORT_UNREACH) {
                            // check that the source IP address of the ICMP Destination Unreachable Message matches with your target server IP address
                            if (recvd_ip->saddr == dest_addr.sin_addr.s_addr) {
                                gettimeofday(&stop, NULL);
                                print(ttl, &from_addr, &start, &stop);
                                close(sockfd_udp);
                                close(sockfd_icmp);
                                exit(0);
                            } else {  // if the source IP address of the ICMP Destination Unreachable Message does not match with your target server IP address
                                gettimeofday(&curr, NULL);
                                struct timeval diff;
                                timersub(&curr, &start, &diff);
                                int time_diff = diff.tv_sec * 1000000 + diff.tv_usec;
                                if (time_diff >= MAX_RUN_TIME) {
                                    time_out = 1;
                                    break;
                                }
                                set_timeout(&tv, MAX_RUN_TIME - time_diff);
                            }
                        } else if (recvd_icmp->type == ICMP_TIME_EXCEEDED && recvd_icmp->code == ICMP_EXC_TTL) {  // if the icmp packet is for the time exceeded
                            gettimeofday(&stop, NULL);
                            print(ttl, &from_addr, &start, &stop);
                            done = 1;
                            break;
                        }
                    } else {  // if the packet is not an icmp packet
                        gettimeofday(&curr, NULL);
                        struct timeval diff;
                        timersub(&curr, &start, &diff);
                        int time_diff = diff.tv_sec * 1000000 + diff.tv_usec;
                        if (time_diff >= MAX_RUN_TIME) {
                            time_out = 1;
                            break;
                        }
                        set_timeout(&tv, MAX_RUN_TIME - time_diff);
                    }
                }
                // if the timeout occurs on the select call
                if (ret == 0) {
                    time_out = 1;
                    break;
                }
            }
            // if the source address of the hop is printed, break from the for loop
            if (done == 1) {
                break;
            }
        }
        if (time_out) {
            // timed out all the 3 times
            print(ttl, NULL, NULL, NULL);
        }
        ttl++;
    }
    close(sockfd_udp);   // close the udp socket
    close(sockfd_icmp);  // close the icmp socket
    exit(0);             // exit the program
}