#pragma once
#include <stdint.h>

typedef enum {
    HM_RES_EMPTY = 0,
    HM_RES_TOMBSTONE = 1,
    HM_RES_PENDING = 2,
    HM_RES_DONE = 3
} hm_res_state_t;

int hm_results_init(void);
void hm_results_shutdown(void);

int hm_results_put_pending(uint32_t request_id, void* job_ptr);
int hm_results_set_done(uint32_t request_id, void* result);

int hm_results_get(uint32_t request_id, hm_res_state_t* state_out, void** result_out, void** job_ptr_out);
int hm_results_remove(uint32_t request_id);
