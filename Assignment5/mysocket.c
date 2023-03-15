#include "mysocket.h"

MyTable Send_Message, Received_Message;
pthread_t tid_s, tid_r;

int __sockfd, __oldsockfd;

pthread_mutex_t mutex_s, mutex_r;
pthread_cond_t cond_s, cond_r;

int min(int a, int b)
{
    return a < b ? a : b;
}

int my_socket(int domain, int type, int protocol)
{
    if (type != SOCK_MyTCP)
    {
        errno = EPROTONOSUPPORT;
        return -1;
    }
    __sockfd = socket(domain, type, protocol);
    __oldsockfd = __sockfd;
    if (__sockfd < 0)
        return __sockfd;

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
    Received_Message.closed = 0;

    pthread_mutex_init(&mutex_s, NULL);
    pthread_mutex_init(&mutex_r, NULL);
    pthread_cond_init(&cond_s, NULL);
    pthread_cond_init(&cond_r, NULL);

    pthread_create(&tid_s, NULL, S, NULL);
    pthread_create(&tid_r, NULL, R, NULL);

    return __sockfd;
}

int my_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}

int my_listen(int sockfd, int backlog)
{
    return listen(sockfd, backlog);
}

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

ssize_t my_send(int sockfd, const void *buf, size_t len, int flags)
{
    pthread_mutex_lock(&mutex_s);
    while (Send_Message.count == TABLE_MAX)
        pthread_cond_wait(&cond_s, &mutex_s);

    memcpy(Send_Message.table[Send_Message.in], buf, len);
    Send_Message.sizes[Send_Message.in] = len;
    Send_Message.in = (Send_Message.in + 1) % TABLE_MAX;
    Send_Message.count++;
    pthread_mutex_unlock(&mutex_s);
    pthread_cond_signal(&cond_s);
    return len;
}

ssize_t my_recv(int sockfd, void *buf, size_t len, int flags)
{
    int read;
    pthread_mutex_lock(&mutex_r);
    while (Received_Message.count == 0 && !Received_Message.closed)
        pthread_cond_wait(&cond_r, &mutex_r);
    if (Received_Message.closed)
    {
        pthread_mutex_unlock(&mutex_r);
        return 0;
    }
    int i;
    for (i = 0; i < Received_Message.sizes[Received_Message.out]; i++)
    {
        if (i > len)
            break;
        ((char *)buf)[i] = Received_Message.table[Received_Message.out][i];
    }
    read = i;
    Received_Message.out = (Received_Message.out + 1) % TABLE_MAX;
    Received_Message.count--;
    pthread_mutex_unlock(&mutex_r);
    pthread_cond_signal(&cond_r);
    return read;
}

int my_close(int sockfd)
{
    sleep(5);
    if (sockfd != __oldsockfd)
        return close(sockfd);

    pthread_kill(tid_s, SIGKILL);
    pthread_kill(tid_r, SIGKILL);

    pthread_mutex_destroy(&mutex_s);
    pthread_mutex_destroy(&mutex_r);
    pthread_cond_destroy(&cond_s);
    pthread_cond_destroy(&cond_r);

    for (int i = 0; i < TABLE_MAX; i++)
    {
        free(Send_Message.table[i]);
        free(Received_Message.table[i]);
    }
    free(Send_Message.table);
    free(Received_Message.table);
    return close(sockfd);
}

void *R(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&mutex_r);
        while (!Received_Message.connected)
            pthread_cond_wait(&cond_r, &mutex_r);
        pthread_mutex_unlock(&mutex_r);
        int size;
        int recived = 0;
        while (recived < 4)
        {
            int r = recv(__sockfd, &size + recived, 4 - recived, 0);
            if (r == 0)
            {
                Received_Message.closed = 1;
                pthread_cond_signal(&cond_r);
            }
            recived += r;
        }

        char *buff = (char *)malloc(sizeof(char) * size);
        recived = 0;
        while (recived < size)
        {
            int r = recv(__sockfd, buff + recived, size - recived, 0);
            recived += r;
        }

        pthread_mutex_lock(&mutex_r);
        while (Received_Message.count == TABLE_MAX)
            pthread_cond_wait(&cond_r, &mutex_r);
        memcpy(Received_Message.table[Received_Message.in], buff, size);
        Received_Message.sizes[Received_Message.in] = size;
        Received_Message.in = (Received_Message.in + 1) % TABLE_MAX;
        Received_Message.count++;
        pthread_mutex_unlock(&mutex_r);
        pthread_cond_signal(&cond_r);
        free(buff);
    }
}

void *S(void *arg)
{
    int size;
    char buff[5000];
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
}