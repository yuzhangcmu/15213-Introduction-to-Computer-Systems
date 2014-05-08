/*
 * mm.c
 *
 * Uses a explicit free list implementation where free blocks are in doubly-linked list,
 * with (next and previous) pointers to adjacent blocks.
 * Free and allocated blocks have headers and footers, size determined by WSIZE.
 * Free blocks are allocated using a first-fit method, where there are also restrictions
 * on the number of operations that can occur while searching the LL.
 * Thus, this allocator is not efficient if many (>300) blocks are freed, and then
 * many allocations are requested.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
 
#include "mm.h"
#include "memlib.h"
 

 
 
/* Basic constants and macros */
#define WSIZE 4       /* Word and header/footer size (bytes) */
#define DSIZE 8       /* Doubleword size (bytes) */
#define MIN_BLK_SIZE 24
 
#define MAX(x, y) ((x) > (y)? (x) : (y))  
 
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))
 
/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))          
#define PUT(p, val)  (*(unsigned int *)(p) = (val))
 
/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                  
#define GET_ALLOC(p) (GET(p) & 0x1)            
 
/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) (bp - WSIZE)
#define FTRP(bp) (bp + GET_SIZE(HDRP(bp)))
 
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((bp) + GET_SIZE(HDRP(bp)) + 2*WSIZE)
#define PREV_BLKP(bp) (bp - GET_SIZE(bp-DSIZE) - 2*WSIZE)
 
#define GET_NEXT(bp)            (*(void **)(bp + DSIZE))
#define GET_PREV(bp)            (*(void **)bp)
#define SET_NEXT(bp, ptr)       (GET_NEXT(bp) = ptr)
#define SET_PREV(bp, ptr)       (GET_PREV(bp) = ptr)
 
#define ALIGN(p) (((size_t)(p) + (7)) & ~(0x7))
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))
 
/* Head of free list */
void* free_list = NULL;
 
/* Function prototypes for internal helper routines*/
static int in_heap(const void *p);
static inline void coalesce(void *ptr);
void mm_check(int verbose);
static inline void *splitter(void *ptr, size_t newsize);
static inline void add(void *ptr);
static inline void delete(void *ptr);
 
/*
 * mm_init -
 * Allocate a padding of 4 bytes so that header blocks are aligned to 8 bytes
 * return -1 if not successful and return 0 successful.
 */
int mm_init(void) {
        void * heap_bottom = mem_heap_lo();
        free_list = NULL;
 
        if ((heap_bottom = mem_sbrk(2*WSIZE)) == (void *)-1) {
                return -1;
        }
        PUT(heap_bottom, PACK(0,1));                             //alignment
        PUT((char *)heap_bottom + WSIZE, PACK(0,1)); //alignment
       
        return 0;
}
 
/*
 * mm_malloc - Allocate using a explicit free list implementation.
 *     Use a first-fit policy to allocate a block of space, using splitting if necessary.
 *     Splitting a free block is done so that the free block precedes the allocated block.
 *     Allocate a new block if no suitable block is found, return NULL if out of memory
 */
void *mm_malloc(size_t size) { 
        if (size <= 0) {
                return NULL;
        }
        unsigned int newsize;
        void *list = free_list;
        int i = 0;
        unsigned int tempsize = 0;
       
        // Adjust size
        if (size <= 4*DSIZE) {
                newsize = 4*DSIZE;
        }
        else {
                newsize = ALIGN(size);
        }      
       
        // search free list for first free block for size newsize
        while (list != NULL && i < 300) {
                tempsize = GET_SIZE(HDRP(list));
                if (tempsize >= newsize) {
                        if (tempsize >= newsize + 32) {
                                return splitter(list, newsize);
                        }                      
                        delete(list);
                        PUT(HDRP(list), PACK(tempsize, 1));
                        PUT(FTRP(list), PACK(tempsize, 1));
                        return list;
                } else {
                        list = GET_NEXT(list); //8 byte pointers -> next block;
                }
                i++;
        }
       
        // If no block is available or found in number of allowed ops, allocate more memory in heap
        list = mem_sbrk(newsize + 2*WSIZE);
       
        // if out of memory, return NULL
        if ((long)list == -1) {
                return NULL;
        }
        // otherwise, allocate memory and update epilogue
        PUT(HDRP(list), PACK(newsize, 1));
        PUT(FTRP(list), PACK(newsize, 1));
        PUT(FTRP(list) + WSIZE, PACK(0, 1));
        return list;
}
 
/*
 * Splitter - splits block so that we are allocating minimally
 * Helps so that we allocate only the appropriate block size.
 * Takes pointer to current block and size needed.
 * Finds newsize for block, places that block into the heap, and returns the pointer.
 */
static inline void* splitter(void *ptr, size_t newsize) {
        int newfreesize = GET_SIZE(HDRP(ptr)) - newsize - 2*WSIZE;
 
        PUT(HDRP(ptr), PACK(newfreesize, 0));
        PUT(FTRP(ptr), PACK(newfreesize, 0));
        void *p = NEXT_BLKP(ptr);
        PUT(HDRP(p), PACK(newsize, 1));
        PUT(FTRP(p), PACK(newsize, 1));
 
        return p;
}
 
/*     
 * Coalesce
 * Done to minimize internal fragmentation.
 * Four Cases:
 * 1. Nearby blocks are both allocated. Simply add pointer of current block. No coalescing done.
 * 2. Previous block is allocated, next block is free. Update size of current block, delete the ptr of next block,
 *    and then add ptr of newly sized block to the list.
 * 3. Next block is allocated, previous is free. Update size of previous block.
 * 4. Nearby blocks are both free. Update size of previous block, delete ptr of next block.
 */
static inline void coalesce(void *ptr) {
        size_t next_alloc = GET_ALLOC((char *)(FTRP(ptr)) + WSIZE);
        size_t prev_alloc = GET_ALLOC((char *)(ptr) - DSIZE);
        size_t size = GET_SIZE(HDRP(ptr));
 
        // case 1
        if (prev_alloc && next_alloc) {
                add(ptr);
        }
        // case 2
        else if (prev_alloc && !next_alloc) {  
                size += GET_SIZE(HDRP(NEXT_BLKP(ptr))) + 2*WSIZE;
                delete(NEXT_BLKP(ptr));
                PUT(HDRP(ptr), PACK(size, 0));
                PUT(FTRP(ptr), PACK(size, 0));
                add(ptr);
        }
        // case 3
        else if (!prev_alloc && next_alloc) {
                ptr = PREV_BLKP(ptr);
                size += GET_SIZE(HDRP(ptr)) + 2*WSIZE;
                PUT(HDRP(ptr), PACK(size, 0));
                PUT(FTRP(ptr), PACK(size, 0));
        }
        // case 4
        else {
                void * prev = PREV_BLKP(ptr);
                void * next = NEXT_BLKP(ptr);          
                size += GET_SIZE(HDRP(prev)) + GET_SIZE(HDRP(next)) + 4*WSIZE;
                PUT(HDRP(prev), PACK(size, 0));
                PUT(FTRP(prev), PACK(size, 0));
                delete(next);                          
        }
}
 
// check if the pointer is in the heap and not in the stack in mm_check
static int in_heap(const void *p) {
        return p <= mem_heap_hi() && p >= mem_heap_lo();
}
 
/*
 * mm_free - Freeing a block.
 * Add to free list if necessary.
 * Coalesce if there are blocks in the free list
 */
void mm_free(void *ptr){
        if(ptr == 0){
                return;
        }
        size_t size = GET_SIZE(HDRP(ptr));
       
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
 
        if(free_list != NULL){
                coalesce(ptr);
        } else {
                add(ptr);
        }
}
 
/* Add - add the specific block ptr to the free list, making it the first element in the list */
static inline void add(void *ptr) {
        void *head = free_list;
        SET_NEXT(ptr, head);
        SET_PREV(ptr, NULL);
        if (head != NULL)
                SET_PREV(head, ptr);
        free_list = ptr;
}
 
/*
 * Delete - remove specific block ptr from free list
 * If we are trying to remove the ptr that is at the front of the list, then
 * update the front of the list to be the next block ptr.
 */
static inline void delete(void *ptr) {
        void *next = GET_NEXT(ptr);
        void *prev = GET_PREV(ptr);
       
        // Front of the list
        if (prev == NULL) {
                free_list = next;
                if (next != NULL) {
                        SET_PREV(next, NULL);
                }
        }
        else {
                SET_NEXT(prev, next);
                if (next != NULL) {
                        SET_PREV(next, prev);
                }
        }
}
 
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
    size_t oldsize;
    void *newptr;
 
    // If size == 0 then call mm_free, and return NULL
    if (size == 0) {
                mm_free(ptr);
                return 0;
    }
 
    // If oldptr is NULL, then this is just malloc
    if (ptr == NULL) {
                return mm_malloc(size);
    }
 
        newptr = mm_malloc(size);
 
    // If realloc() fails the original block is left untouched
    if (!newptr) {
                return 0;
    }
 
    // Copy the old data.
    oldsize = GET_SIZE(HDRP(ptr));
    if (size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);
 
    // Free the old block
    mm_free(ptr);
 
    return newptr;
}
 
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}
 
/*
 * mm_check
 * Checks:
 * 1. if blocks are aligned.
 * 1. if it is in the heap.
 * 2. if the list is aligned.
 * 3. Print the address of that block.
 * 4. Print the previous and next free block.
 * 5. Get next block in free list.
 * 6. Print # of blocks in linked list.
 */
void mm_check(int verbose) {
        int n = 1;     
        void *list = free_list;
       
        printf("Check explicit list\n");       
        if (!verbose) {
                while (list != NULL) {
                        list = GET_NEXT(list);
                }
                printf("\nExplicit List has no seg faults.\n");
                return;
        }
        else {
                while (list != NULL) {
                        printf("Block #%d\n", n);
                       
                        if (in_heap(list)) {
                                printf("Pointer in the heap \n");
                        }
                        else {
                                printf("ERROR:The pointer is not in heap. Exit\n");
                                exit(0);
                        }
                       
                        if (aligned(list)) {
                                printf("Block is aligned\n");
                        }
                        else {
                                printf("Block not aligned\n");
                        }
                        printf("Address of pointer %p \n", list);
                        printf("Address in Prev pointer %p \n", GET_PREV(list));
                        printf("Address in Next pointer %p \n", GET_NEXT(list));
                       
                        list = GET_NEXT(list);
                        n++;
                }
                printf("\nNumber of blocks: %d with no segfaults\n", n);
        }
}

void mm_checkheap(int verbose){
    
}