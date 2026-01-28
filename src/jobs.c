#include "hm_jobs.h"
#include <windows.h>

#define HM_QUEUE_CAP 1024

typedef struct
{
    hm_job_t *items[HM_QUEUE_CAP];

    size_t head, tail, count;

    CRITICAL_SECTION lock;
    HANDLE sem_items;
} hm_job_queue_t;

static hm_job_queue_t g_q;

int hm_jobs_init(void)
{
    InitializeCriticalSection(&g_q.lock);
    g_q.sem_items = CreateSemaphoreA(NULL, 0, HM_QUEUE_CAP, NULL);
    if (!g_q.sem_items)
        return -1;
    g_q.head = g_q.tail = g_q.count = 0;
    return 0;
}

void hm_jobs_shutdown(void)
{
    if (g_q.sem_items)
        CloseHandle(g_q.sem_items);
    DeleteCriticalSection(&g_q.lock);
}

int hm_jobs_push(hm_job_t *job)
{
    int ok = 0;
    EnterCriticalSection(&g_q.lock);

    if (g_q.count == HM_QUEUE_CAP)
    {
        ok = -1;
    }
    else
    {
        g_q.items[g_q.tail] = job;
        g_q.tail = (g_q.tail + 1) % HM_QUEUE_CAP;
        g_q.count++;
    }

    LeaveCriticalSection(&g_q.lock);

    if (ok == 0)
    {
        ReleaseSemaphore(g_q.sem_items, 1, NULL);
    }
    return ok;
}

int hm_jobs_pop_blocking(hm_job_t **out)
{
    DWORD w = WaitForSingleObject(g_q.sem_items, INFINITE);
    if (w != WAIT_OBJECT_0)
        return -1;

    EnterCriticalSection(&g_q.lock);

    *out = g_q.items[g_q.head];
    g_q.items[g_q.head] = NULL;
    g_q.head = (g_q.head + 1) % HM_QUEUE_CAP;
    g_q.count--;

    LeaveCriticalSection(&g_q.lock);
    return 0;
}
