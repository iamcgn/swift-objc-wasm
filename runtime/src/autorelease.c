// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#include "internal.h"
OBJC_ANCHOR_INIT();

/* One global stack of autoreleased objects with pool boundary markers. Objects
 * autoreleased with no pool in place stay on the stack until a pool is pushed and
 * popped above them; the program's lifetime then reclaims them. */

#define POOL_BOUNDARY ((void *)1)
static ptr_vec pool;

void autorelease_init(void) {}

void autorelease_add(id obj) { vec_push(&pool, obj); }

void *objc_autoreleasePoolPush(void) {
    vec_push(&pool, POOL_BOUNDARY);
    return (void *)(uintptr_t)pool.count;   /* index of the first object above the boundary */
}

void objc_autoreleasePoolPop(void *context) {
    uint32_t idx = (uint32_t)(uintptr_t)context;
    if (idx == 0 || idx > pool.count || pool.items[idx - 1] != POOL_BOUNDARY) {
        _objc_inform("libobjc-wasm: objc_autoreleasePoolPop(%p): invalid or already popped pool", context);
        return;
    }
    while (pool.count >= idx) {
        void *item = pool.items[--pool.count];
        if (item != POOL_BOUNDARY) objc_release((id)item);
    }
}
