#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>

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

/* Ultility function to get the minimum of the two numbers
   @params: int - the first number
            int - the second number
   @returns: int - the minimum of the two numbers
*/
int min(int a, int b)
{
   return a < b ? a : b;
}

/* THE SERVER PROCESS */

int main()
{
   int sockfd, newsockfd; // Socket descriptors
   int clilen;
   struct sockaddr_in cli_addr, serv_addr;

   char buf[50]; // Buffer for communicating through client

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

   // Looping structure for the concurrent server
   // forks() function generates a new child process and the parent process comes back and waits at the accept call
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
         printf("Accept error\n");
         exit(0);
      }

      // The child process handles the client request and the parent goes back to accept the next client request
      if (fork() == 0)
      {
         // Close the old socket in the child since all communications will be through the new socket.
         close(sockfd);

         // Send the client the LOGIN prompt for asking him/her to login
         strcpy(buf, "LOGIN:");
         send(newsockfd, buf, strlen(buf) + 1, 0);

         // Recieve the username from the client side and store it in the buffer
         int recv_len = recv(newsockfd, buf, 50, 0);
         while (buf[recv_len - 1])
            recv_len += recv(newsockfd, buf + recv_len, 50 - recv_len, 0);

         // Opening the file to read the list of usernames to match with
         FILE *fp = fopen("users.txt", "r");

         // Linear search for the user in the file user.txt
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

         // Close the file after the searching is complete
         fclose(fp);

         // If the username is not in the list, send the prompt message 'NOT-FOUND' to the client
         if (!found_flag)
         {
            strcpy(buf, "NOT-FOUND");
            send(newsockfd, buf, strlen(buf) + 1, 0);
            close(newsockfd);
            exit(0);
         }

         // Otherwise, send the prompt 'FOUND' to the client
         strcpy(buf, "FOUND");
         send(newsockfd, buf, strlen(buf) + 1, 0);

         // Looping construct to take the shell commands until the client sends exit
         do
         {
            // Recieve the shell command from the client in chunks and keep on appending it to a string structure 'command'
            recv_len = recv(newsockfd, buf, 50, 0);
            struct string command = init_string();
            while (buf[recv_len - 1])
            {
               concat_string(&command, buf, recv_len);
               recv_len = recv(newsockfd, buf, 50, 0);
            }
            concat_string(&command, buf, recv_len);

            // If the client sends the exit command then break out of the loop to close the connection
            if (!strcmp(buf, "exit"))
               break;

            // Extract the head of the command for e.g. 'pwd', 'cd', 'dir'
            char *head_cmd = strtok(command.str, " ");

            // Actions to be done if the command is of the form 'pwd'
            if (!strcmp(head_cmd, "pwd"))
            {
               // Accquire the current working directory by getcwd() function
               // Here we use the string structure for dynamically allocating the string for current working directory
               struct string cwd = init_string();
               while (getcwd(cwd.str, cwd.capacity) != cwd.str)
               {
                  cwd.capacity *= 2;
                  cwd.str = (char *)realloc(cwd.str, cwd.capacity);
               }

               // Send the current working directory to the client in packets of 50 or the size of the string + 1 (whichever is smaller)
               int send_len = send(newsockfd, cwd.str, min(50, cwd.size + 1), 0);
               while (cwd.str[send_len - 1])
                  send_len += send(newsockfd, cwd.str + send_len, min(50, strlen(cwd.str + send_len) + 1), 0);

               // De-initialize the string struct 'cwd'
               deinit_string(cwd);
            }
            // Actions to be taken if the command is of the form 'dir'
            else if (!strcmp(head_cmd, "dir"))
            {
               // The initial directory in which we are supposed to execute the dir command is the current diectory '.'
               struct string wdir = init_string();
               concat_string(&wdir, ".", 1);

               // If an additional directory argument is given to the dir command then change the 'wdir' to that
               char *change_dir = strtok(NULL, " ");
               if (change_dir)
               {
                  deinit_string(wdir);
                  wdir = init_string();
                  concat_string(&wdir, change_dir, strlen(change_dir));
               }

               // Directory handling variables
               DIR *dir;
               struct dirent *entry;

               // String structure for the list of files and directories to send to the client
               struct string dir_list = init_string();

               // Open the directory
               if (dir = opendir(wdir.str))
               {
                  // Loop and concatanate the dir_list will all the files and directories inside the directory given as input
                  while (entry = readdir(dir))
                  {
                     concat_string(&dir_list, entry->d_name, strlen(entry->d_name));
                     concat_string(&dir_list, " ", 1);
                  }

                  // Close the directory
                  closedir(dir);
               }
               else
                  // Send '####' if an error occurs
                  concat_string(&dir_list, "####", 4);

               // Send the directory list to the client in packets of 50 or the size of the string + 1 (whichever is smaller)
               int send_len = send(newsockfd, dir_list.str, min(50, dir_list.size + 1), 0);
               while (dir_list.str[send_len - 1])
                  send_len += send(newsockfd, dir_list.str + send_len, min(50, strlen(dir_list.str + send_len) + 1), 0);

               // De-initialize the string structures used
               deinit_string(dir_list);
               deinit_string(wdir);
            }
            // Actions to be taken if the command is of the form 'cd'
            else if (!strcmp(head_cmd, "cd"))
            {
               // If no argument is given to the cd command then it is supposed to navigate to the '/home' folder
               struct string wdir = init_string();
               concat_string(&wdir, "/home", 5);

               // If an additional directory argument is given to the dir command then change the 'wdir' to that
               char *change_dir = strtok(NULL, " ");
               if (change_dir)
               {
                  deinit_string(wdir);
                  wdir = init_string();
                  concat_string(&wdir, change_dir, strlen(change_dir));
               }

               // Change the directory to the required argument given
               if (chdir(wdir.str) == 0)
               {
                  // Send an empty string to the client for confirmation
                  strcpy(buf, "");
                  send(newsockfd, buf, strlen(buf) + 1, 0);
               }
               else
               {
                  // Send '####' if an error occurs
                  strcpy(buf, "####");
                  send(newsockfd, buf, strlen(buf) + 1, 0);
               }

               // De-initialize the string structure
               deinit_string(wdir);
            }
            else
            {
               // Send '$$$$' in case of invalid command
               strcpy(buf, "$$$$");
               send(newsockfd, buf, strlen(buf) + 1, 0);
            }

            // De-initialize the 'command' string struct
            deinit_string(command);
         } while (strcmp(buf, "exit"));

         // Close the new socket and exit from the child process
         close(newsockfd);
         exit(0);
      }

      close(newsockfd);
   }
   return 0;
}
