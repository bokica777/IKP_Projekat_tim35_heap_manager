#include "heap_manager.h"
#include "hm_metrics.h"
#include <stdio.h>

int main(void)
{
    hm_config_t cfg = hm_default_config();
    cfg.segment_size = 256 * 1024;
    cfg.max_free_segments = 5;
    cfg.enable_thread_safety = 1;

    printf("START\n");
    if (hm_init(cfg) != 0) {
        printf("hm_init FAILED\n");
        return 1;
    }

    void* p = allocate_memory(128);
    if (!p) {
        printf("ALLOC FAILED\n");
        hm_shutdown();
        return 1;
    }

    free_memory(p);

    hm_metrics_t m = hm_get_metrics();
    printf("alloc=%llu free=%llu frag=%.4f total_free=%zu largest_free=%zu\n",
        (unsigned long long)m.alloc_count,
        (unsigned long long)m.free_count,
        m.external_fragmentation,
        m.total_free_bytes,
        m.largest_free_block
    );

    hm_shutdown();
    printf("END\n");
    return 0;
}
