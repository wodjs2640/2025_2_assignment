#include <unistd.h>
#include <stddef.h>
#include <assert.h>
#include "../test/heapmgr.h"

// 블록 정보 구조체
typedef struct Header
{
    size_t size_alloc; // 블록 크기 + 할당여부
} Header;

typedef Header Footer;

// 빈 블록을 연결할 노드
typedef struct FreeNode
{
    struct FreeNode *prev;
    struct FreeNode *next;
} FreeNode;

// 상수 정의
#define ALIGN 16UL
#define HDR_SIZE sizeof(Header)
#define FTR_SIZE sizeof(Footer)
#define OVERHEAD (HDR_SIZE + FTR_SIZE)
#define PACK(sz, a) ((sz) | ((a) ? 1UL : 0UL))
#define GET_SIZE(h) ((h)->size_alloc & ~1UL)
#define IS_ALLOC(h) ((h)->size_alloc & 1UL)
#define MIN_FREE sizeof(FreeNode)
#define MIN_BLOCK ((OVERHEAD + MIN_FREE + (ALIGN - 1)) & ~(ALIGN - 1))

// 전역 변수
static char *heap_lo = NULL;
static char *heap_hi = NULL;
static FreeNode *free_head = NULL;

// 정렬 함수
static size_t align_up(size_t x)
{
    return (x + (ALIGN - 1)) & ~(ALIGN - 1);
}

// 헤더→푸터
static Footer *hdr_to_ftr(Header *h)
{
    return (Footer *)((char *)h + GET_SIZE(h) - FTR_SIZE);
}

// 다음 블록
static Header *next_phys(Header *h)
{
    char *p = (char *)h + GET_SIZE(h);
    if (p >= heap_hi)
        return NULL;
    return (Header *)p;
}

// 이전 블록
static Header *prev_phys(Header *h)
{
    if ((char *)h == heap_lo)
        return NULL;
    Footer *pf = (Footer *)((char *)h - FTR_SIZE);
    size_t psz = pf->size_alloc & ~1UL;
    return (Header *)((char *)h - psz);
}

// 노드 주소
static FreeNode *node_of(Header *h)
{
    return (FreeNode *)((char *)h + HDR_SIZE);
}

// 빈 블록 리스트 앞에 넣기
static void fl_insert(Header *h)
{
    FreeNode *n = node_of(h);
    n->prev = NULL;
    n->next = free_head;
    if (free_head)
        free_head->prev = n;
    free_head = n;
}

// 리스트에서 제거
static void fl_remove(Header *h)
{
    FreeNode *n = node_of(h);
    if (n->prev)
        n->prev->next = n->next;
    else
        free_head = n->next;
    if (n->next)
        n->next->prev = n->prev;
}

// 새 메모리 요청
static Header *heap_grow(size_t need)
{
    size_t ask = (need < 64 * 1024 ? 64 * 1024 : need);
    ask = align_up(ask);
    void *old = sbrk(ask);
    if (old == (void *)-1)
        return NULL;

    if (!heap_lo)
        heap_lo = old;
    heap_hi = (char *)old + ask;

    Header *h = (Header *)old;
    h->size_alloc = PACK(ask, 0);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;
    fl_insert(h);
    return h;
}

// 병합
static Header *coalesce(Header *h)
{
    Header *p = prev_phys(h);
    Header *n = next_phys(h);
    size_t sz = GET_SIZE(h);

    if (p && !IS_ALLOC(p))
    {
        fl_remove(p);
        sz += GET_SIZE(p);
        h = p;
    }
    if (n && !IS_ALLOC(n))
    {
        fl_remove(n);
        sz += GET_SIZE(n);
    }

    h->size_alloc = PACK(sz, 0);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;
    return h;
}

// 분할
static Header *split(Header *h, size_t asize)
{
    size_t csize = GET_SIZE(h);
    fl_remove(h);

    if (csize - asize >= MIN_BLOCK)
    {
        Header *alloc = h;
        Header *remain = (Header *)((char *)h + asize);

        alloc->size_alloc = PACK(asize, 1);
        hdr_to_ftr(alloc)->size_alloc = alloc->size_alloc;

        remain->size_alloc = PACK(csize - asize, 0);
        hdr_to_ftr(remain)->size_alloc = remain->size_alloc;
        fl_insert(remain);

        return alloc;
    }
    else
    {
        h->size_alloc = PACK(csize, 1);
        hdr_to_ftr(h)->size_alloc = h->size_alloc;
        return h;
    }
}

// malloc 함수
void *heapmgr_malloc(size_t nbytes)
{
    if (nbytes == 0)
        return NULL;
    if (!heap_lo)
    {
        void *cur = sbrk(0);
        heap_lo = heap_hi = cur;
    }

    size_t asize = align_up(nbytes + OVERHEAD);
    if (asize < MIN_BLOCK)
        asize = MIN_BLOCK;

    for (FreeNode *it = free_head; it; it = it->next)
    {
        Header *h = (Header *)((char *)it - HDR_SIZE);
        if (GET_SIZE(h) >= asize)
        {
            Header *a = split(h, asize);
            return (char *)a + HDR_SIZE;
        }
    }

    Header *newh = heap_grow(asize);
    Header *a = split(newh, asize);
    return (char *)a + HDR_SIZE;
}

// free 함수
void heapmgr_free(void *ptr)
{
    if (!ptr)
        return;
    Header *h = (Header *)((char *)ptr - HDR_SIZE);
    h->size_alloc = PACK(GET_SIZE(h), 0);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;

    Header *m = coalesce(h);
    fl_insert(m);
}

// 유효성 검사
int check_heap_validity(void)
{
    char *p = heap_lo;
    while (p && p < heap_hi)
    {
        Header *h = (Header *)p;
        size_t sz = GET_SIZE(h);
        if (sz == 0 || sz % ALIGN)
            return 0;
        if (hdr_to_ftr(h)->size_alloc != h->size_alloc)
            return 0;
        p += sz;
    }
    return 1;
}