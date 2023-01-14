#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// A string structure analogous to the C++ std::string
struct string
{
    char *str;
    int size;
    int capacity;
};

// Constructor function for the string structure
struct string init_string()
{
    struct string str;
    str.size = 0;
    str.capacity = 1;
    str.str = (char *)malloc(str.capacity * sizeof(char));
    str.str[0] = '\0';
    return str;
}

// Destructor function for the string structure
void deinit_string(struct string str)
{
    free(str.str);
}

// Function to append a character to the string
struct string push_back(struct string str, const char ch)
{
    if (str.size + 1 == str.capacity)
    {
        str.capacity *= 2;
        str.str = (char *)realloc(str.str, str.capacity * sizeof(char));
    }
    str.str[str.size++] = ch;
    str.str[str.size] = '\0';
    return str;
}

/* THE CLIENT PROCESS */

int main()
{
    FILE*fp=fopen("test.txt", "w");
    int sockfd;
    struct sockaddr_in serv_addr;

    int i;
    double *buf = (double *)malloc(sizeof(double));

    // Opening a socket is exactly similar to the server process
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Unable to create socket\n");
        exit(0);
    }

    // Specify the IP address of the server. We assume that the server is running on the same machine as the client. 127.0.0.1 is a special address for "localhost" (this machine)
    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(20000);

    // connect() system call establishes a connection with the server process
    if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
    {
        perror("Unable to connect to server\n");
        exit(0);
    }

    // Initialise the string structure to store the expression the user inputs
    struct string expr = init_string();

    printf("Enter the expression: ");
    char ch;
    // Take input character by character until a newline character is reached
    while ((ch = getchar()) != '\n')
    {
        expr = push_back(expr, ch);
    }

    // Loop to evaluate expressions until -1 is entered
    while (strcmp(expr.str, "-1"))
    {
        printf("String Sent : %d\n", expr.size);
        fprintf(fp,"%s\n", expr.str);
        // Send the expression to the server and recieve the result
        send(sockfd, expr.str, expr.size + 1, 0);
        recv(sockfd, buf, sizeof(double), 0);
        printf("Result: %lf\n", *buf);

        // Free the string structure and take input for the next expression
        deinit_string(expr);

        printf("Enter the expression: ");
        expr = init_string();
        while ((ch = getchar()) != '\n')
        {
            expr = push_back(expr, ch);
        }
    }

    // Send -1 to the server to indicate that the client is done
    send(sockfd, expr.str, expr.size + 1, 0);

    // Free the dynamically allocated memory
    deinit_string(expr);

    close(sockfd);
    return 0;
}
