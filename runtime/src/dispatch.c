#include "internal.h"
OBJC_ANCHOR_INIT();

/* ---- per-class method cache ---- */

static inline uint32_t cache_hash(SEL sel, uint32_t mask) {
    uint32_t x = (uint32_t)(uintptr_t)sel;
    x ^= x >> 7; x *= 0x9e3779b1u; x ^= x >> 15;
    return x & mask;
}

IMP cache_lookup(Class cls, SEL sel) {
    class_rw_t *rw = cls_rw(cls);
    if (!rw->cache) return NULL;
    uint32_t i = cache_hash(sel, rw->cacheMask);
    for (;;) {
        cache_entry *e = &rw->cache[i];
        if (!e->sel) return NULL;
        if (e->sel == sel) return e->imp;
        i = (i + 1) & rw->cacheMask;
    }
}

static void cache_insert_raw(class_rw_t *rw, SEL sel, IMP imp) {
    uint32_t i = cache_hash(sel, rw->cacheMask);
    while (rw->cache[i].sel) i = (i + 1) & rw->cacheMask;
    rw->cache[i].sel = sel; rw->cache[i].imp = imp;
    rw->cacheOccupied++;
}

void cache_insert(Class cls, SEL sel, IMP imp) {
    class_rw_t *rw = cls_rw(cls);
    uint32_t cap = rw->cache ? rw->cacheMask + 1 : 0;
    if ((rw->cacheOccupied + 1) * 4 >= cap * 3) {
        uint32_t newcap = cap ? cap * 2 : 8;
        cache_entry *old = rw->cache;
        rw->cache = calloc(newcap, sizeof(cache_entry));
        if (!rw->cache) _objc_fatal("libobjc-wasm: out of memory");
        rw->cacheMask = newcap - 1; rw->cacheOccupied = 0;
        for (uint32_t i = 0; i < cap; i++) if (old[i].sel) cache_insert_raw(rw, old[i].sel, old[i].imp);
        free(old);
    }
    cache_insert_raw(rw, sel, imp);
}

static void cache_flush_one(Class cls) {
    if (!cls || cls_is_stub(cls) || !cls_is_realized(cls)) return;
    class_rw_t *rw = cls_rw(cls);
    free(rw->cache);
    rw->cache = NULL; rw->cacheMask = 0; rw->cacheOccupied = 0;
}

void class_flush_caches(Class cls) {
    if (!cls || !cls_is_realized(cls)) return;
    cache_flush_one(cls);
    for (Class sub = cls_rw(cls)->firstSubclass; sub; sub = cls_rw(sub)->nextSiblingClass)
        class_flush_caches(sub);
}

void flush_all_caches(void) {
    for (uint32_t i = 0; i < all_classes.count; i++) {
        Class c = (Class)all_classes.items[i];
        cache_flush_one(c);
        if (c->isa) cache_flush_one(c->isa);
    }
}

/* ---- lookup ---- */

static IMP find_imp_in_hierarchy(Class cls, SEL sel) {
    for (Class c = cls; c; c = c->superclass) {
        class_realize(c);
        method_t *m = class_find_method_here(c, sel);
        if (m) return m->imp;
    }
    return NULL;
}

static bool try_resolve(Class cls, SEL sel) {
    if (sel == SELS.resolveInstanceMethod_ || sel == SELS.resolveClassMethod_) return false;
    if (cls_is_meta(cls)) {
        Class nonmeta = cls_rw(cls)->nonMetaClass;
        if (!nonmeta) return false;
        IMP r = class_lookup_imp(cls, SELS.resolveClassMethod_, false);
        if (r && ((BOOL (*)(id, SEL, SEL))r)((id)nonmeta, SELS.resolveClassMethod_, sel)) return true;
        /* A class method may also be resolved as an instance method of the metaclass. */
        IMP r2 = class_lookup_imp(cls->isa, SELS.resolveInstanceMethod_, false);
        if (r2 && ((BOOL (*)(id, SEL, SEL))r2)((id)cls, SELS.resolveInstanceMethod_, sel)) return true;
        return false;
    }
    IMP r = class_lookup_imp(cls->isa, SELS.resolveInstanceMethod_, false);
    return r && ((BOOL (*)(id, SEL, SEL))r)((id)cls, SELS.resolveInstanceMethod_, sel);
}

IMP class_lookup_imp(Class cls, SEL sel, bool resolve) {
    if (!cls || !sel) return NULL;
    class_realize(cls);
    IMP imp = cache_lookup(cls, sel);
    if (imp) return imp;
    imp = find_imp_in_hierarchy(cls, sel);
    if (!imp && resolve && try_resolve(cls, sel)) imp = find_imp_in_hierarchy(cls, sel);
    if (imp) cache_insert(cls, sel, imp);
    return imp;
}

IMP objc_lookup_or_forward(id receiver, Class cls, SEL sel, bool initialize) {
    class_realize(cls);
    if (initialize) {
        Class toInit = cls_is_meta(cls) ? (Class)receiver : cls;
        class_initialize_if_needed(toInit);
    }
    IMP imp = class_lookup_imp(cls, sel, true);
    if (imp) return imp;

    IMP fwd = class_lookup_imp(cls, SELS.forwardingTargetForSelector_, false);
    if (fwd) {
        id target = ((id (*)(id, SEL, SEL))fwd)(receiver, SELS.forwardingTargetForSelector_, sel);
        if (target && target != receiver)
            _objc_fatal("libobjc-wasm: %c[%s %s] forwardingTargetForSelector: returned %p, but message forwarding is not supported on this platform",
                        cls_is_meta(cls) ? '+' : '-', cls_name(cls), sel_getName(sel), (void *)target);
    }
    IMP dnr = class_lookup_imp(cls, SELS.doesNotRecognizeSelector_, false);
    if (dnr) ((void (*)(id, SEL, SEL))dnr)(receiver, SELS.doesNotRecognizeSelector_, sel);
    _objc_fatal("libobjc-wasm: %c[%s %s]: unrecognized selector sent to %s %p",
                cls_is_meta(cls) ? '+' : '-', cls_name(cls), sel_getName(sel),
                cls_is_meta(cls) ? "class" : "instance", (void *)receiver);
}

static id nil_receiver_imp(id self, SEL _cmd) { (void)self; (void)_cmd; return nil; }

IMP objc_msgLookup(id receiver, SEL sel) {
    if (!receiver) return (IMP)nil_receiver_imp;
    Class cls = receiver->isa;
    if (cls_is_realized(cls)) {
        IMP imp = cache_lookup(cls, sel);
        if (imp) return imp;
    }
    return objc_lookup_or_forward(receiver, cls, sel, true);
}

IMP objc_msgLookupSuper2(struct objc_super *super, SEL sel) {
    Class cur = super->super_class;
    class_realize(cur);
    Class target = cur->superclass;
    if (!target)
        _objc_fatal("libobjc-wasm: super send of %s from root class %s", sel_getName(sel), cls_name(cur));
    return objc_lookup_or_forward(super->receiver, target, sel, false);
}

IMP objc_msgLookupSuper(struct objc_super *super, SEL sel) {
    return objc_lookup_or_forward(super->receiver, super->super_class, sel, false);
}

bool obj_has_custom_rr(id obj) {
    Class c = obj->isa;
    class_realize(c);
    return (cls_rw(c)->flags & RW_HAS_CUSTOM_RR) != 0;
}

/* ---- typed helper sends ---- */

id objc_send0(id self, SEL sel) {
    if (!self) return nil;
    return ((id (*)(id, SEL))objc_msgLookup(self, sel))(self, sel);
}

id objc_send1(id self, SEL sel, void *a) {
    if (!self) return nil;
    return ((id (*)(id, SEL, void *))objc_msgLookup(self, sel))(self, sel, a);
}

id objc_send2(id self, SEL sel, void *a, void *b) {
    if (!self) return nil;
    return ((id (*)(id, SEL, void *, void *))objc_msgLookup(self, sel))(self, sel, a, b);
}

void objc_sendv0(id self, SEL sel) {
    if (!self) return;
    ((void (*)(id, SEL))objc_msgLookup(self, sel))(self, sel);
}

void objc_sendv1(id self, SEL sel, void *a) {
    if (!self) return;
    ((void (*)(id, SEL, void *))objc_msgLookup(self, sel))(self, sel, a);
}

BOOL objc_sendb0(id self, SEL sel) {
    if (!self) return NO;
    return ((BOOL (*)(id, SEL))objc_msgLookup(self, sel))(self, sel);
}

BOOL objc_sendb1(id self, SEL sel, void *a) {
    if (!self) return NO;
    return ((BOOL (*)(id, SEL, void *))objc_msgLookup(self, sel))(self, sel, a);
}
