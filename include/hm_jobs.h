#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HM_JOB_ALLOC = 1,
    HM_JOB_FREE  = 2
} hm_job_type_t;

typedef struct {
    hm_job_type_t type;
    uint32_t request_id;

    // input
    int size;
    void* address;

    // output (worker popunjava)
    void* result;

    // signal za “gotovo”
    void* done_event; // Windows HANDLE, ali držimo kao void* da header ostane “čist”
} hm_job_t;
