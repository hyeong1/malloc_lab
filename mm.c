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
#define CHUNKSIZE   (1<<8) /* Extend heap by this amount -mem_brk를 가지고 CHUNKSIZE만큼 힙 영역을 늘림*/

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
#define PREV_FREE_BLK(ptr)   (*(char **)(ptr)) 
#define NEXT_FREE_BLK(ptr)   (*(char **)(ptr + WSIZE))

#define SEG_MAX 29 /* 2^4 ~ 2^32(최대 블록 크기 16바이트(4*word)) */

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize); 
static void place(void *bp, size_t asize);
static void free_list_add(void* bp);
static void free_list_delete(void* bp);
static int get_seg_list_root(size_t size);

static char *free_listp = 0;
static void *seg_list[SEG_MAX];

/* 
 * mm_init - initialize the malloc package. 힙 초기화
 */
int mm_init(void)
{
    /* Init Seglist */
    for (int i = 0; i < SEG_MAX; i++)
        seg_list[i] = NULL;

    if ((free_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(free_listp, 0);                                /* Alignment padding -heap의 시작점 */
    PUT(free_listp + (1 * WSIZE), PACK(DSIZE, 1));     /* 프롤로그 header */
    PUT(free_listp + (2 * WSIZE), PACK(DSIZE, 1));     /* 프롤로그 footer */
    PUT(free_listp + (3 * WSIZE), PACK(0, 1));         /* 에필로그 header */
    free_listp += (2*WSIZE);

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
    void *bp;
    int idx = get_seg_list_root(asize);
    /* First fit */
    void *tmp = NULL;
    while (idx < SEG_MAX) {
        for (bp = seg_list[idx]; bp != NULL; bp = NEXT_FREE_BLK(bp)) {
            if (asize <= GET_SIZE(HDRP(bp))) {
                if (tmp == NULL)
                    tmp = bp;
                else {
                    tmp = GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(tmp)) ? bp : tmp;
                }
            }
        }
        if (tmp != NULL)
            return tmp;
        idx++;
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
    int seg_idx = get_seg_list_root(GET_SIZE(HDRP(bp))); /* 추가할 size class 찾기 */
    /* LIFO */
    if (seg_list[seg_idx] == NULL) {
        PREV_FREE_BLK(bp) = NULL;
        NEXT_FREE_BLK(bp) = NULL;
    }
    else {
        PREV_FREE_BLK(bp) = NULL;
        NEXT_FREE_BLK(bp) = seg_list[seg_idx];
        PREV_FREE_BLK(seg_list[seg_idx]) = bp;
    }
    seg_list[seg_idx] = bp;
}

static void free_list_delete(void* bp) 
{
    int seg_idx = get_seg_list_root(GET_SIZE(HDRP(bp))); /* 추가할 size class 찾기 */
    if (PREV_FREE_BLK(bp) == NULL) {
        if (NEXT_FREE_BLK(bp) == NULL) /* 리스트에 한 블록만 존재 */
            seg_list[seg_idx] = NULL;
        else {
            PREV_FREE_BLK(NEXT_FREE_BLK(bp)) = NULL;
            seg_list[seg_idx] = NEXT_FREE_BLK(bp);
        }
    }
    else {
        if (NEXT_FREE_BLK(bp) == NULL) 
            NEXT_FREE_BLK(PREV_FREE_BLK(bp)) = NULL;
        else {
            NEXT_FREE_BLK(PREV_FREE_BLK(bp)) = NEXT_FREE_BLK(bp);
            PREV_FREE_BLK(NEXT_FREE_BLK(bp)) = PREV_FREE_BLK(bp);
        }
    }
}

/* seg_list의 인덱스를 찾는 함수 */
static int get_seg_list_root(size_t size)
{
    int idx = 0;
    while (size > 16) {
        size = (size >> 1);
        idx++;
    }
    return idx;
}