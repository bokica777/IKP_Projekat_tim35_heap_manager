#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HM_JOB_ALLOC = 1,
    HM_JOB_FREE  = 2,
    HM_JOB_STOP = 3
} hm_job_type_t;

typedef struct {
    hm_job_type_t type;
    uint32_t request_id;
    int size;
    void* address;
    void* result;
    void* done_event;
} hm_job_t;

int hm_jobs_init(void);
void hm_jobs_shutdown(void);

int hm_jobs_push(hm_job_t* job);
int hm_jobs_pop_blocking(hm_job_t** out);
