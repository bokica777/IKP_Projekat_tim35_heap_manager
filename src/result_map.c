#include "hm_results.h"
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

#define CAP 8192  // power of two (8192), dovoljno za testove

typedef struct {
    uint32_t id;
    hm_res_state_t st;
    void* result;
    void* jobp;
    uint8_t used;      // 0=empty, 1=occupied
    uint8_t tomb;      // 1=deleted marker
} entry_t;

static entry_t g_tab[CAP];
static CRITICAL_SECTION g_lock;

static size_t h32(uint32_t x) {
    return (size_t)(x * 2654435761u);
}

int hm_results_init(void) {
    InitializeCriticalSection(&g_lock);
    for (size_t i = 0; i < CAP; i++) {
        g_tab[i].used = 0;
        g_tab[i].tomb = 0;
        g_tab[i].id = 0;
        g_tab[i].st = HM_RES_EMPTY;
        g_tab[i].result = NULL;
        g_tab[i].jobp = NULL;
    }
    return 0;
}

void hm_results_shutdown(void) {
    DeleteCriticalSection(&g_lock);
}

static entry_t* find_slot(uint32_t id) {
    size_t start = h32(id) & (CAP - 1);
    for (size_t k = 0; k < CAP; k++) {
        size_t i = (start + k) & (CAP - 1);
        if (!g_tab[i].used) {
            if (!g_tab[i].tomb) return NULL; // chain ends
        } else if (g_tab[i].id == id) {
            return &g_tab[i];
        }
    }
    return NULL;
}

static entry_t* find_insert_slot(uint32_t id) {
    size_t start = h32(id) & (CAP - 1);
    entry_t* first_tomb = NULL;

    for (size_t k = 0; k < CAP; k++) {
        size_t i = (start + k) & (CAP - 1);

        if (g_tab[i].used) {
            if (g_tab[i].id == id) return &g_tab[i];
            continue;
        }

        if (g_tab[i].tomb) {
            if (!first_tomb) first_tomb = &g_tab[i];
            continue;
        }

        return first_tomb ? first_tomb : &g_tab[i];
    }

    return first_tomb;
}

int hm_results_put_pending(uint32_t id, void* jobp) {
    int rc = -1;
    EnterCriticalSection(&g_lock);

    entry_t* e = find_insert_slot(id);
    if (e) {
        e->id = id;
        e->st = HM_RES_PENDING;
        e->result = NULL;
        e->jobp = jobp;
        e->used = 1;
        e->tomb = 0;
        rc = 0;
    }

    LeaveCriticalSection(&g_lock);
    return rc;
}

int hm_results_set_done(uint32_t id, void* result) {
    int rc = -1;
    EnterCriticalSection(&g_lock);

    entry_t* e = find_slot(id);
    if (e) {
        e->result = result;
        e->st = HM_RES_DONE;
        rc = 0;
    }

    LeaveCriticalSection(&g_lock);
    return rc;
}

int hm_results_get(uint32_t id, hm_res_state_t* st, void** result, void** jobp) {
    int rc = -1;
    EnterCriticalSection(&g_lock);

    entry_t* e = find_slot(id);
    if (e) {
        if (st) *st = e->st;
        if (result) *result = e->result;
        if (jobp) *jobp = e->jobp;
        rc = 0;
    }

    LeaveCriticalSection(&g_lock);
    return rc;
}

int hm_results_remove(uint32_t id) {
    int rc = -1;
    EnterCriticalSection(&g_lock);

    entry_t* e = find_slot(id);
    if (e) {
        e->used = 0;
        e->tomb = 1;         // ključ!
        e->id = 0;
        e->st = HM_RES_EMPTY;
        e->result = NULL;
        e->jobp = NULL;
        rc = 0;
    }

    LeaveCriticalSection(&g_lock);
    return rc;
}
