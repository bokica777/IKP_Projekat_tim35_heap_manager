#include "heap_manager.h"
#include "hm_internal.h"
#include "hm_metrics.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void hm_lock(void);
void hm_unlock(void);

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

int hm_init(hm_config_t cfg)
{
    g_cfg = cfg;
    hm_segment_system_set_cfg(cfg.segment_size, cfg.max_free_segments);
    g_inited = 1;
    if (!hm_segment_create())
        return -1;

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

    hm_freelist_remove(b);

    if (b->seg && b->seg->is_fully_free)
    {
        b->seg->is_fully_free = 0;
    }

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

    b = hm_coalesce(b);

    hm_freelist_insert(b);

    if (b->seg)
    {
        size_t pro_sz = hm_align_up(sizeof(hm_block_header_t) + sizeof(hm_block_footer_t), HM_ALIGN);
        size_t epi_sz = hm_align_up(sizeof(hm_block_header_t), HM_ALIGN);

        uint8_t *usable_start = (uint8_t *)b->seg->base + pro_sz;
        uint8_t *epilog_ptr = (uint8_t *)b->seg->base + b->seg->size - epi_sz;

        if ((uint8_t *)b == usable_start && (uint8_t *)b + b->size == epilog_ptr)
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
