/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team_6",
    /* First member's full name */
    "Minhyeong Lee",
    /* First member's email address */
    "mi75625@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */

#define WSIZE       4       /* Word and header/footer size */
#define DSIZE       8       /* Double word */
#define CHUNKSIZE   (1<<12) /* Extend heap by this amount -mem_brk를 가지고 CHUNKSIZE만큼 힙 영역을 늘림*/

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(ptr)          (*(unsigned int *)(ptr))
#define PUT(ptr, val)      (*(unsigned long *)(ptr) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(ptr)     (GET(ptr) & ~0x7) /* Check size -하위 3비트 제외*/
#define GET_ALLOC(ptr)    (GET(ptr) & 0x1)  /* Check allocated */

/* Compute address of bp's header and footer */
#define HDRP(ptr)        ((char *)(ptr) - WSIZE) 
#define FTRP(ptr)        ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

/* Compute address of next and previous blocks */
#define NEXT_BLKP(ptr)   ((char *)(ptr) + GET_SIZE(HDRP(ptr)))
#define PREV_BLKP(ptr)   ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE))) /* - (이전 블록+지금 블록 크기) == Double word */

/* Pointer for Explicit list */
#define PREV_PTR(ptr)   (*(char **)(ptr)) 
#define NEXT_PTR(ptr)   (*(char **)(ptr + WSIZE))

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize); 
static void place(void *bp, size_t asize);
static void free_list_add(void* bp);
static void free_list_delete(void* bp);

static char *free_listp = 0;

/* 
 * mm_init - initialize the malloc package. 힙 초기화
 */
int mm_init(void)
{
    if ((free_listp = mem_sbrk(8 * WSIZE)) == (void *)-1)
        return -1;
    PUT(free_listp, 0);                                /* Alignment padding */
    PUT(free_listp + (1 * WSIZE), PACK(DSIZE, 1));     /* 프롤로그 header */
    PUT(free_listp + (2 * WSIZE), PACK(DSIZE, 1));     /* 프롤로그 footer */
    PUT(free_listp + (3 * WSIZE), PACK(2 * DSIZE, 0)); /* 초기 가용 블록 header */
    PUT(free_listp + (4 * WSIZE), NULL);               /* 이전 가용 블록 주소*/
    PUT(free_listp + (5 * WSIZE), NULL);               /* 다음 가용 블록 주소*/
    PUT(free_listp + (6 * WSIZE), PACK(2 * DSIZE, 0)); /* 초기 가용 블럭 footer */
    PUT(free_listp + (7 * WSIZE), PACK(0, 1));         /* 에필로그 header */
    free_listp += (4*WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) /* 최대 할당 크기를 넘었을 때 */
        return -1;
  
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignmnet */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == (void*)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         /* Free block header  */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer  */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /*Coalesce if the previous block was free */
    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignmnet reqs. */
    if (size < DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) 
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0)); 
    PUT(FTRP(bp), PACK(size, 0)); 
    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
    size_t size = GET_SIZE(HDRP(bp)); 

    if(prev_alloc && !next_alloc){
        free_list_delete(NEXT_BLKP(bp)); 
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
    }
    else if(!prev_alloc && next_alloc){
        free_list_delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else if(!prev_alloc && !next_alloc){
        free_list_delete(PREV_BLKP(bp));
        free_list_delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp); 
    }
    free_list_add(bp); 
    return bp;
}

static void *find_fit(size_t asize)
{
    void *bp = free_listp;
    while (bp != NULL) {
        if ((asize <= GET_SIZE(HDRP(bp))))
            return bp;
        bp = NEXT_PTR(bp);
    }
    return NULL; 
}

static void place(void * bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    free_list_delete(bp);
    if ((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize,1)); 
        PUT(FTRP(bp), PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize,0));
        PUT(FTRP(bp), PACK(csize-asize,0));
        free_list_add(bp); 
    }
    else{
        PUT(HDRP(bp), PACK(csize,1));
        PUT(FTRP(bp), PACK(csize,1));
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));  
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void free_list_add(void* bp) 
{
    /* LIFO */
    NEXT_PTR(bp) = free_listp;
    if (free_listp != NULL)
        PREV_PTR(free_listp) = bp;
    free_listp = bp;
}

static void free_list_delete(void* bp) 
{
    if (bp == free_listp) {
        free_listp = NEXT_PTR(bp);
        return;
    }
    NEXT_PTR(PREV_PTR(bp)) = NEXT_PTR(bp);
    if (NEXT_PTR(bp) != NULL)
        PREV_PTR(NEXT_PTR(bp)) = PREV_PTR(bp);
}