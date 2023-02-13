#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define __USE_XOPEN
#include <time.h>

// #define _XOPEN_SOURCE

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
void input_string(struct string *s)
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
        input_string(&command);

        char *cmd = strtok(command.str, " ");
        if (!cmd)
        {
            deinit_string(command);
            continue;
        }
        char *url = strtok(NULL, " ");

        // Extract IP, Port and Path
        char *IP;
        int port = 80;

        if (!strcmp(cmd, "GET"))
        {
            char *IPandFile = strtok(url + 7, ":");
            char *Port = strtok(NULL, ":");
            if (Port)
                port = atoi(Port);

            struct string path = init_string();
            IP = strtok(IPandFile, "/");
            for (char *dir; (dir = strtok(NULL, "/"));)
            {
                concat_string(&path, "/", 1);
                concat_string(&path, dir, strlen(dir));
            }

            serv_addr.sin_family = AF_INET;
            inet_aton(IP, &serv_addr.sin_addr);
            serv_addr.sin_port = htons(port);

            if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
            {
                perror("Unable to connect to server\n");
                exit(1);
            }

            // Frame HTTP request
            struct string request = init_string();

            concat_string(&request, "GET ", 4);
            concat_string(&request, path.str, path.size);
            concat_string(&request, " HTTP/1.1\r\n", 11);

            concat_string(&request, "Host: ", 6);
            concat_string(&request, IP, strlen(IP));
            char port_str[7];
            sprintf(port_str, ":%d", port);
            concat_string(&request, port_str, strlen(port_str));
            concat_string(&request, "\r\n", 2);

            concat_string(&request, "Connection: close\r\n", 19);

            concat_string(&request, "Date: ", 6);
            time_t t = time(NULL);
            struct tm tm = *gmtime(&t);
            char date[50];
            strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", &tm);
            concat_string(&request, date, strlen(date));
            concat_string(&request, "\r\n", 2);

            concat_string(&request, "Accept: ", 8);
            strtok(path.str, ".");
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
            concat_string(&request, "\r\n", 2);

            concat_string(&request, "Accept-Language: en-us, en;q=0.9\r\n", 34);

            concat_string(&request, "If-Modified-Since: ", 19);
            tm.tm_mday -= 2;
            strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", &tm);
            concat_string(&request, date, strlen(date));
            concat_string(&request, "\r\n\r\n", 4);

            printf("%s", request.str);

            send(sockfd, request.str, request.size, 0);
            deinit_string(request);

            // Get the response and process it
            struct pollfd pfd;
            pfd.fd = sockfd;
            pfd.events = POLLIN;
            int ret = poll(&pfd, 1, 3000);
            if (ret < 0)
            {
                perror("Error in poll\n");
                exit(1);
            }
            if (ret == 0)
            {
                printf("Timeout 3 sec...\n");
                deinit_string(path);
                deinit_string(command);
                close(sockfd);
                continue;
            }

            struct string remaining = init_string();
            struct string content_type = init_string();
            int content_len = 0, error_in_response = 0;
            char errors[1024] = "";
            while (1)
            {
                struct string line = init_string();
                int flag = 0;
                while (!flag)
                {
                    int recv_len = recv(sockfd, buf, 50, 0);
                    concat_string(&remaining, buf, recv_len);
                    for (int i = 0; i < remaining.size - 1; i++)
                    {
                        if (remaining.str[i] == '\r' && remaining.str[i + 1] == '\n')
                        {
                            concat_string(&line, remaining.str, i);
                            struct string temp_str = init_string();
                            concat_string(&temp_str, remaining.str + i + 2, remaining.size - i - 2);
                            deinit_string(remaining);
                            remaining = temp_str;
                            flag = 1;
                            break;
                        }
                    }
                }
                printf("%s\r\n", line.str);
                if (line.size == 0)
                    break;
                if (strstr(line.str, "HTTP/1.1"))
                {
                    if (strstr(line.str, "200 OK"))
                    {
                        strcat(errors, "File found\n");
                    }
                    else if (strstr(line.str, "400 Bad Request"))
                    {
                        strcat(errors, "400 Bad Request\n");
                        error_in_response = 1;
                    }
                    else if (strstr(line.str, "403 Forbidden"))
                    {
                        strcat(errors, "403 Forbidden\n");
                        error_in_response = 1;
                    }
                    else if (strstr(line.str, "404 Not Found"))
                    {
                        strcat(errors, "404 File Not Found\n");
                        error_in_response = 1;
                    }
                    else
                    {
                        strtok(line.str, " ");
                        char *status_code = strtok(NULL, " ");
                        strcat(errors, status_code);
                        strcat(errors, " Unknown error\n");
                        error_in_response = 1;
                    }
                }
                else if (strstr(line.str, "Expires: "))
                {
                    struct tm TM;
                    char *date = line.str + strlen("Expires: ");
                    strptime(date, "%a, %d %b %Y %H:%M:%S %Z", &TM);
                    time_t t = mktime(&TM);
                    if (t < time(NULL))
                    {
                        strcat(errors, "File expired\n");
                        error_in_response = 1;
                    }
                }
                else if (strstr(line.str, "Content-Length: "))
                {
                    char *len = line.str + strlen("Content-Length: ");
                    content_len = atoi(len);
                }
                else if (strstr(line.str, "Content-Type: "))
                {
                    char *type = line.str + strlen("Content-Type: ");
                    concat_string(&content_type, type, strlen(type));
                }
                else if (strstr(line.str, "Last-Modified: "))
                {
                    struct tm TM;
                    char *date = line.str + strlen("Last-Modified: ");
                    strptime(date, "%a, %d %b %Y %H:%M:%S %Z", &TM);
                    time_t t = mktime(&TM);
                    time_t last_modified = mktime(&tm);
                    if (t < last_modified)
                    {
                        strcat(errors, "File has been modified much earlier\n");
                        error_in_response = 1;
                    }
                }
                deinit_string(line);
            }

            printf("%s", errors);

            if (error_in_response)
            {
                deinit_string(content_type);
                deinit_string(remaining);
                deinit_string(path);
                deinit_string(command);
                close(sockfd);
                continue;
            }

            // Get the file
            struct string filename = init_string();
            concat_string(&filename, path.str, strlen(path.str));
            concat_string(&filename, ".", 1);
            concat_string(&filename, extension, strlen(extension));
            deinit_string(path);
            int idx = 0;
            for (int i = 0; i < filename.size; i++)
            {
                if (filename.str[i] == '/')
                {
                    idx = i;
                }
            }
            int fd = open(filename.str + idx + 1, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0)
            {
                perror("Error in opening file\n");
                exit(1);
            }
            int bytes_written = write(fd, remaining.str, remaining.size);
            deinit_string(remaining);
            while (bytes_written < content_len)
            {
                int recv_len = recv(sockfd, buf, 50, 0);
                bytes_written += write(fd, buf, recv_len);
            }
            close(fd);

            if (fork() == 0)
            {
                if (!strcmp(content_type.str, "text/html"))
                {
                    execlp("chromium", "chromium", filename.str + idx + 1, NULL);
                }
                else if (!strcmp(content_type.str, "application/pdf"))
                {
                    execlp("evince", "evince", filename.str + idx + 1, NULL);
                }
                else if (!strcmp(content_type.str, "image/jpeg"))
                {
                    execlp("eog", "eog", filename.str + idx + 1, NULL);
                }
                else if (!strcmp(content_type.str, "text/*"))
                {
                    execlp("gedit", "gedit", filename.str + idx + 1, NULL);
                }
                else
                {
                    printf("Unable to open the file\n");
                }
                exit(0);
            }
            else
            {
                wait(NULL);
                deinit_string(filename);
                deinit_string(content_type);
            }
        }
        else if (!strcmp(cmd, "PUT"))
        {
            char *filename = strtok(NULL, " ");
            char *IPandFile = strtok(url + 7, ":");
            char *Port = strtok(NULL, ":");
            if (Port)
                port = atoi(Port);
            struct string path = init_string();
            IP = strtok(IPandFile, "/");
            for (char *dir; (dir = strtok(NULL, "/"));)
            {
                concat_string(&path, "/", 1);
                concat_string(&path, dir, strlen(dir));
            }
            concat_string(&path, "/", 1);
            concat_string(&path, filename, strlen(filename));

            // printf("IP: %s, Port: %d, Filepath: %s\n", IP, port, path.str);

            serv_addr.sin_family = AF_INET;
            inet_aton(IP, &serv_addr.sin_addr);
            serv_addr.sin_port = htons(port);

            if ((connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0)
            {
                perror("Unable to connect to server\n");
                exit(1);
            }

            // Frame HTTP request
            struct string request = init_string();

            concat_string(&request, "PUT ", 4);
            concat_string(&request, path.str, path.size);
            concat_string(&request, " HTTP/1.1\r\n", 11);

            concat_string(&request, "Host: ", 6);
            concat_string(&request, IP, strlen(IP));
            char port_str[7];
            sprintf(port_str, ":%d", port);
            concat_string(&request, port_str, strlen(port_str));
            concat_string(&request, "\r\n", 2);

            concat_string(&request, "Connection: close\r\n", 19);

            concat_string(&request, "Date: ", 6);
            time_t t = time(NULL);
            struct tm tm = *gmtime(&t);
            char date[50];
            strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", &tm);
            concat_string(&request, date, strlen(date));
            concat_string(&request, "\r\n", 2);

            concat_string(&request, "Content-Language: en-us\r\n", 25);

            int fd = open(filename, O_RDONLY);
            if (fd < 0)
            {
                printf("File not found\n");
                deinit_string(request);
                deinit_string(path);
                deinit_string(command);
                close(sockfd);
                continue;
            }

            struct stat st;
            fstat(fd, &st);
            char content_length[20];
            sprintf(content_length, "%ld", st.st_size);
            concat_string(&request, "Content-Length: ", 16);
            concat_string(&request, content_length, strlen(content_length));
            concat_string(&request, "\r\n", 2);

            concat_string(&request, "Content-Type: ", 14);
            strtok(filename, ".");
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
            concat_string(&request, "\r\n\r\n", 4);
            deinit_string(path);

            printf("%s", request.str);

            send(sockfd, request.str, request.size, 0);
            deinit_string(request);

            int read_len;
            while ((read_len = read(fd, buf, 50)) > 0)
            {
                send(sockfd, buf, read_len, 0);
            }
            close(fd);

            // Get the response and process it
            struct pollfd pfd;
            pfd.fd = sockfd;
            pfd.events = POLLIN;
            int ret = poll(&pfd, 1, 3000);
            if (ret < 0)
            {
                perror("Error in poll\n");
                exit(1);
            }
            if (ret == 0)
            {
                printf("Timeout 3 sec...\n");
                deinit_string(command);
                close(sockfd);
                continue;
            }

            struct string remaining = init_string();
            int error_in_response = 0;
            char errors[1024] = "";
            while (1)
            {
                struct string line = init_string();
                int flag = 0;
                while (!flag)
                {
                    int recv_len = recv(sockfd, buf, 50, 0);
                    concat_string(&remaining, buf, recv_len);
                    for (int i = 0; i < remaining.size - 1; i++)
                    {
                        if (remaining.str[i] == '\r' && remaining.str[i + 1] == '\n')
                        {
                            concat_string(&line, remaining.str, i);
                            struct string temp_str = init_string();
                            concat_string(&temp_str, remaining.str + i + 2, remaining.size - i - 2);
                            deinit_string(remaining);
                            remaining = temp_str;
                            flag = 1;
                            break;
                        }
                    }
                }
                printf("%s\r\n", line.str);
                if (line.size == 0)
                    break;
                if (strstr(line.str, "HTTP/1.1"))
                {
                    if (strstr(line.str, "200 OK"))
                    {
                        strcat(errors, "File uploaded successfully\n");
                    }
                    else if (strstr(line.str, "400 Bad Request"))
                    {
                        strcat(errors, "400 Bad Request\n");
                        error_in_response = 1;
                    }
                    else if (strstr(line.str, "403 Forbidden"))
                    {
                        strcat(errors, "403 Forbidden\n");
                        error_in_response = 1;
                    }
                    else if (strstr(line.str, "404 Not Found"))
                    {
                        strcat(errors, "404 File Not Found\n");
                        error_in_response = 1;
                    }
                    else
                    {
                        strtok(line.str, " ");
                        char *status_code = strtok(NULL, " ");
                        strcat(errors, status_code);
                        strcat(errors, " Unknown error\n");
                        error_in_response = 1;
                    }
                }
                deinit_string(line);
            }

            printf("%s", errors);

            if (error_in_response)
            {
                deinit_string(remaining);
                deinit_string(command);
                close(sockfd);
                continue;
            }

            deinit_string(remaining);
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
