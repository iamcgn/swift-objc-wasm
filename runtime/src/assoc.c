// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#include "internal.h"
OBJC_ANCHOR_INIT();

typedef struct assoc_value { id value; uintptr_t policy; } assoc_value;
static objc_map assoc_table;   /* obj -> objc_map * (key -> assoc_value *) */

void assoc_init(void) { map_init(&assoc_table, false); }

static void release_value(assoc_value *v) {
    if (v->policy & 3) objc_release(v->value);
    free(v);
}

void objc_setAssociatedObject(id object, const void *key, id value, objc_AssociationPolicy policy) {
    if (!object || !key) return;
    Class cls = object->isa;
    class_realize(cls);
    if (cls_rw(cls)->flags & RW_FORBIDS_ASSOC)
        _objc_fatal("libobjc-wasm: objc_setAssociatedObject() called on an instance (%p) of class %s that forbids associated objects",
                    (void *)object, cls_name(cls));
    objc_map *m = map_get(&assoc_table, object);
    if (!m) {
        if (!value) return;
        m = calloc(1, sizeof *m);
        map_init(m, false);
        map_set(&assoc_table, object, m);
    }
    id newv = value;
    if (newv) {
        if ((policy & 3) == 3) newv = objc_send0(newv, SELS.copy);
        else if (policy & 3) objc_retain(newv);
    }
    objc_map_entry *e = map_find(m, key);
    assoc_value *old = e ? e->value : NULL;
    if (newv) {
        assoc_value *nv = malloc(sizeof *nv);
        nv->value = newv; nv->policy = policy;
        map_set(m, key, nv);
    } else if (e) {
        map_remove(m, key);
    }
    if (old) release_value(old);
}

id objc_getAssociatedObject(id object, const void *key) {
    if (!object || !key) return nil;
    objc_map *m = map_get(&assoc_table, object);
    if (!m) return nil;
    assoc_value *v = map_get(m, key);
    if (!v) return nil;
    id value = v->value;
    if (v->policy & 0x100) objc_retain(value);
    if (v->policy & 0x200) objc_autorelease(value);
    return value;
}

static void collect_value(const void *key, void *value, void *ctx) {
    (void)key;
    vec_push((ptr_vec *)ctx, value);
}

void assoc_remove_all(id object) {
    objc_map *m = map_get(&assoc_table, object);
    if (!m) return;
    map_remove(&assoc_table, object);
    ptr_vec values = {0};
    map_foreach(m, collect_value, &values);
    map_free(m);
    free(m);
    for (uint32_t i = 0; i < values.count; i++) release_value((assoc_value *)values.items[i]);
    vec_free(&values);
}

void objc_removeAssociatedObjects(id object) {
    if (object) assoc_remove_all(object);
}
