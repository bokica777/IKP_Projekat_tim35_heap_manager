#include "hm_results.h"
#include <windows.h>
#include <stddef.h>

#define HM_RES_CAP 4096

typedef struct {
    uint32_t id;
    hm_res_state_t state;
    void* result;
    void* job_ptr;
} hm_res_entry_t;

static hm_res_entry_t g_tab[HM_RES_CAP];
static CRITICAL_SECTION g_lock;
static int g_inited = 0;

static size_t hm_hash_u32(uint32_t x) {
    return (size_t)(x * 2654435761u);
}

static size_t hm_probe_find(uint32_t id) {
    size_t start = hm_hash_u32(id) & (HM_RES_CAP - 1);
    for (size_t k = 0; k < HM_RES_CAP; k++) {
        size_t idx = (start + k) & (HM_RES_CAP - 1);
        hm_res_state_t st = g_tab[idx].state;
        if (st == HM_RES_EMPTY) return (size_t)-1;
        if (st != HM_RES_TOMBSTONE && g_tab[idx].id == id) return idx;
    }
    return (size_t)-1;
}

static size_t hm_probe_insert(uint32_t id) {
    size_t start = hm_hash_u32(id) & (HM_RES_CAP - 1);
    size_t first_tomb = (size_t)-1;

    for (size_t k = 0; k < HM_RES_CAP; k++) {
        size_t idx = (start + k) & (HM_RES_CAP - 1);
        hm_res_state_t st = g_tab[idx].state;

        if (st == HM_RES_EMPTY) {
            return (first_tomb != (size_t)-1) ? first_tomb : idx;
        }
        if (st == HM_RES_TOMBSTONE) {
            if (first_tomb == (size_t)-1) first_tomb = idx;
            continue;
        }
        if (g_tab[idx].id == id) return idx;
    }
    return (size_t)-1;
}

int hm_results_init(void) {
    InitializeCriticalSection(&g_lock);
    for (size_t i = 0; i < HM_RES_CAP; i++) {
        g_tab[i].id = 0;
        g_tab[i].state = HM_RES_EMPTY;
        g_tab[i].result = NULL;
        g_tab[i].job_ptr = NULL;
    }
    g_inited = 1;
    return 0;
}

void hm_results_shutdown(void) {
    if (!g_inited) return;
    DeleteCriticalSection(&g_lock);
    g_inited = 0;
}

int hm_results_put_pending(uint32_t request_id, void* job_ptr) {
    if (!g_inited || request_id == 0) return -1;

    EnterCriticalSection(&g_lock);
    size_t idx = hm_probe_insert(request_id);
    if (idx == (size_t)-1) {
        LeaveCriticalSection(&g_lock);
        return -1;
    }
    g_tab[idx].id = request_id;
    g_tab[idx].state = HM_RES_PENDING;
    g_tab[idx].result = NULL;
    g_tab[idx].job_ptr = job_ptr;
    LeaveCriticalSection(&g_lock);
    return 0;
}

int hm_results_set_done(uint32_t request_id, void* result) {
    if (!g_inited || request_id == 0) return -1;

    EnterCriticalSection(&g_lock);
    size_t idx = hm_probe_find(request_id);
    if (idx == (size_t)-1) {
        LeaveCriticalSection(&g_lock);
        return -1;
    }
    g_tab[idx].state = HM_RES_DONE;
    g_tab[idx].result = result;
    LeaveCriticalSection(&g_lock);
    return 0;
}

int hm_results_get(uint32_t request_id, hm_res_state_t* state_out, void** result_out, void** job_ptr_out) {
    if (!g_inited || request_id == 0 || !state_out || !result_out || !job_ptr_out) return -1;

    EnterCriticalSection(&g_lock);
    size_t idx = hm_probe_find(request_id);
    if (idx == (size_t)-1) {
        LeaveCriticalSection(&g_lock);
        return -1;
    }
    *state_out = g_tab[idx].state;
    *result_out = g_tab[idx].result;
    *job_ptr_out = g_tab[idx].job_ptr;
    LeaveCriticalSection(&g_lock);
    return 0;
}

int hm_results_remove(uint32_t request_id) {
    if (!g_inited || request_id == 0) return -1;

    EnterCriticalSection(&g_lock);
    size_t idx = hm_probe_find(request_id);
    if (idx == (size_t)-1) {
        LeaveCriticalSection(&g_lock);
        return -1;
    }
    g_tab[idx].id = 0;
    g_tab[idx].state = HM_RES_TOMBSTONE;
    g_tab[idx].result = NULL;
    g_tab[idx].job_ptr = NULL;
    LeaveCriticalSection(&g_lock);
    return 0;
}
