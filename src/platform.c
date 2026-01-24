#include <windows.h>

static CRITICAL_SECTION g_heap_lock;
static int g_heap_lock_inited = 0;

static void ensure_heap_lock(void) {
    if (!g_heap_lock_inited) {
        InitializeCriticalSection(&g_heap_lock);
        g_heap_lock_inited = 1;
    }
}

void hm_lock(void) {
    ensure_heap_lock();
    EnterCriticalSection(&g_heap_lock);
}

void hm_unlock(void) {
    LeaveCriticalSection(&g_heap_lock);
}
