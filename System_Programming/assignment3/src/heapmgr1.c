/*--------------------------------------------------------------------*/
/* heapmgr1.c — boundary-tag allocator with a doubly-linked free list */
/*--------------------------------------------------------------------*/
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "heapmgr.h"

/* -------------------- Layout & Macros -------------------- */
/* Block layout (boundary tag):
   [Header | payload ... | Footer]
   Header/Footer store: size (including header+footer) and alloc bit.
   Free blocks additionally carry prev/next pointers stored in payload
   immediately after header. */

typedef struct Header
{
    size_t size_alloc; /* low bit = 1 if allocated, 0 if free */
} Header;

typedef Header Footer;

typedef struct FreeNode
{
    struct FreeNode *prev;
    struct FreeNode *next;
} FreeNode;

/* Alignment to 16 bytes to match typical CHUNK_UNIT. */
enum
{
    ALIGN = 16
};
static inline size_t align_up(size_t x) { return (x + (ALIGN - 1)) & ~(size_t)(ALIGN - 1); }

/* Sizes that include header+footer. Minimum free block has space for a
   FreeNode (prev,next) to link into the free list. */
#define HDR_SIZE (sizeof(Header))
#define FTR_SIZE (sizeof(Footer))
#define OVERHEAD (HDR_SIZE + FTR_SIZE)
#define MIN_FREE_PAYLD (sizeof(FreeNode))
#define MIN_FREE_SIZE align_up(OVERHEAD + MIN_FREE_PAYLD)

#define IS_ALLOC(h) ((h)->size_alloc & 1UL)
#define SET_ALLOC(h) ((h)->size_alloc |= 1UL)
#define SET_FREE(h) ((h)->size_alloc &= ~1UL)
#define GET_SIZE(h) ((h)->size_alloc & ~(size_t)1UL)
#define PACK(sz, a) ((sz) | ((a) ? 1UL : 0UL))

/* Global heap bounds and free list head (doubly-linked, head insertion). */
static char *g_heap_lo = NULL;
static char *g_heap_hi = NULL; /* exclusive bound (one past last byte) */
static FreeNode *g_free_head = NULL;

/* Forward decls */
static int check_heap_validity(void);

static inline Footer *hdr_to_ftr(Header *h)
{
    return (Footer *)((char *)h + GET_SIZE(h) - FTR_SIZE);
}

/* Helper: get previous/next physical neighbors using boundary tags. */
static inline Header *next_phys(Header *h)
{
    char *p = (char *)h + GET_SIZE(h);
    if (p >= g_heap_hi)
        return NULL;
    return (Header *)p;
}

static inline Header *prev_phys(Header *h)
{
    if ((char *)h == g_heap_lo)
        return NULL;
    Footer *pf = (Footer *)((char *)h - FTR_SIZE);
    size_t psz = pf->size_alloc & ~(size_t)1UL;
    return (Header *)((char *)h - psz);
}

/* Access FreeNode area inside a free block */
static inline FreeNode *free_node(Header *h)
{
    return (FreeNode *)((char *)h + HDR_SIZE);
}

/* -------------------- Free-list ops -------------------- */
static void fl_push_front(Header *h)
{
    FreeNode *n = free_node(h);
    n->prev = NULL;
    n->next = g_free_head;
    if (g_free_head)
        g_free_head->prev = n;
    g_free_head = n;
}

static void fl_remove(Header *h)
{
    FreeNode *n = free_node(h);
    if (n->prev)
        n->prev->next = n->next;
    else
        g_free_head = n->next;
    if (n->next)
        n->next->prev = n->prev;
}

/* -------------------- Heap grow -------------------- */
static Header *heap_grow(size_t need_size)
{
    /* Request at least 64 KiB or requested aligned amount, whichever larger */
    size_t ask = need_size;
    if (ask < 64 * 1024)
        ask = 64 * 1024;
    ask = align_up(ask);

    void *oldbrk = sbrk(ask);
    if (oldbrk == (void *)-1)
        return NULL;

    if (!g_heap_lo)
        g_heap_lo = (char *)oldbrk;
    g_heap_hi = (char *)oldbrk + ask;

    Header *h = (Header *)oldbrk;
    h->size_alloc = PACK(ask, 0);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;

    /* Coalesce backward if previous is free and adjacent */
    Header *prev = prev_phys(h);
    if (prev && !IS_ALLOC(prev) && (char *)prev + GET_SIZE(prev) == (char *)h)
    {
        fl_remove(prev);
        size_t newsize = GET_SIZE(prev) + GET_SIZE(h);
        prev->size_alloc = PACK(newsize, 0);
        hdr_to_ftr(prev)->size_alloc = prev->size_alloc;
        fl_push_front(prev);
        return prev;
    }
    fl_push_front(h);
    return h;
}

/* -------------------- Split / Coalesce -------------------- */
static Header *split_block(Header *h, size_t asize)
{
    /* asize: total size incl header+footer for allocated block */
    size_t csize = GET_SIZE(h);
    assert(!IS_ALLOC(h) && csize >= asize);
    if (csize - asize >= MIN_FREE_SIZE)
    {
        /* Remove original free block, carve allocated, leave remainder free */
        fl_remove(h);
        Header *alloc = h;
        Header *rem = (Header *)((char *)h + asize);

        alloc->size_alloc = PACK(asize, 1);
        hdr_to_ftr(alloc)->size_alloc = alloc->size_alloc;

        rem->size_alloc = PACK(csize - asize, 0);
        hdr_to_ftr(rem)->size_alloc = rem->size_alloc;
        fl_push_front(rem);
        return alloc;
    }
    /* allocate whole block */
    fl_remove(h);
    h->size_alloc = PACK(csize, 1);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;
    return h;
}

static Header *coalesce(Header *h)
{
    Header *p = prev_phys(h);
    Header *n = next_phys(h);
    size_t size = GET_SIZE(h);

    if (p && !IS_ALLOC(p) && (char *)p + GET_SIZE(p) == (char *)h)
    {
        fl_remove(p);
        size += GET_SIZE(p);
        h = p;
    }
    if (n && !IS_ALLOC(n) && (char *)h + size == (char *)n)
    {
        fl_remove(n);
        size += GET_SIZE(n);
    }
    h->size_alloc = PACK(size, 0);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;
    return h;
}

/* -------------------- Public API -------------------- */
void *heapmgr_malloc(size_t ui_bytes)
{
    if (ui_bytes == 0)
        return NULL;
    if (!g_heap_lo)
    {
        /* initial bootstrap to establish heap bounds */
        void *cur = sbrk(0);
        if (cur == (void *)-1)
            return NULL;
        g_heap_lo = g_heap_hi = (char *)cur;
    }

    /* required payload aligned, add overhead */
    size_t asize = align_up(ui_bytes);
    if (asize < sizeof(FreeNode))
        asize = sizeof(FreeNode);
    asize = align_up(asize + OVERHEAD);

    assert(check_heap_validity());

    /* First-fit search */
    for (FreeNode *it = g_free_head; it != NULL; it = it->next)
    {
        Header *h = (Header *)((char *)it - HDR_SIZE);
        if (GET_SIZE(h) >= asize)
        {
            Header *a = split_block(h, asize);
            assert(check_heap_validity());
            return (char *)a + HDR_SIZE;
        }
    }

    /* Grow heap and retry */
    Header *nh = heap_grow(asize);
    if (!nh)
        return NULL;
    Header *a = split_block(nh, asize);
    assert(check_heap_validity());
    return (char *)a + HDR_SIZE;
}

void heapmgr_free(void *pv_bytes)
{
    if (pv_bytes == NULL)
        return;
    Header *h = (Header *)((char *)pv_bytes - HDR_SIZE);

    assert(check_heap_validity());

    /* Mark free and coalesce */
    h->size_alloc = PACK(GET_SIZE(h), 0);
    hdr_to_ftr(h)->size_alloc = h->size_alloc;
    Header *m = coalesce(h);
    fl_push_front(m);

    assert(check_heap_validity());
}

/* -------------------- Validity checks (debug) -------------------- */
static int check_heap_validity(void)
{
#ifndef NDEBUG
    /* 1) Iterate physical blocks from g_heap_lo to g_heap_hi */
    if (g_heap_lo && g_heap_hi && g_heap_lo < g_heap_hi)
    {
        char *p = g_heap_lo;
        while (p < g_heap_hi)
        {
            Header *h = (Header *)p;
            size_t sz = GET_SIZE(h);
            if (sz == 0 || (sz % ALIGN) != 0)
                return 0;
            Footer *f = hdr_to_ftr(h);
            if (f->size_alloc != h->size_alloc)
                return 0;
            p += sz;
        }
        if (p != g_heap_hi)
            return 0;
    }
    /* 2) Free list nodes must point to free blocks inside heap */
    for (FreeNode *it = g_free_head; it != NULL; it = it->next)
    {
        Header *h = (Header *)((char *)it - HDR_SIZE);
        if (IS_ALLOC(h))
            return 0;
        if (it->next && it->next->prev != it)
            return 0;
    }
#endif
    return 1;
}
