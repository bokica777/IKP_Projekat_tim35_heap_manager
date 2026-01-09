#pragma once
#include <stddef.h>

typedef enum {
    HM_ALGO_NEXT_FIT = 0
} hm_algo_t;

typedef struct {
    size_t segment_size;          // npr 1MB, 4MB...
    size_t max_free_segments;     // po specifikaciji = 5
    hm_algo_t algo;               // next fit
    int enable_thread_safety;     // 1=on
} hm_config_t;

hm_config_t hm_default_config(void);
