#include<stdio.h>

int main()
{
    char str[]="abc\r\nhello\r\nbye\r\n\r\n";
    char * substring=strstr(str,"hello");
    printf("%s",substring+1);
}