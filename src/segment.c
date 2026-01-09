#include "hm_internal.h"
#include "hm_config.h"
#include <stdlib.h>

static hm_segment_t* g_segments = NULL;
static size_t g_free_segment_count = 0;
static hm_config_t g_cfg;

void hm_segment_system_set_cfg(hm_config_t cfg) { g_cfg = cfg; }

hm_segment_t* hm_segment_create(void) {
    hm_segment_t* s = (hm_segment_t*)calloc(1, sizeof(hm_segment_t));
    if (!s) return NULL;

    s->size = g_cfg.segment_size;
    s->base = malloc(s->size);
    if (!s->base) { free(s); return NULL; }

    s->next = g_segments;
    g_segments = s;
    return s;
}

void hm_segment_maybe_release_unused(void) {
    // TODO: logika “ako ima > max_free_segments potpuno slobodnih segmenata”
    // po specifikaciji: granica je 5 slobodnih segmenata
    // :contentReference[oaicite:6]{index=6}
}

void hm_segment_destroy_all(void) {
    hm_segment_t* s = g_segments;
    while (s) {
        hm_segment_t* nxt = s->next;
        free(s->base);
        free(s);
        s = nxt;
    }
    g_segments = NULL;
    g_free_segment_count = 0;
}
