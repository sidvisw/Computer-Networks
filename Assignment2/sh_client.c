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
	int sockfd;
	struct sockaddr_in serv_addr;

	char buf[50];

	// Opening a socket is exactly similar to the server process
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Unable to create socket\n");
		exit(0);
	}

	// Specify the IP address of the server. We assume that the server is running on the same machine as the client.
	// 127.0.0.1 is a special address for "localhost" (this machine)
	serv_addr.sin_family = AF_INET;
	inet_aton("127.0.0.1", &serv_addr.sin_addr);
	serv_addr.sin_port = htons(20000);

	// connect() system call establishes a connection with the server process
	if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
	{
		perror("Unable to connect to server\n");
		exit(0);
	}

	int recv_len = recv(sockfd, buf, 50, 0);
	while (buf[recv_len - 1])
	{
		recv_len += recv(sockfd, buf + recv_len, 50 - recv_len, 0);
	}
	printf("%s ", buf);
	scanf("%s%*c", buf);
	send(sockfd, buf, strlen(buf) + 1, 0);

	recv_len = recv(sockfd, buf, 50, 0);
	while (buf[recv_len - 1])
	{
		recv_len += recv(sockfd, buf + recv_len, 50 - recv_len, 0);
	}
	if (!strcmp(buf, "NOT-FOUND"))
	{
		close(sockfd);
		exit(0);
	}

	int i = 0;
	char ch;
	while ((ch = getchar()) != '\n')
	{
		buf[i++] = ch;
		i %= 50;
		if (!i)
			send(sockfd, buf, 50, 0);
	}
	if (i)
		send(sockfd, buf, i, 0);
	send(sockfd, "\0", 1, 0);

	close(sockfd);
	return 0;
}
