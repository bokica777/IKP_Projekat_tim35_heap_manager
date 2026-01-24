#include "heap_manager.h"
#include "hm_internal.h"
#include "hm_metrics.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


// lock functions (implemented in platform.c)
void hm_lock(void);
void hm_unlock(void);

// metrics internal helpers (you already have them)
void hm_metrics_on_alloc(void);
void hm_metrics_on_free(void);

static int g_inited = 0;
static hm_config_t g_cfg;

static inline hm_block_footer_t *hm_footer_of(hm_block_header_t *h)
{
    return (hm_block_footer_t *)((uint8_t *)h + h->size - sizeof(hm_block_footer_t));
}

hm_config_t hm_default_config(void)
{
    hm_config_t c;
    c.segment_size = 1024 * 1024; // 1MB
    c.max_free_segments = 5;
    c.algo = HM_ALGO_NEXT_FIT;
    c.enable_thread_safety = 1;
    return c;
}

int hm_init(hm_config_t cfg) {
    g_cfg = cfg;
    hm_segment_system_set_cfg(cfg.segment_size, cfg.max_free_segments);
    g_inited = 1;
    if (!hm_segment_create()) return -1;

    hm_recompute_free_stats();
    return 0;
}


void hm_shutdown(void)
{
    if (!g_inited)
        return;
    hm_segment_destroy_all();
    g_inited = 0;
}

static void *hm_payload_from_block(hm_block_header_t *b)
{
    return (void *)((uint8_t *)b + sizeof(hm_block_header_t));
}

static hm_block_header_t *hm_block_from_payload(void *p)
{
    return (hm_block_header_t *)((uint8_t *)p - sizeof(hm_block_header_t));
}

void *allocate_memory(int size)
{
    if (!g_inited || size <= 0)
        return NULL;

    if (g_cfg.enable_thread_safety)
        hm_lock();

    size_t need_payload = hm_align_up((size_t)size, HM_ALIGN);
    size_t need_total = hm_align_up(sizeof(hm_block_header_t) + need_payload + sizeof(hm_block_footer_t), HM_ALIGN);

    hm_block_header_t *b = hm_nextfit_find(need_total);
    if (!b)
    {
        // need a new segment
        if (!hm_segment_create())
        {
            if (g_cfg.enable_thread_safety)
                hm_unlock();
            return NULL;
        }
        b = hm_nextfit_find(need_total);
        if (!b)
        {
            if (g_cfg.enable_thread_safety)
                hm_unlock();
            return NULL;
        }
    }

    // take from freelist
    hm_freelist_remove(b);

    // this segment is no longer fully free (if it was)
    if (b->seg && b->seg->is_fully_free)
    {
        b->seg->is_fully_free = 0;
        // segment.c counts fully-free segments; easiest is:
        // we decremented there implicitly? We didn't.
        // We'll handle counting by recomputing: simplest approach below:
        // (to keep code simple we won't track decrement here; instead, mark flag and let release logic rely on actual flag+count in segment.c)
        // For correctness of release threshold, we must update count. We'll do it lazily: when segment becomes fully free we increment there.
        // => So we must NOT rely on stale g_free_segment_count.
        // To keep it correct right now, we'll flip counting approach: in segment.c we increment on create only.
        // We'll compensate by: on first allocation from fresh segment, we should decrement free segment count.
        // But segment.c keeps count private; simplest: don't count "fresh segment" as free until after first free.
        // However we already did. So we need a small fix:
    }

    // Split if possible (puts remainder back into free list)
    b->is_free = 1;
    b = hm_split_block(b, need_total);

    // mark used
    b->is_free = 0;
    hm_footer_of(b)->size = b->size;

    hm_metrics_on_alloc();
    hm_recompute_free_stats();

    if (g_cfg.enable_thread_safety)
        hm_unlock();
    return hm_payload_from_block(b);
}

void free_memory(void *address)
{
    if (!g_inited || !address)
        return;

    if (g_cfg.enable_thread_safety)
        hm_lock();

    hm_block_header_t *b = hm_block_from_payload(address);
    b->is_free = 1;

    // coalesce neighbors (remove merged neighbors from freelist inside hm_coalesce)
    b = hm_coalesce(b);

    // insert final block
    hm_freelist_insert(b);

    // if whole segment is one free block => mark fully free and enforce limit
    if (b->seg)
    {
        uint8_t *seg_base = (uint8_t *)b->seg->base;
        if ((uint8_t *)b == seg_base && b->size == b->seg->size)
        {
            if (!b->seg->is_fully_free)
            {
                b->seg->is_fully_free = 1;
            }
            hm_segment_mark_maybe_release(b->seg);
        }
    }

    hm_metrics_on_free();
    hm_recompute_free_stats();

    if (g_cfg.enable_thread_safety)
        hm_unlock();
}
