/*

   DYNAMIC MEMORY ALLOCATOR
   ------------------------
   This is a dynamic memory allocator that manages the computer memory
   by creating an array of contiguous allocated and free blocks on the
   heap.

   When the user makes a malloc request, the memory allocator searches
   through the free lists(segregated) to determine which one to start
   looking at first based on the requested size and continuing onward
   with lists that have bigger size restrictions(LIST_LIMIT). If a free
   block is found, a block pointer pointing to the payload of that block
   is returned. If no free block is found in the list, memory will be
   extended(Extend_heap) increasing the current heap size. In case, the
   assigned free block is larger than the minimum free block size and
   if the remaining size after assigning this block is larger than
   the minimum free block size(24 in my implementation), then splitting
   occurs. The footer, previous(predecessor) and next(successor) pointers
   are removed and header of the allocated block will be updated
   accordingly.

   Implementation with Segregated free list and LRU:
   ------------------------------------------------
   Free blocks list: Implemented using segregated lists.The free blocks
   are arranged in many linked lists that only contain blocks less than
   fixed sizes.

   Headers: Both allocated and free blocks have headers that indicate
   the size of the blocks, current block's allocation status, and
   previous block's allocation status.

   Footers: Used only for coalescing of free blocks. Footers are
   identical to headers. They are included at the end of each FREE BLOCK
   for possible coalescing. Also, pointers to the previous(predecessor)
   and next(successor) free block in the segregated list are included
   in each free block's payload.

   Coalescing: Coalescing is performed to possibly create a block of a
   larger size to reduce external fragmentation. Calling "free" or heap
   extention casues the addition of a free block to the free list. The
   blocks to be coalesced will be first removed from the their list(one
   of the segregated). Then, they are joined based on 4 cases to form
   a bigger block and adding it to a free list(possibly different).

   When there are free requests, the memory allocator determines the size
   of the freed block and adds (links) it to the appropriate linked list
   by updating the header, adding the footer, and previous and next
   pointers to free blocks in the free list.

 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

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
#endif				/* def DRIVER */



/* Constants */
#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE   168
#define OVERHEAD    8
#define MINBLOCK    24

/* For storing in lower 3 bits of header in allocated blocks
    and header & footer in free blocks */
#define CURRENTALLOCATED   1
#define PREVIOUSALLOCATED  2

/* Variables used as offsets for
   segregated lists headers */
#define SEGLIST1     0
#define SEGLIST2     8
#define SEGLIST3     16
#define SEGLIST4     24
#define SEGLIST5     32
#define SEGLIST6     40
#define SEGLIST7     48
#define SEGLIST8     56
#define SEGLIST9     64
#define SEGLIST10    72
#define SEGLIST11    80
#define SEGLIST12    88
#define SEGLIST13    96
#define SEGLIST14    104


/* Maximum size limit of each list */
#define LIST1_LIMIT      24
#define LIST2_LIMIT      48
#define LIST3_LIMIT      72
#define LIST4_LIMIT      96
#define LIST5_LIMIT      120
#define LIST6_LIMIT      480
#define LIST7_LIMIT      960
#define LIST8_LIMIT      1920
#define LIST9_LIMIT      3840
#define LIST10_LIMIT     7680
#define LIST11_LIMIT     15360
#define LIST12_LIMIT     30720
#define LIST13_LIMIT     61440


/* Total number of segregated lists */
#define TOTALLIST   14


/* Function macros */
#define MAX(x, y)   ((x) > (y) ? (x) : (y))
#define MIN(x, y)   ((x) < (y) ? (x) : (y))


/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write 8 bytes at address p */
#define GET(p)      (*((size_t*) (p)))
#define PUT(p, val) (*((size_t*) (p)) = (val))

/* Read and write 4 bytes at address p */
#define GET4BYTES(p)     (*((unsigned*) (p)))
#define PUT4BYTES(p, val)(*((unsigned*) (p)) = (val))


/* Read the size and allocated fields from address p */
// size include header, footer, and payload
// p: address at header or footer
#define GET_SIZE(p)         (GET4BYTES(p) & ~0x7)
#define GET_ALLOC(p)        (GET4BYTES(p) & 0x1)

/* Read whether previous block is allocated or not*/
#define GET_PREV_ALLOC(p)   (GET4BYTES(p) & 0x2)

/* Given block ptr bp, compute address of its header and footer */
// bp points to payload
#define HDRP(bp)    ((char*)(bp) - WSIZE)
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and prev blocks */
#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))
#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

/* Given free block addr bp, compute addresses in which the previous
    and next free blocks' addresses are stored */
#define SUCCESSOR(bp)   ((char*) ((char*)(bp)))
#define PREDECESSOR(bp)   ((char*) ((char*)(bp) + DSIZE))


/* Helper functions */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);

static void *find(size_t sizeatstart, size_t asize);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static void addingtoseglist(char *bp, size_t size);
static void removefromseglist(char *bp, size_t size);


/* Global variables-- Base of the heap(heap_listp) */
static char *heap_listp;
char *h_start;


/*
 * Function name: Addingtoseglist
 * Description: Adds free block to the appropriate segregated list
 *              by adding it to the front of the list as head &
 *              and linking it to the previous head
 */
static void addingtoseglist(char *bp, size_t size)
{
    /* Address of head of a particular list */
    char *listhead;

    /* Address pointing to the address of head of a particular list */
    char *segstart;


    if (size <= LIST1_LIMIT) {
        segstart = heap_listp + SEGLIST1;
        listhead = (char *) GET(segstart);  // Read 8 bytes from segstart to listhead

    } else if (size <= LIST2_LIMIT) {
        segstart = heap_listp + SEGLIST2;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST3_LIMIT) {
        segstart = heap_listp + SEGLIST3;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST4_LIMIT) {
        segstart = heap_listp + SEGLIST4;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST5_LIMIT) {
        segstart = heap_listp + SEGLIST5;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST6_LIMIT) {
        segstart = heap_listp + SEGLIST6;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST7_LIMIT) {
        segstart = heap_listp + SEGLIST7;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST8_LIMIT) {
        segstart = heap_listp + SEGLIST8;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST9_LIMIT) {
        segstart = heap_listp + SEGLIST9;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST10_LIMIT) {
        segstart = heap_listp + SEGLIST10;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST11_LIMIT) {
        segstart = heap_listp + SEGLIST11;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST12_LIMIT) {
        segstart = heap_listp + SEGLIST12;
        listhead = (char *) GET(segstart);

    } else if (size <= LIST13_LIMIT) {
        segstart = heap_listp + SEGLIST13;
        listhead = (char *) GET(segstart);

    } else {
        segstart = heap_listp + SEGLIST14;
        listhead = (char *) GET(segstart);

    }


    /* If there are blocks in the size list */
    if (listhead != NULL) {

        /* Set the current block as head */
        PUT(segstart, (size_t) bp);

        /* Set the current free block's previous pointer to NULL */
        PUT(PREDECESSOR(bp), (size_t) NULL);

        /* set current free block's next pointer to previous head */
        PUT(SUCCESSOR(bp), (size_t) listhead);

        /* Set the previous head's previous pointer to
           current free block */
        PUT(PREDECESSOR(listhead), (size_t) bp);

    }
    /* If there is no block in that size list */
    else {
        /* Set the current block as head */
        PUT(segstart, (size_t) bp);
        /* Set the free block's next and prev free block
           addresses to NULL */
        PUT(SUCCESSOR(bp), (size_t) NULL);
        PUT(PREDECESSOR(bp), (size_t) NULL);

    }


}


/*
 * Function: removefromseglist
 * Description:
   Removes free block from the appropriate segregated list
   by linking blocks to the left or right of it.  possibly
   updating initial list pointer to head of list.
 */

static void removefromseglist(char *bp, size_t size)
{
    /* Previous block address */
    char *nextaddress = (char *) GET(SUCCESSOR(bp));
    /* Next block address */
    char *prevaddress = (char *) GET(PREDECESSOR(bp));

    // When bp is at listhead: make the succeeding block to be listhead
    if (prevaddress == NULL && nextaddress != NULL) {

        /* Update head pointer of segregated list */
        if (size <= LIST1_LIMIT)
            PUT(heap_listp + SEGLIST1, (size_t) nextaddress);
        else if (size <= LIST2_LIMIT)
            PUT(heap_listp + SEGLIST2, (size_t) nextaddress);
        else if (size <= LIST3_LIMIT)
            PUT(heap_listp + SEGLIST3, (size_t) nextaddress);
        else if (size <= LIST4_LIMIT)
            PUT(heap_listp + SEGLIST4, (size_t) nextaddress);
        else if (size <= LIST5_LIMIT)
            PUT(heap_listp + SEGLIST5, (size_t) nextaddress);
        else if (size <= LIST6_LIMIT)
            PUT(heap_listp + SEGLIST6, (size_t) nextaddress);
        else if (size <= LIST7_LIMIT)
            PUT(heap_listp + SEGLIST7, (size_t) nextaddress);
        else if (size <= LIST8_LIMIT)
            PUT(heap_listp + SEGLIST8, (size_t) nextaddress);
        else if (size <= LIST9_LIMIT)
            PUT(heap_listp + SEGLIST9, (size_t) nextaddress);
        else if (size <= LIST10_LIMIT)
            PUT(heap_listp + SEGLIST10, (size_t) nextaddress);
        else if (size <= LIST11_LIMIT)
            PUT(heap_listp + SEGLIST11, (size_t) nextaddress);
        else if (size <= LIST12_LIMIT)
            PUT(heap_listp + SEGLIST12, (size_t) nextaddress);
        else if (size <= LIST13_LIMIT)
            PUT(heap_listp + SEGLIST13, (size_t) nextaddress);
        else
            PUT(heap_listp + SEGLIST14, (size_t) nextaddress);

        /* Update the new head block's previous to NULL */
        PUT(PREDECESSOR(nextaddress), (size_t) NULL);
    }

    /* If bp is the only free block in list, update head pointer to NULL */
    else if (prevaddress == NULL && nextaddress == NULL) {

        if (size <= LIST1_LIMIT)
            PUT(heap_listp + SEGLIST1, (size_t) NULL);
        else if (size <= LIST2_LIMIT)
            PUT(heap_listp + SEGLIST2, (size_t) NULL);
        else if (size <= LIST3_LIMIT)
            PUT(heap_listp + SEGLIST3, (size_t) NULL);
        else if (size <= LIST4_LIMIT)
            PUT(heap_listp + SEGLIST4, (size_t) NULL);
        else if (size <= LIST5_LIMIT)
            PUT(heap_listp + SEGLIST5, (size_t) NULL);
        else if (size <= LIST6_LIMIT)
            PUT(heap_listp + SEGLIST6, (size_t) NULL);
        else if (size <= LIST7_LIMIT)
            PUT(heap_listp + SEGLIST7, (size_t) NULL);
        else if (size <= LIST8_LIMIT)
            PUT(heap_listp + SEGLIST8, (size_t) NULL);
        else if (size <= LIST9_LIMIT)
            PUT(heap_listp + SEGLIST9, (size_t) NULL);
        else if (size <= LIST10_LIMIT)
            PUT(heap_listp + SEGLIST10, (size_t) NULL);
        else if (size <= LIST11_LIMIT)
            PUT(heap_listp + SEGLIST11, (size_t) NULL);
        else if (size <= LIST12_LIMIT)
            PUT(heap_listp + SEGLIST12, (size_t) NULL);
        else if (size <= LIST13_LIMIT)
            PUT(heap_listp + SEGLIST13, (size_t) NULL);
        else
            PUT(heap_listp + SEGLIST14, (size_t) NULL);
    }

    /* If bp is the tail block, simply update its preceding block's next pointer */
    else if (prevaddress != NULL && nextaddress == NULL) {

        /* Update new tail block's next to null */
        PUT(SUCCESSOR(prevaddress), (size_t) NULL);


    }

    /* If in middle of a list, link blocks on either sides */
    else {

        /* Link next block's previous to current's previous block */
        PUT(PREDECESSOR(nextaddress), (size_t) prevaddress);

        /* Link previous block's next to current's next block */
        PUT(SUCCESSOR(prevaddress), (size_t) nextaddress);
    }
}


/*
 * Extend_heap - Extends the heap upward of size bytes
   and adds free block to appropriate free list before
   coalescing.
 */
static void *extend_heap(size_t words)
{
    char *bp;

    if ((long) (bp = mem_sbrk(words)) < 0)
        return NULL;

    /* Change epilogue header to new free block header */
    PUT4BYTES(HDRP(bp), PACK(words, GET_PREV_ALLOC(HDRP(bp))));

    /* Set new free block footer the same as header */
    PUT4BYTES(FTRP(bp), GET4BYTES(HDRP(bp)));

    /* Add to the appropriate size chain in the segregated list */
    addingtoseglist(bp, words);

    /* Set new epilogue header */
    PUT4BYTES(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* coalesce free blocks if needed */
    return coalesce(bp);
}


/*
 * coalesce - combines free blocks from left and right with current
 *              free block to form larger free block.  removes current
 *              and/or left and/or right free blocks from their own lists
 *              and add to appropriate free list of new combined sizs.
 *              Called after each addition of a free block.
 */
static void *coalesce(void *bp)
{
    /* store prev and next block's info */
    size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    /* get size of current, prev, next free block (including header) */
    size_t size = GET_SIZE(HDRP(bp));
    size_t nsize = 0;
    size_t psize = 0;

    /* previous block's address and its header address */
    char *prev_hd;
    char *prev_blk;

    char *next_blk;

    /* 4 Cases */
    /* Case 1: Prev and Next both allocated */
    if (prev_alloc && next_alloc) {

        /* return current pointer to free block */
        return bp;
    }

    /* Case 2: Prev allocated, Next free */
    else if (prev_alloc && !next_alloc) {

        next_blk = NEXT_BLKP(bp);

        /* remove current free block and next free block from lists */
        removefromseglist(bp, size);
        removefromseglist(next_blk, GET_SIZE(HDRP(next_blk)));

        /* new block size is current free size plus next free size */
        size += GET_SIZE(HDRP(next_blk));

        /* change header to reflect new size */
        PUT4BYTES(HDRP(bp), PACK(size, prev_alloc));

        /* change footer to reflect new size */
        PUT4BYTES(FTRP(bp), PACK(size, prev_alloc));

        /* add new free block to segregated list */
        addingtoseglist(bp, size);

        /* return current pointer to free block
           since block expanded to next */
        return bp;
    }

    /* Case 3- Prev free, Next allocated */
    else if (!prev_alloc && next_alloc) {

        /* get previous block's location and header location */
        prev_blk = PREV_BLKP(bp);
        prev_hd = HDRP(prev_blk);

        psize = GET_SIZE(prev_hd);

        /* remove current free block and prev free block from lists */
        removefromseglist(bp, size);
        removefromseglist(prev_blk, psize);

        /* size is current free size plus prev free size */
        size += psize;

        /* change header to reflect new size */
        PUT4BYTES(prev_hd, PACK(size, GET_PREV_ALLOC(prev_hd)));

        /* change footer to reflect new size */
        PUT4BYTES(FTRP(prev_blk), GET4BYTES(prev_hd));

        /* add new free block to segregated list */
        addingtoseglist(prev_blk, size);

        /* return prev pointer to prev block
           since block expanded to prev */
        return prev_blk;
    }

    /* Case 4- Prev free, Next free */
    else {

        /* Get previous block's location and header location */
        prev_blk = PREV_BLKP(bp);
        prev_hd = HDRP(prev_blk);

        next_blk = NEXT_BLKP(bp);

        psize = GET_SIZE(prev_hd);
        nsize = GET_SIZE(HDRP(next_blk));

        /* Remove current, prev, and next free block from lists */
        removefromseglist(bp, size);
        removefromseglist(prev_blk, psize);
        removefromseglist(next_blk, nsize);

        /* Size is current free plus prev free and next free size */
        size += psize + nsize;

        /* Change header to reflect new size */
        PUT4BYTES(prev_hd, PACK(size, GET_PREV_ALLOC(prev_hd)));

        /* Change footer to reflect new size */
        PUT4BYTES(FTRP(prev_blk), GET4BYTES(prev_hd));

        /* Add new free block to segregated list */
        addingtoseglist(prev_blk, size);

        /* Return prev pointer to free block
           since block expanded to prev */
        return prev_blk;
    }

}


/*
 * mm_init - initializes the heap by storing pointers to
 * start of each free list at beginning of heap as well as
 * creating the initial prologue and eplilogue blocks.
 * The heap is also initially extended to CHUNKSIZE bytes.
 */
int mm_init(void)
{

    /* Allocating segregated free list pointers on heap */
    if ((heap_listp = mem_sbrk(TOTALLIST * DSIZE)) == NULL)
        return -1;

    /* Creating prologue and epilogue */
    if ((h_start = mem_sbrk(4 * WSIZE)) == NULL)
        return -1;

    /* Alignment padding */
    PUT4BYTES(h_start, 0);
    /* Prologue header */
    PUT4BYTES(h_start + WSIZE, PACK(DSIZE, 1));
    /* Prologue footer */
    PUT4BYTES(h_start + (2 * WSIZE), PACK(DSIZE, 1));
    /* Epilogue header */
    PUT4BYTES(h_start + (3 * WSIZE),
          PACK(0, PREVIOUSALLOCATED | CURRENTALLOCATED));

    /* Initializing the segregated list pointers on heap */
    PUT(heap_listp + SEGLIST1, (size_t) NULL);
    PUT(heap_listp + SEGLIST2, (size_t) NULL);
    PUT(heap_listp + SEGLIST3, (size_t) NULL);
    PUT(heap_listp + SEGLIST4, (size_t) NULL);
    PUT(heap_listp + SEGLIST5, (size_t) NULL);
    PUT(heap_listp + SEGLIST6, (size_t) NULL);
    PUT(heap_listp + SEGLIST7, (size_t) NULL);
    PUT(heap_listp + SEGLIST8, (size_t) NULL);
    PUT(heap_listp + SEGLIST9, (size_t) NULL);
    PUT(heap_listp + SEGLIST10, (size_t) NULL);
    PUT(heap_listp + SEGLIST11, (size_t) NULL);
    PUT(heap_listp + SEGLIST12, (size_t) NULL);
    PUT(heap_listp + SEGLIST13, (size_t) NULL);
    PUT(heap_listp + SEGLIST14, (size_t) NULL);

    /* Create initial empty space of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE) == NULL)
        return -1;

    return 0;
}


/*
 * Function: find_fit
 * Description: Searches through all free lists containing
   blocks with size greater than size requested and returns
   pointer to a satisfacotry free block or NULL if none
   are found.
 */
static void *find_fit(size_t asize)
{
    size_t sizeatstart;
    char *bp = NULL;

    /* Segregated lists- Size breakdown */
    if (asize <= LIST1_LIMIT) {
        for (sizeatstart = 0; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST2_LIMIT) {
        for (sizeatstart = 1; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST3_LIMIT) {
        for (sizeatstart = 2; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST4_LIMIT) {
        for (sizeatstart = 3; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST5_LIMIT) {
        for (sizeatstart = 4; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST6_LIMIT) {
        for (sizeatstart = 5; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST7_LIMIT) {
        for (sizeatstart = 6; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST8_LIMIT) {
        for (sizeatstart = 7; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST9_LIMIT) {
        for (sizeatstart = 8; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST10_LIMIT) {
        for (sizeatstart = 9; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST11_LIMIT) {
        for (sizeatstart = 10; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST12_LIMIT) {
        for (sizeatstart = 11; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else if (asize <= LIST13_LIMIT) {
        for (sizeatstart = 12; sizeatstart < TOTALLIST; sizeatstart++) {
            if ((bp = find(sizeatstart, asize)) != NULL)
            return bp;
        }
    }
    else {
        sizeatstart = 13;
        if ((bp = find(sizeatstart, asize)) != NULL) {
            return bp;
        }
    }

    return bp;
}


/*
 * Function: find
 * Description:
   Find searches through a particular size free list
   to find a possible free block >= size requested and returns
   pointer to a possible free block or NULL if none are found.
 */
static void *find(size_t sizeatstart, size_t asize)
{
    char *current = NULL;       // current: the head pointer at a particular size free list

    /* Finding which list to look into */
    if (sizeatstart == 0)
        current = (char *) GET(heap_listp + SEGLIST1);
    else if (sizeatstart == 1)
        current = (char *) GET(heap_listp + SEGLIST2);
    else if (sizeatstart == 2)
        current = (char *) GET(heap_listp + SEGLIST3);
    else if (sizeatstart == 3)
        current = (char *) GET(heap_listp + SEGLIST4);
    else if (sizeatstart == 4)
        current = (char *) GET(heap_listp + SEGLIST5);
    else if (sizeatstart == 5)
        current = (char *) GET(heap_listp + SEGLIST6);
    else if (sizeatstart == 6)
        current = (char *) GET(heap_listp + SEGLIST7);
    else if (sizeatstart == 7)
        current = (char *) GET(heap_listp + SEGLIST8);
    else if (sizeatstart == 8)
        current = (char *) GET(heap_listp + SEGLIST9);
    else if (sizeatstart == 9)
        current = (char *) GET(heap_listp + SEGLIST10);
    else if (sizeatstart == 10)
        current = (char *) GET(heap_listp + SEGLIST11);
    else if (sizeatstart == 11)
        current = (char *) GET(heap_listp + SEGLIST12);
    else if (sizeatstart == 12)
        current = (char *) GET(heap_listp + SEGLIST13);
    else if (sizeatstart == 13)
        current = (char *) GET(heap_listp + SEGLIST14);


    /* Finding available free block in list */
    while (current != NULL) {
        if (asize <= GET_SIZE(HDRP(current))) {
            break;
        }
        current = (char *) GET(SUCCESSOR(current));
    }

    return current;
}


/*
 *  Function: place
 *  Description:
    Allocates block of memory at address bp. if remaining
    memory is >= min free block size, splitting block: allocate requested
    amount and form new free block to add to segregated
    list. If not, allocate entire block of memory at
    address bp.
 */
static void place(void *bp, size_t asize)
{

    /* Original free block size */
    size_t csize = GET_SIZE(HDRP(bp));
    /* Remaining free block size */
    size_t remainsize = csize - asize;

    /* Next adjacent block address */
    char *nextaddress = NEXT_BLKP(bp);

    /* Remove free block from the appropriate seg list */
    removefromseglist(bp, csize);

    /* If the remaining block is larger than min block size of
       24 bytes, then splitting is done to form new free block
     */
    if (remainsize >= MINBLOCK) {

        /* Update new header information, store info bits */
        PUT4BYTES(HDRP(bp),
              PACK(asize,
                   GET_PREV_ALLOC(HDRP(bp)) | CURRENTALLOCATED));

        /* Update next block's address to remaining free block's address */
        nextaddress = NEXT_BLKP(bp);

        /* Inform next adjacent free block that its previous block
           is allocated */
        PUT4BYTES(HDRP(nextaddress), remainsize | PREVIOUSALLOCATED);
        PUT4BYTES(FTRP(nextaddress), remainsize | PREVIOUSALLOCATED);

        /* Add remaining free block to appropriate segregated list */
        addingtoseglist(nextaddress, remainsize);

    }

    /* If the remaining is not larger than min block, then assign
       entire free block to allocated */
    else {

        /* Update new header information, store info bits */
        PUT4BYTES(HDRP(bp),
              PACK(csize,
                   GET_PREV_ALLOC(HDRP(bp)) | CURRENTALLOCATED));

        /* Inform next adjacent block that its previous block
           is allocated */
        PUT4BYTES(HDRP(nextaddress),
              GET4BYTES(HDRP(nextaddress)) | PREVIOUSALLOCATED);

        /* Update next adjacent block's footer only if free */
        if (!GET_ALLOC(HDRP(nextaddress)))
            PUT4BYTES(FTRP(nextaddress), GET4BYTES(HDRP(nextaddress)));

    }
}




/*
 * malloc - returns a pointer to an allocated block payload
 * of at least size bytes.  It first searches through different
 * size free lists for a free block and if needed, extend the
 * heap.
 */
void *malloc(size_t size)
{

    size_t asize;		/* adjusted block size */
    size_t extendsize;		/* size to be extended */
    char *bp;

    /* Ignore less size requests */
    if (size <= 0)
        return NULL;

    /* Adjust block size to include header and alignment requests */
    if (size <= 2 * DSIZE)
        asize = MINBLOCK;
    else
        /* Rounds up to the nearest multiple of ALIGNMENT */
        asize = (((size_t) (size + WSIZE) + 7) & ~0x7);

    /* Search through heap for possible fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);	/* Actual assignment */
        return bp;
    }

    /* If no fit, get more memory and allocate memory */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize)) == NULL)
        return NULL;
    place(bp, asize);		/* Assignment */

    return bp;
}

/*
 * Function: free
 * Description: It frees the block pointed to by ptr and returns
   nothing. Then, it adds freed block to apprpriate size
   free list.
 */
void free(void *ptr)
{
    char *nxtblkheader;
    size_t size;

    if (ptr == NULL)
        return;

    size = GET_SIZE(HDRP(ptr));
    nxtblkheader = HDRP(NEXT_BLKP(ptr));

    /* Update header and footer to unallocated */
    PUT4BYTES(HDRP(ptr), size | GET_PREV_ALLOC(HDRP(ptr)));
    PUT4BYTES(FTRP(ptr), GET4BYTES(HDRP(ptr)));

    /* Update next block, its previous is no longer allocated */
    PUT4BYTES(nxtblkheader,
          GET_SIZE(nxtblkheader) | GET_ALLOC(nxtblkheader));

    /* Add free block to appropriate segregated list */
    addingtoseglist(ptr, size);

    coalesce(ptr);
}

/*
 * Function: realloc
 * Description: Returns a pointer to an allocated region
   of at least size bytes with the contents of pointer
   oldptr up to the size bytes. The default action is to
   simply free current allocated block and copy size bytes
   at oldptr to a new free block and free oldptr.
 */
void *realloc(void *oldptr, size_t size)
{

    /* After malloc, new address with size "size" */
    char *newaddress;

    /* Size to be copied */
    size_t copysize;

    /* If ptr is NULL, call equivalent to malloc(size) */
    if (oldptr == NULL)
        return malloc(size);

    /* If size = 0, call equivalanet to free(oldptr), returns null */
    if (size == 0) {
        free(oldptr);
        return NULL;
    }

    /* Else, allocate new free block, copy old content over */
    newaddress = malloc(size);
    copysize = MIN(size, GET_SIZE(HDRP(oldptr)) - WSIZE);

    /* Move from source to dest */
    memmove(newaddress, oldptr, copysize);

    /* Free the old block */
    free(oldptr);

    return newaddress;
}


/*
 * calloc - Allocates memory for an array of nmemb elements
 * of size bytes each and returns a pointer to the allocated
 * memory. The memory is set to zero before returning.
 */
void *calloc(size_t nmemb, size_t size)
{
    size_t i;
    size_t tsize = nmemb * size;
    char *ptr = malloc(tsize);

    char *temp = ptr;

    for (i = 0; i < nmemb; i++) {
        *temp = 0;
        temp = temp + size;
    }

    return ptr;
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
static int aligned(const void *p)
{
    return (size_t) (((size_t) (p) + 7) & ~0x7) == (size_t) p;
}


/*
 * mm_checkheap:
   This function scans the heap and checks it
   for consistency.

 * Invariants:
        -> All blocks are aligned to 8 bytes (checked)
        -> All blocks store info if previous
            block is allocated
        -> All free blocks' headers/footers
            are the same (checked)
        -> Each free list contains only blocks
            within size ranges (checked)
        -> Each block is within the heap (checked)
        -> No consecutive free blocks (checked)
 */
void mm_checkheap(int verbose)
{

    /* Variables for testing if block falls under appropriate size list */
    char *listpointer = NULL;
    unsigned minimumblocksize = 0;
    unsigned maximumblocksize = 0;
    unsigned sizeatstart = 0;

    /* Pointer to the very first block */
    char *ptr = heap_listp + 2 * DSIZE + TOTALLIST * DSIZE;

    /* If block is free, check for:
       1. Header/footer mismatch
       2. Next/prev free pointer inconsistencies
       3. Adjacent free blocks
     */
    if (!GET_ALLOC(HDRP(ptr))) {

    /*Header/footer mismatch*/
    if ((GET4BYTES(HDRP(ptr)) != GET4BYTES(FTRP(ptr))))
        printf("Free block pointer %p: \
header and footer mismatch\n", ptr);

    /* Next/prev free pointer inconsistencies*/
    if ((char *) GET(SUCCESSOR(ptr)) != NULL &&
        (char *) GET(PREDECESSOR(GET(SUCCESSOR(ptr)))) != ptr)
        printf("Free block pointer %p's \
next pointer is inconsistent\n", ptr);

    if ((char *) GET(PREDECESSOR(ptr)) != NULL &&
        (char *) GET(SUCCESSOR(GET(PREDECESSOR(ptr)))) != ptr)
        printf("Free block pointer %p's \
previous pointer is inconsistent\n", ptr);

    /* Adjacent free blocks */
    if ((GET4BYTES(HDRP(NEXT_BLKP(ptr))) != 1) &&
        (GET4BYTES(HDRP(NEXT_BLKP(ptr))) != 3) &&
        !GET_ALLOC(HDRP(NEXT_BLKP(ptr))))
        printf("Error: Free block pointer %p \
and %p are adjacent\n", ptr, NEXT_BLKP(ptr));

    }

    /* Checking if all blocks in each freelist fall within
       the appropriate ranges (Different segregated lists)*/
    for (sizeatstart = 0; sizeatstart < TOTALLIST; sizeatstart++) {
        if (sizeatstart == 0) {
            listpointer = (char *) GET(heap_listp + SEGLIST1);
            minimumblocksize = 0;
            maximumblocksize = LIST1_LIMIT;
        } else if (sizeatstart == 1) {
            listpointer = (char *) GET(heap_listp + SEGLIST2);
            minimumblocksize = LIST1_LIMIT;
            maximumblocksize = LIST2_LIMIT;
        } else if (sizeatstart == 2) {
            listpointer = (char *) GET(heap_listp + SEGLIST3);
            minimumblocksize = LIST2_LIMIT;
            maximumblocksize = LIST3_LIMIT;
        } else if (sizeatstart == 3) {
            listpointer = (char *) GET(heap_listp + SEGLIST4);
            minimumblocksize = LIST3_LIMIT;
            maximumblocksize = LIST4_LIMIT;
        } else if (sizeatstart == 4) {
            listpointer = (char *) GET(heap_listp + SEGLIST5);
            minimumblocksize = LIST4_LIMIT;
            maximumblocksize = LIST5_LIMIT;
        } else if (sizeatstart == 5) {
            listpointer = (char *) GET(heap_listp + SEGLIST6);
            minimumblocksize = LIST5_LIMIT;
            maximumblocksize = LIST6_LIMIT;
        } else if (sizeatstart == 6) {
            listpointer = (char *) GET(heap_listp + SEGLIST7);
            minimumblocksize = LIST6_LIMIT;
            maximumblocksize = LIST7_LIMIT;
        } else if (sizeatstart == 7) {
            listpointer = (char *) GET(heap_listp + SEGLIST8);
            minimumblocksize = LIST7_LIMIT;
            maximumblocksize = LIST8_LIMIT;
        } else if (sizeatstart == 8) {
            listpointer = (char *) GET(heap_listp + SEGLIST9);
            minimumblocksize = LIST8_LIMIT;
            maximumblocksize = LIST9_LIMIT;
        } else if (sizeatstart == 9) {
            listpointer = (char *) GET(heap_listp + SEGLIST10);
            minimumblocksize = LIST9_LIMIT;
            maximumblocksize = LIST10_LIMIT;
        } else if (sizeatstart == 10) {
            listpointer = (char *) GET(heap_listp + SEGLIST11);
            minimumblocksize = LIST10_LIMIT;
            maximumblocksize = LIST11_LIMIT;
        } else if (sizeatstart == 11) {
            listpointer = (char *) GET(heap_listp + SEGLIST12);
            minimumblocksize = LIST11_LIMIT;
            maximumblocksize = LIST12_LIMIT;
        } else if (sizeatstart == 12) {
            listpointer = (char *) GET(heap_listp + SEGLIST13);
            minimumblocksize = LIST12_LIMIT;
            maximumblocksize = LIST13_LIMIT;
        } else {
            listpointer = (char *) GET(heap_listp + SEGLIST14);
            minimumblocksize = LIST13_LIMIT;
            maximumblocksize = ~0;
        }

        while (listpointer != NULL) {
            if (!(minimumblocksize < GET_SIZE(HDRP(listpointer)) &&
              GET_SIZE(HDRP(listpointer)) <= maximumblocksize)) {
                printf("Free block pointer %p is not in the appropriate list", listpointer);
            }
            listpointer = (char *) GET(SUCCESSOR(listpointer));
        }
    }


    /* Traverse through the entire heap:
     *  For all blocks, check for:
     1. Alignment
     2. Existence in heap
     */
    while ((GET4BYTES(HDRP(ptr)) != 1) && (GET4BYTES(HDRP(ptr)) != 3)) {

        if (!aligned(ptr)) {
            printf("Block pointer %p isn't aligned\n", ptr);
        }
        if (!in_heap(ptr)) {
            printf("Block pointer %p isn't in heap\n", ptr);
        }


        /* This information is printed for every block irrespective
           of the error
         */
        if (verbose) {

            printf("\nBlock pointer: %p\n", ptr);
            printf("Block size is: %d\n", GET_SIZE(HDRP(ptr)));

            if (GET_ALLOC(HDRP(ptr)))
            printf("Block is allocated\n");
            else
            printf("Block is free\n");

            if (verbose > 1) {
            if (GET_PREV_ALLOC(HDRP(ptr)))
                printf("Previous block is allocated\n");
            else
                printf("Previous block is free\n");
            }
        }

        ptr = NEXT_BLKP(ptr);
    }

}