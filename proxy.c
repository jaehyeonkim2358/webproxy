#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
/* Request */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

static const char *connection_hdr =
    "Connection: close\r\n";

static const char *proxy_connection_hdr =
    "Proxy-Connection: close\r\n";

static const char *ptos_request_version = "HTTP/1.0";

/* Response */
static const char *CL = "Content-Length: "; /* response body content length */ 

/* Cache */
/* 새로운 Cache는 header에 붙일거야 
   가장 최근에 사용된 Cache는 header로 위치를 옮겨줄거야
   마지막 사용이 가장 늦은 Cache는 Footer의 prev가 될거야 */
typedef struct node_c {
    struct node_c *next, *prev;
    char uri[MAXLINE];
    char data_header[MAXLINE];
    char *data;
    size_t data_size;
} node_c;

typedef struct {
    node_c *header, *footer;
    size_t len;
} linked_list;

linked_list *cache_list;

/* Helper functions prototype */
void *thread(void *vargp);
void doit(int connfd);
void do_client(rio_t *rp, int connfd, char *new_request);
void create_new_request(char *method, char *new_uri, char *new_request, char *host_hdr);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *host, char *port, char *new_request);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
node_c *cache_search(char *uri);
void all_cache_free();


int main(int argc, char **argv) {
    int listenfd, *connfd;  // listening descriptor, connected descriptor
    char hostname[MAXLINE], port[MAXLINE];
    struct sockaddr_storage clientaddr;
    socklen_t clientlen;
    pthread_t tid;
    

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* Cache list 초기화 */
    cache_list = (linked_list *)malloc(sizeof(linked_list));
    node_c *header = (node_c *)malloc(sizeof(node_c));
    node_c *footer = (node_c *)malloc(sizeof(node_c));
    header->next = footer;
    header->prev = NULL;
    footer->prev = header;
    footer->next = NULL;
    cache_list->footer = footer;
    cache_list->header = header;
    cache_list->len = 0;

    printf("header: %p\n", cache_list->header);
    printf("footer: %p\n", cache_list->footer);


    listenfd = Open_listenfd(argv[1]); /* 연결 대기 */
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); /* 연결 */
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfd);
    }
    all_cache_free();
}


void *thread(void *vargp) {
    int connfd = *((int*)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}


void doit(int connfd) {
    rio_t rio;
    node_c *cache;
    char host_hdr[MAXLINE];
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char ptos_host[MAXLINE], ptos_port[MAXLINE], new_uri[MAXLINE], new_request[MAXLINE];
    int clientfd;

    /* rio <-> connfd */
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);  // buf = client request line (method URI version)

    printf("Request headers:\n");
    printf("%s", buf);

    sscanf(buf, "%s %s %s", method, uri, version);

    if(parse_uri(uri, ptos_host, ptos_port, new_uri) == 0) {    // URI --> PTOS_HOST, PTOS_PORT, NEW_URI
        clienterror(connfd, buf, "400", "Bad Request", "이상해요");
        return;
    }

    read_requesthdrs(&rio);  // Client Request를 읽음

    // new_uri와 같은 uri를 갖는 cache를 찾아서 return,
    // 해당하는 cache가 없으면 cache_list->footer를 return
    if((cache = cache_search(new_uri)) != cache_list->footer) {
        Rio_writen(connfd, cache->data_header, strlen(cache->data_header));
        Rio_writen(connfd, cache->data, cache->data_size);
        return;
    }
    
    /* Proxy --NEW_REQUEST--> Server */
    clientfd = Open_clientfd(ptos_host, ptos_port);
    sprintf(host_hdr, "Host: %s:%s\r\n", ptos_host, ptos_port);
    create_new_request(method, new_uri, new_request, host_hdr);
    Rio_writen(clientfd, new_request, MAXLINE);
    
    /* clientfd에 적힌 내용을 Read하기 위해: rio <-> clientfd */
    Rio_readinitb(&rio, clientfd);

    
    do_client(&rio, connfd, new_uri);
    Close(clientfd);
}


node_c *cache_search(char *uri) {
    node_c *cur = cache_list->header->next;
    node_c *nil = cache_list->footer;
    while(cur != nil) {
        printf("cur: %p\n", cur);
        if(!strcmp(cur->uri, uri)) {
            return cur;
        }
        cur = cur->next;
    }
    return nil;
}


void all_cache_free() {
    node_c *cur = cache_list->header;
    node_c *next;
    while(cur != cache_list->footer) {
        next = cur->next;
        free(cur);
        cur = next;
    }
    free(cache_list->footer);
    free(cache_list);
}


/* [Server]  ----response---->  [Proxy]  -----response---->  [Client] */
void do_client(rio_t *rp, int connfd, char *new_uri) {
    int p_clientfd;
    char buf[MAXLINE];
    char *srcp;
    int filesize = 0;
    char new_header[MAXLINE];

    /* Server가 보낸 response header 읽고 connfd에 적어줌 */
    Rio_readlineb(rp, buf, MAXLINE);
    sprintf(new_header, "%s", buf);
    Rio_writen(connfd, buf, strlen(buf));

    /* Send response header to client */
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        char *ptr;
        if ((ptr = strstr(buf, CL)) != NULL) {  // CL == "Content-Length: "
            filesize = atoi(ptr + (strlen(CL)));
        }
        sprintf(new_header, "%s%s", new_header, buf);
        Rio_writen(connfd, buf, strlen(buf));
    }

    printf("new_header:::::: %s", new_header);
    
    /* Send response body to client */
    if (filesize) {
        srcp = Malloc(filesize);
        Rio_readnb(rp, srcp, filesize);
        Rio_writen(connfd, srcp, filesize);

        if(filesize <= MAX_OBJECT_SIZE) {   /* Cache에 저장할 수 있는 사이즈인가요? */
            node_c *new_cache = Malloc(sizeof(node_c));
            fprintf(stderr, "%s", srcp);
            new_cache->data = srcp;
            new_cache->data_size = filesize;
            // new_cache->data_header = (char *)malloc(strlen(new_header));
            // new_cache->data_header = new_header;
            strcpy(new_cache->data_header, new_header);
            strcpy(new_cache->uri, new_uri);
            if(cache_list->len >= (int)(MAX_CACHE_SIZE / MAX_OBJECT_SIZE)) {        /* Cache List가 꽉찼으면 Footer쪽의 Cache를 버려요 */
                node_c *old_node = cache_list->footer->prev;
                cache_list->footer->prev = old_node->prev;
                old_node->prev->next = cache_list->footer;
                free(old_node);
            }
            /* 새로운 Cache는 Header쪽에 붙여요 */
            new_cache->prev = cache_list->header;
            new_cache->next = cache_list->header->next;
            cache_list->header->next->prev = new_cache;
            cache_list->header->next = new_cache;
            cache_list->len += 1;
            printf("new cache: %p\n", new_cache);
        } else {                            /* Cache로 저장할 수 없는 경우만 바로 free() 해요 */
            Free(srcp);
        }
    }
    
}


int parse_uri(char *uri, char *host, char *port, char *new_uri) {
    const char *http = "://";
    const char *default_portnum = "8000";
    char *p;

    /* uri == "http://{server_ip}:{server_port}/{new_uri_source}" */

    if ((p = strstr(uri, http)) != NULL) {
        uri = p + (strlen(http));           /* uri     == "{server_ip}:{server_port}/{new_uri_source}" */
    }
    if ((p = index(uri, '/')) != NULL) {
        strcpy(new_uri, p);                 /* new_uri =  "/{new_uri_source}" */
        *p = '\0';                          /* uri     == "{server_ip}:{server_port}" */
    } else {
        strcpy(new_uri, "/");
    }
    if ((p = index(uri, ':')) != NULL) {
        strcpy(port, p + 1);                /* port    =  "{server_port}" */
        *p = '\0';                          /* uri     == "{server_ip}" */
    } else {
        strcpy(port, default_portnum);
    }
    if (strlen(uri) != 0) {
        strcpy(host, uri);                  /* host    =  "{server_ip}" */
    } else {
        return 0;
    }
    return 1;
}


void create_new_request(char *method, char *new_uri, char *new_request, char *host_hdr) {
    /* request line */
    sprintf(new_request, "%s %s %s\r\n", method, new_uri, ptos_request_version);

    /* request header */
    sprintf(new_request, "%s%s", new_request, host_hdr);
    sprintf(new_request, "%s%s", new_request, user_agent_hdr);
    sprintf(new_request, "%s%s", new_request, connection_hdr);
    sprintf(new_request, "%s%s\r\n\r\n", new_request, proxy_connection_hdr);
}


void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}


void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body,
            "%s<body bgcolor="
            "ffffff"
            ">\r\n",
            body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
