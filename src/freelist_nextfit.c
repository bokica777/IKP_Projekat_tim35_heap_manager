#include "hm_internal.h"

static hm_block_header_t *g_free_head = NULL;
static hm_block_header_t *g_free_tail = NULL;
static hm_block_header_t *g_cursor = NULL;

void hm_freelist_insert(hm_block_header_t *b)
{
    b->is_free = 1;
    b->prev_free = NULL;
    b->next_free = g_free_head;
    if (g_free_head)
        g_free_head->prev_free = b;
    g_free_head = b;
    if (!g_free_tail)
        g_free_tail = b;

    if (!g_cursor)
        g_cursor = b;
}

void hm_freelist_remove(hm_block_header_t *b)
{
    if (!b)
        return;

    if (b->prev_free)
        b->prev_free->next_free = b->next_free;
    else
        g_free_head = b->next_free;

    if (b->next_free)
        b->next_free->prev_free = b->prev_free;
    else
        g_free_tail = b->prev_free;

    if (g_cursor == b)
        g_cursor = b->next_free ? b->next_free : g_free_head;

    b->next_free = b->prev_free = NULL;
}

hm_block_header_t *hm_nextfit_find(size_t need_total)
{
    if (!g_free_head)
        return NULL;
    if (!g_cursor)
        g_cursor = g_free_head;

    hm_block_header_t *start = g_cursor;
    hm_block_header_t *cur = start;

    do
    {
        if (cur->is_free && cur->size >= need_total)
        {
            g_cursor = cur->next_free ? cur->next_free : g_free_head;
            return cur;
        }
        cur = cur->next_free ? cur->next_free : g_free_head;
    } while (cur && cur != start);

    return NULL;
}

void hm_nextfit_set_cursor(hm_block_header_t *b)
{
    g_cursor = b;
}
