#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

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

/* Utility function to input a string from the user
   @params: struct string* - The string structure to which the input will be stored
*/
void inputString(struct string *s)
{
    char ch;
    int i = 0;
    while ((ch = getchar()) != '\n')
    {
        if (i + 1 == s->capacity)
        {
            s->capacity *= 2;
            s->str = (char *)realloc(s->str, s->capacity);
        }
        s->str[i++] = ch;
        s->str[i] = '\0';
    }
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

    while (1)
    {
        // Opening a socket is exactly similar to the server process
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("Unable to create socket\n");
            exit(0);
        }

        printf("MyOwnBrowser> ");
        struct string command = init_string();
        inputString(&command);

        char *cmd = strtok(command.str, " ");
        char *url = strtok(NULL, " ");
        int port = 80;
        char *IP;
        if (!strcmp(cmd, "GET"))
        {
            char *IPandFile = strtok(url + 7, ":");
            char *Port = strtok(NULL, ":");
            if (Port)
                port = atoi(Port);
            struct string file = init_string();
            IP = strtok(IPandFile, "/");
            for (char *path; path = strtok(NULL, "/");)
            {
                concat_string(&file, "/", 1);
                concat_string(&file, path, strlen(path));
            }
            printf("IP: %s, Port: %d, file: %s\n", IP, port, file.str);

            serv_addr.sin_family = AF_INET;
            inet_aton(IP, &serv_addr.sin_addr);
            serv_addr.sin_port = htons(port);

            if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
            {
                perror("Unable to connect to server\n");
                exit(0);
            }

            // Frame HTTP request
            struct string request = init_string();

            concat_string(&request, "GET ", 4);
            concat_string(&request, file.str, file.size);
            concat_string(&request, " HTTP/1.1\n", 10);

            concat_string(&request, "Host: ", 6);
            concat_string(&request, IP, strlen(IP));
            char port_str[7];
            sprintf(port_str, ":%d", port);
            concat_string(&request, port_str, strlen(port_str));
            concat_string(&request, "\n", 1);

            concat_string(&request, "Connection: close\n", 18);

            concat_string(&request, "Date: ", 6);
            time_t t = time(NULL);
            struct tm tm = *gmtime(&t);
            char date[50];
            strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", &tm);
            concat_string(&request, date, strlen(date));
            concat_string(&request, "\n", 1);

            concat_string(&request, "Accept: ", 8);
            strtok(file.str, ".");
            char *extension = strtok(NULL, ".");
            if (!strcmp(extension, "html"))
            {
                concat_string(&request, "text/html", 9);
            }
            else if (!strcmp(extension, "pdf"))
            {
                concat_string(&request, "application/pdf", 15);
            }
            else if (!strcmp(extension, "jpg"))
            {
                concat_string(&request, "image/jpeg", 10);
            }
            else
            {
                concat_string(&request, "text/*", 6);
            }
            concat_string(&request, "\n", 1);

            concat_string(&request, "Accept-Language: en-us, en;q=0.9\n", 33);

            concat_string(&request, "If-Modified-Since: ", 19);
            // Put date - 2 days
            tm.tm_mday -= 2;
            strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", &tm);
            concat_string(&request, date, strlen(date));
            concat_string(&request, "\n\n", 2);

            deinit_string(file);
            // printf("%s", request.str);

            send(sockfd, request.str, request.size + 1, 0);
            deinit_string(request);

            // Receive the response from the server
            struct string response = init_string();
            int recv_len = recv(sockfd, buf, 50, 0);
            while (buf[recv_len - 1])
            {
                concat_string(&response, buf, recv_len);
                recv_len = recv(sockfd, buf, 50, 0);
            }
            concat_string(&response, buf, recv_len);
            printf("%s\n", response.str);

            // Extract the response code
            char *response_code = strtok(response.str, " ");
            response_code = strtok(NULL, " ");
            printf("Response code: %s\n", response_code);

            if (!strcmp(response_code, "200"))
            {
                // Extract the content length
                char *content_length = strtok(NULL, " ");
                content_length = strtok(NULL, " ");
                printf("Content length: %s\n", content_length);
                int content_length_int = atoi(content_length);

                // Extract the content type
                char *content_type = strtok(NULL, " ");
                content_type = strtok(NULL, " ");
                printf("Content type: %s\n", content_type);

                // Extract the content
                char *content = strtok(NULL, " ");
                content = strtok(NULL, " ");
            }

            deinit_string(response);
        }
        else if (!strcmp(cmd, "PUT"))
        {
            char *filename = strtok(NULL, " ");
            char *IPandFile = strtok(url + 7, ":");
            char *Port = strtok(NULL, ":");
            if (Port)
                port = atoi(Port);
            struct string file = init_string();
            IP = strtok(IPandFile, "/");
            for (char *path; path = strtok(NULL, "/");)
            {
                concat_string(&file, "/", 1);
                concat_string(&file, path, strlen(path));
            }
            concat_string(&file, "/", 1);
            concat_string(&file, filename, strlen(filename));
            printf("IP: %s, Port: %d, file: %s\n", IP, port, file.str);
        }
        else if (!strcmp(cmd, "QUIT"))
        {
            deinit_string(command);
            close(sockfd);
            break;
        }
        else
            printf("Invalid command\n");

        deinit_string(command);

        // Close the socket
        close(sockfd);
    }
    return 0;
}
