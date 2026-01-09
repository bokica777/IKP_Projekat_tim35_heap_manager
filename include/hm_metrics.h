#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t alloc_count;
    uint64_t free_count;
    double external_fragmentation;
    size_t total_free_bytes;
    size_t largest_free_block;
} hm_metrics_t;

hm_metrics_t hm_get_metrics(void);
void hm_reset_metrics(void);

/* internal helpers (koriste ih src fajlovi) */
void hm_metrics_on_alloc(void);
void hm_metrics_on_free(void);
void hm_metrics_set_free_stats(size_t total_free, size_t largest_free);
