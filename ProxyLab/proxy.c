/*
 * Name: Bin Feng
 * Andrew id: bfeng
*/

#include "csapp.h"
#include "cache.h"

//static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
//static const char *accept_str = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
//static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
//static const char *connection_str = "Connection: close\r\nProxy-Connection: close\r\n";

int browser_to_server(rio_t *browser_rio, int proxy_as_client_fd, char *uri);
int server_to_browser(rio_t *proxy_as_client_rio, int browser_fd, char *url);
int write_buf_to_cache_browser(int browser_fd, int *tsize_p, char **cache_p, char **cache_cur_p, char *buf, int length);
void *thread(void *vargp);
int parse_uri(char* uri, char* host, char* path, unsigned short *port_p);


int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD,SIG_IGN);

    int *connfdp;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    Cache_init();

    int port = atoi(argv[1]);
    socklen_t clientlen = sizeof(clientaddr);
    int listenfd = Open_listenfd(port);
    while (1)
    {
        connfdp = (int *)malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    return 0;
}


void* thread(void *vargp)
{
    rio_t browser_rio, proxy_as_client_rio;
    char buf[MAXLINE],
         uri[MAXLINE], host[MAXLINE];
    unsigned short port;

    Pthread_detach(pthread_self());
    int browser_fd = *(int *)vargp;
    Free(vargp);

    Rio_readinitb(&browser_rio, browser_fd);

    // Read client request line, eg: GET www.cmu.edu/index.html HTTP/1.1
    Rio_readlineb(&browser_rio, buf, MAXLINE);

    char method[MAXLINE], url[MAXLINE], version[MAXLINE];
    // Parse client request line to get method, url, version
    sscanf(buf, "%s %s %s", method, url, version);

    parse_uri(url, host, uri, &port);



    // Ignore non-get methods
    if (strcasecmp(method, "GET"))
    {
        Close(browser_fd);
        fprintf(stderr, "Only GET method is supported\n");
        pthread_exit(NULL);
    }

    // Check whether in cache already
    int cstat = Get_cache(url, browser_fd);

    // If already cached and forwarded to client, simply quit
    if(cstat == CACHED)
    {
        Close(browser_fd);
        fprintf(stderr, "Found in cache\n");
        pthread_exit(NULL);
    }

    // Proxy as client to connect to server
    int proxy_as_client_fd = Open_clientfd(host, port);
    Rio_readinitb(&proxy_as_client_rio, proxy_as_client_fd);

    // If not yet cached, proxy get request from client
    if(browser_to_server(&browser_rio, proxy_as_client_fd, uri) == -1)
    {
        Close(browser_fd);
        Close(proxy_as_client_fd);
        fprintf(stderr, "browser_to_server error\n");
        pthread_exit(NULL);
    }

    // Proxy forward response to client browser
    server_to_browser(&proxy_as_client_rio, browser_fd, url);
    Close(browser_fd);
    Close(proxy_as_client_fd);
    pthread_exit(NULL);
}


int browser_to_server(rio_t *browser_rio, int proxy_as_client_fd, char *uri)
{
    char buf[MAXLINE];
    int n = 0;

    sprintf(buf, "GET %s HTTP/1.0\r\n", uri);

    // Send http request line to server
    Rio_writen(proxy_as_client_fd, buf, strlen(buf));

    // Read http request header from client browser
    while((n = rio_readlineb(browser_rio, buf, MAXLINE)))
    {
        if(n == -1)
            return -1;

        if(strcmp(buf, "\r\n")==0)
            break;

        // Forward http request header to server

        if(Rio_writen(proxy_as_client_fd, buf, strlen(buf)) == -1)
            return -1;
    }

    strncpy(buf, "\r\n", 4);
    Rio_writen(proxy_as_client_fd, buf, strlen(buf));

    return 0;
}

// Read response header and body from server, forward to client browser and save a copy
// in cache
int server_to_browser(rio_t *proxy_as_client_rio, int browser_fd, char *url)
{
    int tsize = 0;  // total size of cache
    int csize = 0;  // size of a response block
    int n = 0;
    char* cache = malloc(MAX_OBJECT_SIZE);
    char* cache_cur = cache;

    char buf[MAXLINE];

    // Read response line from server, eg: HTTP/1.1 200 OK
    rio_readlineb(proxy_as_client_rio, buf, MAXLINE);

    // 1. Proxy write server response header line
    if(write_buf_to_cache_browser(browser_fd, &tsize, &cache, &cache_cur, buf, strlen(buf)) == -1)
        return -1;

    // Read response header from server
    while((n = rio_readlineb(proxy_as_client_rio, buf, MAXLINE)))
    {
        if (!strncasecmp(buf, "Content-Length: ", 16))
            csize = atoi(buf + 16);     // record content length

        // 2. Proxy write server response header
        write_buf_to_cache_browser(browser_fd, &tsize, &cache, &cache_cur, buf, strlen(buf));

        if(!strcmp(buf, "\r\n"))
            break;
    }

    // ===============================================================
    // Continue only if response body exists!

    // Read response body and forward to client
    if(csize == 0)
    {
        while((n = rio_readnb(proxy_as_client_rio, buf, MAXLINE)))
        {
            // 3. Proxy write response body back to browser
            write_buf_to_cache_browser(browser_fd, &tsize, &cache, &cache_cur, buf, n);
        }
    }
    else
    {
        if((tsize += csize) > MAX_OBJECT_SIZE)
            if(cache)
            {
                Free(cache);
                cache = NULL;
            }

        // Read MAXLINE size each time
        while(csize >= MAXLINE)
        {
            n = rio_readnb(proxy_as_client_rio, buf, MAXLINE);

            // 4. Proxy write response body back to browser
            write_buf_to_cache_browser(browser_fd, NULL, &cache, &cache_cur, buf, n);

            csize -= MAXLINE;
        }

        // Read remaining part: csize<MAXLINE
        if(csize > 0)
        {
            n = rio_readnb(proxy_as_client_rio, buf, csize);

            // 5. Proxy write remaining response body back to browser
            write_buf_to_cache_browser(browser_fd, NULL, &cache, &cache_cur, buf, n);
        }
    }

    // Insert <url,cache> pair to linked list
//    if(cache)
        if(Insert_cache(url, cache, tsize) == -1)
            return -1;

    return 0;
}


int parse_uri(char* uri, char* host, char* path, unsigned short *port_p)
{

    char* uri_begin = uri+7;
    char port_str[1024];
    char* port_begin;
    char* port_end;
    *port_p = 80;

    // Find if there is : in uri
    port_begin = strstr(uri_begin, ":");
    // printf("port_begin:%s\n", port_begin);

    if(port_begin != NULL){
        port_end = strstr(port_begin, "/");
        // printf("port_end:%s\n", port_end);

        int port_begin_index = port_begin - uri_begin;
        int port_end_index = port_end - uri_begin;

        // printf("uri_begin:%s\n", uri_begin);
        strncpy(host, uri_begin, port_begin_index);
        host[port_begin_index] = '\0';
        // printf("host:%s\n", host);

        // printf("%d\n", port_begin_index);
        // printf("%d\n", port_end_index);

        strncpy(port_str, uri_begin+port_begin_index+1, port_end_index-port_begin_index-1);
        *port_p = atoi(port_str);

        strcpy(path, uri_begin+port_end_index);
        // printf("path:%s\n", path);
        // printf("port_str:%s\n", port_str);
        // printf("port:%d\n", atoi(port_str));
    }
    else{
        port_end = strstr(uri_begin, "/");
        strncpy(host, uri_begin, port_end - uri_begin);
        host[port_end - uri_begin] = '\0';
        // printf("host:%s\n", host);

        // printf("port_end:%s\n", port_end);

        strcpy(path, port_end);
        // printf("path:%s\n", path);
    }

    return 0;
}

// Write buf to client browser and cache
int write_buf_to_cache_browser(int browser_fd, int *tsize_p, char **cache_p, char **cache_cur_p, char *buf, int length)
{
    // Proxy send response data to browser
    int n = 0;

    if(browser_fd > 0)
        n = Rio_writen(browser_fd, buf, length);

    if(n <= 0){
        return -1;
    }

    // Store to cache
    if(*cache_p)
    {
        if (tsize_p != NULL)
        {
            if(*tsize_p + n > MAX_OBJECT_SIZE)   // Too big, ignore
            {
                if(*cache_p)
                {
                    Free(*cache_p);
                    *cache_p = NULL;
                }
            }
            else
            {
                memcpy(*cache_cur_p, buf, n);
                *cache_cur_p += n;      // Update current index in cache_p
                *tsize_p += n;   // Inrease cache current size
            }
        }
        else
        {
            memcpy(*cache_cur_p, buf, n);
            *cache_cur_p += n;
        }
    }
    return 0;
}
