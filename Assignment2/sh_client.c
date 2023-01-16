#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* A structure to hold a string */
struct string
{
	char *str;
	size_t size;
	size_t capacity;
};
// An initializer for the string structure (works like a constructor)
struct string init_string()
{
	struct string s;
	s.str = (char *)calloc(1, sizeof(char));
	s.size = 0;
	s.capacity = 1;
	return s;
}
// A de-initializer function for string structure (works like a destructor)
void deinit_string(struct string s)
{
	free(s.str);
}

/* Utility function to concatanate two strings
   @params: struct string * - The string to which concatanation will take place
			char * - The string to be concatanated onto the struct string
			size_t - The length of the sting char* to be concatenated
*/
void concat_string(struct string *s1, char *str, size_t len)
{
	if (s1->size + len >= s1->capacity)
	{
		s1->capacity = s1->size + len + 1;
		s1->str = (char *)realloc(s1->str, s1->capacity);
	}
	memcpy(s1->str + s1->size, str, len);
	s1->size += len;
	s1->str[s1->size] = '\0';
}

/* THE CLIENT PROCESS */

int main()
{
	int sockfd; // Socket descriptors
	struct sockaddr_in serv_addr;

	char buf[50]; // Buffer for communicating through client

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

	// Recieve the 'LOGIN' prompt from the server to start communication
	int recv_len = recv(sockfd, buf, 50, 0);
	while (buf[recv_len - 1])
	{
		recv_len += recv(sockfd, buf + recv_len, 50 - recv_len, 0);
	}
	printf("%s ", buf);

	// Take the username as input from the user and send it to the server for verification
	scanf("%s%*c", buf);
	send(sockfd, buf, strlen(buf) + 1, 0);

	// Recieve the validity string for the username
	recv_len = recv(sockfd, buf, 50, 0);
	while (buf[recv_len - 1])
	{
		recv_len += recv(sockfd, buf + recv_len, 50 - recv_len, 0);
	}

	// If the username is not found from the server side close the socket and exit
	if (!strcmp(buf, "NOT-FOUND"))
	{
		printf("Invalid username\n");
		close(sockfd);
		exit(0);
	}

	// If the username is validated on the server side proceed to take bash commands
	else if (!strcmp(buf, "FOUND"))
	{
        // Looping construct to ask for users the shell commands until they exit
		do
		{
			int i = 0;
			char ch;
			while ((ch = getchar()) != '\n')
			{
				buf[i++] = ch;
				i %= 50;
				if (i == 0)
					send(sockfd, buf, 50, 0);
			}
			buf[i] = '\0';
			send(sockfd, buf, strlen(buf) + 1, 0);
			if (!strcmp(buf, "exit"))
			{
				break;
			}
			struct string recv_string = init_string();
			recv_len = recv(sockfd, buf, 50, 0);
			while (buf[recv_len - 1])
			{
				concat_string(&recv_string, buf, recv_len);
				recv_len = recv(sockfd, buf, 50, 0);
			}
			concat_string(&recv_string, buf, recv_len);
			if (!strcmp(recv_string.str, "$$$$"))
			{
				printf("Invalid command\n");
			}
			else if (!strcmp(recv_string.str, "####"))
			{
				printf("Error in runnig command\n");
			}
			else if (!strcmp(recv_string.str, ""))
			{
			}
			else
			{
				printf("%s\n", recv_string.str);
			}
			deinit_string(recv_string);
		} while (strcmp(buf, "exit"));
	}

	close(sockfd);
	return 0;
}
