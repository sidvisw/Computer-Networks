#include "mysocket.h"

// Global variables initialization

MyTable Send_Message, Received_Message;
pthread_t tid_s, tid_r;

int __sockfd, __oldsockfd;

pthread_mutex_t mutex_s, mutex_r;
pthread_cond_t cond_s, cond_r;

// this function is wrapper around the socket call with following extra functionality:
// 1. It creates two threads, one for sending and one for receiving
// 2. It initializes two tables Send_Message and Received_Message
// 3. It use two global variables __sockfd and __oldsockfd to store the socket descriptor
int my_socket(int domain, int type, int protocol)
{
    if (type != SOCK_MyTCP)
    {
        // if any type other than SOCK_MyTCP is passed, return error with errno set to EPROTONOSUPPORT
        errno = EPROTONOSUPPORT;
        return -1;
    }

    __sockfd = socket(domain, type, protocol);
    __oldsockfd = __sockfd;
    if (__sockfd < 0)
        return __sockfd;

    // dynamically allocate memory for the two tables
    Send_Message.sizes = (int *)malloc(sizeof(int) * TABLE_MAX);
    Send_Message.table = (char **)malloc(sizeof(char *) * TABLE_MAX);
    for (int i = 0; i < TABLE_MAX; i++)
        Send_Message.table[i] = (char *)malloc(sizeof(char) * MAX_MSG);
    Send_Message.in = Send_Message.out = Send_Message.count = 0;
    Send_Message.connected = 0;

    Received_Message.sizes = (int *)malloc(sizeof(int) * TABLE_MAX);
    Received_Message.table = (char **)malloc(sizeof(char *) * TABLE_MAX);
    for (int i = 0; i < TABLE_MAX; i++)
        Received_Message.table[i] = (char *)malloc(sizeof(char) * MAX_MSG);
    Received_Message.in = Received_Message.out = Received_Message.count = 0;
    Received_Message.connected = 0;

    // initialize the mutex and condition variables
    pthread_mutex_init(&mutex_s, NULL);
    pthread_mutex_init(&mutex_r, NULL);
    pthread_cond_init(&cond_s, NULL);
    pthread_cond_init(&cond_r, NULL);

    // create two threads
    pthread_create(&tid_s, NULL, S, NULL);
    pthread_create(&tid_r, NULL, R, NULL);

    return __sockfd;
}

// this function is wrapper around the bind call
int my_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}

// this function is wrapper around the listen call
int my_listen(int sockfd, int backlog)
{
    return listen(sockfd, backlog);
}

// this function is wrapper around the accept call with following extra functionality:
// 1. It sets the global variable __sockfd to the socket descriptor returned by accept
// 2. It sets the global variable Received_Message.connected to 1
int my_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    __sockfd = accept(sockfd, addr, addrlen);
    if (__sockfd > 0)
    {
        pthread_mutex_lock(&mutex_r);
        Received_Message.connected = 1;
        pthread_mutex_unlock(&mutex_r);
        pthread_cond_signal(&cond_r);
    }
    return __sockfd;
}

// this function is wrapper around the connect call with following extra functionality:
// 1. It sets the global variable Received_Message.connected to 1
int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret = connect(sockfd, addr, addrlen);
    if (ret == 0)
    {
        pthread_mutex_lock(&mutex_r);
        Received_Message.connected = 1;
        pthread_mutex_unlock(&mutex_r);
        pthread_cond_signal(&cond_r);
    }
    return ret;
}

// this function is wrapper around the send call with following extra functionality:
// 1. It copies the data to be sent to the Send_Message table
// 2. It sets the size of the data to be sent in the Send_Message table
// 3. It increments the in pointer of the Send_Message table
// 4. It increments the count of the Send_Message table
// 5. It signals the send thread
ssize_t my_send(int sockfd, const void *buf, size_t len, int flags)
{
    // lock the mutex
    pthread_mutex_lock(&mutex_s);
    // if the table is full, wait
    while (Send_Message.count == TABLE_MAX)
        pthread_cond_wait(&cond_s, &mutex_s);
    // copy the data to be sent to the table
    memcpy(Send_Message.table[Send_Message.in], buf, len);
    Send_Message.sizes[Send_Message.in] = len;
    Send_Message.in = (Send_Message.in + 1) % TABLE_MAX;
    Send_Message.count++;
    // unlock the mutex
    pthread_mutex_unlock(&mutex_s);
    // signal the send thread
    pthread_cond_signal(&cond_s);
    return len;
}

ssize_t my_recv(int sockfd, void *buf, size_t len, int flags)
{
    int read;
    // lock the mutex
    pthread_mutex_lock(&mutex_r);
    // if the table is empty and connection is not closed, wait
    while (Received_Message.count == 0 && Received_Message.connected)
        pthread_cond_wait(&cond_r, &mutex_r);
    // if the connection is closed and the table is empty, return -1
    if (!Received_Message.connected && Received_Message.count == 0)
    {
        pthread_mutex_unlock(&mutex_r);
        return -1;
    }
    // copy the data from the table to the buffer
    int i;
    for (i = 0; i < Received_Message.sizes[Received_Message.out]; i++)
    {
        if (i >= len)
            break;
        ((char *)buf)[i] = Received_Message.table[Received_Message.out][i];
    }
    read = i;
    Received_Message.out = (Received_Message.out + 1) % TABLE_MAX;
    Received_Message.count--;
    // unlock the mutex
    pthread_mutex_unlock(&mutex_r);
    // signal the receive thread
    pthread_cond_signal(&cond_r);
    return read;
}

// this function is wrapper around the close call with following extra functionality:
// 1. It kills the send and receive threads
// 2. It frees the memory allocated to the Send_Message and Received_Message tables
int my_close(int sockfd)
{
    // sleep for 5 seconds to allow the send and receive threads to finish
    sleep(5);
    // if the socket descriptor is not the one returned by my_socket, call the close function
    if (sockfd != __oldsockfd)
        return close(sockfd);

    // kill the send and receive threads
    pthread_cancel(tid_s);
    pthread_cancel(tid_r);

    pthread_mutex_destroy(&mutex_s);
    pthread_mutex_destroy(&mutex_r);
    pthread_cond_destroy(&cond_s);
    pthread_cond_destroy(&cond_r);

    // free the memory allocated to the Send_Message and Received_Message tables
    for (int i = 0; i < TABLE_MAX; i++)
    {
        free(Send_Message.table[i]);
        free(Received_Message.table[i]);
    }
    free(Send_Message.table);
    free(Received_Message.table);
    // call the close function
    return close(sockfd);
}

void *R(void *arg)
{
    while (1)
    {
        // lock the mutex
        pthread_mutex_lock(&mutex_r);
        // if connection call is not made, wait
        while (!Received_Message.connected)
            pthread_cond_wait(&cond_r, &mutex_r);
        pthread_mutex_unlock(&mutex_r);

        // start receiving data
        int size;
        int recived = 0;
        // obtain 4 byte header which contains the size of the data stored in integer format
        while (recived < 4)
        {
            int r = recv(__sockfd, &size + recived, 4 - recived, 0);
            // if the connection is closed, change the global variable and signal the main thread
            if (r <= 0)
            {
                pthread_mutex_lock(&mutex_r);
                Received_Message.connected = 0;
                pthread_mutex_unlock(&mutex_r);
                pthread_cond_signal(&cond_r);
                break;
            }
            recived += r;
        }

        // if the connection is closed, continue
        if (!Received_Message.connected)
            continue;

        // start receiving the data
        // allocate memory for the data
        char *buff = (char *)malloc(sizeof(char) * size);
        recived = 0;
        while (recived < size)
        {
            int r = recv(__sockfd, buff + recived, size - recived, 0);
            // if the connection is closed, change the global variable and signal the main thread
            if (r <= 0)
            {
                pthread_mutex_lock(&mutex_r);
                Received_Message.connected = 0;
                pthread_mutex_unlock(&mutex_r);
                pthread_cond_signal(&cond_r);
                break;
            }
            recived += r;
        }

        // if the connection is closed, free the memory and continue
        if (!Received_Message.connected)
        {
            free(buff);
            buff = (char *)NULL;
            continue;
        }

        // obtain the lock and copy the data to the table
        pthread_mutex_lock(&mutex_r);
        // if the table is full, wait
        while (Received_Message.count == TABLE_MAX)
            pthread_cond_wait(&cond_r, &mutex_r);

        // copy the data to the table
        memcpy(Received_Message.table[Received_Message.in], buff, size);
        Received_Message.sizes[Received_Message.in] = size;
        Received_Message.in = (Received_Message.in + 1) % TABLE_MAX;
        Received_Message.count++;
        // unlock the mutex
        pthread_mutex_unlock(&mutex_r);
        // signal the main thread
        pthread_cond_signal(&cond_r);

        free(buff);

        buff = (char *)NULL;
    }
}

void *S(void *arg)
{
    int size;
    char *buff = (char *)malloc(sizeof(char) * MAX_MSG);
    sleep(T);
    while (1)
    {
        pthread_mutex_lock(&mutex_s);
        if (Send_Message.count == 0)
        {
            pthread_mutex_unlock(&mutex_s);
            sleep(T);
            continue;
        }
        size = Send_Message.sizes[Send_Message.out];
        memcpy(buff, Send_Message.table[Send_Message.out], size);
        Send_Message.out = (Send_Message.out + 1) % TABLE_MAX;
        Send_Message.count--;
        pthread_mutex_unlock(&mutex_s);
        pthread_cond_signal(&cond_s);
        int sent = 0;
        while (sent < 4)
            sent += send(__sockfd, &size + sent, 4 - sent, 0);
        sent = 0;
        while (sent < size)
            sent += send(__sockfd, buff + sent, min(size - sent, SEND_MAX), 0);
    }
    free(buff);
}

int min(int a, int b)
{
    return a < b ? a : b;
}