// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#include "internal.h"
OBJC_ANCHOR_INIT();

static objc_hook_getClass hook_getClass;
static objc_hook_getImageName hook_getImageName;
static objc_hook_lazyClassNamer hook_lazyClassNamer;

static BOOL default_getClass(const char *name, Class *outClass) { (void)name; *outClass = Nil; return NO; }
static BOOL default_getImageName(Class cls, const char **outName) { (void)cls; *outName = NULL; return NO; }
static const char *default_lazyClassNamer(Class cls) { (void)cls; return NULL; }

void hooks_init(void) {
    hook_getClass = default_getClass;
    hook_getImageName = default_getImageName;
    hook_lazyClassNamer = default_lazyClassNamer;
}

void objc_setHook_getClass(objc_hook_getClass newValue, objc_hook_getClass *outOldValue) {
    if (outOldValue) *outOldValue = hook_getClass;
    hook_getClass = newValue ? newValue : default_getClass;
}

void objc_setHook_getImageName(objc_hook_getImageName newValue, objc_hook_getImageName *outOldValue) {
    if (outOldValue) *outOldValue = hook_getImageName;
    hook_getImageName = newValue ? newValue : default_getImageName;
}

void objc_setHook_lazyClassNamer(objc_hook_lazyClassNamer newValue, objc_hook_lazyClassNamer *outOldValue) {
    if (outOldValue) *outOldValue = hook_lazyClassNamer;
    hook_lazyClassNamer = newValue ? newValue : default_lazyClassNamer;
}

Class objc_hook_lookup_class(const char *name) {
    Class cls = Nil;
    if (hook_getClass(name, &cls)) return cls;
    return Nil;
}

const char *objc_hook_image_name(Class cls) {
    const char *n = NULL;
    if (hook_getImageName(cls, &n)) return n;
    return NULL;
}

const char *objc_hook_lazy_name(Class cls) {
    static bool naming;
    if (naming) return NULL;   /* the namer may message classes; avoid re-entrancy */
    naming = true;
    const char *n = hook_lazyClassNamer(cls);
    naming = false;
    return n;
}
