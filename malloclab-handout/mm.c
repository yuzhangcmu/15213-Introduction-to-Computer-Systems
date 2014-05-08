/*
 * Name: Bin Feng
 * Andrew ID: bfeng
 *
 * Here is a general purpose dynamic storage allocator which uses
 * segrated free list and first fit approach to find free blocks.
 *
 * In general, it has the following features:
 * -Managing free blocks
 * -Finding a free block
 * -Splitting blocks
 * -Allocating/Freeing blocks
 * -Coalescing to handle "false fragmentation"
 */

#include <stdio.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif              /* def DRIVER */

#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE   168
#define OVERHEAD    8
#define ALIGNMENT   8
#define MINBLOCK    24

/* Indicating current and previous block allcoation status */
#define CUR_ALLOC   1
#define PREV_ALLOC  2

/* # of segregated list */
#define TOTALLIST   14

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write 8 bytes at address p */
#define GET(p)      (*((size_t*) (p)))
#define PUT(p, val) (*((size_t*) (p)) = (val))

/* Read and write 4 bytes at address p */
#define GET4BYTES(p)     (*((unsigned*) (p)))
#define PUT4BYTES(p, val)(*((unsigned*) (p)) = (val))

/* rounds UP to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* Align version of size of size_t */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))

/* Given header/footer address, return block size, allocation status */
/* size include header, footer, and payload */
/* p: address at header or footer */
#define GET_SIZE(p)         (GET4BYTES(p) & ~0x7)
#define GET_ALLOC(p)        (GET4BYTES(p) & 0x1)

/* Return allocation status of previous block */
#define GET_PREV_ALLOC(p)   (GET4BYTES(p) & 0x2)

/* bp: points to payload, calculate header/footer address */
#define HDRP(bp)    ((char*)(bp) - WSIZE)
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* bp: points to payload, calculate next and prev blocks's address */
#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))
#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

/* successor: next free block after bp block
   predecessor: previous free block before bp block
*/
#define SUCCESSOR(bp)   ((char*) ((char*)(bp)))
#define PREDECESSOR(bp)   ((char*) ((char*)(bp) + DSIZE))

/* Prototypes */
inline static unsigned int index_of(unsigned int size);
inline static void *extend_heap(size_t asize);
inline static void coalesce(void **bp);
inline static void *find(size_t list_index, size_t asize);
inline static void *find_list(size_t asize);
inline static void place(void *bp, size_t asize);
inline static void insert_list(char *bp, size_t size);
inline static void remove_list(char *bp);


/* Global variables */
char **lists;
char *h_start;


inline static unsigned int index_of(unsigned int size)
{
#if 0
    unsigned int index = 0;

    if(size == 0){
        index = 1;
    }
    else if(size <= 120){
        // index = size%24==0 ? size / 24 : size / 24 + 1;
        index = ceil(size/24.0);
    }
    // else if(size <= 480){
    //     index = 6;
    // }
    else{
        // index = (size-480)%480==0 ? (size-480)/480 + 6: (size-480)/480 + 7;
        // printf("--%f\n", ceil(size/480.0));
        index = ceil(log(ceil(size/480.0))/log(2)) + 6;
    }

    if(index >= 14){
        index = 14;
    }

    return index;
#endif

    if(size <= 24){
        return 1;
    }
    else if(size <= 48){
        return 2;
    }
    else if(size <= 72){
        return 3;
    }
    else if(size <= 96){
        return 4;
    }
    else if(size <= 120){
        return 5;
    }
    else if(size <= 480){
        return 6;
    }
    else if(size <= 960){
        return 7;
    }
    else if(size <= 1920){
        return 8;
    }
    else if(size <= 3840){
        return 9;
    }
    else if(size <= 7680){
        return 10;
    }
    else if(size <= 15360){
        return 11;
    }
    else if(size <= 30720){
        return 12;
    }
    else if(size <= 61440){
        return 13;
    }
    else{
        return 14;
    }
}


/*
 * Extend_heap - Extends the heap upward of size bytes
   and adds free block to appropriate free list before
   coalescing.
 */
inline static void *extend_heap(size_t asize)
{
    char *bp;

    if(asize <= 0){
        return NULL;
    }

    if ((long) (bp = mem_sbrk(asize)) < 0){
        return NULL;
    }

    size_t alloc = GET_PREV_ALLOC(HDRP(bp));

    /* Update epilogue header and footer */
    PUT4BYTES(HDRP(bp), PACK(asize, alloc));
    PUT4BYTES(FTRP(bp), PACK(asize, alloc));

    /* Insert newly extended heap to appropriate free list */
    insert_list(bp, asize);

    /* Update epilogue header */
    PUT4BYTES(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* Coalesce to remove false fragmentation */
    coalesce((void**)&bp);
    return bp;
}


/*
 * Coalescing combine free blocks which are adjacent to given block
 * to remove false fragmentation
 */
inline static void coalesce(void **bp)
{
    size_t prev_alloc;      /* Previous block allocation status */
    size_t next_alloc;      /* Next block allocation status */

    /* get size of current, prev, next free block (including header) */
    size_t size;
    size_t nsize;
    size_t psize;

    /* previous block's address and its header address */
    char *prev_hd;
    char *prev_blk;
    char *next_blk;

    if(bp == NULL){
        return;
    }
    else{
        prev_alloc = GET_PREV_ALLOC(HDRP(*bp));
        next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(*bp)));
        size = GET_SIZE(HDRP(*bp));
        nsize = 0;
        psize = 0;
    }

    /* Case 1: Both Prev and Next blocks are allocated */
    if (prev_alloc && next_alloc) {
        return;
    }

    /* Case 2: Prev block is allocated, Next is free */
    else if (prev_alloc && !next_alloc) {
        next_blk = NEXT_BLKP(*bp);

        /* remove coalesced blocks */
        remove_list(*bp);
        remove_list(next_blk);

        /* update new block size */
        size += GET_SIZE(HDRP(next_blk));

        /* update header and footer */
        PUT4BYTES(HDRP(*bp), PACK(size, prev_alloc));
        PUT4BYTES(FTRP(*bp), PACK(size, prev_alloc));

        /* insert combined blocks to free list */
        insert_list(*bp, size);
    }

    /* Case 3- Previous block is free, Next block is allocated */
    else if (!prev_alloc && next_alloc) {
        remove_list(*bp);

        prev_blk = PREV_BLKP(*bp);
        *bp = prev_blk;

        prev_hd = HDRP(*bp);
        psize = GET_SIZE(prev_hd);

        remove_list(*bp);

        size += psize;

        PUT4BYTES(prev_hd, PACK(size, GET_PREV_ALLOC(prev_hd)));
        PUT4BYTES(FTRP(*bp), GET4BYTES(prev_hd));

        insert_list(*bp, size);
    }

    /* Case 4- Previous and next block are both free */
    else {
        remove_list(*bp);

        next_blk = NEXT_BLKP(*bp);

        prev_blk = PREV_BLKP(*bp);
        *bp = prev_blk;
        prev_hd = HDRP(*bp);

        psize = GET_SIZE(prev_hd);
        nsize = GET_SIZE(HDRP(next_blk));

        remove_list(*bp);
        remove_list(next_blk);

        size += psize + nsize;

        PUT4BYTES(prev_hd, PACK(size, GET_PREV_ALLOC(prev_hd)));
        PUT4BYTES(FTRP(*bp), GET4BYTES(prev_hd));

        insert_list(*bp, size);
    }
}

/*
 * Find a free block in a specific sized list, using first fit strategy
 */
inline static void *find(size_t list_index, size_t asize)
{
    char *current = NULL;       /* the head pointer at a particular size free list */

    current = (char*)GET(&((char*)lists)[8*list_index]);

    while (current != NULL) {
        if (asize <= GET_SIZE(HDRP(current))) {
            return current;
        }
        current = (char *) GET(SUCCESSOR(current));
    }

    return NULL;
}

/*
 * Traveral through free lists to find appropriate sized list
 *
 */
inline static void *find_list(size_t asize)
{
    size_t list_index;
    char *bp = NULL;

    if(asize <= 0){
        return NULL;
    }

    /* Traveral through each list */
    for (list_index = index_of(asize)-1; list_index < TOTALLIST; list_index++) {
        if ((bp = find(list_index, asize)) != NULL)
            return bp;
    }

    return NULL;
}

/*
 * Mark asize as allocated in the free block.
 * If the free block is big enough, split it:
 * First part is used to store asize, second part
 * is the remaining free block.
 *
 */
inline static void place(void *bp, size_t asize)
{
    size_t csize;           /* Original free block size */
    size_t remain_size;     /* Remaining free block size */
    char *next_bp;          /* Next adjacent block address */

    if(asize <= 0){
        return;
    }
    else{
        csize = GET_SIZE(HDRP(bp));
        remain_size = csize - asize;
    }

    if(bp == NULL){
        return;
    }
    else{
        next_bp = NEXT_BLKP(bp);
    }

    size_t prec_alloc = GET_PREV_ALLOC(HDRP(bp));;
    size_t next_alloc = GET4BYTES(HDRP(next_bp));

    remove_list(bp);

    /* Split is done when free block is big enough */
    if (remain_size >= MINBLOCK) {

        /* Update header of bp to be allocated */
        PUT4BYTES(HDRP(bp), PACK(asize, prec_alloc | CUR_ALLOC));

        /* Update address of next block */
        next_bp = NEXT_BLKP(bp);

        /* Update next block's allocation status */
        PUT4BYTES(HDRP(next_bp), (remain_size & ~CUR_ALLOC) | PREV_ALLOC);
        PUT4BYTES(FTRP(next_bp), (remain_size & ~CUR_ALLOC) | PREV_ALLOC);

        /* Insert remaining free block to free list */
        insert_list(next_bp, remain_size);
    }
    else {      /* No splitting */

        /* Update header of bp to be allocated */
        PUT4BYTES(HDRP(bp), PACK(csize, prec_alloc | CUR_ALLOC));

        /* Update next block's allocation status */
        PUT4BYTES(HDRP(next_bp), next_alloc | PREV_ALLOC);

        if (!next_alloc)
            PUT4BYTES(FTRP(next_bp), next_alloc | PREV_ALLOC);
    }
}


/*
 * Insertion policy: LIFO (last-in-first-out)
 * Insert free block at the head of the free list
 *
 */
inline static void insert_list(char *bp, size_t size)
{
    char *head;         /* First 8 bytes in the head of a specific list */
    char *head_addr;    /* Address at the head of a specific list */

    if(bp == NULL || size<=0){
        return;
    }

    /* Find appropriate list based on given size */
    head_addr = (char*)lists + 8*(index_of(size) - 1);
    head = (char *) GET(head_addr);


    if (!head) {    /* If no block in the list */
        /* linked-list operations */
        PUT(head_addr, (size_t) bp);
        PUT(PREDECESSOR(bp), (size_t) NULL);
        PUT(SUCCESSOR(bp), (size_t) NULL);
    }
    else {
        /* linked-list operations */
        PUT(PREDECESSOR(bp), (size_t) NULL);
        PUT(head_addr, (size_t) bp);
        PUT(PREDECESSOR(head), (size_t) bp);
        PUT(SUCCESSOR(bp), (size_t) head);
    }
}

/*
 * When a block is allocated, it is removed from
 * the free list.
 */
inline static void remove_list(char *bp)
{
    char *next_bp;  /* Previous block */
    char *prevaddress;  /* Next block */
    char* head_addr;    /* Address at the head of a specific list */
    size_t size;        /* Size of block bp points to */

    if(bp == NULL){
        return;
    }

    next_bp = (char *) GET(SUCCESSOR(bp));
    prevaddress = (char *) GET(PREDECESSOR(bp));
    size = GET_SIZE(HDRP(bp));

    /* When bp is at head of list: make the succeeding block to be listhead */
    if (prevaddress == NULL && next_bp != NULL) {
        /* Update list */
        head_addr = (char*)lists + 8*(index_of(size) - 1);
        PUT(head_addr, (size_t) next_bp);
        PUT(PREDECESSOR(next_bp), (size_t) NULL);
    }
    /* There is only bp in the list */
    else if (prevaddress == NULL && next_bp == NULL) {
        head_addr = (char*)lists + 8*(index_of(size) - 1);
        PUT(head_addr, (size_t) next_bp);
    }
    /* If bp points to the tail block */
    else if (prevaddress != NULL && next_bp == NULL) {
        PUT(SUCCESSOR(prevaddress), (size_t) NULL);
    }
    /* If bp block is in the middle of a list */
    else if(prevaddress != NULL && next_bp != NULL){
        PUT(PREDECESSOR(next_bp), (size_t) prevaddress);
        PUT(SUCCESSOR(prevaddress), (size_t) next_bp);
    }
}


/*
 * mm_init - Called when a new trace starts.
 */
int mm_init(void)
{
    unsigned int i = 0;

    /* Allocate segregated free list */
    if ((lists = (char**)mem_sbrk(TOTALLIST * DSIZE)) == NULL)
        return -1;

    /* Initializes each index in the array of head pointers */
    for(i = 0; i < TOTALLIST; i++){
       lists[i] = NULL;
    }

    /* Prologue and epilogue */
    if ((h_start = mem_sbrk(4 * WSIZE)) == NULL)
        return -1;

    PUT4BYTES(h_start, 0);
    PUT4BYTES(h_start + WSIZE, PACK(DSIZE, CUR_ALLOC));
    PUT4BYTES(h_start + (2 * WSIZE), PACK(DSIZE, CUR_ALLOC));
    PUT4BYTES(h_start + (3 * WSIZE), PACK(0, PREV_ALLOC | CUR_ALLOC));


    if (extend_heap(CHUNKSIZE) == NULL)
        return -1;

    return 0;
}


/*
 * malloc - Allocate a block by incrementing the brk pointer.
 *      Always allocate a block whose size is a multiple of the alignment.
 */
void *malloc(size_t size)
{

    size_t req_size;             /* Block size after overhead and alignment */
    size_t extend_size;          /* Final size to be extended */
    char *bp;

    if (size <= 0)
        return NULL;

    /* Adding overhead and alignment */
    req_size = (size <= 2*DSIZE) ? MINBLOCK : ALIGN(size + WSIZE);

    /* Find appropriate free list match size */
    if ((bp = find_list(req_size)) != NULL) {
        place(bp, req_size);
        return bp;
    }

    /* If heap is too small, request for more memory */
    extend_size = req_size >= CHUNKSIZE ? req_size : CHUNKSIZE;

    if ((bp = extend_heap(extend_size)) != NULL){
        place(bp, req_size);
        return bp;
    }

    return NULL;
}

/*
 * Free a piece of previous allocated memory
 */
void free(void *ptr)
{
    char *next_header;
    size_t size, next_size;
    size_t prev_alloc, next_alloc;

    if (ptr == NULL)
        return;

    size = GET_SIZE(HDRP(ptr));
    next_header = HDRP(NEXT_BLKP(ptr));
    next_size = GET_SIZE(next_header);

    prev_alloc = GET_PREV_ALLOC(HDRP(ptr));
    next_alloc = GET_ALLOC(next_header);

    /* Update header and footer */
    PUT4BYTES(HDRP(ptr), size | prev_alloc);
    PUT4BYTES(FTRP(ptr), GET4BYTES(HDRP(ptr)));

    /* Update next block's allocation status */
    PUT4BYTES(next_header, next_size | next_alloc);

    /* Insert free block to free list */
    insert_list(ptr, size);

    /* Coalesce to remove false fragmentation */
    coalesce(&ptr);
}

/*
 * Realloc a block of memory to a different size
 */
void *realloc(void *oldptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        free(oldptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(oldptr == NULL) {
        return malloc(size);
    }

    newptr = malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = *SIZE_PTR(oldptr);
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    /* Free the old block. */
    free(oldptr);

    return newptr;
}


/*
 * calloc - Allocate the block and set it to zero.
 */
void *calloc(size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
inline static int aligned(const void *p)
{
    return (size_t) (((size_t) (p) + 7) & ~0x7) == (size_t) p;
}


void get_min_max(int index, int* min, int* max)
{
    switch(index){
    case 0:
        *min = 0;
        *max = 24;
        break;
    case 1:
        *min = 24;
        *max = 48;
        break;
    case 2:
        *min = 48;
        *max = 72;
        break;
    case 3:
        *min = 72;
        *max = 96;
        break;
    case 4:
        *min = 96;
        *max = 120;
        break;
    case 5:
        *min = 120;
        *max = 480;
        break;
    case 6:
        *min = 480;
        *max = 960;
        break;
    case 7:
        *min = 960;
        *max = 1920;
        break;
    case 8:
        *min = 1920;
        *max = 3840;
        break;
    case 9:
        *min = 3840;
        *max = 7680;
        break;
    case 10:
        *min = 7680;
        *max = 15360;
        break;
    case 11:
        *min = 15360;
        *max = 30720;
        break;
    case 12:
        *min = 30720;
        *max = 61440;
        break;
    default:
        *min = 30720;
        *max = ~0;
    }
}


/*
 * checkheap
 */
void mm_checkheap(int verbose)
{
    char *list_head = NULL;
    int min = 0;
    int max = 0;
    int list_index = 0;

    char *start;

    if(verbose == 0){
        return;
    }


    /* Check each block in its boundary */
    for (list_index = 0; list_index < TOTALLIST; list_index++) {

        list_head = (char *) GET((char*)lists + 8*list_index);
        get_min_max(list_index, &min, &max);

        while (list_head != NULL) {
            if (((unsigned)min >= GET_SIZE(HDRP(list_head)) ||
                GET_SIZE(HDRP(list_head)) > (unsigned)max)) {
                printf("Error: %p boundary error\n", list_head);
                return;
            }
            list_head = (char *) GET(SUCCESSOR(list_head));
        }
    }


    /* Check each block whether it is aligned and in heap */
    for(start = (char*)lists + 2 * DSIZE + TOTALLIST * DSIZE;
        (GET4BYTES(HDRP(start)) != 1) && (GET4BYTES(HDRP(start)) != 3);
        start = NEXT_BLKP(start))
    {
        if (!aligned(start)) {
            printf("Error: %p not aligned\n", start);
        }
        if (!in_heap(start)) {
            printf("Error: %p isn't in heap\n", start);
        }
    }


}
