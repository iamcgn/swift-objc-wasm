// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
/*
 * CFLite.c — the few CoreFoundation and malloc-zone entry points the Swift runtime
 * calls under Objective-C interop, implemented over the ObjC runtime.
 */
#include <objc/runtime.h>
#include <objc/message.h>
#include <stdint.h>
#include <stddef.h>

typedef unsigned long CFTypeID;
typedef unsigned long CFHashCode;
typedef long CFIndex;
typedef const void *CFTypeRef;

#define kCFStringTypeID 7

CFTypeID CFStringGetTypeID(void) { return kCFStringTypeID; }

CFTypeID CFGetTypeID(CFTypeRef obj) {
    if (!obj) return 0;
    SEL sel = sel_registerName("_cfTypeID");
    id o = (id)obj;
    if (!class_respondsToSelector(object_getClass(o), sel)) return 1;
    return ((unsigned long (*)(id, SEL))objc_msgLookup(o, sel))(o, sel);
}

CFHashCode CFStringHashCString(const uint8_t *bytes, CFIndex len) {
    CFHashCode h = 5381;
    for (CFIndex i = 0; i < len; i++) h = h * 33 + bytes[i];
    return h;
}

CFHashCode CFStringHashNSString(id str) {
    if (!str) return 0;
    SEL sel = sel_registerName("hash");
    return ((unsigned long (*)(id, SEL))objc_msgLookup(str, sel))(str, sel);
}

/* malloc zones do not exist on WASI; one dummy zone stands in. */
static int default_zone_storage;
void *malloc_default_zone(void) { return &default_zone_storage; }
void *malloc_zone_from_ptr(const void *ptr) { (void)ptr; return &default_zone_storage; }
size_t malloc_size(const void *ptr) { (void)ptr; return 0; }
