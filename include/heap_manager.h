#pragma once
#include <stddef.h>
#include "hm_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Inicijalizacija / shutdown (nije eksplicitno u specifikaciji,
// ali će ti olakšati segment management i testiranje)
int hm_init(hm_config_t cfg);
void hm_shutdown(void);

void* allocate_memory(int size);
void  free_memory(void* address);

#ifdef __cplusplus
}
#endif
