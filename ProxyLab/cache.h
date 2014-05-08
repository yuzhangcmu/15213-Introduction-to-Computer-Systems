#include "csapp.h"

#define CACHED 1
#define UNCACHED 2
#define CACHE_SUCCESS 3
#define CACHE_FAILURE 4
#define CACHE_BY_OTHER 5
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

struct data_node
{
    char *url;
    int size;
    void *cache;
    int read_cnt;
    sem_t cmutex;   // mutex for cache reader
    struct data_node *next;
    struct data_node *prev;
};

typedef struct data_node cdata;

void Cache_init();

// Check if given request url is in cache, if yes, then forward cache to browser with CACHED returned
// otherwise, return UNCACHED
int Get_cache(char *url, int browserfd);

// Insert a <url,ptr> pair to cache
int Insert_cache(char *url, char *ptr, int size);


