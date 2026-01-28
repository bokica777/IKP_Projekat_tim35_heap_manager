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

static size_t hm_prolog_size(void)
{
    return hm_align_up(sizeof(hm_block_header_t) + sizeof(hm_block_footer_t), HM_ALIGN);
}

static size_t hm_epilog_size(void)
{
    return hm_align_up(sizeof(hm_block_header_t), HM_ALIGN);
}

static hm_block_header_t *hm_segment_usable_first_block(hm_segment_t *seg)
{
    return (hm_block_header_t *)((uint8_t *)seg->base + hm_prolog_size());
}

static hm_block_header_t *hm_segment_epilog(hm_segment_t *seg)
{
    return (hm_block_header_t *)((uint8_t *)seg->base + seg->size - hm_epilog_size());
}

static size_t hm_segment_usable_span(hm_segment_t *seg)
{
    uint8_t *b = (uint8_t *)hm_segment_usable_first_block(seg);
    uint8_t *e = (uint8_t *)hm_segment_epilog(seg);
    return (size_t)(e - b);
}

static size_t hm_count_fully_free_segments(void)
{
    size_t c = 0;
    for (hm_segment_t *s = g_segments; s; s = s->next)
    {
        if (s->is_fully_free)
            c++;
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
            hm_block_header_t *main_free = hm_segment_usable_first_block(cur);
            hm_freelist_remove(main_free);

            // unlink from segment list
            if (prev)
                prev->next = cur->next;
            else
                g_segments = cur->next;

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
    }
}

hm_segment_t *hm_segment_create(void)
{
    if (g_segment_size == 0)
        return NULL;

    hm_segment_t *s = (hm_segment_t *)calloc(1, sizeof(hm_segment_t));
    if (!s)
        return NULL;

    s->size = g_segment_size;
    s->base = malloc(s->size);
    if (!s->base)
    {
        free(s);
        return NULL;
    }

    uint8_t *base = (uint8_t *)s->base;
    uint8_t *end = base + s->size;

    const size_t pro_sz = hm_prolog_size();
    const size_t epi_sz = hm_epilog_size();
    const size_t min_free = hm_align_up(sizeof(hm_block_header_t) + sizeof(hm_block_footer_t), HM_ALIGN);

    if (s->size < pro_sz + epi_sz + min_free)
    {
        free(s->base);
        free(s);
        return NULL;
    }

    hm_block_header_t *pro = (hm_block_header_t *)base;
    pro->seg = s;
    pro->is_free = 0;
    pro->next_free = pro->prev_free = NULL;
    pro->size = pro_sz;
    hm_footer_of(pro)->size = pro->size;

    hm_block_header_t *epi = (hm_block_header_t *)(end - epi_sz);
    epi->seg = s;
    epi->is_free = 0;
    epi->next_free = epi->prev_free = NULL;
    epi->size = 0;

    hm_block_header_t *b = (hm_block_header_t *)(base + pro_sz);
    b->seg = s;
    b->is_free = 1;
    b->next_free = b->prev_free = NULL;
    b->size = (size_t)((uint8_t *)epi - (uint8_t *)b);
    hm_footer_of(b)->size = b->size;

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
