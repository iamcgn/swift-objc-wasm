// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#include "internal.h"
OBJC_ANCHOR_INIT();

static objc_map selectors;      /* name -> canonical name (SEL) */
struct objc_selectors SELS;

SEL sel_intern(const char *name, bool copy) {
    if (!name) _objc_fatal("libobjc-wasm: NULL selector name");
    objc_map_entry *e = map_find(&selectors, name);
    if (e) return (SEL)e->key;
    const char *canonical = copy ? strdup(name) : name;
    map_set(&selectors, canonical, (void *)canonical);
    return (SEL)canonical;
}

void selectors_init(void) {
    map_init(&selectors, true);
#define S(field, name) SELS.field = sel_intern(name, false)
    S(retain, "retain"); S(release, "release"); S(autorelease, "autorelease");
    S(retainCount, "retainCount"); S(dealloc, "dealloc"); S(initialize, "initialize");
    S(load, "load"); S(resolveInstanceMethod_, "resolveInstanceMethod:");
    S(resolveClassMethod_, "resolveClassMethod:");
    S(forwardingTargetForSelector_, "forwardingTargetForSelector:");
    S(doesNotRecognizeSelector_, "doesNotRecognizeSelector:");
    S(cxx_construct, ".cxx_construct"); S(cxx_destruct, ".cxx_destruct");
    S(alloc, "alloc"); S(allocWithZone_, "allocWithZone:"); S(init, "init"); S(new_, "new");
    S(self, "self"); S(class_, "class"); S(isKindOfClass_, "isKindOfClass:");
    S(respondsToSelector_, "respondsToSelector:");
    S(retainWeakReference, "retainWeakReference"); S(allowsWeakReference, "allowsWeakReference");
    S(_tryRetain, "_tryRetain"); S(_isDeallocating, "_isDeallocating");
    S(copyWithZone_, "copyWithZone:"); S(mutableCopyWithZone_, "mutableCopyWithZone:");
    S(copy, "copy"); S(mutableCopy, "mutableCopy"); S(description, "description");
    S(hash, "hash"); S(isEqual_, "isEqual:");
#undef S
}

SEL sel_registerName(const char *str) { return sel_intern(str, true); }
SEL sel_getUid(const char *str) { return sel_intern(str, true); }
const char *sel_getName(SEL sel) { return sel ? (const char *)sel : "<null selector>"; }
BOOL sel_isMapped(SEL sel) { return sel && map_find(&selectors, (const char *)sel) != NULL; }
BOOL sel_isEqual(SEL lhs, SEL rhs) { return lhs == rhs; }
