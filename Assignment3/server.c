#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

/* THE SERVER PROCESS */

int main(int argc, char *argv[])
{
	// Check for command line arguments
	if (argc != 2)
	{
		printf("Usage: ./server <port>\n");
		exit(EXIT_FAILURE);
	}

	int sockfd, newsockfd; // Socket descriptors
	int clilen;
	struct sockaddr_in cli_addr, serv_addr;

	char buf[50]; // Buffer for communicating through load balancer

	// System call to open a socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Cannot create socket\n");
		exit(0);
	}

	// Assign the values for sin_family, sin_addr, and sin_port according to the TCP communication
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(atoi(argv[1]));

	// Associate the server with its port using the bind() system call
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("Unable to bind local address\n");
		exit(0);
	}

	// Specifies that up to 5 concurrent client requests will be queued up
	listen(sockfd, 5);

	/*
		Looping construct for iterative server. After the
		communication is over, the process comes back to wait again on
		the original socket descriptor.
	*/
	while (1)
	{
		/*
			The accept() system call accepts a client connection.
			Blocks the server until a client request comes.
		*/
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

		if (newsockfd < 0)
		{
			perror("Accept error\n");
			exit(0);
		}

		// The recv() call receives the message from the load balancer
		int recv_len = recv(newsockfd, buf, 50, 0);
		while (buf[recv_len - 1])
		{
			recv_len += recv(newsockfd, buf + recv_len, 50 - recv_len, 0);
		}

		if (!strcmp(buf, "Send Time"))
		{
			// Send the current date and time
			time_t t = time(NULL);
			struct tm tm = *localtime(&t);
			sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

			send(newsockfd, buf, strlen(buf) + 1, 0);
		}
		else if (!strcmp(buf, "Send Load"))
		{
			// Send a random number between 1 and 100
			srand(time(0));
			int load = rand() % 100 + 1;
			sprintf(buf, "%d", load);

			send(newsockfd, buf, strlen(buf) + 1, 0);

			printf("Load sent: %s\n", buf);
		}

		// Close the new socket descriptor
		close(newsockfd);
	}
	return 0;
}
