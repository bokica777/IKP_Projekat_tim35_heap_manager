#include "hm_metrics.h"
#include <windows.h>

static CRITICAL_SECTION g_metrics_lock;
static int g_lock_inited = 0;

static uint64_t g_alloc_count = 0;
static uint64_t g_free_count  = 0;
static size_t g_total_free = 0;
static size_t g_largest_free = 0;

static void ensure_lock(void) {
    if (!g_lock_inited) {
        InitializeCriticalSection(&g_metrics_lock);
        g_lock_inited = 1;
    }
}

hm_metrics_t hm_get_metrics(void) {
    ensure_lock();
    EnterCriticalSection(&g_metrics_lock);

    hm_metrics_t m;
    m.alloc_count = g_alloc_count;
    m.free_count  = g_free_count;
    m.total_free_bytes = g_total_free;
    m.largest_free_block = g_largest_free;

    LeaveCriticalSection(&g_metrics_lock);

    if (m.total_free_bytes == 0) m.external_fragmentation = 0.0;
    else m.external_fragmentation = 1.0 - ((double)m.largest_free_block / (double)m.total_free_bytes);

    return m;
}

void hm_reset_metrics(void) {
    ensure_lock();
    EnterCriticalSection(&g_metrics_lock);

    g_alloc_count = 0;
    g_free_count = 0;
    g_total_free = 0;
    g_largest_free = 0;

    LeaveCriticalSection(&g_metrics_lock);
}

void hm_metrics_on_alloc(void) {
    ensure_lock();
    EnterCriticalSection(&g_metrics_lock);
    g_alloc_count++;
    LeaveCriticalSection(&g_metrics_lock);
}

void hm_metrics_on_free(void) {
    ensure_lock();
    EnterCriticalSection(&g_metrics_lock);
    g_free_count++;
    LeaveCriticalSection(&g_metrics_lock);
}

void hm_metrics_set_free_stats(size_t total_free, size_t largest_free) {
    ensure_lock();
    EnterCriticalSection(&g_metrics_lock);
    g_total_free = total_free;
    g_largest_free = largest_free;
    LeaveCriticalSection(&g_metrics_lock);
}
