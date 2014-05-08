/*
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  Blocks are never coalesced or reused.  The size of
 * a block is found at the first aligned word before the block (we need
 * it for realloc).
 *
 * This code is correct and blazingly fast, but very bad usage-wise since
 * it never frees anything.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
//#define DEBUG
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
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// Align version of size of size_t
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))

//================================================================
// Additional macros
/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst
                            // Initial heap size(bytes)

#define OVERHEAD 8      // overhead of header and footer

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
 // For header or footer
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
// convert p from void* to unsigned int*
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put

/* Read the size and allocated fields from header address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp

/* Given block ptr bp, compute address (bp) of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //line:vm:mm:prevblkp
/* $end mallocmacros */

#define BLK_HDR_SIZE ALIGN(sizeof(BlkHdr))

// Given block header address, calculate real payload address
#define GET_BLK_PAYLOAD_ADDR(bp)    ((char*)bp + BLK_HDR_SIZE)

typedef struct header{
    size_t size;
    struct header* next;
    struct header* prev;
}BlkHdr;

void* find_fit(size_t size);
void print_heap();

//================================================================


/*
 * mm_init - Called when a new trace starts.
 */
int mm_init(void)
{
    BlkHdr* p = mem_sbrk(BLK_HDR_SIZE);
    dbg_printf("mm_init p:%p\n", p);
    dbg_printf("mem_heap_lo: %p\n", mem_heap_lo());
    p->size = BLK_HDR_SIZE;
    p->next = p;
    p->prev = p;
    return 0;
}

/*
 * malloc - Allocate a block by incrementing the brk pointer.
 *      Always allocate a block whose size is a multiple of the alignment.
 */
void *malloc(size_t size)
{
//    print_heap();
//    exit(0);
    int newsize = ALIGN(BLK_HDR_SIZE + size);
    BlkHdr *bp = find_fit(newsize);    // Find an empty space, return block header address
    //dbg_printf("malloc %u => %p\n", size, p);

    if(bp == NULL){     // No more space in current free list
        bp = mem_sbrk(newsize);
        if ((long)bp < 0)     // No more space in memory
            return NULL;
        else {
            // Allocate this block of memory
            bp->size = newsize;
            PACK(bp->size, 1);  // Mark as allocated
        }
    }
    else{
//        bp->size |= 1;      // Mark as allocated
        PACK(bp->size, 1);
        // Remove from free list
        bp->prev->next = bp->next;
        bp->next->prev = bp->prev;
    }

//    return (char*)bp + BLK_HDR_SIZE;       // Address to the real payload
    return GET_BLK_PAYLOAD_ADDR(bp);
}



/*
 * free - We don't know how to free a block.  So we ignore this call.
 *      Computers have big memories; surely it won't be a problem.
 */
// ptr: payload pointer
void free(void *ptr){
    // Get block pointer from payload pointer
    BlkHdr* bp = ptr - BLK_HDR_SIZE,
          * head = mem_heap_lo();
    bp->size &= ~1;     // Mark free block

    bp->next = head->next;
    bp->prev = head;
    head->next = bp;
    bp->next->prev = bp;
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  I'm too lazy
 *      to do better.
 */
void *realloc(void *oldptr, size_t size)
{
    BlkHdr* bp = oldptr - BLK_HDR_SIZE;
    void* newptr = malloc(size);
    if(newptr == NULL)  return NULL;

    int copySize = bp->size - BLK_HDR_SIZE;
    if(size < copySize) copySize = size;
    memcpy(newptr, oldptr, copySize);
    free(oldptr);
    return newptr;
}

/*
 * calloc - Allocate the block and set it to zero.
 */
void *calloc (size_t nmemb, size_t size)
{
  size_t bytes = nmemb * size;
  void *newptr;

  newptr = malloc(bytes);
  memset(newptr, 0, bytes);

  return newptr;
}

/*
 * mm_checkheap - There are no bugs in my code, so I don't need to check,
 *      so nah!
 */
void mm_checkheap(int verbose){
    /*Get gcc to be quiet. */
    verbose = verbose;
}


void* find_fit(size_t size)
{
    BlkHdr* p;
    for(p=((BlkHdr*)mem_heap_lo())->next;
        p!=mem_heap_lo() && p->size<size;
        p = p->next);

    // Now we find available space
    if(p != mem_heap_lo())  return p;
    else    return NULL;
}


void print_heap()
{
    BlkHdr* bp = (BlkHdr*)mem_heap_lo();

    dbg_printf("***********************************\n");
    while(bp < (BlkHdr*)mem_heap_hi()){
        dbg_printf("%s block at %p, size %d\n",
               (bp->size & 1) ? "allocated" : "free",
               bp,
               (int)(bp->size & ~1));
        bp = (BlkHdr*)((char*)bp + (bp->size & ~1));
//        bp = NEXT_BLKP(bp);   // ???
    }
}
