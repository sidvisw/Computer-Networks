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

int main()
{
	int sockfd, newsockfd; // Socket descriptors
	int clilen;
	struct sockaddr_in cli_addr, serv_addr;

	int i;
	char buf[100]; // Buffer for communicating through client

	// System call to open a socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Cannot create socket\n");
		exit(0);
	}

	// Assign the values for sin_family, sin_addr, and sin_port according to the TCP communication
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(20000);

	// Associate the server with its port using the bind() system call.
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

		// We initialize the buffer, copy the message to it, and send the message to the client.
		// Extract the date and time from time.h module
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
		sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

		send(newsockfd, buf, strlen(buf) + 1, 0);

		close(newsockfd);
	}
	return 0;
}
