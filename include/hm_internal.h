#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct hm_block_header {
    size_t size;                 // veličina korisničkog bloka (payload)
    uint8_t is_free;
    struct hm_block_header* next_free;
    struct hm_block_header* prev_free;
} hm_block_header_t;

typedef struct hm_segment {
    void*  base;
    size_t size;
    struct hm_segment* next;
    // slobodna lista po segmentu ili globalno (odluka kasnije)
} hm_segment_t;
