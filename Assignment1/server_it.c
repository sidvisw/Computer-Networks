#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

/*
    Function to evaluate a given expression in string format
    Supports +, -, *, /, (, )
    @param: const char * - pointer to the string to be evaluated
    @return: double - the result of the expression
*/
double evaluate(const char *const expr)
{
    double result = 0.0; // Variable to store the result
    char op = '+';       // Variable to store the current operator
    double value = 0.0;  // Variable to store the current read number
    int i = 0;

    // Loop through the string to calculate the value of the expression
    while (expr[i] != '\0')
    {
        // Action to be taken when an operator is encountered
        if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/')
        {
            op = expr[i];
            i++;
        }
        // Action to be taken when we encounter an opening parenthesis
        else if (expr[i] == '(')
        {
            char *sub_expr; // Variable to store the sub-expression inside the parenthesis
            int j = i + 1;
            int count = 1;

            // Loop to calculate the bounds of the sub-expression
            while (count != 0)
            {
                if (expr[j] == '(')
                {
                    count++;
                }
                else if (expr[j] == ')')
                {
                    count--;
                }
                j++;
            }

            sub_expr = (char *)malloc(sizeof(char) * (j - i - 1));

            int k = 0;
            // Store the sub-expression in a string
            for (int l = i + 1; l < j - 1; l++)
            {
                sub_expr[k++] = expr[l];
            }
            sub_expr[k] = '\0';

            // Evaluate the sub-expression
            value = evaluate(sub_expr);

            free(sub_expr);

            // Calculate the result by performing the required operator on the result and value
            switch (op)
            {
            case '+':
                result = result + value;
                break;
            case '-':
                result = result - value;
                break;
            case '*':
                result = result * value;
                break;
            case '/':
                result = result / value;
                break;
            }
            i = j;
        }
        // Action to be taken when we encounter a number
        else if (expr[i] >= '0' && expr[i] <= '9')
        {
            value = 0.0; // Initialise the value with 0.0
            // Loop till the last digit to obtain the number
            while (expr[i] >= '0' && expr[i] <= '9')
            {
                value = value * 10 + (expr[i] - '0');
                i++;
            }
            // Case for handling floating point numbers
            if (expr[i] == '.')
            {
                double factor = 0.1;
                i++;
                while (expr[i] >= '0' && expr[i] <= '9')
                {
                    value = value + (expr[i] - '0') * factor;
                    factor = factor / 10;
                    i++;
                }
            }
            // Appropriately apply the operator to result and value
            switch (op)
            {
            case '+':
                result = result + value;
                break;
            case '-':
                result = result - value;
                break;
            case '*':
                result = result * value;
                break;
            case '/':
                result = result / value;
                break;
            }
        }
        else
        {
            i++;
        }
    }
    return result; // Return the result of the expression
}

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

        // recieve 100 bytes of the expression in the buffer from the client
        recv(newsockfd, buf, 100, 0);
        // Loop to continue evaluating expression till -1 is not entered
        while (strcmp(buf, "-1"))
        {
            char *expr = (char *)calloc(100, sizeof(char)); // String to store the expression
            int cur_len = 100;
            strcpy(expr, buf);
            int recieved = !expr[cur_len - 1];
            // Loop till the entire expression is recieved
            while (!recieved)
            {
                expr = (char *)realloc(expr, (cur_len + 100) * sizeof(char));
                cur_len += 100;
                recv(newsockfd, expr + cur_len - 100, 100, 0);
                for (int i = 0; i < cur_len; i++)
                {
                    if (!expr[i])
                        recieved = 1;
                }
            }
            // Evaluate the expression after all of it is recieved from the client
            double result = evaluate(expr);
            free(expr);
            // Send the result back to the client
            send(newsockfd, &result, sizeof(result), 0);
            // Continue recieving next expression from the client
            recv(newsockfd, buf, 100, 0);
        }

        close(newsockfd);
    }
    return 0;
}
