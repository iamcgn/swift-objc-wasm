#include "internal.h"
OBJC_ANCHOR_INIT();

static objc_map weak_table;   /* obj -> ptr_vec * of id * locations */

void weak_init(void) { map_init(&weak_table, false); }

static void weak_register(id obj, id *loc) {
    ptr_vec *v = map_get(&weak_table, obj);
    if (!v) { v = calloc(1, sizeof *v); map_set(&weak_table, obj, v); }
    vec_push(v, loc);
}

static void weak_unregister(id obj, id *loc) {
    ptr_vec *v = map_get(&weak_table, obj);
    if (!v) return;
    vec_remove(v, loc);
    if (!v->count) { vec_free(v); free(v); map_remove(&weak_table, obj); }
}

void weak_clear(id obj) {
    ptr_vec *v = map_get(&weak_table, obj);
    if (!v) return;
    map_remove(&weak_table, obj);
    for (uint32_t i = 0; i < v->count; i++) *(id *)v->items[i] = nil;
    vec_free(v);
    free(v);
}

static bool obj_allows_weak(id obj) {
    if (obj_is_deallocating(obj)) return false;
    if (obj_has_custom_rr(obj) && class_lookup_imp(obj->isa, SELS.allowsWeakReference, false))
        return objc_sendb0(obj, SELS.allowsWeakReference);
    return true;
}

id objc_storeWeak(id *location, id obj) {
    id old = *location;
    if (old == obj) return obj;
    if (old) weak_unregister(old, location);
    if (obj && !obj_allows_weak(obj)) obj = nil;
    if (obj) weak_register(obj, location);
    *location = obj;
    return obj;
}

id objc_storeWeakOrNil(id *location, id obj) { return objc_storeWeak(location, obj); }
id objc_initWeak(id *location, id val) { *location = nil; return objc_storeWeak(location, val); }
id objc_initWeakOrNil(id *location, id val) { return objc_initWeak(location, val); }

id objc_loadWeakRetained(id *location) {
    id obj = *location;
    if (!obj) return nil;
    if (obj_has_custom_rr(obj)) {
        if (class_lookup_imp(obj->isa, SELS.retainWeakReference, false))
            return objc_sendb0(obj, SELS.retainWeakReference) ? obj : nil;
        return objc_retain(obj);
    }
    return _objc_rootTryRetain(obj) ? obj : nil;
}

id objc_loadWeak(id *location) { return objc_autorelease(objc_loadWeakRetained(location)); }

void objc_destroyWeak(id *location) {
    id obj = *location;
    if (obj) weak_unregister(obj, location);
}

void objc_copyWeak(id *to, id *from) {
    id v = objc_loadWeakRetained(from);
    objc_initWeak(to, v);
    objc_release(v);
}

void objc_moveWeak(id *to, id *from) {
    objc_copyWeak(to, from);
    objc_destroyWeak(from);
    *from = nil;
}
