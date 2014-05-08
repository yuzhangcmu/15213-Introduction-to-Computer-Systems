#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

/*this ifdef is to turn the mm_checkheap on and off */
//#define CHECKHEAP
#ifdef CHECKHEAP
# define heapcheck(...) mm_checkheap(__VA_ARGS__)
#else
# define heapcheck(...)
#endif

//void heapcheck(int verbose);

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* This is a segregated list allocater that uses doubly linked explicit
 * lists in each of the size "buckets" of the segregated list.
 *
 * Allocated and free blocks both have 4 byte header and footers bordering
 * the beginning and end of the payload of the block to track the size of
 * those blocks and whether the blocks are allocated. For free blocks,
 * the next and prev pointers are stored at the beginning of the payload,
 * with next at the payload address, and prev after it. Since we only
 * needed to implement a 32 bit heap, 8 bit pointers for the next and prev
 * were unnecessary, and next and prev are stored as unsigned int
 * offsets from the start of the heap. Conversions from int to pointer
 * and pointer to int are handled via macros. This means the minimum size
 * of a block given to a user will be 8 bytes, and the total block size
 * will be 16 bytes. This saves mem space over many thousands of
 * allocations.
 *
 * When a block is stored to the freelist, it is indexed into a specific
 * bucket via a log2 based indexing function. Thus, each bucket size
 * class is exponentially larger than that of the last bucket.
 * When the user wants to allocate a block, malloc will look first in the
 * bucket corresponding to the size class desired by the user, and if that
 * bucket has no blocks which are large enough, malloc will look in all the
 * buckets above until it finds the a sufficiently large free block. While
 * the search strategy is first fit within the buckets, it approximates
 * best fit since it will never look for blocks in a size class below the
 * size requested by the user.
 *
 *
 * When a block is freed it is "delinked" from the bucket list it was in,
 * and since explicit lists are used malloc cannot touch this block until
 * the user wants to free it. When the user frees the block, the alloc bits
 * in the header and footer are first reset to 0, and then malloc attempts
 * to coalesce the block with other blocks already in the free list.
 * If coalescing is possible, the bordering blocks are removed from the
 * list. When coalescing is finished, malloc will place the new block at
 * the head of the appropriate sized bucket.
 *
 *
 */

#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* These macros obtain and set values referred to by pointers in a clean
 * way
 */
#define GET(p) (*(unsigned int *)(p))
#define SET(p, val) (*(unsigned int *)(p) = (val))


/* having a macro for mem_heap_hi is a convenient way to track the top
 * of the heap.
 * heap lo is kept as a global variable to avoid function calls
 * to mem_heap_lo, since the heap's offset is critical to optimization in
 * the 32 bit addressing scheme.
 */

#define HEAP_HI mem_heap_hi()
char *heap_lo;

/* The next and prev pointers of the linkedlist are kept in the payload
 * section of the block of memory as unsigned integers since the heap can
 * be 32 bit addressed. This saves 16 bytes of space over the use
 * of pointers.
 * Since the pointers are stored this way, macros to convert the pointers
 * to ints and back again were needed.
 */

#define PTR_TO_INT(p) ((unsigned int)(p - heap_lo))
#define INT_TO_PTR(i) ((char *)(i + heap_lo))


/* these macros take in a pointer, and mask the value at that pointer into
 * a size or an allocation bit
 */

#define SIZE(p) (GET(p) & ~0x7)
#define ALLOC(p) (GET(p) & 0x1)

/* these macros give pointers for the header and the footer
 * the footer can never be set unless the header is set first
 * since it bases its data off the header
 */
#define HEADER(p) ((char *)(p) - 4)
#define FOOTER(p) ((char *)(p) + SIZE(HEADER(p)) - 8)

/* these macros take in a pointer and return the next and prev pointers
 * doing all necessary int to ptr conversions and arithmetic
 */

#define NEXT(p) (INT_TO_PTR(GET(p)))
#define PREV(p) (INT_TO_PTR(GET(p+4)))

/* a special null checker which determines whether ints convereted to
 * pointers are actually null pointers, in which case they point at
 * the start of the heap
 */
#define NULL_CHECK(p) (!((unsigned long)p & ~((unsigned long)heap_lo)))


/* global variables:
 * lists is an array of pointers to the segregated list buckets
 * begin is the start of the memory area. It is used by checkheap
 * numlists is the number of buckets in the seglist.
 */

char **lists;
char *begin;
unsigned int numlists = 6;

/*
 * index - function takes in a size, then by finding the most
 * significant bit of that size, finds the index of the list
 * corresponding to that size.
 * The rough function for the bucket index is msb/2. However, sizes
 * only start at msb = 2 (size = 8). Additionally, to optimize speed
 * the buckets for sizes below 64 (msb = 3, 4, 5) are lumped together.
 * The same is done for the top end, where all sizes at or above 2^14
 * are lumped together.
 */

inline unsigned int index_of(unsigned int size){

    unsigned int index = 0;
    unsigned int msb;

    /* this assembly (bit shift reverse) is an approximation of log2,
     * and finds the most significant bit of a number.
     */

     __asm__( "bsr %1, %0"
           : "=r" (msb)
           : "r" (size)
           );

    if(msb < 6) index = 0;
    else if(msb > 13) index = 5;
    else index = (msb - 4)/2;

    return index;
}


/*
 * mm_init - Called when a new trace starts. Initializes the allocator
 * data structure
 */

inline int mm_init(void)
{
    unsigned int i = 0;

    //resets the heap ptr
    mem_reset_brk();

    //initializes the array of head pointers
    //////////////////1
    lists = (char **)mem_sbrk(numlists * sizeof(char *));
    begin = (char *)mem_sbrk(4);

    //sets the prologue to 1, then creates the epilogue and sets it to 1
    SET(begin, 1);
    begin = (char *)mem_sbrk(4);
    SET(begin, 1);

    //initializes each index in the array of head pointers
    for(i = 0; i < numlists; i++){
       lists[i] = NULL;
    }

    heap_lo = (char *)mem_heap_lo();

    return 0;
}

/*
 * delink - takes in a prev and next pointer and dissociates them from the
 * nodes they point to, removing the pointer's block from its respective
 * list
 */

inline void delink(char *ptr, char *next, char *prev){

    int index = 0;

    if(!NULL_CHECK(next) && !NULL_CHECK(prev)){
    //Case 1: neither prev or next is null

    //link prev and next together
    SET(prev, PTR_TO_INT(next));
    SET(next + 4, PTR_TO_INT(prev));
    }else if(!NULL_CHECK(next)){
    //Case 2: prev is null

    //set next's prev to null, reset the head of the list
    SET(next + 4, 0);
        index = index_of(SIZE(HEADER(ptr)) - 8);
    lists[index] = next;
    }else if(!NULL_CHECK(prev)){
    //Case 3: next is null

    //set prev's next to null
    SET(prev, 0);
    }else {
    //Case 4: prev and next are null

    //set the ptr's bucket list to null
    index = index_of(SIZE(HEADER(ptr)) - 8);
    lists[index] = NULL;
    }

}

/*
 * place_block - takes a block and places it at the head the appropriate
 * size list
 */

inline void place_block(char *ptr){

    //determines the appropriate list for the ptr
    unsigned int index = index_of(SIZE(HEADER(ptr)) - 8);


    /* if there is nothing in the list, the prev and next of the
     * of the pointer are simply null. Otherwise, the block is pointed
     * the current head of the list, and the head is made to point back
     * The head is then reset to the new block
     */
    if(lists[index] == NULL) {
    lists[index] = ptr;
    SET(lists[index], 0);
    SET(lists[index] + 4, 0);
    }else{
    SET(lists[index] + 4, PTR_TO_INT(ptr));
    SET(ptr, PTR_TO_INT(lists[index]));
    SET(ptr + 4, 0);
    lists[index] = ptr;
    }
}

/*
 * split - takes a pointer, a desired size and an old block size
 * then splits the block in the freelist pointed to by ptr. The smaller
 * split block remains in the freelist, the new block is allocated.
 */

inline void split(size_t size, char *ptr){

    unsigned int oldsize = SIZE(HEADER(ptr));
    unsigned int newsize = size + 8;
    unsigned int splitsize = oldsize - newsize;

    //the pointer for the leftover block
    char *split = ptr + newsize;

    //the taken block must be delinked from the list
    delink(ptr, NEXT(ptr), PREV(ptr));

    //change header and footer of leftover block
    SET(HEADER(split), splitsize);
    SET(FOOTER(split), splitsize);

    //change header and footer of allocated block
    SET(HEADER(ptr), ((newsize)|1));
    SET(FOOTER(ptr), ((newsize)|1));

    //put the leftover block into a new list if necessary
    place_block(split);
}


/*
 * traverse_list - iterates through the given freelist and tries to find a
 * free block of the size requested by the user.
 */

inline char *traverse_list(unsigned int size, unsigned int index){

    unsigned int blocksize = 0;
    unsigned int newsize = size + 8;
    char *ptr = lists[index];

    /* the function starts searching for the size at the list head
     * of the given list.
     */

    if(ptr == NULL) return NULL;

    //will look at the block pointed to by ptr so long as ptr is not null
    while(!NULL_CHECK(ptr)){


    blocksize = SIZE(HEADER(ptr));

    /* if the block in the list is large enough, the function
         * must determine whether to split it, or hand it to the
         * user as is. If splitting the block would not leave a
         * a block of at least 16 bytes, the block isn't split,
         * and its alloc bit is set and it is delinked
         */
    if(blocksize >= newsize){
        if((long)blocksize - (long)newsize - 16 < 0){
            //set header and footer to allocated
            SET(HEADER(ptr), (blocksize|1));
            SET(FOOTER(ptr), (blocksize|1));

            delink(ptr, NEXT(ptr), PREV(ptr));
        SET(ptr, 0);
        SET(ptr + 4, 0);

        }else{
        /* if the block is large enough, the size desired by
                 * the user will be split off */
            split(size, ptr);
        }
        return ptr;
    }else{
        //moves on to the next pointer in the list
        ptr = NEXT(ptr);
        }
    }

return NULL;
}

/*
 * fit_block - wrapper for traverse_list that manages the search
 * through the list buckets. Takes a size from malloc to look for in the
 * size class lists
 */

inline char *fit_block(unsigned int size){
    unsigned int alignsize;
    unsigned int index;

    char *ptr = NULL;

    /* the size is aligned to 8 bits, and the preferred list index for the
     * size is obtained, then traverses the first list to find a block.
     */
    alignsize = ALIGN(size);
    index = index_of(alignsize);

    /* if a block wasn't found in the first list, the fit_block will
     * search the other lists for a sufficiently large block starting
     * with the next size class list. Lower lists are not searched
     */

    ptr = traverse_list(alignsize, index);
    if(ptr != NULL) return ptr;

    while(1){

    while(index < numlists){
        if(lists[index] == NULL) index++;
        else break;
    }
    if(index >= numlists) break;

    ptr = traverse_list(alignsize, index);
    if(ptr != NULL) return ptr;

    index++;
   }

  return NULL;
}

/*
 * malloc - Takes in a size request from the user, then first tries to
 * repurpose a free block within the heap. If there are no blocks of
 * sufficient size, it grows the heap via sbrk, and gives the user the
 * new memory at the end of the heap.
 */
void *malloc(size_t size)
{
    unsigned int *header = NULL;
    unsigned int *footer = NULL;
    void *ptr = NULL;

    unsigned int blocksize = 0;
    unsigned int alignsize = 0;

    /* malloc first tries to find a free block which is large enough to
     * fulfill the user's request. If there is no such block, malloc will
     * prepare to create a new block.
     */

    ptr = fit_block(size);
    if(ptr != NULL) return ptr;

    /* to create a new block, malloc aligns the size of the given to 8
     * bytes, adds 8 bytes for header and footer space, then sbrk's new
     * memory. In doing this, it also erases the previous epilogue, and
     * resets it at the new high byte of the memory.
     */

    alignsize = ALIGN(size);
    blocksize = alignsize + 8;

    SET ((HEAP_HI - 3), 0);
    ptr =  (char *)mem_sbrk(blocksize);
    ptr -= 4;
    SET ((HEAP_HI - 3), 1);

    // setting header and footer of the newly created block
    header = (unsigned int *)ptr;
    footer = (unsigned int *)(ptr + alignsize + 4);

    SET(header, (blocksize|1));
    SET(footer, (blocksize|1));

    if ((long)ptr < 0)
    return NULL;
    else {

    //pointing p at the payload
    ptr += 4;

    return ptr;
    }
}

/*
 * Coalesce - takes in a pointer and determines if bordering blocks are
 * allocated, and if they are not removes them from the corresponding
 * lists and merges them into a new block.
 *
 */

inline void *coalesce(void *ptr){

    /* to determine how we coalesce, we must first
     * determine the allocation status of the next and previous
     * adjacent blocks within the heap
     */

    unsigned int prev_alloc = ALLOC(ptr - 8);
    unsigned int next_alloc = ALLOC(FOOTER(ptr) + 4);

    /* depending on what case of coalesce were in, we need
     * the size of the next and prev block, and we will need to give
     * the new block a size as well.
     */
    unsigned int size = SIZE(HEADER(ptr));
    unsigned int newsize = 0;

    unsigned int prev_size = 0;
    unsigned int next_size = 0;

    /* To delink the adjacent blocks from their lists, we'll
     * need the pointers associated with them. Initially they won't be
     * set to their corresponding values so as to avoid derefencing
     * any null pointers at the edge of any lists.
     */

    char *prev_adj_next = NULL;
    char *prev_adj_prev = NULL;

    char *next_adj_next = NULL;
    char *next_adj_prev = NULL;

    char *prev_adj = NULL;
    char *next_adj = NULL;

    //case 1: both blocks are allocated, nothing to do
    if(prev_alloc && next_alloc) return ptr;

    else if(!prev_alloc && next_alloc){
    //case2: prev adjacent block is freed, next block is allocated

        /* gets previous block's size, as well as it's
         * next and prev pointers. The adjacent block is
         * then delinked from the list
         */
        prev_size = SIZE(ptr - 8);
    prev_adj = ptr - prev_size;
        prev_adj_next = NEXT(ptr - prev_size);
    prev_adj_prev = PREV(ptr - prev_size);

    delink(prev_adj, prev_adj_next, prev_adj_prev);

    /* once the block is delinked, the two blocks are merged.
         * the previous block is now the beginning of the entire
         * new block, and the pointer must be moved there
         * the new size is calculated, and the header and footer
         * are set to that size
         */
    ptr -= prev_size;
    newsize = size + prev_size;
    SET(HEADER(ptr), newsize);
    SET(FOOTER(ptr), newsize);

    return ptr;

    }

    else if(prev_alloc && !next_alloc){
    //case3: prev adjacent block is allocated, next block is freed

    /* This case is very similar to the previous one.
         * We must get the size of the next block, get its next pointers,
         * remove it from the list, then merge the blocks.
         *
     * The difference here is that the pointer to the payload
         * stays the same since the block being freed is the
         * start of the new coalesced block
         */

    next_size = SIZE(ptr + size - 4);
    next_adj = ptr + size;
        next_adj_next = NEXT(next_adj);
        next_adj_prev = PREV(next_adj);

    delink(next_adj, next_adj_next, next_adj_prev);

    newsize = size + next_size;
        SET(HEADER(ptr), newsize);
        SET(FOOTER(ptr), newsize);

    return ptr;
    }

    else{
    //case 4: both bordering blocks are freed

    /* When both blocks are freed, we must obtain the information
         * of both the next and prev blocks
         */

    prev_size = SIZE(ptr - 8);
    next_size = SIZE(ptr + size - 4);

    next_adj = ptr + size;
    prev_adj = ptr - prev_size;

    next_adj_next = NEXT(next_adj);
    next_adj_prev = PREV(next_adj);

    prev_adj_next = NEXT(prev_adj);
    prev_adj_prev = PREV(prev_adj);


    /* Since we are dealing with two blocks, when
         * we delink the blocks, we must account for the case
         * the blocks may be pointing to each other.
         * If this is the case, both blocks are removed from the
         * list at the same time by linking the previously unlinked
         * blocks which pointed to the blocks we are now coalescing.
         * However, if the blocks do not point to each other, we can
         * delink them independently
         */
    if(next_adj == prev_adj_next){
        delink(next_adj, next_adj_next, prev_adj_prev);

    }else if(prev_adj == next_adj_next){
        delink(prev_adj, prev_adj_next, next_adj_prev);

    }else{
        delink(next_adj, next_adj_next, next_adj_prev);
        delink(prev_adj, prev_adj_next, prev_adj_prev);
        }

    /* The size of the new block is the size of all three blocks
         * combined. The new pointer will point at what was the previous
         * adjacent block, since that is now at the beginning of the new
         * block.
         */

    newsize = size + prev_size + next_size;
    ptr -= prev_size;
    SET(HEADER(ptr), newsize);
    SET(FOOTER(ptr), newsize);
    return ptr;
   }
   return ptr;
}

/*
 * free - takes a pointer from the user, resets the allocation bits,
 * attempts to coalesce the block with other freed blocks. Once coalesce
 * has been attempted, the block is placed into a list
 */

void free(void *ptr){
    unsigned int size = 0;

    //checks to see whether the pointer given is within the heap
    if((ptr < (void *)heap_lo) || (ptr > HEAP_HI)) return;

    /* gets the data stored in the header,
     * resets the data so that includes only
     * the 8 byte aligned size, zero-ing the alloc
     * bit
     */
    size = GET(HEADER(ptr));
    size = size & ~0x7;
    SET(HEADER(ptr), size);
    SET(FOOTER(ptr), size);

    ptr = coalesce(ptr);
    place_block(ptr);
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  I'm too lazy
 *      to do better.
 */
void *realloc(void *oldptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /*p If size == 0 then this is just free, and we return NULL. */
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
    oldsize = *(unsigned int *)(oldptr - 4);
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    /* Free the old block. */
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
 * mm_checkheap
 *
 * invariants:
 * -epilogue and prologue are always set to 1
 * -pointers are always 8 byte aligned
 * -pointers always point within the heap
 * -next and prev pointers always point within the heap
 * -the next's prev points back to original pointer
 * -the block is in the appropriate size list
 * -the header always equals the footer
 * -the footer points to within the heap
 * -the minimum block size is 16 bytes
 * -the alloc bit is 0 if in the free list
 * -the number of nodes in each freelist is equal to the number of 0'ed
 * alloc bits corresponding to that size class
 *
 * The heaper checker will check the invariants of the free list first,
 * starting at the head of each list, then iterating until the end
 * of the list. Then it checkes non freelist invariants by using the size
 * contained within the header of the current pointer to find the next
 * pointer.
 * Each invariant has its own error code associated with it, should the
 * heap fail a check, the heap checker will jump to the switch statement
 * of error codes and print the relevant information. The int passed into
 * the heap checker serves to locate the function call where the heap's
 * consistency check failed.
 */
void mm_checkheap(int verbose){
    char *ptr = NULL;
    unsigned int i = 0;
    unsigned int loc = 0;
    int err_code = 0;

    ptr = begin + 4;

    if(GET(heap_lo + (numlists*8)) != 1){
    err_code = 13;
    goto err;
    }
    if(GET(HEAP_HI - 3) != 1){
    err_code = 14;
    goto err;
    }

    loc = 0;
    for(i = 0; i < numlists; i++){
    ptr = lists[i];
    if(ptr == NULL) ptr = heap_lo;
    loc = 0;
        while(!NULL_CHECK(ptr)){

        loc++;
        if((ptr > (char *)HEAP_HI) ||
        (ptr <= (char *)heap_lo)) err_code = 10;
        else if ((long)ptr%8 != 0) err_code = 5;
        else if (index_of(SIZE(HEADER(ptr)) - 8) != i) err_code = 15;
        else if (ALLOC(HEADER(ptr)) != 0) err_code = 6;
        else if (ALLOC(FOOTER(ptr)) != 0) err_code = 7;
        else if ((NEXT(ptr) > (char *)HEAP_HI) ||
            (NEXT(ptr) < heap_lo)) err_code = 11;

        else if ((PREV(ptr) > (char *)HEAP_HI) ||
            (PREV(ptr) < heap_lo)) err_code = 12;

        else if (!NULL_CHECK(NEXT(ptr))){
            if(ptr != PREV(NEXT(ptr))) err_code = 9;
        }

        if(err_code != 0) goto err;
        ptr = (NEXT(ptr));
        }
    }

   loc = 0;
   ptr = begin + 4;

    while(ptr != NULL){
        loc++;
    if(((void *)ptr > HEAP_HI) || (ptr < heap_lo)) err_code = 10;
    else if ((void *)FOOTER(ptr) > HEAP_HI) err_code = 4;
    else if((long)ptr%8 != 0) err_code = 5;
    else if (SIZE(HEADER(ptr)) != SIZE(FOOTER(ptr))) err_code = 2;
    else if (SIZE(HEADER(ptr)) < 16) err_code = 3;

    if(err_code != 0) goto err;
    ptr += SIZE(HEADER(ptr));
    if(ptr + 4 > (char *)HEAP_HI) break;
    }

    if(err_code == 0) {
    goto ret;
    }

    err: dbg_printf("----------------HEAP CORRUPTED-------------------\n");
    dbg_printf("corrupted at: ");

    switch(verbose){
    case(0):
        dbg_printf("split\n");
        break;
    case(1):
        dbg_printf("traverse list\n");
        break;
    case(2):
        dbg_printf("malloc\n");
        break;
    case(3):
        dbg_printf("coalesce\n");
        break;
    case(4):
        dbg_printf("free\n");
        break;
    case(5):
        dbg_printf("place_block\n");
        break;
    case(6):
        dbg_printf("delink\n");
        break;
    default:
        dbg_printf("no info given\n");
    }

    switch(err_code){
    case(0): break;
    case(1):
    case(2):
       dbg_printf("header and footer not equal\n");
       break;
    case(3):
       dbg_printf("size below minimum\n");
       break;
    case(4):
       dbg_printf("footer beyond heap\n");
       break;
    case(5):
       dbg_printf("ptr not aligned to 8 bytes\n");
       break;
    case(6):
    case(7):
       dbg_printf("alloc bit set\n");
       break;
    case(8):
       dbg_printf("null pointer in the middle of list\n");
       break;
    case(9):
       dbg_printf("next's prev does not point back to ptr\n");
       break;
    case(10):
       dbg_printf("ptr out of bounds of heap\n");
       break;
    case(11):
       dbg_printf("next pointer out of bounds of heap\n");
       break;
    case(12):
       dbg_printf("prev pointer out of bounds of heap\n");
       break;
    case(13):
       dbg_printf("prologue not set to allocated\n");
       break;
    case(14):
       dbg_printf("epilogue not set to allocated\n");
       break;
    case(15):
       dbg_printf("block is not in the correct size list\n");
       break;
    }
    dbg_printf("ptr: %lx\n", (unsigned long)ptr);
    dbg_printf("heap hi: %lx\n", (unsigned long)HEAP_HI);
    dbg_printf("header p: %lx\n", (unsigned long)HEADER(ptr));
    dbg_printf("header size: %u\n", SIZE(HEADER(ptr)));
    dbg_printf("header alloc: %u\n", ALLOC(HEADER(ptr)));
    dbg_printf("footer p: %lx\n", (unsigned long)FOOTER(ptr));
    dbg_printf("footer size: %u\n", SIZE(FOOTER(ptr)));
    dbg_printf("footer alloc: %u\n", ALLOC(FOOTER(ptr)));
    dbg_printf("next: %lx\n", (unsigned long)NEXT(ptr));
    dbg_printf("prev: %lx\n", (unsigned long)PREV(ptr));
    dbg_printf("list: %u\n", i);
    dbg_printf("list loc: %d\n", loc);
    dbg_printf("list_head: %lx\n\n", (unsigned long)lists[i]);

    ret: return;

}
