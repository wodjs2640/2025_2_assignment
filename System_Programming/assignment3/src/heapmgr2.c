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

// 빈 블록 연결용
typedef struct FreeNode
{
    struct FreeNode *prev;
    struct FreeNode *next;
} FreeNode;

#define ALIGN 16UL
#define HDR_SIZE sizeof(Header)
#define FTR_SIZE sizeof(Footer)
#define OVERHEAD (HDR_SIZE + FTR_SIZE)
#define PACK(sz, a) ((sz) | ((a) ? 1UL : 0UL))
#define GET_SIZE(h) ((h)->size_alloc & ~1UL)
#define IS_ALLOC(h) ((h)->size_alloc & 1UL)
#define MIN_FREE sizeof(FreeNode)
#define MIN_BLOCK ((OVERHEAD + MIN_FREE + (ALIGN - 1)) & ~(ALIGN - 1))

static char *heap_lo = NULL;
static char *heap_hi = NULL;
static FreeNode *free_head = NULL;
static FreeNode *rover = NULL;

static inline size_t align_up(size_t x) { return (x + (ALIGN - 1)) & ~(ALIGN - 1); }
static inline Footer *hdr_to_ftr(Header *h) { return (Footer *)((char *)h + GET_SIZE(h) - FTR_SIZE); }
static inline FreeNode *node_of(Header *h) { return (FreeNode *)((char *)h + HDR_SIZE); }

// 다음 블록 찾기
static Header *next_phys(Header *h)
{
    char *p = (char *)h + GET_SIZE(h);

    if (p >= heap_hi)
        return NULL;

    return (Header *)p;
}

// 이전 블록 찾기
static inline Header *prev_phys(Header *h)
{
    if (!heap_lo || (char *)h == heap_lo)
        return NULL;
    Footer *pf = (Footer *)((char *)h - FTR_SIZE);
    size_t psz = pf->size_alloc & ~1UL;
    return (Header *)((char *)h - psz);
}

// free list 앞에 삽입
static void fl_insert_front(Header *h)
{
    FreeNode *n = node_of(h);
    n->prev = NULL;
    n->next = free_head;

    if (free_head)
        free_head->prev = n;

    free_head = n;

    if (!rover)
        rover = n;
}

// free list 에서 제거
static void fl_remove(Header *h)
{
    FreeNode *n = node_of(h);
    FreeNode *p = n->prev, *q = n->next;

    if (p)
        p->next = q;

    else
        free_head = q;

    if (q)
        q->prev = p;

    if (rover == n)
        rover = free_head;

    n->prev = n->next = NULL;
}

// sbrk로 힙 확장
static Header *heap_grow(size_t need)
{
    size_t ask = need < (128 * 1024) ? (128 * 1024) : need;
    ask = align_up(ask);
    void *old = sbrk(ask);

    if (old == (void *)-1)
        return NULL;

    Header *newblk = (Header *)old;
    newblk->size_alloc = PACK(ask, 0);
    hdr_to_ftr(newblk)->size_alloc = newblk->size_alloc;

    if (heap_hi && heap_hi > heap_lo)
    {
        Footer *pf = (Footer *)(heap_hi - FTR_SIZE);
        if ((char *)pf >= heap_lo)
        {
            size_t psz = pf->size_alloc & ~1UL;
            Header *prev = (Header *)(heap_hi - psz);

            if ((char *)prev + GET_SIZE(prev) == (char *)newblk && !IS_ALLOC(prev))
            {
                fl_remove(prev);
                size_t total = GET_SIZE(prev) + ask;
                prev->size_alloc = PACK(total, 0);
                hdr_to_ftr(prev)->size_alloc = prev->size_alloc;
                fl_insert_front(prev);

                heap_hi = (char *)prev + total;
                rover = node_of(prev);
                return prev;
            }
        }
    }

    if (!heap_lo)
        heap_lo = (char *)old;

    heap_hi = (char *)old + ask;
    fl_insert_front(newblk);
    rover = node_of(newblk);
    return newblk;
}

// 병합(양쪽)
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

    if ((char *)h + sz > heap_hi)
        heap_hi = (char *)h + sz;

    return h;
}

// 분할(필요시만)
static Header *split(Header *h, size_t asize)
{
    size_t csize = GET_SIZE(h);
    fl_remove(h);

    if (csize >= asize + MIN_BLOCK)
    {
        Header *alloc = h;
        Header *remain = (Header *)((char *)h + asize);

        alloc->size_alloc = PACK(asize, 1);
        hdr_to_ftr(alloc)->size_alloc = alloc->size_alloc;

        size_t rsz = csize - asize;
        remain->size_alloc = PACK(rsz, 0);
        hdr_to_ftr(remain)->size_alloc = remain->size_alloc;

        fl_insert_front(remain);
        return alloc;
    }

    else
    {
        h->size_alloc = PACK(csize, 1);
        hdr_to_ftr(h)->size_alloc = h->size_alloc;
        return h;
    }
}

// next-fit 탐색
static Header *find_fit(size_t asize)
{
    if (!free_head)
        return NULL;

    if (!rover)
        rover = free_head;

    FreeNode *start = rover;
    FreeNode *it = start;

    do
    {
        Header *h = (Header *)((char *)it - HDR_SIZE);

        if (GET_SIZE(h) >= asize)
        {
            rover = it->next ? it->next : free_head;
            return h;
        }

        it = it->next ? it->next : free_head;
    } while (it && it != start);

    return NULL;
}

// malloc
void *heapmgr_malloc(size_t nbytes)
{
    if (nbytes == 0)
        return NULL;

    if (!heap_lo)
    {
        void *cur = sbrk(0);

        if (cur == (void *)-1)
            return NULL;

        heap_lo = heap_hi = (char *)cur;
    }

    size_t asize = align_up(nbytes + OVERHEAD);

    if (asize < MIN_BLOCK)
        asize = MIN_BLOCK;

    Header *h = find_fit(asize);

    if (!h)
    {
        if (!heap_grow(asize))
            return NULL;

        h = find_fit(asize);

        if (!h)
            return NULL;
    }

    Header *a = split(h, asize);
#ifndef NDEBUG
    assert(check_heap_validity());
#endif
    return (char *)a + HDR_SIZE;
}

// free
void heapmgr_free(void *ptr)
{
    if (!ptr)
        return;
    Header *h = (Header *)((char *)ptr - HDR_SIZE);
    h->size_alloc = PACK(GET_SIZE(h), 0);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;

    Header *m = coalesce(h);
    fl_insert_front(m);

#ifndef NDEBUG
    assert(check_heap_validity());
#endif
}

// 간단 검증
int check_heap_validity(void)
{
    if (!heap_lo || !heap_hi)
        return 1;

    char *p = heap_lo;

    while (p < heap_hi)
    {
        Header *h = (Header *)p;
        size_t sz = GET_SIZE(h);

        if (sz == 0 || (sz % ALIGN))
            return 0;

        if (hdr_to_ftr(h)->size_alloc != h->size_alloc)
            return 0;

        p += sz;
    }

    return (p == heap_hi);
}