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
static const char *CL = "Content-Length: "; /* "Content-Length: " */


/* Cache List */
typedef struct cache_node {
    struct cache_node *next;        // 현재 cache_node보다 마지막 요청 시기가 늦는 cache_node
    struct cache_node *prev;        // 현재 cache_node보다 마지막 요청 시기가 빠른 cache_node
    char uri[MAXLINE];
    char response_header[MAXLINE];
    char *response_body;
    size_t response_size;
} cache_node;

typedef struct {
    cache_node *header;
    cache_node *footer;
    size_t len;
} linked_list;

linked_list *cache_list;


/* Helper functions prototype */
void *thread(void *vargp);

void doit(int connfd);
void do_client(rio_t *rp, int connfd, char *new_request);
void create_new_request(char *new_request, char *method, char *new_uri, char *host_hdr);
void send_request(char *ptos_host, char *ptos_port);
int parse_uri(char *uri, char *host, char *port, char *new_request);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int read_response_header(rio_t *rp, char *new_header);

void init_cache_list();
void cache_list_free();
cache_node *cache_search(char *uri);
cache_node *create_cache_node(char *source_pointer, size_t filesize, char *new_header, char *new_uri);
void delete_cache_node();
void insert_cache_node(cache_node *new_cache);


int main(int argc, char **argv) {
    int listenfd, *connfd;
    char hostname[MAXLINE], port[MAXLINE];
    struct sockaddr_storage clientaddr;
    socklen_t clientlen;
    pthread_t tid;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    init_cache_list();

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfd);
    }
    cache_list_free();
}


void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void doit(int connfd) {
    char host_hdr[MAXLINE];
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char ptos_host[MAXLINE], ptos_port[MAXLINE], new_uri[MAXLINE], new_request[MAXLINE];
    int clientfd;
    rio_t rio;
    cache_node *cache;

    /* rio <-> connfd */
    Rio_readinitb(&rio, connfd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers: %s\n", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (parse_uri(uri, ptos_host, ptos_port, new_uri) == 0) {
        clienterror(connfd, buf, "400", "Bad Request", "The request sent by the client was syntactically incorrect.");
        return;
    }

    /* Cached request라면, Client에게 Cache된 response를 전달하고 함수 종료 */
    if ((cache = cache_search(new_uri)) != NULL) {
        Rio_writen(connfd, cache->response_header, strlen(cache->response_header));
        Rio_writen(connfd, cache->response_body, cache->response_size);
        return;
    }

    sprintf(host_hdr, "Host: %s:%s\r\n", ptos_host, ptos_port);
    create_new_request(new_request, method, new_uri, host_hdr);

    /* Server 연결 */
    clientfd = Open_clientfd(ptos_host, ptos_port);
    Rio_writen(clientfd, new_request, MAXLINE);

    /* rio <-> clientfd */
    Rio_readinitb(&rio, clientfd);
    do_client(&rio, connfd, new_uri);
    Close(clientfd);
}



void do_client(rio_t *rp, int connfd, char *new_uri) {
    char buf[MAXLINE], new_header[MAXLINE];
    int p_clientfd;
    char *srcp;
    size_t filesize = 0;

    /* Server에게 response받아서 Client로 전달 */
    filesize = read_response_header(rp, new_header);        // Server에서 response header 읽기
    Rio_writen(connfd, new_header, strlen(new_header));     // Client에게 response header 쓰기

    if(filesize <= 0) {                 // Content-Length가 0인경우 종료
        return;
    }
    
    srcp = Malloc(filesize);
    Rio_readnb(rp, srcp, filesize);                         // Server에서 response body 읽기
    Rio_writen(connfd, srcp, filesize);                     // Client에게 response body 쓰기

    if(filesize > MAX_OBJECT_SIZE) {    // Caching 가능한 크기보다 큰 경우 종료
        Free(srcp);
        return;
    }

    /* Caching */
    cache_node *new_cache = create_cache_node(srcp, filesize, new_header, new_uri);
    if (cache_list->len >= (int)(MAX_CACHE_SIZE / MAX_OBJECT_SIZE)) {
        delete_cache_node();
    }
    insert_cache_node(new_cache);
}

/**
 * Server가 보낸 response header를 읽는다.
 * response header의 Content-Length 값을 return,
 * Content-Length가 존재하지 않으면 0을 return한다.
 */
int read_response_header(rio_t *rp, char *new_header) {
    char buf[MAXLINE];
    int filesize = 0;

    while (strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        char *ptr;
        if ((ptr = strstr(buf, CL)) != NULL) {
            filesize = atoi(ptr + (strlen(CL)));
        }
        sprintf(new_header, "%s%s", new_header, buf);
    }
    return filesize;
}

/* URI를 parsing하여 PTOS_HOST, PTOS_PORT, NEW_URI에 저장한다. */
int parse_uri(char *uri, char *host, char *port, char *new_uri) {
    const char *http = "://";
    const char *default_portnum = "8000";
    char *p;

    /* uri == "http://{server_ip}:{server_port}/{new_uri_source}" */

    if ((p = strstr(uri, http)) != NULL) {
        uri = p + (strlen(http));   // uri     == "{server_ip}:{server_port}/{new_uri_source}"
    }
    if ((p = index(uri, '/')) != NULL) {
        strcpy(new_uri, p);         // new_uri =  "/{new_uri_source}"
        *p = '\0';                  // uri     == "{server_ip}:{server_port}"
    } else {
        strcpy(new_uri, "/");
    }
    if ((p = index(uri, ':')) != NULL) {
        strcpy(port, p + 1);        // port    =  "{server_port}"
        *p = '\0';                  // uri     == "{server_ip}"
    } else {
        strcpy(port, default_portnum);
    }
    if (strlen(uri) != 0) {
        strcpy(host, uri);          // host    =  "{server_ip}"
    } else {
        return 0;
    }
    return 1;
}

/**
 * METHOD, NEW_URI, HOST_HDR와 static 전역 변수를 이용하여 
 * Server로 보낼 새로운 request를 NEW_REQUEST에 저장한다.
*/
void create_new_request(char *new_request, char *method, char *new_uri, char *host_hdr) {
    /* request line */
    sprintf(new_request, "%s %s %s\r\n", method, new_uri, ptos_request_version);

    /* request header */
    sprintf(new_request, "%s%s", new_request, host_hdr);
    sprintf(new_request, "%s%s", new_request, user_agent_hdr);
    sprintf(new_request, "%s%s", new_request, connection_hdr);
    sprintf(new_request, "%s%s\r\n\r\n", new_request, proxy_connection_hdr);
}

/**
 * Client로 부터 받은 request에 대한 에러를 출력한다.
*/
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

/* Cache list 초기화 */
void init_cache_list() {
    cache_list = (linked_list *)malloc(sizeof(linked_list));
    cache_node *header = (cache_node *)malloc(sizeof(cache_node));
    cache_node *footer = (cache_node *)malloc(sizeof(cache_node));
    header->next = footer;
    header->prev = NULL;
    footer->prev = header;
    footer->next = NULL;
    cache_list->footer = footer;
    cache_list->header = header;
    cache_list->len = 0;
}

/**
 * URI를 가지고있는 NODE_C 구조체의 pointer를 return한다.
 * 없으면 NULL을 return.
 */
cache_node *cache_search(char *uri) {
    cache_node *cur = cache_list->header->next;
    cache_node *nil = cache_list->footer;
    while (cur != nil) {
        if (!strcmp(cur->uri, uri)) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/**
 * cache_list의 모든 할당된 메모리를 반환한다.
*/
void cache_list_free() {
    cache_node *cur = cache_list->header;
    cache_node *next;
    while (cur != cache_list->footer) {
        next = cur->next;
        free(cur);
        cur = next;
    }
    free(cache_list->footer);
    free(cache_list);
}

/**
 * 인자로 받은 변수를 멤버로 갖는 새로운 cache_node pointer를 생성해서 return한다.
*/
cache_node *create_cache_node(char *source_pointer, size_t filesize, char *new_header, char *new_uri) {
    cache_node *new_cache = Malloc(sizeof(cache_node));
    new_cache->response_body = source_pointer;
    new_cache->response_size = filesize;
    strcpy(new_cache->response_header, new_header);
    strcpy(new_cache->uri, new_uri);
    return new_cache;
}

/**
 * cache_list의 footer 직전(prev) cache_node를 제거하고, 할당된 메모리를 반환한다.
*/
void delete_cache_node() {
    cache_node *old_node = cache_list->footer->prev;
    cache_list->footer->prev = old_node->prev;
    old_node->prev->next = cache_list->footer;
    free(old_node);
}

/**
 * CACHE를 cache_list의 header 직후(next) cache_node로 넣어준다.
*/
void insert_cache_node(cache_node *cache) {
    cache->prev = cache_list->header;
    cache->next = cache_list->header->next;
    cache_list->header->next->prev = cache;
    cache_list->header->next = cache;
    cache_list->len += 1;
}