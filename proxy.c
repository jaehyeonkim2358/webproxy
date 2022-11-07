#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

/* Helper functions prototype */
void doit(int connfd);
void do_client(char *ptos_host, char *ptos_port, int connfd, char *new_request);
void create_request(rio_t *rp, char *method, char *new_uri, const char *version, char *new_request, char *host_hdr);
// void parse_requesthdrs(rio_t* rp);
void read_requesthdrs(rio_t *rp);
void parse_uri(char *uri, char *host, char *port, char *new_request);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
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
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        // 1. 브라우저에게 Request를 받아요
        doit(connfd);  // 2. doit()해요
        Close(connfd);
    }
    // printf("%s", user_agent_hdr);
}

void doit(int connfd) {
    // doit()이는 뭘 해요 ?
    // 1. 파싱해요
    // 2. 하면서 검사해요
    // 3. 문제 없으면 p_clientfd = Open_clientfd() 해요
    // 4. do_client(p_clientfd) 해요
    struct stat sbuf;
    char host_hdr[MAXLINE];
    const char *ptos_request_version = "HTTP/1.0";
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char ptos_host[MAXLINE], ptos_port[MAXLINE], new_uri[MAXLINE], new_request[MAXLINE];
    rio_t rio;
    int p_clientfd;

    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    parse_uri(uri, ptos_host, ptos_port, new_uri);
    sprintf(host_hdr, "Host: %s:%s\r\n", ptos_host, ptos_port);

    // Proxy -> Server로 가는 new_request 만들어요
    create_request(&rio, method, new_uri, ptos_request_version, new_request, host_hdr);
    // p_clientfd = Open_clientfd();
    do_client(ptos_host, ptos_port, connfd, new_request);
}

void do_client(char *ptos_host, char *ptos_port, int connfd, char *new_request) {
    // do_client()이는 뭘 해요?
    // 1. 나랑 Server랑 연결하는 socket(p_clientfd)을 열어요
    // 2. Server에게 Request를 보내요
    // 3. Response를 받아요 (Rio_readn())
    // 4. 소켓 p_clientfd를 닫아요
    // 5. 받은 Response를 Client에게 줘요 (Rio_writen())
    const char *CL = "Content-length: ";
    int p_clientfd;
    char buf[MAXLINE];
    char *srcp;
    int filesize;
    rio_t rio;

    p_clientfd = Open_clientfd(ptos_host, ptos_port);
    Rio_readinitb(&rio, p_clientfd);
    Rio_writen(p_clientfd, new_request, MAXLINE);

    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(&rio, buf, MAXLINE);
        char *ptr;
        if ((ptr = strstr(buf, CL)) != NULL) {
            filesize = atoi(ptr + (strlen(CL)));
            printf("%d\n", filesize);
        }
        Rio_writen(connfd, buf, strlen(buf));
    }

    srcp = malloc(filesize);
    Rio_readn(p_clientfd, srcp, filesize);
    Close(p_clientfd);
    Rio_writen(connfd, srcp, filesize);
    free(srcp);
    return;
}

void parse_uri(char *uri, char *host, char *port, char *new_uri) {
    const char *http = "://";
    char *p;

    /* uri == "http://{server_ip}:{server_port}/{new_uri_source}" */
    if ((p = strstr(uri, http)) != NULL) {
        uri = p + (strlen(http)); /* uri == "{server_ip}:{server_port}/{new_uri_source}" */
    }
    if ((p = index(uri, '/')) != NULL) {
        strcpy(new_uri, p); /* new_uri = "/{new_uri_source}" */
        *p = '\0';          /* uri == "{server_ip}:{server_port}" */
    }
    if ((p = index(uri, ':')) != NULL) {
        strcpy(port, p + 1); /* port = "{server_port}" */
        *p = '\0';           /* uri == "{server_ip}" */
    }
    strcpy(host, uri); /* host = "{server_ip}" */
}

void create_request(rio_t *rp, char *method, char *new_uri, const char *version, char *new_request, char *host_hdr) {
    char buf[MAXLINE];
    /* request line */
    sprintf(new_request, "%s %s %s\r\n", method, new_uri, version);

    /* request header */
    sprintf(new_request, "%s%s", new_request, host_hdr);
    sprintf(new_request, "%s%s", new_request, user_agent_hdr);
    sprintf(new_request, "%s%s", new_request, connection_hdr);
    sprintf(new_request, "%s%s\r\n\r\n", new_request, proxy_connection_hdr);
}

// void parse_requesthdrs(rio_t* rp) {
// }
