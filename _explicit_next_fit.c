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
    "ateam",
    /* First member's full name */
    "Harry Potter",
    /* First member's email address */
    "magic@hogwarts.com",
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


/* Basic constants and macros */
#define WSIZE       4     /* Word and header/footer size (bytes)*/
#define DSIZE       8     /* Double Word size (bytes)*/
#define CHUNKSIZE (1<<9) /* Extend heap by this amount (bytes) */

#define MAX(x,y) ((x) > (y)) ? (x) : (y)

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p)) = (val)

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// #define EFRP(bp) ((char *)(bp) + WSIZE) // Explicit FRee pointer

/* Given block ptr bp, compute address of its header and footer */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define SUCP(bp) (*(void **)(bp))
#define PREP(bp) (*(void **)(bp + WSIZE))


static void *extend_heap(size_t);
static void *coalesce(void *);
static void *find_fit(size_t);
static void place(void *, size_t);
void removeBlock(void *);
void removeBlock2(void *);
void putFreeBlock(void *);
// static void *find_nextp; // next fit
static char *heap_listp; // points to the prologue block or first block
static char *free_listp;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap*/
    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE*2, 1)); /* Prologue header */
    
    PUT(heap_listp + (2*WSIZE), (int)NULL); /* Prologue SUCCESOR, 제일 처음은 root 역할 */
    PUT(heap_listp + (3*WSIZE), (int)NULL); /* Prologue PREDECCESSOR */

    PUT(heap_listp + (4*WSIZE), PACK(DSIZE*2, 1)); /* Prologue footer */
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));     /* Epilogue header */
    // heap_listp += (2*WSIZE); //  Prologue header와 footer 사이로 포인터 위치 옮김
    // find_nextp = heap_listp; // next_fit
    free_listp = heap_listp + DSIZE; // 가용리스트에 블록이 추가될 때마다 항상 리스트의 제일 앞에 추가될 것이므로
                                     // 지금 생성한 Prologue block은 항상 가용리스트의 끝에 위치함
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) // mem_brk지점을 old_ptr로 반환함.
    return NULL;

    /* Allocate an even number of words to maintain alignment */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */ //끝 부분이라서 이전께 비었는지 확인만 하면 됨
    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    /* 책 내용 */
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    /* Ignore spurious requests */ 
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ( ( size + (DSIZE) + (DSIZE-1) ) / DSIZE); // '/'연산자는 '몫'!, 중간 DSIZE는 header와 footer

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

   /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) // extend_heap은 인자가 WORD 단위로 들어감
        return NULL;
    place(bp, asize);
    return bp;
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // 이미 가용리스트에 존재하던 블록은 연결하기 이전에 제거
    if (prev_alloc && next_alloc)
    { /* Case 1 */
        // return bp;
    }

    if (prev_alloc && !next_alloc)
    { /* Case 2 */
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)
    { /* Case 3 */
        removeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else if (!prev_alloc && !next_alloc)
    { /* Case 4 */
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    putFreeBlock(bp);
    // find_nextp = bp;
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free, 사이즈를 줄이거나 늘리는 함수 !
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    // copySize = *(size_t *) ((char *)oldptr - SIZE_T_SIZE); // 원래 들어있던 코드 틀림
    copySize = GET_SIZE(HDRP(oldptr));

    if (size < copySize) // oldptr 사이즈가 새로 만들 newptr의 size보다 더 작은 경우
      copySize = size;
      
    //memcpy(destination, source, num)
    memcpy(newptr, oldptr, copySize); // newptr 위치에 oldptr 주소로부터 copySize 만큼 복사하기 
    mm_free(oldptr);
    return newptr;
}

// first_fit
static void *find_fit(size_t asize)
{
    /* first-fit search */
    void *bp;

    // 가용리스트 내부의 유일한 할당 블록은 맨 뒤의 프롤로그 블록이므로 할당 블록을 만나면 for문 종료
    for (bp = free_listp; GET_ALLOC(HDRP(bp)) != 1; bp = SUCP(bp))
    {
        if (asize <= GET_SIZE(HDRP(bp)))
        {
            // If a fit is found, return the address the of block pointer
            return bp;
        }
    }

    return NULL; /* No fit */
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    removeBlock(bp);
    if ((csize - asize) >= (2 * DSIZE)) // Header와 footer를 포함하여 최소 4 word 필요 !
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        putFreeBlock(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

// // 할당 후 남은 가용 리스트를 루트가 아니라 그대로 앞뒤로 연결하는 방법
// static void place(void *bp, size_t asize)
// {
//     size_t csize = GET_SIZE(HDRP(bp));
//     // removeBlock(bp);
//     if ((csize - asize) >= (2 * DSIZE)) // Header와 footer를 포함하여 최소 4 word 필요 !
//     {
        
//         PUT(HDRP(bp), PACK(asize, 1));
//         PUT(FTRP(bp), PACK(asize, 1));
//         bp = NEXT_BLKP(bp);

//         PUT(HDRP(bp), PACK(csize - asize, 0));
//         PUT(FTRP(bp), PACK(csize - asize, 0));
//         removeBlock2(bp);
//         // putFreeBlock(bp);
//     }
//     else
//     {
//         removeBlock(bp);
//         PUT(HDRP(bp), PACK(csize, 1));
//         PUT(FTRP(bp), PACK(csize, 1));
//     }
// }



// 새로 반환되거나 생성된 가용 블록을 가용리스트 맨 앞에 추가한다.
void putFreeBlock(void *bp){
    PREP(bp) = NULL;
    SUCP(bp) = free_listp;
    PREP(free_listp) = bp;
    free_listp = bp;
}

// 항상 가용리스트 맨 뒤에 프롤로그 블록이 존재하고 있기 때문에 조건을 간소화할 수 있다.
void removeBlock(void *bp){

    // 첫번째 블럭을 없앨 때
    if (bp == free_listp){
        PREP(SUCP(bp)) = NULL;
        free_listp = SUCP(bp);
    }
    // 앞 뒤 모두 있을 때
    else{
        SUCP(PREP(bp)) = SUCP(bp);
        PREP(SUCP(bp)) = PREP(bp);
    }
}

// void removeBlock2(void *bp){

//     // 첫번째 블럭을 없앨 때
//     if (PREV_BLKP(bp) == free_listp){ // 루트일 떄
//         PREP(SUCP(PREV_BLKP(bp))) = bp; 
//         free_listp = bp;
//         SUCP(bp) = SUCP(PREV_BLKP(bp));
//         PREP(bp) = NULL;
//     }
//     // 앞 뒤 모두 있을 때
//     else{
//         SUCP(PREP(PREV_BLKP(bp))) = bp;
//         PREP(SUCP(PREV_BLKP(bp))) = bp;
//         SUCP(bp) = SUCP(PREV_BLKP(bp));
//         PREP(bp) = PREP(PREV_BLKP(bp));
//     }
// }