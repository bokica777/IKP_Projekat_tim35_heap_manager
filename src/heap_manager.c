#include "heap_manager.h"
#include "hm_internal.h"
#include "hm_metrics.h"
#include <stddef.h>

void hm_lock(void);
void hm_unlock(void);

// from segment.c
void hm_segment_system_set_cfg(hm_config_t cfg);
hm_segment_t* hm_segment_create(void);
void hm_segment_destroy_all(void);

static int g_inited = 0;
static hm_config_t g_cfg;

hm_config_t hm_default_config(void) {
    hm_config_t c;
    c.segment_size = 1024 * 1024; // 1MB default
    c.max_free_segments = 5;
    c.algo = HM_ALGO_NEXT_FIT;
    c.enable_thread_safety = 1;
    return c;
}

int hm_init(hm_config_t cfg) {
    g_cfg = cfg;
    hm_segment_system_set_cfg(cfg);
    g_inited = 1;
    return 0;
}

void hm_shutdown(void) {
    if (!g_inited) return;
    hm_segment_destroy_all();
    g_inited = 0;
}

void* allocate_memory(int size) {
    if (size <= 0) return NULL;

    if (g_cfg.enable_thread_safety) hm_lock();

    // TODO:
    // 1) pronađi slobodan blok (next-fit)
    // 2) split ako treba
    // 3) update metrics (alloc_count)
    // 4) ako nema prostora => dodaj segment

    // placeholder
    void* result = NULL;

    if (g_cfg.enable_thread_safety) hm_unlock();
    return result;
}

void free_memory(void* address) {
    if (!address) return;

    if (g_cfg.enable_thread_safety) hm_lock();

    // TODO:
    // 1) vrati blok u free list
    // 2) coalesce susednih blokova
    // 3) update metrics (free_count, fragmentation stats)
    // 4) ako segment postane potpuno slobodan => inkrement free segments
    //    i ako > 5 => oslobodi višak segmenata

    if (g_cfg.enable_thread_safety) hm_unlock();
}
