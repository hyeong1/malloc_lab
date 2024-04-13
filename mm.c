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
// #define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
// #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE       4       /* Word and header/footer size */
#define DSIZE       8       /* Double word */
#define CHUNKSIZE   (1<<12) /* Extend heap by this amount -mem_brk를 가지고 CHUNKSIZE만큼 힙 영역을 늘림*/

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(ptr)          (*(unsigned int *)(ptr))
#define PUT(ptr, val)     (*(unsigned int *)(ptr) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(ptr)     (GET(ptr) & ~0x7) /* Check size -하위 3비트 제외*/
#define GET_ALLOC(ptr)    (GET(ptr) & 0x1)  /* Check allocated */

/* Compute address of bp's header and footer */
#define HDRP(ptr)        ((char *)(ptr) - WSIZE) 
#define FTRP(ptr)        ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

/* Compute address of next and previous blocks */
#define NEXT_BLKP(ptr)   ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE)))
#define PREV_BLKP(ptr)   ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE))) /* - (이전 블록+지금 블록 크기) == Double word */

static void *extend_heap(size_t words);
static void *coalesce(void *ptr);
static void *first_fit(size_t asize);
static void *next_fit(size_t asize);
static void *best_fit(size_t asize);
static void place(void *ptr, size_t asize);

static char *heap_listp; // 항상 prologue 블록을 가리킨다.
static void *nextptr;

/* 
 * mm_init - initialize the malloc package. 힙 초기화
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                          /* Alignemt padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header -PACK(block size, alloc)*/
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer -PACK(block size, alloc)*/
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header -end*/
    heap_listp += (2*WSIZE);
    nextptr = heap_listp; // next-fit 구현

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}

static void *extend_heap(size_t words)
{
    char *ptr;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; 
    if ((long)(ptr = mem_sbrk(size)) == -1) // 할당 성공했으면 ptr은 epilogue의 바로 다음 주소
        return NULL;

    PUT(HDRP(ptr), PACK(size, 0));           /* Free block header -가용 블록으로 만들기 */
    PUT(FTRP(ptr), PACK(size, 0));           /* Free block footer */
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));   /* New epilogue header -새로운 끝 설정 */

    return coalesce(ptr);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *ptr;

    if (size == 0)
        return NULL;
    
    if (size <= DSIZE)
        asize = 2*DSIZE; /* size는 실제로 사용할 데이터 영역의 크기만 지정 -> 헤더, 풋터 포함하려고 더블워드 2배*/
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); 
    
    if ((ptr = next_fit(asize)) != NULL) {
        place(ptr, asize); /* 요청 블록 배치 */
        return ptr;
    }

    /* 맞는 블록이 없으면 힙 영역을 확장하고 요청 블록을 새 가용 블록에 배치 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((ptr = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(ptr, asize);
    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0)); /* 가용 블록으로 변경 -header의 내용이 아니라 size라서 하위 3비트는 0임 */
    PUT(FTRP(ptr), PACK(size, 0)); 
    coalesce(ptr);
}

static void *coalesce(void *ptr) // 가용 메모리 연결
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (prev_alloc && next_alloc) // case1. 이전과 다음이 모두 할당
        return ptr;
    else if (prev_alloc && !next_alloc) { // case2. 이전만 할당
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr))); // 다음 리스트랑 연결
        PUT(HDRP(ptr), PACK(size, 0)); // header의 블록 사이즈 변경 + 가용 블록 명시
        PUT(FTRP(ptr), PACK(size, 0)); // footer의 블록 사이즈 변경
    }
    else if (!prev_alloc && next_alloc) { // case3. 다음만 할당
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))); // 이전 리스트랑 연결
        PUT(FTRP(ptr), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr); // 지금 ptr을 이전 리스트 시작으로 변경
    }
    else { // case4. 이전과 다음이 모두 가용
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    nextptr = ptr;
    return ptr;
}

static void *first_fit(size_t asize) 
{
    void *ptr;
    for (ptr = heap_listp; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
        if (!GET_ALLOC(HDRP(ptr)) && (asize <= GET_SIZE(HDRP(ptr)))) {
            return ptr;
        }
    }
    return NULL;
}

static void *next_fit(size_t asize) 
{
    void *ptr;
    for (ptr = nextptr; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
        if (!GET_ALLOC(HDRP(ptr)) && (asize <= GET_SIZE(HDRP(ptr)))) {
            nextptr = ptr;
            return ptr;
        }
    }
    return NULL;
}

static void *best_fit(size_t asize) 
{
    void *ptr;
    void *fitptr = NULL;
    for (ptr = heap_listp; GET_SIZE(HDRP(ptr)) > 0; ptr = NEXT_BLKP(ptr)) {
        if (!GET_ALLOC(HDRP(ptr)) &&(asize <= GET_SIZE(HDRP(ptr)))) {
            if (fitptr == NULL)
                fitptr = ptr;
            else
                fitptr = GET_SIZE(ptr) < GET_SIZE(fitptr) ? ptr : fitptr;
        }
    }
    return fitptr;
}

static void place(void * ptr, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(ptr)); /* 현재 가용 블록의 크기 */

    if ((csize - asize) >= (2*DSIZE)) { /* 남은 공간이 최소 블록 크기 이상일 때 */
        PUT(HDRP(ptr), PACK(asize, 1));
        PUT(FTRP(ptr), PACK(asize, 1));
        ptr = NEXT_BLKP(ptr);
        PUT(HDRP(ptr), PACK(csize-asize, 0)); /* 분할 */
        PUT(FTRP(ptr), PACK(csize-asize, 0));
    }
    else {
        PUT(HDRP(ptr), PACK(csize, 1));
        PUT(FTRP(ptr), PACK(csize, 1));
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














