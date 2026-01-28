#include <windows.h>
#include "hm_jobs.h"
#include "heap_manager.h"

int hm_jobs_pop_blocking(hm_job_t *out);

typedef struct
{
    HANDLE *threads;
    int thread_count;
    volatile LONG stop;
} hm_pool_t;

static hm_pool_t g_pool;

static DWORD WINAPI worker_main(LPVOID p)
{
    (void)p;
    while (InterlockedCompareExchange(&g_pool.stop, 0, 0) == 0)
    {
        hm_job_t job;
        if (hm_jobs_pop_blocking(&job) != 0)
            continue;

        if (job.type == HM_JOB_ALLOC)
        {
            job.result = allocate_memory(job.size);
        }
        else if (job.type == HM_JOB_FREE)
        {
            free_memory(job.address);
            job.result = NULL;
        }

        if (job.done_event)
        {
            SetEvent((HANDLE)job.done_event);
        }
    }
    return 0;
}

int hm_pool_start(int workers)
{
    g_pool.thread_count = workers;
    g_pool.stop = 0;

    g_pool.threads = (HANDLE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(HANDLE) * workers);
    if (!g_pool.threads)
        return -1;

    for (int i = 0; i < workers; i++)
    {
        g_pool.threads[i] = CreateThread(NULL, 0, worker_main, NULL, 0, NULL);
        if (!g_pool.threads[i])
            return -1;
    }
    return 0;
}

void hm_pool_stop(void)
{
    InterlockedExchange(&g_pool.stop, 1);

    WaitForMultipleObjects(g_pool.thread_count, g_pool.threads, TRUE, 2000);

    for (int i = 0; i < g_pool.thread_count; i++)
    {
        if (g_pool.threads[i])
            CloseHandle(g_pool.threads[i]);
    }
    HeapFree(GetProcessHeap(), 0, g_pool.threads);
    g_pool.threads = NULL;
}
