#include "csapp.h"

void echo(int connfc);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;    // 소켓의 바이트 길이를 저장하는 변수.
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);  // <- 커맨드 칠 때 이렇게 치세요.. 라고 알려주는 것.
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen,   // clientaddr를 통해서 얻은,
                    client_hostname, MAXLINE,       // 1. host name이 client_hostname에 저장됨
                    client_port, MAXLINE, 0);       // 2. port name(service)이 client_port에 저장됨
        printf("Connected to (%s %s)\n", client_hostname, client_port);
        echo(connfd);
        Close(connfd);
    }
    exit(0);
}

void echo(int connfd) {
    size_t n;
    char buf[MAXLINE];
    rio_t rio;  // 커서 같은 놈.

    Rio_readinitb(&rio, connfd);
    while(1) {
        fflush(stdout);
        n = Rio_readlineb(&rio, buf, MAXLINE);
        if(n == 0) break;
        printf("server received %d bytes: %s", (int)n, buf);
        /* 받은 텍스트 줄(buf안에 저장됨)을 그대로 'echo'해준다. */
        Rio_writen(connfd, buf, n);
        
    }
    // while (() != 0) {
    //     printf("server received %d bytes\n", (int)n);
    //     /* 받은 텍스트 줄(buf안에 저장됨)을 그대로 'echo'해준다. */
    //     Rio_writen(connfd, buf, n);
    // }
}