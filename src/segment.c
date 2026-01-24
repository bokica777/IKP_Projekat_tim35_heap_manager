#include "hm_internal.h"
#include <stdlib.h>
#include <stdint.h>

static hm_segment_t *g_segments = NULL;
hm_segment_t *hm__segments_head = NULL;

static size_t g_segment_size = 0;
static size_t g_max_free_segments = 5;

static inline hm_block_footer_t *hm_footer_of(hm_block_header_t *h)
{
    return (hm_block_footer_t *)((uint8_t *)h + h->size - sizeof(hm_block_footer_t));
}

// Mora da bude pre korišćenja u C
static size_t hm_count_fully_free_segments(void)
{
    size_t c = 0;
    hm_segment_t *s = g_segments;
    while (s)
    {
        if (s->is_fully_free)
            c++;
        s = s->next;
    }
    return c;
}

void hm_segment_system_set_cfg(size_t segment_size, size_t max_free_segments)
{
    g_segment_size = segment_size;
    g_max_free_segments = max_free_segments;
}

static void hm_segment_release_one_free_segment(void)
{
    hm_segment_t *prev = NULL;
    hm_segment_t *cur = g_segments;

    while (cur)
    {
        if (cur->is_fully_free)
        {
            // remove its only free block from freelist
            hm_block_header_t *b = (hm_block_header_t *)cur->base;
            hm_freelist_remove(b);

            // unlink
            if (prev)
                prev->next = cur->next;
            else
                g_segments = cur->next;

            // free memory
            free(cur->base);
            free(cur);

            hm__segments_head = g_segments;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

void hm_segment_mark_maybe_release(hm_segment_t *seg)
{
    (void)seg;

    while (hm_count_fully_free_segments() > g_max_free_segments)
    {
        hm_segment_release_one_free_segment();
        // ako nema više fully-free segmenata, loop će stati prirodno
    }
}

hm_segment_t* hm_segment_create(void) {
    if (g_segment_size == 0) return NULL;

    hm_segment_t* s = (hm_segment_t*)calloc(1, sizeof(hm_segment_t));
    if (!s) return NULL;

    s->size = g_segment_size;
    s->base = malloc(s->size);
    if (!s->base) {        // <-- OVO TI JE FALILO
        free(s);
        return NULL;
    }

    // --- layout: [PROLOG used][FREE block][EPILOG used(size=0)] ---
    uint8_t* base = (uint8_t*)s->base;
    uint8_t* end  = base + s->size;

    const size_t prolog_size = hm_align_up(sizeof(hm_block_header_t) + sizeof(hm_block_footer_t), HM_ALIGN);
    const size_t epilog_size = hm_align_up(sizeof(hm_block_header_t), HM_ALIGN);
    const size_t min_free    = hm_align_up(sizeof(hm_block_header_t) + sizeof(hm_block_footer_t), HM_ALIGN);

    if (s->size < prolog_size + epilog_size + min_free) {
        free(s->base);
        free(s);
        return NULL;
    }

    // PROLOG (used)
    hm_block_header_t* pro = (hm_block_header_t*)base;
    pro->seg = s;
    pro->is_free = 0;
    pro->next_free = pro->prev_free = NULL;
    pro->size = prolog_size;
    hm_footer_of(pro)->size = pro->size;

    // EPILOG header sentinel (size=0 used)
    hm_block_header_t* epi = (hm_block_header_t*)(end - epilog_size);
    epi->seg = s;
    epi->is_free = 0;
    epi->next_free = epi->prev_free = NULL;
    epi->size = 0;

    // MAIN FREE block between prolog and epilog
    hm_block_header_t* b = (hm_block_header_t*)(base + prolog_size);
    b->seg = s;
    b->is_free = 1;
    b->next_free = b->prev_free = NULL;
    b->size = (size_t)((uint8_t*)epi - (uint8_t*)b);  // up to epilog
    hm_footer_of(b)->size = b->size;

    // add segment to list
    s->next = g_segments;
    g_segments = s;
    hm__segments_head = g_segments;

    s->is_fully_free = 1;

    hm_freelist_insert(b);
    hm_segment_mark_maybe_release(s);

    return s;
}


void hm_segment_destroy_all(void)
{
    hm_segment_t *s = g_segments;
    while (s)
    {
        hm_segment_t *nxt = s->next;
        free(s->base);
        free(s);
        s = nxt;
    }
    g_segments = NULL;
    hm__segments_head = NULL;
}
