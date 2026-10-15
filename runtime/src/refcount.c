// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#include "internal.h"
OBJC_ANCHOR_INIT();

/* Reference counts of objects whose class uses the default (NSObject) retain/release
 * live in a side table keyed by address; an absent entry means a count of one.
 * Classes with custom retain/release (SwiftObject and its subclasses, NSProxy-like
 * classes) are messaged instead, as on Darwin. */

static objc_map refcounts;      /* obj -> (uintptr_t)count */
static objc_map deallocating;   /* obj -> 1 */
static id (*bad_alloc_handler)(Class);

extern void *_Block_copy(const void *block);

void refcount_init(void) {
    map_init(&refcounts, false);
    map_init(&deallocating, false);
}

void refcount_forget(id obj) {
    map_remove(&refcounts, obj);
    map_remove(&deallocating, obj);
}

bool obj_is_deallocating(id obj) { return map_find(&deallocating, obj) != NULL; }

id _objc_rootRetain(id obj) {
    objc_map_entry *e = map_find(&refcounts, obj);
    if (e) e->value = (void *)((uintptr_t)e->value + 1);
    else map_set(&refcounts, obj, (void *)2);
    return obj;
}

bool _objc_rootReleaseWasZero(id obj) {
    objc_map_entry *e = map_find(&refcounts, obj);
    if (!e) {
        if (obj_is_deallocating(obj)) return false;
        map_set(&deallocating, obj, (void *)1);
        return true;
    }
    uintptr_t c = (uintptr_t)e->value;
    if (c > 2) e->value = (void *)(c - 1);
    else map_remove(&refcounts, obj);
    return false;
}

void _objc_rootRelease(id obj) {
    if (_objc_rootReleaseWasZero(obj)) objc_sendv0(obj, SELS.dealloc);
}

uintptr_t _objc_rootRetainCount(id obj) {
    objc_map_entry *e = map_find(&refcounts, obj);
    return e ? (uintptr_t)e->value : 1;
}

bool _objc_rootTryRetain(id obj) {
    if (obj_is_deallocating(obj)) return false;
    _objc_rootRetain(obj);
    return true;
}

bool _objc_rootIsDeallocating(id obj) { return obj_is_deallocating(obj); }
id _objc_rootAutorelease(id obj) { autorelease_add(obj); return obj; }
id _objc_rootAlloc(Class cls) { return _objc_rootAllocWithZone(cls, NULL); }

id _objc_rootAllocWithZone(Class cls, void *zone) {
    (void)zone;
    id obj = class_createInstance(cls, 0);
    if (!obj && bad_alloc_handler) obj = bad_alloc_handler(cls);
    return obj;
}

id _objc_rootInit(id obj) { return obj; }
void _objc_rootDealloc(id obj) { objc_destructInstance(obj); free(obj); }
void _objc_rootFinalize(id obj) { (void)obj; }
void *_objc_rootZone(id obj) { (void)obj; return NULL; }
uintptr_t _objc_rootHash(id obj) { return (uintptr_t)obj; }
void _objc_setBadAllocHandler(id (*newHandler)(Class)) { bad_alloc_handler = newHandler; }

BOOL objc_isUniquelyReferenced(id obj) {
    if (!obj) return NO;
    if (obj_has_custom_rr(obj)) return NO;
    return _objc_rootRetainCount(obj) == 1 && !obj_is_deallocating(obj);
}

/* ---- ARC entry points ---- */

id objc_retain(id obj) {
    if (!obj) return nil;
    if (obj_has_custom_rr(obj)) return objc_send0(obj, SELS.retain);
    return _objc_rootRetain(obj);
}

void objc_release(id obj) {
    if (!obj) return;
    if (obj_has_custom_rr(obj)) { objc_sendv0(obj, SELS.release); return; }
    _objc_rootRelease(obj);
}

void objc_release_x(id obj) { objc_release(obj); }

id objc_autorelease(id obj) {
    if (!obj) return nil;
    if (obj_has_custom_rr(obj)) return objc_send0(obj, SELS.autorelease);
    return _objc_rootAutorelease(obj);
}

id objc_retainAutorelease(id obj) { return objc_autorelease(objc_retain(obj)); }
id objc_retainAutoreleaseReturnValue(id obj) { return objc_retainAutorelease(obj); }
id objc_autoreleaseReturnValue(id obj) { return objc_autorelease(obj); }
id objc_retainAutoreleasedReturnValue(id obj) { return objc_retain(obj); }
id objc_unsafeClaimAutoreleasedReturnValue(id obj) { return obj; }
id objc_retainBlock(id obj) { return obj ? (id)_Block_copy(obj) : nil; }

void objc_storeStrong(id *location, id obj) {
    id old = *location;
    if (old == obj) return;
    objc_retain(obj);
    *location = obj;
    objc_release(old);
}

/* ---- allocation shortcuts ---- */

id objc_alloc(Class cls) { return cls ? objc_send0((id)cls, SELS.alloc) : nil; }
id objc_allocWithZone(Class cls) { return cls ? objc_send1((id)cls, SELS.allocWithZone_, NULL) : nil; }
id objc_alloc_init(Class cls) { return objc_send0(objc_alloc(cls), SELS.init); }
id objc_opt_new(Class cls) { return cls ? objc_send0((id)cls, SELS.new_) : nil; }
id objc_opt_self(id obj) { return objc_send0(obj, SELS.self); }
Class objc_opt_class(id obj) { return (Class)objc_send0(obj, SELS.class_); }
BOOL objc_opt_isKindOfClass(id obj, Class cls) { return objc_sendb1(obj, SELS.isKindOfClass_, cls); }
BOOL objc_opt_respondsToSelector(id obj, SEL sel) { return objc_sendb1(obj, SELS.respondsToSelector_, sel); }

int objc_sync_enter(id obj) { (void)obj; return 0; }
int objc_sync_exit(id obj) { (void)obj; return 0; }

/* ---- @synthesize helpers ---- */

id objc_getProperty(id self, SEL _cmd, ptrdiff_t offset, BOOL atomic) {
    (void)_cmd;
    id *slot = (id *)((char *)self + offset);
    return atomic ? objc_retainAutorelease(*slot) : *slot;
}

void objc_setProperty(id self, SEL _cmd, ptrdiff_t offset, id newValue, BOOL atomic, signed char shouldCopy) {
    (void)_cmd; (void)atomic;
    id *slot = (id *)((char *)self + offset);
    if (shouldCopy == 2) newValue = objc_send1(newValue, SELS.mutableCopyWithZone_, NULL);
    else if (shouldCopy) newValue = objc_send1(newValue, SELS.copyWithZone_, NULL);
    else objc_retain(newValue);
    id old = *slot;
    *slot = newValue;
    objc_release(old);
}

void objc_setProperty_atomic(id self, SEL _cmd, id newValue, ptrdiff_t offset) { objc_setProperty(self, _cmd, offset, newValue, YES, 0); }
void objc_setProperty_nonatomic(id self, SEL _cmd, id newValue, ptrdiff_t offset) { objc_setProperty(self, _cmd, offset, newValue, NO, 0); }
void objc_setProperty_atomic_copy(id self, SEL _cmd, id newValue, ptrdiff_t offset) { objc_setProperty(self, _cmd, offset, newValue, YES, 1); }
void objc_setProperty_nonatomic_copy(id self, SEL _cmd, id newValue, ptrdiff_t offset) { objc_setProperty(self, _cmd, offset, newValue, NO, 1); }

void objc_copyStruct(void *dest, const void *src, ptrdiff_t size, BOOL atomic, BOOL hasStrong) {
    (void)atomic; (void)hasStrong;
    memmove(dest, src, (size_t)size);
}

void objc_copyCppObjectAtomic(void *dest, const void *src, void (*copyHelper)(void *, const void *)) {
    copyHelper(dest, src);
}
