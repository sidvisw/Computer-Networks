#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* THE CLIENT PROCESS */

int main()
{
	int sockfd; // Socket descriptor
	struct sockaddr_in lb_addr;

	char buf[50]; // Buffer for communicating through load balancer

	// Opening a socket is exactly similar to the server process
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Unable to create socket\n");
		exit(0);
	}

	// Specify the IP address of the load balancer. We assume that the server is running on the same machine as the client. 127.0.0.1 is a special address for "localhost" (this machine)
	lb_addr.sin_family = AF_INET;
	inet_aton("127.0.0.1", &lb_addr.sin_addr);
	lb_addr.sin_port = htons(20000);

	// connect() system call establishes a connection with the load balancer process
	if ((connect(sockfd, (struct sockaddr *)&lb_addr, sizeof(lb_addr))) < 0)
	{
		perror("Unable to connect to server\n");
		exit(0);
	}

	// Recieve the date and time from the load balancer and store it in a null initialized buffer
	int recv_len = recv(sockfd, buf, 50, 0);
	while (buf[recv_len - 1])
		recv_len += recv(sockfd, buf + recv_len, 50 - recv_len, 0);

	// Print the date and time
	printf("%s\n", buf);

	// Close the socket
	close(sockfd);
	return 0;
}
