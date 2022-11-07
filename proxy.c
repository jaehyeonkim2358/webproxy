#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char* user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* Helper functions prototype */
void doit(int connfd);
void do_client(int p_clientfd, int connfd, char *new_request);
void parse_requesthdrs(rio_t* rp);
void read_requesthdrs(rio_t* rp);
void parse_uri(char* uri, char* host, char* port, char* new_request);
void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg);

int main(int argc, char** argv) {
    int listenfd, connfd;  // listening descriptor, connected descriptor
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);  // line:netp:tiny:accept
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        // printf("Accepted connection from (%s, %s)\n", hostname, port);
        // 1. 브라우저에게 Request를 받아요
        doit(connfd);  // 2. doit()해요
        Close(connfd);
    }
    // printf("%s", user_agent_hdr);
    return 0;
}

void doit(int connfd) {
    // doit()이는 뭘 해요 ?
    // 1. 파싱해요
    // 2. 하면서 검사해요
    // 3. 문제 없으면 p_clientfd = Open_clientfd() 해요
    // 4. do_client(p_clientfd) 해요
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char ptos_host[MAXLINE], ptos_port[MAXLINE], new_request[MAXLINE];
    rio_t rio;
    int p_clientfd;

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    // parse_requesthdrs(&rio);

    parse_uri(uri, ptos_host, ptos_port, new_request);
    // 일단 request line 하드코딩
    
    p_clientfd = Open_clientfd(ptos_host, ptos_port);
    do_client(p_clientfd, connfd, new_request);
}

void do_client(int p_clientfd, int connfd, char *new_request) {
    // do_client()이는 뭘 해요?
    // 1. 나랑 Server랑 연결하는 socket(p_clientfd)을 열어요
    // 2. Server에게 Request를 보내요
    // 3. Response를 받아요 (Rio_readn())
    // 4. 소켓 p_clientfd를 닫아요
    // 5. 받은 Response를 Client에게 줘요 (Rio_writen())
}

void parse_uri(char* uri, char* host, char* port, char* new_uri) {
    const char* http = "://";
    char* p;

    // printf("%s\n", uri);
    if ((p = strstr(uri, http)) != NULL) {
        uri = p + (strlen(http));
    }
    // printf("%s\n", uri);
    if ((p = index(uri, '/')) != NULL) {
        // printf("%s\n", p);
        strcpy(new_uri, p);
        *p = '\0';
    }
    if ((p = index(uri, ':')) != NULL) {
        strcpy(port, p + 1);
        *p = '\0';
    }
    strcpy(host, uri);
    // printf("%s\n", uri);
}

void parse_requesthdrs(rio_t* rp) {
}
