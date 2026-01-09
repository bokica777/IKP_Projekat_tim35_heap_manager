#include "heap_manager.h"
#include "hm_metrics.h"
#include <stdio.h>

int main(void) {
    hm_config_t cfg = hm_default_config();
    hm_init(cfg);

    // Minimal demo: alociraj/oslobađaj (kasnije zameni sockets + request queue)
    void* p1 = allocate_memory(128);
    void* p2 = allocate_memory(256);
    free_memory(p1);
    free_memory(p2);

    hm_metrics_t m = hm_get_metrics();
    printf("alloc=%llu free=%llu frag=%.4f\n",
        (unsigned long long)m.alloc_count,
        (unsigned long long)m.free_count,
        m.external_fragmentation
    );

    hm_shutdown();
    return 0;
}
