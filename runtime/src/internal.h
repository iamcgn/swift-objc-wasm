// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
/*
 * internal.h — private interfaces of libobjc-wasm.
 */
#ifndef LIBOBJC_WASM_INTERNAL_H
#define LIBOBJC_WASM_INTERNAL_H

#include "abi.h"
#include <objc/runtime.h>
#include <objc/message.h>
#include <objc/objc-internal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define OBJC_ASSERT(cond) \
    do { if (!(cond)) _objc_fatal("libobjc-wasm: assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__); } while (0)

/* ---- hash map (open addressing, string or pointer keys) ---- */

typedef struct objc_map_entry { const void *key; void *value; } objc_map_entry;
typedef struct objc_map {
    objc_map_entry *entries;
    uint32_t capacity;   /* power of two or 0 */
    uint32_t count;      /* live entries */
    uint32_t used;       /* live + tombstones */
    bool string_keys;
} objc_map;

void map_init(objc_map *m, bool string_keys);
objc_map_entry *map_find(const objc_map *m, const void *key);
void *map_get(const objc_map *m, const void *key);
bool map_set(objc_map *m, const void *key, void *value);   /* true if newly inserted */
bool map_remove(objc_map *m, const void *key);
typedef void (*map_visitor)(const void *key, void *value, void *ctx);
void map_foreach(const objc_map *m, map_visitor fn, void *ctx);
void map_free(objc_map *m);

/* ---- pointer vector ---- */

typedef struct ptr_vec { void **items; uint32_t count, capacity; } ptr_vec;
void vec_push(ptr_vec *v, void *item);
void vec_insert_front(ptr_vec *v, void *item);
bool vec_remove(ptr_vec *v, void *item);
bool vec_contains(const ptr_vec *v, const void *item);
void vec_free(ptr_vec *v);

/* ---- runtime class data ---- */

typedef struct cache_entry { SEL sel; IMP imp; } cache_entry;

typedef struct class_rw_t {
    uint32_t flags;             /* RW_*; RW_REALIZED distinguishes rw from ro */
    uint32_t version;
    const class_ro_t *ro;
    ptr_vec methods;            /* method_list_t *, index 0 is searched first */
    ptr_vec protocols;          /* protocol_list_t * */
    ptr_vec properties;         /* property_list_t * */
    Class firstSubclass;
    Class nextSiblingClass;
    Class nonMetaClass;         /* metaclasses only */
    const char *lazyName;
    cache_entry *cache;
    uint32_t cacheMask;
    uint32_t cacheOccupied;
} class_rw_t;

#define RW_REALIZED        (1u << 31)
#define RW_META            (1u << 0)
#define RW_HAS_CUSTOM_RR   (1u << 1)
#define RW_INITIALIZED     (1u << 2)
#define RW_INITIALIZING    (1u << 3)
#define RW_LOADED          (1u << 4)
#define RW_CONSTRUCTED     (1u << 5)   /* objc_allocateClassPair, not registered yet */
#define RW_REGISTERED      (1u << 6)
#define RW_RO_OWNED        (1u << 7)   /* ro allocated by the runtime */
#define RW_HAS_CXX_CTOR    (1u << 8)
#define RW_HAS_CXX_DTOR    (1u << 9)
#define RW_FORBIDS_ASSOC   (1u << 10)
#define RW_HAS_LOAD        (1u << 11)

static inline uintptr_t cls_data(Class c) { return c->bits & FAST_DATA_MASK; }
static inline bool cls_is_swift(Class c) { return (c->bits & FAST_IS_SWIFT_MASK) != 0; }
static inline bool cls_is_realized(Class c) { return (*(const uint32_t *)cls_data(c) & RW_REALIZED) != 0; }
static inline class_rw_t *cls_rw(Class c) { return (class_rw_t *)cls_data(c); }
static inline const class_ro_t *cls_ro(Class c) {
    return cls_is_realized(c) ? cls_rw(c)->ro : (const class_ro_t *)cls_data(c);
}
static inline bool cls_is_meta(Class c) { return (cls_ro(c)->flags & RO_META) != 0; }
static inline bool cls_is_stub(Class c) { return IS_STUB_CLASS_ISA(c->isa); }
static inline bool cls_uses_swift_rc(Class c) {
    return cls_is_swift(c) && (((const struct swift_class_t *)c)->flags & SWIFT_CLASS_FLAG_USES_SWIFT_REFCOUNTING);
}
static inline uint32_t align_up(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

/* ---- selectors (selector.c) ---- */

struct objc_selectors {
    SEL retain, release, autorelease, retainCount, dealloc, initialize, load,
        resolveInstanceMethod_, resolveClassMethod_, forwardingTargetForSelector_,
        doesNotRecognizeSelector_, cxx_construct, cxx_destruct, alloc, allocWithZone_,
        init, new_, self, class_, isKindOfClass_, respondsToSelector_,
        retainWeakReference, allowsWeakReference, _tryRetain, _isDeallocating,
        copyWithZone_, mutableCopyWithZone_, copy, mutableCopy, description, hash, isEqual_;
};
extern struct objc_selectors SELS;
void selectors_init(void);
SEL sel_intern(const char *name, bool copy);

/* ---- classes (class.c) ---- */

extern objc_map named_classes;   /* name -> Class */
extern ptr_vec all_classes;      /* every known non-meta class */

void class_table_init(void);
void class_register_name(Class cls);
void class_track(Class cls);
const char *cls_name(Class cls);
Class class_realize(Class cls);                 /* honors the Swift metadata initializer */
Class class_realize_without_swift(Class cls);
void class_initialize_if_needed(Class cls);     /* +initialize; cls is a non-meta class */
method_t *method_list_find(method_list_t *list, SEL sel);
method_t *class_find_method_here(Class cls, SEL sel);
void method_list_fixup(method_list_t *list);
void class_load_category(category_t *cat);      /* at image load */
void class_attach_queued_categories(Class cls);
void class_update_custom_rr(Class cls, method_list_t *list);
void stub_set_class(struct objc_class_stub *stub, Class cls);
Class stub_get_class(struct objc_class_stub *stub);
void class_call_load(Class cls);
void category_call_load(category_t *cat);
id object_cxx_construct(id obj, Class cls);
void object_cxx_destruct(id obj, Class cls);

/* ---- dispatch (dispatch.c) ---- */

IMP cache_lookup(Class cls, SEL sel);
void cache_insert(Class cls, SEL sel, IMP imp);
void class_flush_caches(Class cls);             /* cls and its subclasses */
void flush_all_caches(void);
IMP class_lookup_imp(Class cls, SEL sel, bool resolve);   /* NULL if absent */
IMP objc_lookup_or_forward(id receiver, Class cls, SEL sel, bool initialize);
bool obj_has_custom_rr(id obj);

id objc_send0(id self, SEL sel);
id objc_send1(id self, SEL sel, void *a);
id objc_send2(id self, SEL sel, void *a, void *b);
void objc_sendv0(id self, SEL sel);
void objc_sendv1(id self, SEL sel, void *a);
BOOL objc_sendb1(id self, SEL sel, void *a);
BOOL objc_sendb0(id self, SEL sel);

/* ---- protocols (protocol.c) ---- */

extern objc_map named_protocols;
void protocol_table_init(void);
void protocol_register(protocol_t *p);
protocol_t *protocol_canonical(protocol_t *p);
void protocol_fixup(protocol_t *p);
bool protocol_list_conforms(protocol_list_t *list, protocol_t *target);
void protocol_list_canonicalize(protocol_list_t *list);

/* ---- refcount / weak / assoc / autorelease ---- */

void refcount_init(void);
void refcount_forget(id obj);
bool obj_is_deallocating(id obj);
void weak_init(void);
void weak_clear(id obj);
void assoc_init(void);
void assoc_remove_all(id obj);
void autorelease_init(void);
void autorelease_add(id obj);

/* ---- type encodings (encoding.c) ---- */

const char *enc_skip_qualifiers(const char *p);
const char *enc_skip_type(const char *p);       /* past one type (no offset digits) */
const char *enc_skip_offset(const char *p);
unsigned enc_arg_count(const char *types);      /* arguments including self and _cmd */
char *enc_copy_nth(const char *types, unsigned n);   /* 0 = return type; malloc'd */
void enc_get_nth(const char *types, unsigned n, char *dst, size_t len);

/* ---- hooks (hooks.c) ---- */

void hooks_init(void);
Class objc_hook_lookup_class(const char *name);
const char *objc_hook_lazy_name(Class cls);
const char *objc_hook_image_name(Class cls);

/* ---- images (image.c) ---- */

void objc_runtime_init(void);
void objc_load_images(void);

/* image.c holds the runtime's constructor. A static archive member is only linked when
 * something references it, so every module anchors it. */
extern const int objc_runtime_init_anchor;
#define OBJC_ANCHOR_INIT() \
    static const int *const objc_init_anchor_ref __attribute__((used)) = &objc_runtime_init_anchor

#endif /* LIBOBJC_WASM_INTERNAL_H */
