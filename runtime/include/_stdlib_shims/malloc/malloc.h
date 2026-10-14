/* malloc/malloc.h shim: malloc zones are a Darwin concept; one dummy zone exists. */
#ifndef _MALLOC_MALLOC_H_
#define _MALLOC_MALLOC_H_
#include <stddef.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct _malloc_zone_t malloc_zone_t;
malloc_zone_t *malloc_default_zone(void);
malloc_zone_t *malloc_zone_from_ptr(const void *ptr);
size_t malloc_size(const void *ptr);
#ifdef __cplusplus
}
#endif
#endif
