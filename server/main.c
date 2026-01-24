#include "heap_manager.h"
#include "hm_metrics.h"
#include <stdio.h>

int main(void) {
    printf("START\n");
    fflush(stdout);

    hm_config_t cfg = hm_default_config();
    cfg.segment_size = 256 * 1024;   // 256KB (brzo i pregledno za test)
    cfg.max_free_segments = 5;
    cfg.enable_thread_safety = 1;

    int rc = hm_init(cfg);
    if (rc != 0) {
        printf("hm_init FAILED (rc=%d)\n", rc);
        fflush(stdout);
        return 1;
    }

    printf("MAIN: before first alloc\n");
    fflush(stdout);

    void* p = allocate_memory(64);

    printf("MAIN: after first alloc ptr=%p\n", p);
    fflush(stdout);

    if (!p) {
        printf("ALLOC FAILED\n");
        fflush(stdout);
        hm_shutdown();
        return 1;
    }

    printf("MAIN: before first free\n");
    fflush(stdout);

    free_memory(p);

    printf("MAIN: after first free\n");
    fflush(stdout);

    hm_metrics_t m = hm_get_metrics();
    printf("alloc=%llu free=%llu frag=%.4f total_free=%zu largest_free=%zu\n",
        (unsigned long long)m.alloc_count,
        (unsigned long long)m.free_count,
        m.external_fragmentation,
        m.total_free_bytes,
        m.largest_free_block
    );
    fflush(stdout);

    hm_shutdown();

    printf("END\n");
    fflush(stdout);
    return 0;
}
