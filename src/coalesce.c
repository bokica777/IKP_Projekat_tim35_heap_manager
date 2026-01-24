#include "hm_internal.h"
#include <stdint.h>

static inline hm_block_footer_t* hm_footer_of(hm_block_header_t* h) {
    return (hm_block_footer_t*)((uint8_t*)h + h->size - sizeof(hm_block_footer_t));
}

static inline hm_block_header_t* hm_next_phys(hm_block_header_t* h) {
    return (hm_block_header_t*)((uint8_t*)h + h->size);
}

static inline hm_block_header_t* hm_prev_phys(hm_block_header_t* h) {
    hm_block_footer_t* prevf = (hm_block_footer_t*)((uint8_t*)h - sizeof(hm_block_footer_t));
    size_t prev_size = prevf->size;
    return (hm_block_header_t*)((uint8_t*)h - prev_size);
}

static inline int hm_within_segment(hm_segment_t* seg, void* p) {
    uint8_t* b = (uint8_t*)seg->base;
    return ((uint8_t*)p >= b) && ((uint8_t*)p < b + seg->size);
}

hm_block_header_t* hm_split_block(hm_block_header_t* b, size_t need_total) {
    // b is removed from freelist by caller
    // split only if remainder can hold at least header+footer (aligned)
    const size_t min_block =
        hm_align_up(sizeof(hm_block_header_t) + sizeof(hm_block_footer_t), HM_ALIGN);

    if (b->size < need_total) {
        // should not happen, but be safe
        return b;
    }

    if (b->size < need_total + min_block) {
        // not enough space to split
        hm_footer_of(b)->size = b->size; // keep footer consistent
        return b;
    }

    size_t old = b->size;

    // shrink current block to requested size
    b->size = need_total;
    hm_footer_of(b)->size = b->size;

    // remainder block
    hm_block_header_t* r = (hm_block_header_t*)((uint8_t*)b + b->size);
    r->size = old - b->size;
    r->is_free = 1;
    r->seg = b->seg;
    r->next_free = r->prev_free = NULL;
    hm_footer_of(r)->size = r->size;

    // put remainder into freelist
    hm_freelist_insert(r);

    return b;
}

hm_block_header_t* hm_coalesce(hm_block_header_t* b) {
    hm_segment_t* seg = b->seg;

    // merge with next if possible
    hm_block_header_t* n = hm_next_phys(b);
    if (hm_within_segment(seg, n) && n->size != 0 && n->is_free) {
        hm_freelist_remove(n);
        b->size += n->size;
        hm_footer_of(b)->size = b->size;
    }

    // merge with prev if possible (ensure not at segment start)
    if ((uint8_t*)b > (uint8_t*)seg->base) {
        hm_block_header_t* p = hm_prev_phys(b);
        if (hm_within_segment(seg, p) && p->is_free) {
            hm_freelist_remove(p);
            p->size += b->size;
            hm_footer_of(p)->size = p->size;
            b = p;
        }
    }

    return b;
}
