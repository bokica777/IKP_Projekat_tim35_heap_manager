#include <stdint.h>
#include "hm_config.h"

#if defined(_WIN32)
  // TODO: windows critical section wrappers
#else
  #include <pthread.h>
  static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
  void hm_lock(void)   { pthread_mutex_lock(&g_lock); }
  void hm_unlock(void) { pthread_mutex_unlock(&g_lock); }
#endif
