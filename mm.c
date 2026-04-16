/*
 * mm.c - boundary tag와 단일 연결 free 리스트를 사용하는 malloc 패키지.
 *
 * 일반 free 블록은 explicit한 단일 연결 리스트로 관리하고, 정확히 8바이트인
 * free 블록은 tiny block으로 취급하여 free 리스트에 넣지 않는다.
 * tiny block은 탐색 대상에서는 제외되지만, 병합(coalescing)에는 정상적으로
 * 참여한다. realloc은 가능한 경우 오른쪽 free 블록을 이용한 제자리 확장을
 * 우선 시도하고, 실패하면 새 블록을 할당한 뒤 데이터를 복사한다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * 학생 안내: 다른 작업을 하기 전에 먼저 아래 구조체에
 * 팀 정보를 입력하세요.
 ********************************************************/
team_t team = {
    /* 팀 이름 */
    "ateam",
    /* 첫 번째 팀원의 이름 */
    "Harry Bovik",
    /* 첫 번째 팀원의 이메일 주소 */
    "bovik@cs.cmu.edu",
    /* 두 번째 팀원의 이름 (없으면 빈칸) */
    "",
    /* 두 번째 팀원의 이메일 주소 (없으면 빈칸) */
    ""
};

/* 기본 크기 상수 */
#define WSIZE           4
#define DSIZE           8
#define CHUNKSIZE       (1 << 12)

/* 설계 규칙 */
#define TINY_SIZE       8
#define MIN_FREE_SIZE   16

/* 정렬 */
#define ALIGNMENT       8
#define ALIGN(size)     (((size) + (ALIGNMENT - 1)) & ~0x7)

/* 헤더/푸터 값 만들기 */
#define PACK(size, alloc)   ((size) | (alloc))

/* 읽기/쓰기 */
#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

/* 크기 / 할당비트 추출 */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* 블록 주소 계산 */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* 블록 종류 판별 */
#define IS_TINY(bp)         (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) == TINY_SIZE))
#define IS_NORMAL_FREE(bp)  (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= MIN_FREE_SIZE))

/* 일반 free 리스트용 next 포인터 */
#define NEXT_FREEP(bp)      (*(char **)(bp))

/* 전역 포인터 */
static char *heap_listp = NULL;
static char *free_listp = NULL;

/* 내부 보조 함수 */
static size_t adjust_block_size(size_t size);
static void insert_free_block(void *bp);
static void remove_free_block(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void *extend_heap(size_t bytes);

/* 
 * mm_init - malloc 패키지를 초기화한다.
 */
int mm_init(void)
{
    free_listp = NULL;

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                              /* padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  /* prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  /* prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      /* epilogue header */

    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE) == NULL)
        return -1;

    return 0;
}

/* 
 * mm_malloc - brk 포인터를 증가시켜 블록을 할당한다.
 *     항상 정렬 단위의 배수 크기로 블록을 할당한다.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    asize = adjust_block_size(size);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = (asize > CHUNKSIZE) ? asize : CHUNKSIZE;
    if ((bp = extend_heap(extendsize)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

/*
 * mm_free - 블록을 해제해도 아무 동작도 하지 않는다.
 */
void mm_free(void *ptr)
{
    size_t size;
    char *bp;

    if (ptr == NULL)
        return;

    size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    bp = coalesce(ptr);
    if (IS_NORMAL_FREE(bp))
        insert_free_block(bp);
}

/*
 * mm_realloc - mm_malloc과 mm_free를 이용해 단순하게 구현한다.
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t asize;
    size_t oldsize;
    size_t nextsize;
    size_t total;
    size_t rem;
    size_t copySize;
    char *next_bp;
    char *newptr;

    if (ptr == NULL)
        return mm_malloc(size);

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    asize = adjust_block_size(size);
    oldsize = GET_SIZE(HDRP(ptr));

    /* 줄이는 경우: split 규칙을 그대로 적용한다. */
    if (asize <= oldsize) {
        rem = oldsize - asize;

        if (rem >= TINY_SIZE) {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));

            next_bp = NEXT_BLKP(ptr);
            PUT(HDRP(next_bp), PACK(rem, 0));
            PUT(FTRP(next_bp), PACK(rem, 0));

            next_bp = coalesce(next_bp);
            if (IS_NORMAL_FREE(next_bp))
                insert_free_block(next_bp);
        }

        return ptr;
    }

    /* 오른쪽 free 블록을 이용한 제자리 확장을 우선한다. */
    next_bp = NEXT_BLKP(ptr);
    if (!GET_ALLOC(HDRP(next_bp))) {
        nextsize = GET_SIZE(HDRP(next_bp));
        total = oldsize + nextsize;

        if (total >= asize) {
            if (IS_NORMAL_FREE(next_bp))
                remove_free_block(next_bp);

            rem = total - asize;

            if (rem >= TINY_SIZE) {
                PUT(HDRP(ptr), PACK(asize, 1));
                PUT(FTRP(ptr), PACK(asize, 1));

                next_bp = NEXT_BLKP(ptr);
                PUT(HDRP(next_bp), PACK(rem, 0));
                PUT(FTRP(next_bp), PACK(rem, 0));

                if (IS_NORMAL_FREE(next_bp))
                    insert_free_block(next_bp);
            } else {
                PUT(HDRP(ptr), PACK(total, 1));
                PUT(FTRP(ptr), PACK(total, 1));
            }

            return ptr;
        }
    }

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;

    copySize = oldsize - DSIZE;
    if (size < copySize)
        copySize = size;
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    return newptr;
}

static size_t adjust_block_size(size_t size)
{
    size_t asize;

    asize = ALIGN(size + DSIZE);
    if (asize < MIN_FREE_SIZE)
        asize = MIN_FREE_SIZE;

    return asize;
}

static void insert_free_block(void *bp)
{
    NEXT_FREEP(bp) = free_listp;
    free_listp = bp;
}

static void remove_free_block(void *bp)
{
    char *prev = NULL;
    char *curr = free_listp;

    while (curr != NULL) {
        if (curr == bp) {
            if (prev == NULL)
                free_listp = NEXT_FREEP(curr);
            else
                NEXT_FREEP(prev) = NEXT_FREEP(curr);
            return;
        }
        prev = curr;
        curr = NEXT_FREEP(curr);
    }
}

static void *find_fit(size_t asize)
{
    char *bp = free_listp;

    while (bp != NULL) {
        if (GET_SIZE(HDRP(bp)) >= asize)
            return bp;
        bp = NEXT_FREEP(bp);
    }

    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    size_t rem = csize - asize;
    char *next_bp;

    if (IS_NORMAL_FREE(bp))
        remove_free_block(bp);

    if (rem == 0) {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        return;
    }

    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));

    next_bp = NEXT_BLKP(bp);
    PUT(HDRP(next_bp), PACK(rem, 0));
    PUT(FTRP(next_bp), PACK(rem, 0));

    if (rem >= MIN_FREE_SIZE)
        insert_free_block(next_bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    char *prev_bp = PREV_BLKP(bp);
    char *next_bp = NEXT_BLKP(bp);

    if (prev_alloc && next_alloc) {
        return bp;
    }

    if (prev_alloc && !next_alloc) {
        if (IS_NORMAL_FREE(next_bp))
            remove_free_block(next_bp);

        size += GET_SIZE(HDRP(next_bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return bp;
    }

    if (!prev_alloc && next_alloc) {
        if (IS_NORMAL_FREE(prev_bp))
            remove_free_block(prev_bp);

        size += GET_SIZE(HDRP(prev_bp));
        PUT(HDRP(prev_bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return prev_bp;
    }

    if (IS_NORMAL_FREE(prev_bp))
        remove_free_block(prev_bp);
    if (IS_NORMAL_FREE(next_bp))
        remove_free_block(next_bp);

    size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
    PUT(HDRP(prev_bp), PACK(size, 0));
    PUT(FTRP(next_bp), PACK(size, 0));
    return prev_bp;
}

static void *extend_heap(size_t bytes)
{
    char *bp;
    size_t size;

    size = ALIGN(bytes);
    if (size < MIN_FREE_SIZE)
        size = MIN_FREE_SIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    bp = coalesce(bp);
    if (IS_NORMAL_FREE(bp))
        insert_free_block(bp);

    return bp;
}













