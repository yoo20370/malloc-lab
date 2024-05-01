
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"
//gdfhdgh
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 책에 나온 매크로 정의하기
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// 함수 선언
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *find_expendSpace(void *ptr, size_t e_size);
void *mm_realloc(void *ptr, size_t size);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *searchFreeBlock(void *bp, size_t asize);

/*
 * mm_init - initialize the malloc package.
 */
// 첫 번째 블록 bp를 가리키는 포인터
void *heap_listp;

void *recent_bp = NULL;

void *bestBp = NULL;
// 초기화
int mm_init(void)
{
    // printf("###################### mm_init ######################");
    //  초기화 과정이 성공적이라면 0 그렇지 않으면 -1을 반환
    heap_listp = mem_sbrk(4 * WSIZE);
    if (heap_listp == (void *)-1)
    {
        return -1;
    }

    // 빈 패딩을 채운다.
    PUT(heap_listp, 0);
    // 프롤로그 헤더를 추가한다.
    PUT(heap_listp + 1 * WSIZE, PACK(DSIZE, 1));
    // 프롤로그 푸터를 추가한다.
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));
    // 에필로그 헤더를 추가한다.
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));
    heap_listp += 2 * WSIZE;
    recent_bp = heap_listp;

    // 힙을 페이지 크기(4096Byte == 4KB) 만큼 확장하기 위해서 수행
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // printf("###################### mm_malloc ######################");
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
    {
        return NULL;
    }

    if (size <= DSIZE)
    {
        asize = 2 * DSIZE;
    }
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    bp = find_fit(asize);
    if (bp != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    bp = extend_heap(extendsize / WSIZE);
    if (bp == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    // printf("###################### mm_free ######################");
    //  해제할 블록의 사이즈 가져오기
    size_t size = GET_SIZE(HDRP(ptr));

    // 가용 블록으로 만든다.
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);

    // 댕글리 포인터가 아무것도 가리키지 않게 만든다.
    ptr = 0;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */

void *find_expendSpace(void *bp, size_t e_size)
{
    // printf("###################### find_expendSpace ######################");
    //  이전 블록 가용 상태 반환
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    // 다음 블록 가용 상태 반환
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 현재 블록의 포인터 반환
    size_t size = GET_SIZE(HDRP(bp));

    // 가용 블록이 둘 다 아닌 경우
    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    // 다음 블록만 가용 블록인 경우
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        if (e_size <= size)
        {
            // 현재 블록의 헤더를 헤더로 설정
            PUT(HDRP(bp), PACK(size, 0));

            // 다음 블록의 푸터를 푸터로 설정
            PUT(FTRP(bp), PACK(size, 0));
            return bp;
        }
        else
        {
            return NULL;
        }
    }
    // 이전 블록만 가용 블록인 경우
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        if (e_size <= size)
        {

            // 이전 블록의 푸터를 헤더로 설정
            PUT(FTRP(bp), PACK(size, 0));

            // 현재 블록의 헤더를 푸터로 설정
            PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

            // bp가 변경되었으므로 갱신
            return PREV_BLKP(bp);
        }
        else
        {
            return NULL;
        }
    }
    // 이전 블록 다음 블록 모두 가용 블록인 경우
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        if (e_size < size)
        {

            // 이전 블록의 헤더를 헤더로
            PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

            // 다음 블록의 헤더를 헤더로
            PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

            // bp가 변경되었으므로 갱신
            return PREV_BLKP(bp);
        }
        else
        {
            return NULL;
        }
    }
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize; 

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t oldSize = GET_SIZE(HDRP(ptr));

    if(oldSize == size){
        return ptr;
    }

    if(prev_alloc && !next_alloc){
        oldSize += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        if(size < oldSize){
            PUT(HDRP(ptr), PACK(oldSize,0));
            PUT(FTRP(ptr), PACK(oldSize,0));
            size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
            place(ptr, size);
            return ptr;
        }
    }

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    
    copySize = GET_SIZE(HDRP(ptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;

}


// 힙 영역 확장
static void *extend_heap(size_t words)
{

    // printf("###################### extend_heap ######################");
    char *bp;
    size_t size;

    // 홀수인 경우 8의 배수로 만들어주기 위한 삼항 연산자
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    // 동적 할당에 실패했을 때 리턴
    (long)(bp = mem_sbrk(size));
    if (bp == (long)-1)
    {
        return NULL;
    }

    // 가용 블록 생성(헤더, 푸터)
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    recent_bp = bp;
    // 에필로그 블록 생성
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 인접한 가용 리스트 연결
    return coalesce(bp);
}

// 인접한 가용 리스트 연결을 위한 함수
static void *coalesce(void *bp)
{

    // printf("###################### coalesce ######################");
    //  이전 블록 가용 상태 반환
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    // 다음 블록 가용 상태 반환
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 현재 블록의 포인터 반환
    size_t size = GET_SIZE(HDRP(bp));

    // 가용 블록이 둘 다 아닌 경우
    if (prev_alloc && next_alloc)
    {
        return bp;
    }

    // 다음 블록만 가용 블록인 경우
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // 현재 블록의 헤더를 헤더로 설정
        PUT(HDRP(bp), PACK(size, 0));

        // 다음 블록의 푸터를 푸터로 설정
        // PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        PUT(FTRP(bp), PACK(size, 0));

        
    }
    // 이전 블록만 가용 블록인 경우
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        // 이전 블록의 푸터를 헤더로 설정
        PUT(FTRP(bp), PACK(size, 0));

        // 현재 블록의 헤더를 푸터로 설정
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        // bp가 변경되었으므로 갱신
        bp = PREV_BLKP(bp);
    }
    // 이전 블록 다음 블록 모두 가용 블록인 경우
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        // 이전 블록의 헤더를 헤더로
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        // 다음 블록의 헤더를 헤더로
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

        // bp가 변경되었으므로 갱신
        bp = PREV_BLKP(bp);
    }
    
    recent_bp = bp;
    return bp;
}

static void *searchFreeBlock(void *bp, size_t asize)
{
    // printf("###################### searchFreeBlock ######################");
    for (; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        // 가용 블록에 저장 가능 여부 확인
        if (!GET_ALLOC(HDRP(bp)) && asize <= GET_SIZE(HDRP(bp)))
        {
            return bp;
        }
    }
    return NULL;
}

// static void * find_fit(size_t asize){
//   //printf("###################### find_fit ######################");
//     void *bp;

//     // first-fit

//     // 가용 블록을 순차적으로 접근
//     bp = searchFreeBlock(heap_listp, asize);

//     // 적당한 가용 블록을 찾지 못함
//     return bp;

// }

// static void *find_fit(size_t asize)
// {
//     // void *bp;
//     // // next-fit

//     // // 최근에 탐색한 가용 블록이 없는 경우
//     // if (recent_bp == NULL)
//     // {
//     //     bp = heap_listp;
//     // }
//     // else
//     // {
//     //     // 최근에 탐색한 가용 블록이 있는 경우
//     //     bp = recent_bp;
//     // }
//     // for (; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
//     // {
//     //     // 가용 블록에 저장 가능 여부 확인
//     //     if (!GET_ALLOC(HDRP(bp)) && asize <= GET_SIZE(HDRP(bp)))
//     //     {
//     //         return bp;
//     //     }
//     // }
//     // // 적당한 가용 블록을 찾지 못함
//     // return NULL;
//     void *bp = NULL;
//     // next-fit
    
//     // 최근에 탐색한 가용 블록이 없는 경우 
//     if(recent_bp == NULL){
//         bp = heap_listp;
//         bp = searchFreeBlock(bp, asize);
        
//     } else {
//         // 최근에 탐색한 가용 블록이 있는 경우
//         bp = recent_bp;
//         // 최근에 찾은 가용 블록 위치 부터 탐색 
//         bp = searchFreeBlock(bp, asize);
//         if(bp == NULL){
//             // 최근부터 끝까지 찾았으나 없다면 처음부터 최근 가용 블록 이전까지 탐색 수행 
//             for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
//                 if(HDRP(bp) != HDRP(recent_bp)){
//                     return NULL;
//                 }
//                 return bp;
//             }
//         }
//     }
//     // 적당한 가용 블록을 찾지 못함 
//     return bp;
// }

// static void *find_fit(size_t asize)
// {
//     // printf("find_fit 함수 호출\n");
//     void *bp;
//     void *bestBp = NULL;

//     // best-fit
//     // 가용 블록을 순차적으로 접근
//     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
//     {
//         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
//         {
//             // 아직 bestBp를 찾지 못한 경우
//             if (bestBp == NULL)
//             {
//                 bestBp = bp;
//             }
//             else
//             {
//                 if (GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(bestBp)))
//                 {
//                     bestBp = bp;
//                 }
//             }
//         }
//     }

//     // 적당한 가용 블록을 찾지 못함
//     return bestBp;
// }

static void *find_fit(size_t asize) {
    void * bp;
    
    // 최근 부분 부터 끝 까지 
    for(bp = recent_bp ; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            recent_bp = bp;
            return bp;
        }
    }

    // 끝까지 탐색했다면 처음부터 최근 부분 까지 진행 
    for(bp = heap_listp; bp != recent_bp; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
            recent_bp = bp;
            return bp;
        }
    }

    return NULL;

}

// static void *find_fit(size_t asize) {
//     void * bp;
    
//     // 최근 부분 부터 끝 까지 
//     for(bp = recent_bp ; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
//         if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
//             recent_bp = bp;
//             return bp;
//         }
//     }

//     // 끝까지 탐색했다면 처음부터 최근 부분 까지 진행 
//     for(bp = heap_listp; bp != recent_bp; bp = NEXT_BLKP(bp)){
//         if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
//             recent_bp = bp;
//             return bp;
//         }
//     }

//     return NULL;

// }

// 할당 후 나머지 가용 블록으로 변환
static void place(void *bp, size_t asize)
{
    // printf("###################### place ######################");
    //  현재 가용 블록의 크기를 가져온다.
    size_t csize = GET_SIZE(HDRP(bp));

    // 나머지가 16byte보다 크거나 같다면 블록을 분할해야 함
    if ((csize - asize) >= 2 * DSIZE)
    {
        // 블록 할당
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        // 남은 나머지 블록 가용 블록으로 변환
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        recent_bp = bp;
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        recent_bp = NEXT_BLKP(bp);
        
    }
}
