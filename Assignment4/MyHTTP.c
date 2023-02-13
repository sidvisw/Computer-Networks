// HTTP1.1 Server to handle GET and PUT requests
// GET: returns the file requested
// PUT: creates a file with the name and content specified in the request
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#define __USE_XOPEN
#define _GNU_SOURCE
#include <time.h>
#define LOG_FILE "AccessLog.txt"

#define BUFF_MAX 1024

void send_400(int newsockfd, char *version)
{
    char buffer[BUFF_MAX];
    memset(buffer, 0, BUFF_MAX);
    sprintf(buffer, "%s 400 Bad Request\r\n\r\n", version);
    send(newsockfd, buffer, strlen(buffer), 0);
    close(newsockfd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    memset(&serv_addr, 0, sizeof(serv_addr));
    memset(&cli_addr, 0, sizeof(cli_addr));
    char buffer[BUFF_MAX];

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }

    // Set the server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[1]));

    // Bind the socket to the server address
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }

    // Listen for connections
    listen(sockfd, 5);

    // Accept a connection
    while (1)
    {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("ERROR on accept");
            exit(1);
        }

        // fork a child process to handle the request
        if (fork() == 0)
        {
            close(sockfd);
            // create a string which will realloc as we receive data
            char *msg = (char *)malloc(BUFF_MAX);
            int msg_size;
            memset(msg, 0, BUFF_MAX);
            msg_size = 0;
            memset(buffer, 0, BUFF_MAX);
            int n = recv(newsockfd, buffer, BUFF_MAX, 0);
            if (n == 0)
            {
                printf("Client closed connection1\n");
                exit(EXIT_SUCCESS);
            }
            int method_header_end = 0;
            int newsize, oldsize;
            // keep receiving data until we get a blank line
            while (1)
            {
                oldsize = msg_size;
                newsize = oldsize + n;

                msg = (char *)realloc(msg, newsize);
                for(int i=0; i<n; i++)
                    msg[oldsize+i] = buffer[i];
                msg_size = newsize;
                
                int flag = 0;
                for(int i=0; i<n-3; i++)
                {
                    if(buffer[i]=='\r' && buffer[i+1]=='\n' && buffer[i+2]=='\r' && buffer[i+3]=='\n')
                    {   
                        method_header_end = i+4;
                        flag = 1;
                        break;
                    }
                }

                if(flag)
                    break;
                memset(buffer, 0, sizeof(buffer));
                n = recv(newsockfd, buffer, BUFF_MAX, 0);
                if (n == 0)
                    break;
            }
            if (n == 0)
            {
                printf("Client closed connection\n");
                exit(EXIT_SUCCESS);
            }
            // print method and header
            for(int i=0; i<method_header_end; i++)
                printf("%c", msg[i]);
            
            // Parse the request
            // get method, path and version
            // create a copy of the message
            char *msg_copy = (char *)malloc(msg_size);
            for(int i=0; i<msg_size; i++)
                msg_copy[i] = msg[i];
            int bad_request = 0;
            char *method = strtok(msg, " ");
            if(method == NULL)
                bad_request = 1;
            char *path = strtok(NULL, " ");
            if(path == NULL)
                bad_request = 1;
            path++;
            char *version = strtok(NULL, "\r\n");
            if(version == NULL)
                bad_request = 1;
            if (bad_request)
                send_400(newsockfd, version);
            if (strcmp(method, "GET") == 0)
            {   
                // print to access log following details: <Date(ddmmyy)>:<Time(hhmmss)>:<Client IP>:<ClientPort>:<GET/PUT>:<URL>
                FILE *fp = fopen(LOG_FILE, "a");
                if(fp == NULL)
                {
                    perror("Error opening log file");
                    exit(EXIT_FAILURE);
                }
                char *date = (char *)malloc(9);
                char *timet = (char *)malloc(7);
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);
                sprintf(date, "%02d/%02d/%02d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900);
                sprintf(timet, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
                fprintf(fp, "Date:%s, Time:%s, IP:%s, Port:%d, Request:%s, URL:%s\n", date, timet, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), method, path);
                fclose(fp);

                // open the file
                if (access(path, F_OK) == -1)
                {
                    // file not found
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "%s 404 Not Found\r\n\r\n", version);
                    send(newsockfd, buffer, strlen(buffer), 0);
                    close(newsockfd);
                    exit(EXIT_SUCCESS);
                }
                else if(access(path, R_OK) == -1)
                {
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "%s 403 Forbidden\r\n\r\n", version);
                    send(newsockfd, buffer, strlen(buffer), 0);
                    close(newsockfd);
                    exit(EXIT_SUCCESS);
                }
                {
                    /* get values of the following headers:
                    1.host,
                    2.connection,
                    3.accept,
                    4.accept-language,
                    5.If-Modified-Since
                    */
                    char *values[5];
                    // parse through headers to get values of the above headers
                    while (1)
                    {
                        char *header = strtok(NULL, "\r\n");
                        if (header == NULL)
                        {
                            break;
                        }
                        // get key and value without using strtok
                        char *key = header;
                        char *value = header;
                        while (*value != ':')
                        {
                            value++;
                        }
                        *value = '\0';
                        value++;

                        // remove leading spaces
                        while (value[0] == ' ' || value[0] == '\t')
                        {
                            value++;
                        }
                        if (strcmp(key, "Host") == 0)
                        {
                            values[0] = value;
                        }
                        else if (strcmp(key, "Connection") == 0)
                        {
                            values[1] = value;
                        }
                        else if (strcmp(key, "Accept") == 0)
                        {
                            values[2] = value;
                        }
                        else if (strcmp(key, "Accept-Language") == 0)
                        {
                            values[3] = value;
                        }
                        else if (strcmp(key, "If-Modified-Since") == 0)
                        {
                            values[4] = value;
                        }
                    }
                    // check if host header is present else send 400
                    if(values[0]==NULL)
                        send_400(newsockfd, version);
                    // send the file only if the file has been modified since the date specified in the request
                    if (values[4] != NULL)
                    {
                        struct stat file_stat;
                        if (stat(path, &file_stat) == 0)
                        {   
                            struct tm *tm2 = gmtime(&file_stat.st_mtime);
                            time_t time2 = mktime(tm2);

                            struct tm tm1;
                            strptime(values[4], "%a, %d %b %Y %H:%M:%S GMT", &tm1);
                            time_t time1 = mktime(&tm1);

                            if (difftime(time1, time2) >= 0)
                            {
                                // The file has not been modified since the specified time
                                // send a "Not Modified" response
                                memset(buffer, 0, BUFF_MAX);
                                sprintf(buffer, "%s 304 Not Modified\r\n\r\n", version);
                                send(newsockfd, buffer, strlen(buffer), 0);
                                close(newsockfd);
                                exit(EXIT_SUCCESS);
                            }
                        }
                        else
                        {
                            perror("stat");
                            exit(EXIT_FAILURE);
                        }
                    }
                    // The file has been modified since the specified time
                    // send the file contents
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "%s 200 OK\r\n", version);
                    send(newsockfd, buffer, strlen(buffer), 0);
                    // create headers for the response
                    // 1. Expires: always set to current time + 3 days
                    time_t now = time(NULL);
                    struct tm *now_tm = gmtime(&now);
                    now_tm->tm_mday += 3;
                    mktime(now_tm);
                    char expires[100];
                    strftime(expires, sizeof(expires), "%a, %d %b %Y %H:%M:%S GMT", now_tm);
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "Expires: %s\r\n", expires);
                    send(newsockfd, buffer, strlen(buffer), 0);
                    // 2. Cache-control: should set to no-store always
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "Cache-Control: no-store\r\n");
                    send(newsockfd, buffer, strlen(buffer), 0);
                    // 3. Content-language: set to en-us
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "Content-Language: en-us\r\n");
                    send(newsockfd, buffer, strlen(buffer), 0);
                    // 4. Content-length
                    struct stat st;
                    stat(path, &st);
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "Content-Length: %ld\r\n", st.st_size);
                    send(newsockfd, buffer, strlen(buffer), 0);
                    // 5. Content-type: set to text/html if the file asked in the url has extension .html
                    // application/pdf if the file asked in the url has extension .pdf
                    // image/jpeg if the file asked for has extension .jpg
                    // text/* for anything else
                    char content_type[100];
                    char *ext = strrchr(path, '.');
                    if (strcmp(ext, ".html") == 0)
                    {   
                        strcpy(content_type, "text/html");
                    }
                    else if (strcmp(ext, ".pdf") == 0)
                    {   
                        strcpy(content_type, "application/pdf");
                    }
                    else if (strcmp(ext, ".jpg") == 0)
                    {   
                        strcpy(content_type, "image/jpeg");
                    }
                    else
                    {
                        strcpy(content_type, "text/*");
                    }
                    
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "Content-Type: %s\r\n", content_type);
                    send(newsockfd, buffer, strlen(buffer), 0);
                    // 6. Last-Modified: set to the last modified time of the file
                    struct stat file_stat;
                    if (stat(path, &file_stat) == 0)
                    {
                        struct tm *tm = gmtime(&file_stat.st_mtime);
                        char last_modified[100];
                        strftime(last_modified, sizeof(last_modified), "%a, %d %b %Y %H:%M:%S GMT", tm);
                        memset(buffer, 0, BUFF_MAX);
                        sprintf(buffer, "Last-Modified: %s\r\n", last_modified);
                        send(newsockfd, buffer, strlen(buffer), 0);
                    }
                    else
                    {
                        perror("stat");
                        exit(EXIT_FAILURE);
                    }
                    // put an empty line to indicate the end of the headers and start sending the file
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "\r\n");
                    send(newsockfd, buffer, strlen(buffer), 0);
                    // send the file
                    FILE *fp = fopen(path, "r");
                    size_t nread;
                    memset(buffer, 0, BUFF_MAX);
                    while ((nread = fread(buffer, 1, BUFF_MAX, fp)) > 0)
                    {
                        send(newsockfd, buffer, nread, 0);
                        memset(buffer, 0, BUFF_MAX);
                    }
                    fclose(fp);
                    // close the connection
                    close(newsockfd);
                    exit(EXIT_SUCCESS);
                }
            }
            else if (strcmp(method, "PUT") == 0)
            {   
                // print to access log following details: <Date(ddmmyy)>:<Time(hhmmss)>:<Client IP>:<ClientPort>:<GET/PUT>:<URL>
                FILE *fp = fopen(LOG_FILE, "a");
                if(fp == NULL)
                {
                    perror("Error opening log file");
                    exit(EXIT_FAILURE);
                }
                char *date = (char *)malloc(9);
                char *timet = (char *)malloc(7);
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);
                sprintf(date, "%02d/%02d/%02d", tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900);
                sprintf(timet, "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
                fprintf(fp, "Date:%s, Time:%s, IP:%s, Port:%d, Request:%s, URL:%s\n", date, timet, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), method, path);
                fclose(fp);
                // PUT method

                // create the file
                int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
                if(fd < 0)
                {
                    // return forbidden
                    printf("File could not be created\n");
                    memset(buffer, 0, BUFF_MAX);
                    sprintf(buffer, "%s 403 Forbidden\r\n\r\n", version);
                    send(newsockfd, buffer, strlen(buffer), 0);
                    close(newsockfd);
                    exit(EXIT_SUCCESS);
                }
                // file created successfully
                // parse the headers
                // parse through headers to get values of the above headers
                // 1. Host, 2. connection, 3. date, 4. content-length, 5. content-type, 6. content-language
                // printf("Msg copy: %s\n", msg_copy);
                char *values[6];
                int body_start = 0;
                for(int i=0; i<msg_size; i++)
                {   
                    if(msg_copy[i] == '\r' && msg_copy[i+1] == '\n')
                    {   
                        body_start = i+2;
                        break;
                    }
                }
                int header_start = body_start;
                while (1)
                {   
                    int flag = 0;
                    if(msg_copy[body_start] == '\r' && msg_copy[body_start+1] == '\n' && msg_copy[body_start+2] == '\r' && msg_copy[body_start+3] == '\n')
                        flag = 1;
                    if(msg_copy[body_start] == '\r' && msg_copy[body_start+1] == '\n')
                    {   
                        msg_copy[body_start] = '\0';
                        
                        // get key and value without using strtok
                        char *key = msg_copy + header_start;
                        char *value = msg_copy + header_start;
                        while (*value != ':')
                        {
                            value++;
                        }
                        *value = '\0';
                        value++;
                        // remove leading spaces
                        while (value[0] == ' ' || value[0] == '\t')
                        {
                            value++;
                        }
                        if (strcmp(key, "Host") == 0)
                        {
                            values[0] = value;
                        }
                        else if (strcmp(key, "Connection") == 0)
                        {
                            values[1] = value;
                        }
                        else if (strcmp(key, "Date") == 0)
                        {
                            values[2] = value;
                        }
                        else if (strcmp(key, "Content-Length") == 0)
                        {
                            values[3] = value;
                        }
                        else if (strcmp(key, "Content-Type") == 0)
                        {
                            values[4] = value;
                        }
                        else if (strcmp(key, "Content-Language") == 0)
                        {
                            values[5] = value;
                        }
                        if(flag)
                        {
                            break;
                        }
                        header_start = body_start + 2;
                        body_start+=2;
                        continue;
                    }
                    body_start++;
                }
                body_start += 4;
                if(values[0]==NULL || values[1]==NULL || values[2]==NULL)
                    send_400(newsockfd, version);

                msg_copy+=body_start;
                msg_size-=body_start;
                if(msg_size)
                    write(fd, msg_copy, msg_size);
                int content_length = atoi(values[3]);
                int written = msg_size;
                while (written != content_length)
                {   
                    memset(buffer, 0, BUFF_MAX);
                    int nread = recv(newsockfd, buffer, BUFF_MAX, 0);
                    write(fd, buffer, nread);
                    written += nread;
                }
                close(fd);
                // send 200 OK
                memset(buffer, 0, BUFF_MAX);
                sprintf(buffer, "%s 200 OK\r\n\r\n", version);
                send(newsockfd, buffer, strlen(buffer), 0);
                close(newsockfd);
                exit(EXIT_SUCCESS);
            }
            else
            {
                // method not supported
                char *response = "HTTP/1.1 501 Not Implemented\r\n\r\n";
                send(newsockfd, response, strlen(response)+1, 0);
                exit(EXIT_SUCCESS);
            }
        }
        else
        {
            close(newsockfd);
        }
    }
}