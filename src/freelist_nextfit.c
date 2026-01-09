#include "hm_internal.h"
#include <stddef.h>

// pokazivač na “poslednje mesto” gde je stao next-fit
static hm_block_header_t* g_nextfit_cursor = NULL;

hm_block_header_t* hm_nextfit_find(size_t size_needed) {
    // TODO: implement next-fit preko free liste
    // - počni od g_nextfit_cursor (ili head)
    // - kruži dok ne nađeš dovoljno velik blok
    // - ažuriraj cursor na poziciju posle alokacije
    (void)size_needed;
    return NULL;
}

void hm_nextfit_set_cursor(hm_block_header_t* blk) {
    g_nextfit_cursor = blk;
}
