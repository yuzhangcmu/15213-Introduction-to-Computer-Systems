/*
Andrew ID : guest486
Name : Li Yuanchun
Email : tzlyc@pku.edu.cn
    CMU的malloclab，用的是二叉树，当然较小的块用链表组织。
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
#endif /* def DRIVER */


//Word, 4 bytes
#define WSIZE 4
//Double words, 8 bytes
#define DSIZE 8
//Header size
#define HEADSIZE 4
//Header and footer sign, 8 bytes
#define HNF 8
//Alignment request, 8 bytes
#define ALIGNMENT 8
//size of 16-byte block
#define S_BLOCK_SIZE 16
//size of 8-byte block
#define M_BLOCK_SIZE 8
//Minimum block size
#define BLKSIZE 24
//Extend heap by this amount(bytes)
#define CHUNKSIZE (1<<8)

/*Macros*/
/*Max and min value of 2 values*/
#define MAX(x, y) ( (x)>(y)? (x): (y) )
#define MIN(x, y) ( (x)<(y)? (x): (y) )

/*Make the block to meet with the standard alignment requirements*/
#define ALIGN_SIZE(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/*Pack a size and allocated bit into a word*/
#define PACK(size, alloc) ((size)|(alloc))

/*Read and write a word at address p*/
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p)=(val))

/*Read the size and allocated fields from address p*/
#define SIZE(p) (GET(p) & (~0x7))
#define ALLOC(p) (GET(p) & (0x1))
#define PREVALLOC(p) (GET(p) & (0x2))
#define PREV_E_FREE(p) (GET(p) & (0x4))

/*Given block pointer bp, read the size and allocated fields*/
#define GET_SIZE(bp) ((GET(HEAD(bp)))&~0x7)
#define GET_ALLOC(bp) ((GET(HEAD(bp)))&0x1)
#define GET_PREVALLOC(bp) ((GET(HEAD(bp)))&0x2)
#define GET_PREV_E_FREE(bp) ((GET(HEAD(bp)))&0x4)

#define SET_PREVALLOC(bp) (GET(HEAD(bp)) |= 0x2)
#define RESET_PREVALLOC(bp) (GET(HEAD(bp)) &= ~0x2)
#define SET_PREV_E_FREE(bp) (GET(HEAD(bp)) |= 0x4)
#define RESET_PREV_E_FREE(bp) (GET(HEAD(bp)) &= ~0x4)
/*Given pointer p at the second word of the data structure, compute addresses
of its HEAD,LEFT,RIGHT,PARENT,BROTHER and FOOT pointer*/
#define HEAD(p) ((void *)(p) - WSIZE)
#define LEFT(p) ((void *)(p))
#define RIGHT(p) ((void *)(p) + WSIZE)
#define PARENT(p) ((void *)(p) + 2 * WSIZE)
#define BROTHER(p) ((void *)(p) + 3 * WSIZE)
#define FOOT(p) ((void *)(p) + SIZE(HEAD(p)) - DSIZE)

/*Given block pointer bp, get the POINTER of its directions*/
#define GET_PREV(bp) ( GET_PREV_E_FREE(bp) ? ((void *)bp - M_BLOCK_SIZE ): ((void *)(bp) - SIZE((void *)(bp) - DSIZE)) )
#define GET_NEXT(bp) ((void *)(bp) + SIZE(((void *)(bp) - WSIZE)))

/*Get the LEFT,RIGHT,PARENT,BROTHER and FOOT pointer of the block to which bp points*/
#define GET_LEFT(bp) ((long)GET(LEFT(bp))|(0x800000000))
#define GET_RIGHT(bp) ((long)GET(RIGHT(bp))|(0x800000000))
#define GET_PARENT(bp) ((long)GET(PARENT(bp))|(0x800000000))
#define GET_BROTHER(bp) ((long)GET(BROTHER(bp))|(0x800000000))

/*Define value to each character in the block bp points to.*/
#define PUT_HEAD(bp, val) (PUT(HEAD(bp), val))
#define PUT_FOOT(bp, val) (PUT(FOOT(bp), val))
#define PUT_LEFT(bp, val) (PUT(LEFT(bp), ((unsigned int)(long)val)))
#define PUT_RIGHT(bp, val) (PUT(RIGHT(bp), ((unsigned int)(long)val)))
#define PUT_PARENT(bp, val) (PUT(PARENT(bp), ((unsigned int)(long)val)))
#define PUT_BROTHER(bp, val) (PUT(BROTHER(bp), ((unsigned int)(long)val)))

//All functions and global variables used in the program:

/* static functions */
static void *coalesce ( void *bp );
static void *extend_heap ( size_t size );
static void place ( void *ptr, size_t asize );
static void insert_node ( void *bp );
static void delete_node ( void *bp );
static void *match ( size_t asize );
static void printblock(void *bp);
static void checkblock(void *bp);
static void checktree(void * temp);
static void checkminilist(void * bp); 
static void checksmalllist(void * bp);

/* Global variables */
static void *heap_list_ptr;//head block node of all heap blocks
static void *root;//root of the BST
static void *small_list_ptr;//head of the 16-bit list
static void *mini_list_ptr;//head of the 8-byte list
static void *HEAP_NIL;//virtual NULL pointer (0x800000000)

/*
MM_INIT
similar with the code in textbook
*/

int mm_init(void)
{
    /* create the initial empty heap */
    if( (heap_list_ptr = mem_sbrk(6 * WSIZE)) == (void *)-1 )
        return -1;
    PUT( heap_list_ptr + (2 * WSIZE), 0 ); // alignment padding
    PUT( heap_list_ptr + (3 * WSIZE), PACK(DSIZE, 1) ); // prologue header
    PUT( heap_list_ptr + (4 * WSIZE), PACK(DSIZE, 1) ); // prologue footer
    PUT( heap_list_ptr + (5 * WSIZE), PACK(0, 3) ); // epilogue header
    heap_list_ptr += (4 * WSIZE);
    /*init the global variables*/
    HEAP_NIL = (void *)0x800000000; 
    root = HEAP_NIL;
    small_list_ptr = HEAP_NIL;
    mini_list_ptr = HEAP_NIL;
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if( extend_heap(BLKSIZE)==NULL )
        return -1;
    return 0;
}
/*EXTEND_HEAP

extend_heap extends the heap with a new free block. It is invoked when the
heap is initialized or malloc or realloc is unable to find a suitable fit.

before we extend the heap, check if the last block is free, if yes, extend the
heap for size - GET_SIZE(prev) bits, At the end of the function, we call the coalesce
function to merge the two free blocks and return the block pointer to the
merged blocks.*/

void *extend_heap(size_t size_)
{
    void *bp;
    void *end = mem_heap_hi() - 3;

    int size = size_;
    if( !PREVALLOC(end) ){
        if(PREV_E_FREE(end)) size -= M_BLOCK_SIZE;
        else size -= SIZE(end - 4);
    }
    if(size <= 0) return NULL;
    
    size = MAX(CHUNKSIZE, size);

    if((long)(bp = mem_sbrk(size)) == -1)
    //if( (int)(bp=mem_sbrk(size)) <0 )//new
        return NULL;
    /* Initialize free block header/footer and the epilogue header */
    size_t sign = 0 | GET_PREVALLOC(bp) | GET_PREV_E_FREE(bp);
    PUT_HEAD(bp, PACK(size,sign) ); /* free block header */
    PUT_FOOT(bp, PACK(size,sign) ); /* free block footer */

    PUT_HEAD(GET_NEXT(bp), PACK(0,1) ); /* new epilogue header */
    
    insert_node(coalesce(bp));
    return (void *)bp;
}

/*
MALLOC

After initializing, we use malloc to allocate a block by incrementing the brk
pointer. We always allocate a block whose size is a multiple of the alignment.
The function below is very likely to the function described on the textbook.
The behaviors of the function are:

1. Checking the spurious requests.
2. Adjusting block size to include alignment requirements.
3. Searching the BST for a fit.
4. Placing the block into its fit.

At the end of the function, I looked in the traces and found the best strategy to
meet the performance evaluation principle of the project, so I added the if
sentence after it.*/

void *malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    void *bp;
    /* Ignore spurious requests */
    if( size <= 0 )
        return NULL;
    /* Adjust block size to include HNF and alignment requirements. */
    asize = ALIGN_SIZE( size + HEADSIZE );
    /* Search the free list for a fit */
    if( (bp = match(asize)) == HEAP_NIL ){
        extend_heap( asize );
        if( (bp = match(asize)) == HEAP_NIL )
            return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
FREE

firstly check the block's size and allocated bit
then change the allocated bit, insert the bp into our data structure
*/

void free(void *bp)
{
    if(bp == NULL) return;
    size_t size = GET_SIZE(bp);
    size_t checkalloc = GET_ALLOC(bp);
    if(checkalloc == 0) return;
    size_t sign = 0 | GET_PREVALLOC(bp) | GET_PREV_E_FREE(bp);
    PUT_HEAD(bp, PACK(size, sign) );
    PUT_FOOT(bp, PACK(size, sign) );

    insert_node(coalesce(bp));
}

/*
REALLOC

The behavior of the function is listed below:
1.When ptr==HEAP_NIL, call malloc(size), when size==0, free ptr.
2.When size>0, compare new size and old size and then adopt relative
strategies.:
    (1) if newsize < oldsize, check if the size of the rest space,
        if > HNF, divide the block into 2 pieces
    (2) else, check if the next space is free, if so, coalesce the block and 
        the next block
*/
void *realloc(void *ptr, size_t size)
{
    if( ptr==NULL ) return malloc(size);
    if( size == 0 ){
        free(ptr);
        return NULL;
    }
    if( size > 0 ){
        size_t oldsize = GET_SIZE( ptr );
        size_t newsize = ALIGN_SIZE( size + HEADSIZE );
        if( newsize <= oldsize ){ /* newsize is less than oldsize */
            if( GET_ALLOC( GET_NEXT(ptr) ) ){
            /* the next block is allocated */
                if( (oldsize-newsize) >= HNF ){
                /* the remainder is greater than BLKSIZE */
                    size_t sign = 1 | GET_PREVALLOC(ptr) | GET_PREV_E_FREE(ptr);
                    PUT_HEAD(ptr, PACK(newsize,sign) );
                    //this pointer points to extra space
                    void *temp = GET_NEXT(ptr);
                    PUT_HEAD( temp, PACK(oldsize-newsize,2) );
                    PUT_FOOT( temp, PACK(oldsize-newsize,2) );
                    insert_node( coalesce(temp) );
                }
                return ptr;
            }
            else{ /* the next block is free */
                size_t csize = oldsize + GET_SIZE( GET_NEXT(ptr) );
                delete_node( GET_NEXT(ptr) );
                size_t sign = 1 | GET_PREVALLOC(ptr) | GET_PREV_E_FREE(ptr);
                PUT_HEAD( ptr, PACK(newsize,sign) );
                void *temp = GET_NEXT(ptr);
                PUT_HEAD( temp, PACK(csize-newsize,2) );
                PUT_FOOT( temp, PACK(csize-newsize,2) );
                insert_node( coalesce(temp) );
                return ptr;
            }
        }
        else{ /* newsize is greater than oldsize */
            void * next = GET_NEXT(ptr);
            size_t next_alloc = GET_ALLOC(next);
            size_t next_size = GET_SIZE(next);
               
            if (next_size == 0 || GET( GET_NEXT(next) ) == 0) {
                extend_heap( newsize - oldsize );
            }

            size_t csize;
            // the next block is free and the addition of the two blocks no less than the newsize
            if( !next_alloc && ((csize = oldsize + next_size) >= newsize) ){
                delete_node(GET_NEXT(ptr));
                if((csize-newsize) >= HNF){
                    size_t sign = 1 | GET_PREVALLOC(ptr) | GET_PREV_E_FREE(ptr);
                    PUT_HEAD( ptr, PACK(newsize,sign) );
                    void *temp=GET_NEXT(ptr);
                    PUT_HEAD( temp, PACK(csize-newsize,2) );
                    PUT_FOOT( temp, PACK(csize-newsize,2) );
                    insert_node( coalesce(temp) );
				}else{
				    size_t sign = 1 | GET_PREVALLOC(ptr) | GET_PREV_E_FREE(ptr);
					PUT_HEAD(ptr,PACK(csize,sign) );
                    SET_PREVALLOC( GET_NEXT(ptr) );
				}
				return ptr;
			}
			else{
				void *newptr;
				if((newptr=match(newsize))==HEAP_NIL){        
					extend_heap( newsize );
					if((newptr = match(newsize)) == HEAP_NIL)
                         return NULL;
				}
				place( newptr, newsize );
				/*copy content from memory*/
				memcpy(newptr, ptr, oldsize - HEADSIZE);
				free(ptr);
				return newptr;
			}
		}
	}
    else return NULL;
}

/*

COALSCE

coalesce is to merge one free block with any adjacent free blocks and to
update binary tree's structure in time.
There are 4 possibilities:
Each possibility is listed below as case 0 to 3.
After coalescing, pointer returns to the big freed block.*/

static void *coalesce(void *bp)
{	
	size_t size = GET_SIZE(bp);

	size_t prev_alloc = GET_PREVALLOC(bp);
	
	size_t next_alloc = GET_ALLOC( GET_NEXT(bp) );
	
	if ( prev_alloc && next_alloc ){ /* Case 0 */
        return bp;
	}		
	else if ( !prev_alloc && next_alloc ) { /* Case 1*/
	    void * prev = (void *)GET_PREV(bp);
	    size_t sign = 0 | GET_PREVALLOC(prev) | GET_PREV_E_FREE(prev);
	    delete_node(prev);
		size += GET_SIZE( prev );
		PUT_HEAD( prev, PACK(size, sign) );
		PUT_FOOT( prev, PACK(size, sign) );
		return prev;
	}
	else if ( prev_alloc && !next_alloc ) { /* Case 2 */
	    size += GET_SIZE( GET_NEXT(bp) );
	    delete_node( GET_NEXT(bp) );
		size_t sign = 0 | GET_PREVALLOC(bp) | GET_PREV_E_FREE(bp);
		PUT_HEAD( bp, PACK(size,sign) );
		PUT_FOOT( bp, PACK(size,sign) );
		return bp;
	}
	else { /* Case 3 */
	    void * prev = (void *)GET_PREV(bp);
	    void * next = (void *)GET_NEXT(bp);
	    size += GET_SIZE( prev ) + GET_SIZE( next );
	    
		delete_node( prev );
		delete_node( next );
		
		size_t sign = 0 | GET_PREVALLOC(bp) | GET_PREV_E_FREE(bp);
        PUT_HEAD( prev, PACK(size,sign) );
		PUT_FOOT( prev, PACK(size,sign) );
		return prev;
	}
}

/*PLACE

place is to place the requested block.
If the remainder of the block after slitting would be greater than or equal to the
minimum block size, then we go ahead and split the block. We should realize
that we need to place the new allocated block before moving to the next block.
It is very likely to the operation on realloc.*/

static void place(void *bp,size_t asize)
{
	size_t csize = GET_SIZE(bp);
	delete_node( bp );
	if((csize-asize) >= HNF){
	    size_t sign = 1 | GET_PREVALLOC(bp) | GET_PREV_E_FREE(bp);
		PUT_HEAD( bp,PACK(asize,sign) );

		void * temp = GET_NEXT(bp);
		PUT_HEAD( temp, PACK(csize-asize,2) );
		PUT_FOOT( temp, PACK(csize-asize,2) );
		
		insert_node( coalesce(temp) );
	}
	else{
	    size_t sign = 1 | GET_PREVALLOC(bp) | GET_PREV_E_FREE(bp);
		PUT_HEAD(bp,PACK(csize, sign) );
	}
}

/*match

match performs a fit search. Our basic principles for BEST FIT strategies is

8-byte list :
    if asize < 8 , check the 8-byte list, if the list is not empty, return the head of the list

16-bit list :
    if asize < 16 , check the 16-byte list, if the list is not empty, return the head of the list
    
BINARY TREE :
    1.We must eventually find a fit block after searching the binary free tree.
    2.We must choose the least size free block compared with the requested size.
      So we should initially move toward left and go on. When the block in the left is
      not big enough to support the block, move right.
    3.If the block is so big that every node cannot fit it (till the rightmost), extend the
       heap and put the block in the rightmost of the tree.
*/

static void *match( size_t asize )
{
    if(asize == 8 && mini_list_ptr != HEAP_NIL) return mini_list_ptr;
    if(asize <= 16 && small_list_ptr != HEAP_NIL) return small_list_ptr;
	/* the most fit block */
	void *fit = HEAP_NIL;
	/* temporary location of the search */
	void *temp = root;
	/* use tree to implement a comparative best fit search */
	while(temp != HEAP_NIL){
		/* The following node in the search may be worse, so we need to record the most fit so far. */
		if( asize <= GET_SIZE(temp) ){
			fit = temp;
			temp = (void *)GET_LEFT(temp);
		}
		else
			temp = (void *)GET_RIGHT(temp);
	}
	return fit;
}

/*
INSERT_NODE
principles:
    if the block size = 8; insert it to the head of the 8-byte list
    if the block size = 16; insert it to the head of the 16-bit list
    if the block size >= 24; insert it to BST
        if the block size is less than the size of the block on the node
           insert it to the node's left child node
        if the block size is greater than the size of the block on the node
           insert it to the node's right child node
        if the block size equals to the size of the block on the node
           insert it to the node's position and set the node as its brother
*/

inline static void insert_node( void *bp )
{
    RESET_PREVALLOC(GET_NEXT(bp));
    size_t bpsize = GET_SIZE(bp);
    if(bpsize == 8){
        SET_PREV_E_FREE( GET_NEXT(bp) );
        PUT_LEFT(bp, mini_list_ptr);
        mini_list_ptr = bp;
    }    
    if(bpsize == 16){//if the block size = 16; insert it to the head of the 16-bit list
        PUT_LEFT(bp, HEAP_NIL);
        PUT_RIGHT(bp, small_list_ptr);
        PUT_LEFT(small_list_ptr, bp);
        small_list_ptr = bp;
        return;
    }
    else if(bpsize < BLKSIZE) return;
	/* root is HEAP_NIL */
	if( root == HEAP_NIL ){
		root = bp;
		PUT_LEFT( bp, HEAP_NIL);
        PUT_RIGHT( bp, HEAP_NIL);
		PUT_PARENT( bp, HEAP_NIL);
		PUT_BROTHER( bp, HEAP_NIL);
		return;
	}
	/* treat temp as the start */
	void *parent = HEAP_NIL;
	void *temp = root;
	int dir = -1;
	/* loop to locate the position */
	while( 1 ){
	    if( temp == HEAP_NIL ){
            PUT_LEFT( bp, HEAP_NIL);
            PUT_RIGHT( bp, HEAP_NIL);
            PUT_PARENT( bp, parent);
            PUT_BROTHER( bp, HEAP_NIL);
            break;
        }
		/* Case 1: size of the block exactly matches the node. */
		if( GET_SIZE(bp) == GET_SIZE(temp) ){
		    void * tempL = (void *)GET_LEFT(temp);
		    void * tempR = (void *)GET_RIGHT(temp);
		    PUT(LEFT(bp), GET(LEFT(temp)));
            PUT(RIGHT(bp), GET(RIGHT(temp)));
            PUT_PARENT( tempL, bp );
            PUT_PARENT( tempR, bp );
		    PUT_PARENT(bp, parent);
            PUT_BROTHER(bp, temp);
            PUT_LEFT(temp, bp);
		    break;
		}
		/* Case 2: size of the block is less than that of the node. */
		else if( GET_SIZE(bp) < GET_SIZE(temp) ){
			parent = temp;
			dir = 0;
			temp = (void *)GET_LEFT(temp);
		}
		/* Case 3 size of the block is greater than that of the node. */
		else{
			parent = temp;
			dir = 1;
			temp = (void *)GET_RIGHT(temp);
		}
	}
	if(dir == -1) root = bp;
	else if(dir == 0) PUT_LEFT(parent, bp);
	else if(dir == 1) PUT_RIGHT(parent, bp);
	else return;
}

/*DELETE_NODE

delete_node is to delete a free block from the free-block binary tree. It also has
many possibilities.

*/

inline static void delete_node(void *bp)
{
    SET_PREVALLOC(GET_NEXT(bp));
    size_t bpsize = GET_SIZE(bp);
    if(bpsize == 8) {//if the block size = 8, remove it from the 8-byte list, O(n)
        RESET_PREV_E_FREE( GET_NEXT(bp) );
        void * temp = mini_list_ptr;
        if(temp == bp){
            mini_list_ptr = (void *)GET_LEFT(bp);
            return;
        }
        while(temp != HEAP_NIL){
            if((void *)GET_LEFT(temp) == bp) break;
            temp = (void *)GET_LEFT(temp); 
        } 
        PUT_LEFT(temp, (void *)GET_LEFT(bp));
    }     
    if(bpsize == 16) {//if the block size = 16, remove it from the 16-bit list, O(1)
        void * bpL = (void *)GET_LEFT(bp);
        void * bpR = (void *)GET_RIGHT(bp);
        
        if(bp == small_list_ptr)
            small_list_ptr = bpR;    
        
        PUT_RIGHT( bpL, bpR);
        PUT_LEFT( bpR, bpL);
        return;
    }    
    if(bpsize < BLKSIZE) return;
    /* Case that the block is one of the following ones in the node. */
    if( ((void *)GET_LEFT(bp) != HEAP_NIL) && ((void *)GET_BROTHER( GET_LEFT(bp) ) == bp) ){
        PUT_BROTHER( GET_LEFT(bp), GET_BROTHER(bp) );
        PUT_LEFT( GET_BROTHER(bp), GET_LEFT(bp) );
    }
	/* Case that the block is the only one in the node
       we are all familar with the BST delete node operation
       but in the heap, it seems a little complex */
	else if( (void *)GET_BROTHER(bp) == HEAP_NIL ){
		if( bp == root ){/* the node is the root */
			if( (void *)GET_RIGHT(bp) == HEAP_NIL ){/* no right child */
				root = (void *)GET_LEFT(bp);
				if( root != HEAP_NIL )
				    PUT_PARENT( root, HEAP_NIL );
			}
			else{/* it has a right child */
				void *temp = (void *)GET_RIGHT(bp);
				while( (void *)GET_LEFT(temp) != HEAP_NIL )
                    temp = (void *)GET_LEFT(temp);
				void *rootL = (void *)GET_LEFT(bp);
				void *rootR = (void *)GET_RIGHT(bp);
				void *tempR = (void *)GET_RIGHT(temp);
				void *tempP = (void *)GET_PARENT(temp);
				root = temp;
				PUT_PARENT( root, HEAP_NIL );
				PUT_LEFT( root, rootL );
				PUT_PARENT( rootL, root );
				if( root != rootR ){
					PUT_RIGHT( root, rootR );
					PUT_PARENT( rootR, root );
					PUT_LEFT( tempP, tempR );
					PUT_PARENT( tempR, tempP );
				}
			}
		}
		else{/* the node is not the root */
			if( (void *)GET_RIGHT(bp) == HEAP_NIL ){/* no right child */
				if( (void *)GET_LEFT( GET_PARENT( bp ) ) == bp )
					PUT_LEFT( GET_PARENT(bp), GET_LEFT(bp) );
				else
					PUT_RIGHT( GET_PARENT(bp), GET_LEFT(bp) );
                PUT_PARENT( GET_LEFT(bp), GET_PARENT(bp) );
			}else{/* it has a right child */
				void *temp = (void *)GET_RIGHT(bp);
				while( (void *)GET_LEFT(temp) != HEAP_NIL )
				    temp = (void *)GET_LEFT(temp);
				void *bpL = (void *)GET_LEFT(bp);
				void *bpR = (void *)GET_RIGHT(bp);
				void *bpP = (void *)GET_PARENT(bp);
				void *tempR = (void *)GET_RIGHT(temp);
				void *tempP = (void *)GET_PARENT(temp);
				
				if( (void *)GET_LEFT( bpP ) == bp )
					PUT_LEFT( bpP, temp );
				else
					PUT_RIGHT( bpP, temp );
			    PUT_PARENT( temp, bpP );

				PUT_LEFT( temp, bpL );
				PUT_PARENT( bpL, temp );
				if( temp != bpR ){
					PUT_RIGHT( temp, bpR );
					PUT_PARENT( bpR, temp );
					PUT_LEFT( tempP, tempR );
					PUT_PARENT( tempR, tempP );
				}
			}
		}
	}
	/* Case that the block is first one in the node. */
    else{
	    void *temp = (void *)GET_BROTHER(bp);
		if( bp == root ){/* the node is the root */
			root = temp;
			PUT_PARENT( temp, HEAP_NIL );
		}else{/* the node is not the root */		
		    if( (void *)GET_LEFT(GET_PARENT(bp)) == bp )
				PUT_LEFT( GET_PARENT(bp), temp );
			else
				PUT_RIGHT( GET_PARENT(bp), temp );
			PUT_PARENT( temp, GET_PARENT(bp) );
		}
		PUT(LEFT(temp), GET(LEFT(bp)));
        PUT(RIGHT(temp), GET(RIGHT(bp)));
		PUT_PARENT( GET_LEFT(bp), temp );
	    PUT_PARENT( GET_RIGHT(bp), temp );
	}
}

/* 
 * mm_checkheap - Check the heap for consistency 
     options:
         verbose = 0: only print the checkblock message
         verbose = 1: print the heap block
         verbose = 2: print the BST nodes in mid-brother-left-right order
         verbose = 3: print the 8-byte list(minilist) and the 16-bit list(smalllist)
 */
void mm_checkheap(int verbose) 
{
    char *bp = heap_list_ptr;

    if (verbose == 1)
        printf("Heap (%p):\n", heap_list_ptr);

    if ((SIZE(heap_list_ptr) != DSIZE) || !ALLOC(heap_list_ptr))
        printf("Bad prologue header\n");
        
    if (verbose == 1)
        printblock(heap_list_ptr);
    for (bp = GET_NEXT(bp); GET_SIZE(bp) > 0; bp = GET_NEXT(bp)) {
        if (verbose == 1) printblock(bp);
        checkblock(bp);
    }

    if (verbose == 1) printblock(bp);
    if ((GET_SIZE(bp) != 0) || !(GET_ALLOC(bp)))
        printf("Bad epilogue header\n");
    if (verbose == 2){
        printf("=============tree=============\n");
        checktree(root);
    }
    if (verbose == 3){
        printf("=========mini list(size = 8)==========\n");
        checkminilist(mini_list_ptr);
        printf("=========short list(size = 16)==========\n");
        checksmalllist(small_list_ptr);
    }    
}

/*print the information of a block, including the address, size and allocated bit*/
static void printblock(void *bp) 
{
    size_t hsize;

    hsize = SIZE(HEAD(bp)); 

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p--prev_e_free bit[%d] prevalloc bit[%d] allocated bit[%d] size[%d]\n",
     bp, !!(int)GET_PREV_E_FREE(bp), !!(int)GET_PREVALLOC(bp), (int)GET_ALLOC(bp), (int)hsize); 
}

/*check if the block is doubleword aligned*/
static void checkblock(void *bp) 
{
    if ((size_t)bp % 8)
    printf("Error: %p is not doubleword aligned\n", bp);
}

/*check a free block in the BST
    print the block and check the next node in the list including the brother node
    Then check the left child node and the right child node
*/
static void checktree(void * temp){
    if(temp != HEAP_NIL){
        printblock(temp);
        printf("\tparent:%p;\tbrother:%p;\n\tleft:%p;\tright:%p\n",
         (void *)GET_PARENT(temp), (void *)GET_BROTHER(temp), (void *)GET_LEFT(temp), (void *)GET_RIGHT(temp));
        void * temp1 = (void *)GET_BROTHER(temp);
        while(temp1 != HEAP_NIL){
            printblock(temp1);
            printf("\tparent:%p;\tbrother:%p;\n\tleft:%p;\tright:%p\n",
             (void *)GET_PARENT(temp1), (void *)GET_BROTHER(temp1), (void *)GET_LEFT(temp1), (void *)GET_RIGHT(temp1));
            temp1 = (void *)GET_BROTHER(temp1);
        }
        checktree((void *)GET_LEFT(temp));
        checktree((void *)GET_RIGHT(temp));
    }
}     

/*check a 16-bit free block in the small list
    print the block and check the next node in the list
*/
static void checksmalllist(void * bp){
    if(bp != HEAP_NIL){
        printblock(bp);
        printf("\tleft:%p;\tright:%p\n", (void *)GET_LEFT(bp), (void *)GET_RIGHT(bp));
        checksmalllist((void *)GET_RIGHT(bp));
    }
}

/*check a 16-bit free block in the mini list
    print the block and check the next node in the list
*/
static void checkminilist(void * bp){
    if(bp != HEAP_NIL){
        printblock(bp);
        printf("\tleft:%p\n", (void *)GET_LEFT(bp));
        checkminilist((void *)GET_LEFT(bp));
    }
}
