#include "cache.h"

static cdata *head, *rear;
static sem_t qmutex;    // queue mutex
static int tsize;       // total cache size

cdata *get_from_cache(char *url);
int create_cache(cdata* acache);
void delete_node(cdata *p);
void add_to_rear(cdata *p);


void Cache_init()
{
    Sem_init(&qmutex, 0, 1);
    tsize = 0;
    head = NULL;
    rear = NULL;
}

int Get_cache(char *url, int browserfd)
{
    cdata *acache = get_from_cache(url);
    if(acache == NULL)
        return UNCACHED;

    // write to browser
    rio_writen(browserfd, acache->cache, acache->size);

    // Decrease reader number
    P(&(acache->cmutex));
    (acache->read_cnt)--;
    V(&(acache->cmutex));
    return CACHED;
}

int Insert_cache(char *url, char *ptr, int size)
{
    cdata *acache = (cdata *)malloc(sizeof(cdata));
    int status;

    if(ptr == NULL)
        return -1;

    acache->url = malloc(strlen(url)+1);
    strcpy(acache->url,url);

    Sem_init(&(acache->cmutex), 0, 1);
    acache->cache = malloc(size);
    memcpy(acache->cache, ptr, size);
    acache->size = size;
    acache->read_cnt = 0;

    while((status = create_cache(acache)) == CACHE_FAILURE)
        sleep(2);
    if(status == CACHE_BY_OTHER)
    {
        if(acache->cache != NULL)
            Free(acache->cache);
        if(acache->url != NULL)
            Free(acache->url);
        if(acache != NULL)
            Free(acache);
    }

    return 0;
}

// Check if already cached, if yes, then return
// If cache is oversized, execute LRU policy to remove oldest node
int create_cache(cdata* acache)
{
    P(&qmutex);
    cdata *p = head, *tmp;
    while(p)
    {
        if(strcmp(p->url, acache->url) == 0)    // already cached by other threads
        {
            V(&qmutex);
            return CACHE_BY_OTHER;
        }
        p = p->next;
    }

    if((tsize += acache->size) > MAX_CACHE_SIZE)
    {
        p = head;
        while(p && tsize > MAX_CACHE_SIZE)
        {
            // Delete from head, delete it only if it is not occupied
            P(&(acache->cmutex));
            if(acache->read_cnt == 0)
            {
                tsize -= p->size;
                delete_node(p);
                tmp = p;
                p = p->next;
                if(tmp->url != NULL)
                    Free(tmp->url);
                if(tmp->cache != NULL)
                    Free(tmp->cache);
                if(tmp != NULL)
                    Free(tmp);
            }
            else
            {
                V(&qmutex);
                return CACHE_FAILURE;
            }
            V(&(acache->cmutex));
        }
    }

    // now there should be enough space to add, add to rear as latest
    add_to_rear(acache);
    V(&qmutex);
    return CACHE_SUCCESS;
}

// Delete cache node p from cache
void delete_node(cdata *p)
{
    if(p == NULL)
        return;

    if(p == head)
    {
        head = p->next;
    }

    if(p == rear)
    {
        rear = p->prev;
    }


    if(p!=NULL && p->prev)
    {
        p->prev->next = p->next;
    }

    if(p!=NULL && p->next)
    {
        p->next->prev = p->prev;
    }


}

// Insert cache node p to the rear of cache
void add_to_rear(cdata *p)
{
    if(p == NULL)
        return;
    if(head==NULL || rear==NULL)        // If linked list is empty
    {
        head = p;
        rear = p;
        if(head->prev != NULL)
            head->prev = NULL;
        if(head->next != NULL)
            head->next = NULL;
    }
    else
    {
        if(rear != NULL)
            p->prev = rear;
        if(p != NULL)
            rear->next = p;
        if(p->next != NULL)
            p->next = NULL;
        rear = p;
    }
}

// Find cache node cooresponding to given url, increase reader count,
// move this node to the end of linked list
cdata *get_from_cache(char *url)
{
    P(&qmutex);
    cdata *p = head;
    while(p)
    {
        if(strcmp(p->url, url) == 0)
        {
            P(&(p->cmutex));
            (p->read_cnt)++;
            V(&(p->cmutex));

            // move p to rear
            if(rear != p)
            {
                delete_node(p);
                add_to_rear(p);
            }
            break;
        }
        p = p->next;
    }

    V(&qmutex);
    return p;
}


