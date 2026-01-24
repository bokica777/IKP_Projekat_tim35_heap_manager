#pragma once
#include <stddef.h>
#include <stdint.h>

#define HM_ALIGN 16

static inline size_t hm_align_up(size_t x, size_t a) {
    return (x + (a - 1)) & ~(a - 1);
}

typedef struct hm_segment hm_segment_t;

typedef struct hm_block_header {
    size_t size;                 // TOTAL block size (header+payload+footer)
    uint8_t is_free;
    uint8_t _pad[7];
    hm_segment_t* seg;

    // valid only when is_free=1
    struct hm_block_header* next_free;
    struct hm_block_header* prev_free;
} hm_block_header_t;

typedef struct hm_block_footer {
    size_t size;                 // TOTAL block size (same as header->size)
} hm_block_footer_t;

struct hm_segment {
    void* base;                  // points to start of segment memory
    size_t size;                 // total size of segment memory
    struct hm_segment* next;
    int is_fully_free;           // 1 if entire segment is one free block
};

// free list / next-fit
void hm_freelist_insert(hm_block_header_t* b);
void hm_freelist_remove(hm_block_header_t* b);
hm_block_header_t* hm_nextfit_find(size_t need_total);
void hm_nextfit_set_cursor(hm_block_header_t* b);

// split/coalesce helpers
hm_block_header_t* hm_split_block(hm_block_header_t* b, size_t need_total);
hm_block_header_t* hm_coalesce(hm_block_header_t* b);

// segment system
void hm_segment_system_set_cfg(size_t segment_size, size_t max_free_segments);
hm_segment_t* hm_segment_create(void);
void hm_segment_destroy_all(void);
void hm_segment_mark_maybe_release(hm_segment_t* seg);

// stats
void hm_recompute_free_stats(void);
