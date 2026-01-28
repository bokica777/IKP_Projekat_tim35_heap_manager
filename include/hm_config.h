#pragma once
#include <stddef.h>

typedef enum {
    HM_ALGO_NEXT_FIT = 0
} hm_algo_t;

typedef struct {
    size_t segment_size;          
    size_t max_free_segments;  
    hm_algo_t algo;         
    int enable_thread_safety;  
} hm_config_t;

hm_config_t hm_default_config(void);
