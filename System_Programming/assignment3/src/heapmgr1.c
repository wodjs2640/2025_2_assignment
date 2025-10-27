#include <unistd.h>
#include <stddef.h>
#include <assert.h>
#include "heapmgr.h"

typedef struct Header
{
    size_t size_alloc;
} Header;

typedef Header Footer;

typedef struct FreeNode
{
    struct FreeNode *prev;
    struct FreeNode *next;
} FreeNode;

#define ALIGN 16UL
#define HDR_SIZE (sizeof(Header))
#define FTR_SIZE (sizeof(Footer))
#define OVERHEAD (HDR_SIZE + FTR_SIZE)
#define PACK(sz, a) ((sz) | ((a) ? 1UL : 0UL))
#define GET_SIZE(h) ((h)->size_alloc & ~1UL)
#define IS_ALLOC(h) ((h)->size_alloc & 1UL)

#define MIN_FREE_PAYLOAD sizeof(FreeNode)
#define MIN_BLOCK_SIZE ((OVERHEAD + MIN_FREE_PAYLOAD + (ALIGN - 1)) & ~(ALIGN - 1))

static char *heap_lo = NULL;
static char *heap_hi = NULL;
static FreeNode *free_head = NULL;

static inline size_t align_up(size_t x) { return (x + (ALIGN - 1)) & ~(ALIGN - 1); }
static inline Footer *hdr_to_ftr(Header *h) { return (Footer *)((char *)h + GET_SIZE(h) - FTR_SIZE); }
static inline Header *next_phys(Header *h)
{
    char *p = (char *)h + GET_SIZE(h);
    if (p >= heap_hi)
        return NULL;
    return (Header *)p;
}

static inline Header *prev_phys(Header *h)
{
    if ((char *)h == heap_lo)
        return NULL;
    Footer *pf = (Footer *)((char *)h - FTR_SIZE);
    size_t psz = pf->size_alloc & ~1UL;
    return (Header *)((char *)h - psz);
}

static inline FreeNode *node_of(Header *h) { return (FreeNode *)((char *)h + HDR_SIZE); }

static void fl_insert_front(Header *h)
{
    FreeNode *n = node_of(h);
    n->prev = NULL;
    n->next = free_head;
    if (free_head)
        free_head->prev = n;
    free_head = n;
}

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
    fl_insert_front(h);
    return h;
}

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

static Header *split(Header *h, size_t asize)
{
    size_t csize = GET_SIZE(h);
    if (csize - asize >= MIN_BLOCK_SIZE)
    {
        fl_remove(h);
        Header *alloc = h;
        Header *remain = (Header *)((char *)h + asize);
        alloc->size_alloc = PACK(asize, 1);
        hdr_to_ftr(alloc)->size_alloc = alloc->size_alloc;
        remain->size_alloc = PACK(csize - asize, 0);
        hdr_to_ftr(remain)->size_alloc = remain->size_alloc;
        fl_insert_front(remain);
        return alloc;
    }
    fl_remove(h);
    h->size_alloc = PACK(csize, 1);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;
    return h;
}

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
    if (asize < MIN_BLOCK_SIZE)
        asize = MIN_BLOCK_SIZE;
    assert(check_heap_validity());
    for (FreeNode *it = free_head; it; it = it->next)
    {
        Header *h = (Header *)((char *)it - HDR_SIZE);
        if (GET_SIZE(h) >= asize)
        {
            Header *a = split(h, asize);
            assert(check_heap_validity());
            return (char *)a + HDR_SIZE;
        }
    }
    Header *newh = heap_grow(asize);
    Header *a = split(newh, asize);
    assert(check_heap_validity());
    return (char *)a + HDR_SIZE;
}

void heapmgr_free(void *ptr)
{
    if (!ptr)
        return;
    Header *h = (Header *)((char *)ptr - HDR_SIZE);
    h->size_alloc = PACK(GET_SIZE(h), 0);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;
    Header *m = coalesce(h);
    fl_insert_front(m);
    assert(check_heap_validity());
}

int check_heap_validity(void)
{
#ifndef NDEBUG
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
#endif
    return 1;
}