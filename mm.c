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
    "이민형, 권지현, 진재혁 ",
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

#define SEG_MAX 12 /* 2의 제곱수 크기만 */
#define GET_ROOT(class) (*(void **)((char *)(free_listp) + (WSIZE * class)))

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize); 
static void place(void *bp, size_t asize);
static void free_list_add(void* bp);
static void free_list_delete(void* bp);
static int get_seg_list_class(size_t size);

static char *free_listp = 0;

/* 
 * mm_init - initialize the malloc package. 힙 초기화
 */
int mm_init(void)
{
    if ((free_listp = mem_sbrk((SEG_MAX + 4) * WSIZE)) == (void *)-1) 
        return -1;
    PUT(free_listp, 0);
    PUT(free_listp + (1 * WSIZE), PACK((SEG_MAX + 2) * WSIZE, 1));
    for (int i = 0; i < SEG_MAX; i++)
        PUT(free_listp + ((2 + i) * WSIZE), NULL);
    PUT(free_listp + ((SEG_MAX + 2) * WSIZE), PACK((SEG_MAX + 2) * WSIZE, 1)); 
    PUT(free_listp + ((SEG_MAX + 3) * WSIZE), PACK(0, 1)); 
    free_listp += (2 * WSIZE);
    
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

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
    if (size <= DSIZE)
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
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc){
        free_list_delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else if(!prev_alloc && !next_alloc){
        free_list_delete(PREV_BLKP(bp));
        free_list_delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); 
    }
    free_list_add(bp); 
    return bp;
}

static void *find_fit(size_t asize)
{
    int size_class = get_seg_list_class(asize);
    void *bp;
    while (size_class < SEG_MAX) {
        bp = GET_ROOT(size_class);
        while (bp != NULL) {
            if (asize <= GET_SIZE(HDRP(bp))) 
                return bp;
            bp = NEXT_FREE_BLK(bp);
        }
        size_class++;
    }
    return NULL;
}

static void place(void * bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    free_list_delete(bp);
    if ((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1)); 
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize  -asize, 0));
        free_list_add(bp); 
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    size_t old_size, new_size;

    if ((int)size <= 0) {
        free(ptr);
        return NULL;
    }
    if (ptr == NULL)
		return (mm_malloc(size));
    
    old_size = GET_SIZE(HDRP(ptr));
    new_size = size + (2*WSIZE);
    if (new_size <= old_size)
        return ptr;
    
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
    size_t total_size = old_size + next_size;
    if (!next_alloc && new_size <= total_size) {
        free_list_delete(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(total_size, 1));
        PUT(FTRP(ptr), PACK(total_size, 1));
        return ptr;
    }
    else {
        newptr = mm_malloc(new_size);
        if (newptr == NULL)
            return NULL;
        memcpy(newptr, ptr, old_size);
        mm_free(ptr);
        return newptr;
    }
}

static void free_list_add(void* bp) 
{
    int size_class = get_seg_list_class(GET_SIZE(HDRP(bp))); /* 추가할 size class 찾기 */
    NEXT_FREE_BLK(bp) = GET_ROOT(size_class);
    if (GET_ROOT(size_class) != NULL) 
        PREV_FREE_BLK(GET_ROOT(size_class)) = bp;
    GET_ROOT(size_class) = bp;
}

static void free_list_delete(void* bp) 
{
    int class = get_seg_list_class(GET_SIZE(HDRP(bp)));
    if (bp == GET_ROOT(class)) {
        GET_ROOT(class) = NEXT_FREE_BLK(GET_ROOT(class)); 
        return;
    }
    NEXT_FREE_BLK(PREV_FREE_BLK(bp)) = NEXT_FREE_BLK(bp);
    if (NEXT_FREE_BLK(bp) != NULL) 
        PREV_FREE_BLK(NEXT_FREE_BLK(bp)) = PREV_FREE_BLK(bp);
}

/* seg_list의 인덱스를 찾는 함수 */
static int get_seg_list_class(size_t size)
{
    int class = 0;
    while (size > 16) {
        size = (size >> 1);
        class++;
    }

    if (class >= SEG_MAX)
        return SEG_MAX - 1;
    return class;
}