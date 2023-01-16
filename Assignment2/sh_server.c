#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>

struct string{
   char *str;
   size_t size;
   size_t capacity;
};
struct string init_string(){
   struct string s;
   s.str = (char*)calloc(1, sizeof(char));
   s.size = 0;
   s.capacity = 1;
   return s;
}
void deinit_string(struct string s){
   free(s.str);
}
void concat_string(struct string *s1, char*str, size_t len){
   if(s1->size + len >= s1->capacity){
      s1->capacity = s1->size + len + 1;
      s1->str = (char*)realloc(s1->str, s1->capacity);
   }
   memcpy(s1->str + s1->size, str, len);
   s1->size += len;
   s1->str[s1->size] = '\0';
}

int min(int a, int b){
   return a < b ? a : b;
}

/* THE SERVER PROCESS */

int main()
{
   int sockfd, newsockfd; /* Socket descriptors */
   int clilen;
   struct sockaddr_in cli_addr, serv_addr;

   char buf[50]; /* We will use this buffer for communication */

   /* The following system call opens a socket. The first parameter
      indicates the family of the protocol to be followed. For internet
      protocols we use AF_INET. For TCP sockets the second parameter
      is SOCK_STREAM. The third parameter is set to 0 for user
      applications.
   */
   if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Cannot create socket\n");
      exit(0);
   }

   /* The structure "sockaddr_in" is defined in <netinet/in.h> for the
      internet family of protocols. This has three main fields. The
      field "sin_family" specifies the family and is therefore AF_INET
      for the internet family. The field "sin_addr" specifies the
      internet address of the server. This field is set to INADDR_ANY
      for machines having a single IP address. The field "sin_port"
      specifies the port number of the server.
   */
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(20000);

   /* With the information provided in serv_addr, we associate the server
      with its port using the bind() system call.
   */
   if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
   {
      perror("Unable to bind local address\n");
      exit(0);
   }

   listen(sockfd, 5); /* This specifies that up to 5 concurrent client
                 requests will be queued up while the system is
                 executing the "accept" system call below.
              */

   /* In this program we are illustrating a concurrent server -- one
      which forks to accept multiple client connections concurrently.
      As soon as the server accepts a connection from a client, it
      forks a child which communicates with the client, while the
      parent becomes free to accept a new connection. To facilitate
      this, the accept() system call returns a new socket descriptor
      which can be used by the child. The parent continues with the
      original socket descriptor.
   */
   while (1)
   {

      /* The accept() system call accepts a client connection.
         It blocks the server until a client request comes.

         The accept() system call fills up the client's details
         in a struct sockaddr which is passed as a parameter.
         The length of the structure is noted in clilen. Note
         that the new socket descriptor returned by the accept()
         system call is stored in "newsockfd".
      */
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

      if (newsockfd < 0)
      {
         printf("Accept error\n");
         exit(0);
      }

      /* Having successfully accepted a client connection, the
         server now forks. The parent closes the new socket
         descriptor and loops back to accept the next connection.
      */
      if (fork() == 0)
      {

         /* This child process will now communicate with the
            client through the send() and recv() system calls.
         */
         close(sockfd); 
         /* Close the old socket since all
                   communications will be through
                   the new socket.
                */

         /* We initialize the buffer, copy the message to it,
            and send the message to the client.
         */

         strcpy(buf, "LOGIN:");
         send(newsockfd, buf, strlen(buf) + 1, 0);

         int recv_len = recv(newsockfd, buf, 50, 0);
         while (buf[recv_len - 1])
         {
            recv_len += recv(newsockfd, buf + recv_len, 50 - recv_len, 0);
         }

         FILE*fp=fopen("users.txt", "r");
         char user[50];
         int found_flag = 0;
         while (fscanf(fp, "%s%*c", user) != EOF)
         {
            if (strcmp(user, buf) == 0)
            {
               found_flag = 1;
               break;
            }
         }
         fclose(fp);

         if(!found_flag){
            strcpy(buf, "NOT-FOUND");
            send(newsockfd, buf, strlen(buf) + 1, 0);
            close(newsockfd);
            exit(0);
         }
         
         strcpy(buf, "FOUND");
         send(newsockfd, buf, strlen(buf) + 1, 0);
         do{
         recv_len = recv(newsockfd, buf, 50, 0);
         struct string command=init_string();
         while (buf[recv_len - 1])
         {
            concat_string(&command, buf, recv_len);
            recv_len = recv(newsockfd, buf, 50, 0);
         }
         concat_string(&command, buf, recv_len);

         printf("Command recieved : %s\n", command.str);
         if(!strcmp(buf, "exit"))break;
         char *head_cmd=strtok(command.str, " ");
         if(!strcmp(head_cmd, "pwd")){
            struct string cwd=init_string();
            while(getcwd(cwd.str, cwd.capacity)!=cwd.str){
               cwd.capacity*=2;
               cwd.str=(char*)realloc(cwd.str, cwd.capacity);
            }
            int send_len=send(newsockfd, cwd.str, min(50, cwd.size+1), 0);
            while(cwd.str[send_len-1]){
               send_len+=send(newsockfd, cwd.str+send_len, min(50, strlen(cwd.str+send_len)+1), 0);
            }
            deinit_string(cwd);
         }
         else if(!strcmp(head_cmd, "dir")){
            struct string wdir=init_string();
            concat_string(&wdir, ".", 1);
            char *change_dir=strtok(NULL, " ");
            if(change_dir){
               deinit_string(wdir);
               wdir=init_string();
               concat_string(&wdir, change_dir, strlen(change_dir));
            }
            DIR *dir;
            struct dirent *entry;
            struct string dir_list=init_string();
            if (dir = opendir(wdir.str)) {
               while (entry = readdir(dir)) {
                  concat_string(&dir_list, entry->d_name, strlen(entry->d_name));
                  concat_string(&dir_list, " " , 1);
               }
               closedir(dir);
            }
            else{
               concat_string(&dir_list, "####", 4);
            }
            int send_len=send(newsockfd, dir_list.str, min(50, dir_list.size+1), 0);
            while(dir_list.str[send_len-1]){
               send_len+=send(newsockfd, dir_list.str+send_len, min(50, strlen(dir_list.str+send_len)+1), 0);
            }
            deinit_string(dir_list);
            deinit_string(wdir);
         }
         else if(!strcmp(head_cmd, "cd")){
            struct string wdir=init_string();
            concat_string(&wdir, "/", 1);
            char *change_dir=strtok(NULL, " ");
            if(change_dir){
               deinit_string(wdir);
               wdir=init_string();
               concat_string(&wdir, change_dir, strlen(change_dir));
            }
            if(chdir(wdir.str)==0){
               strcpy(buf, "");
               send(newsockfd, buf, strlen(buf)+1, 0);
            }
            else{
               strcpy(buf, "####");
               send(newsockfd, buf, strlen(buf) + 1, 0);
            }
            deinit_string(wdir);
         }
         else if(!strcmp(head_cmd, "exit")){

         }
         else{
            strcpy(buf, "$$$$");
            send(newsockfd, buf, strlen(buf) + 1, 0);
         }

         deinit_string(command);
         }while(strcmp(buf, "exit"));

         close(newsockfd);
         exit(0);
      }

      close(newsockfd);
   }
   return 0;
}