// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#include "internal.h"
OBJC_ANCHOR_INIT();
#include <stdio.h>

objc_map named_classes;
ptr_vec all_classes;
static objc_map known_classes;        /* Class -> 1, to avoid double tracking */
static objc_map pending_categories;   /* Class or stub -> ptr_vec * of category_t * */
static objc_map stub_classes;         /* stub -> Class */
static _objc_classCopyFixupHandler copy_fixup_handler;

uintptr_t _objc_empty_cache[4];
void (*_objc_empty_vtable)(void);

void class_table_init(void) {
    map_init(&named_classes, true);
    map_init(&known_classes, false);
    map_init(&pending_categories, false);
    map_init(&stub_classes, false);
}

/* ---- names ---- */

const char *cls_name(Class cls) {
    if (!cls) return "nil";
    if (cls_is_stub(cls)) return "<swift stub class>";
    const class_ro_t *ro = cls_ro(cls);
    if (ro->name) return ro->name;
    if (cls_is_realized(cls)) {
        class_rw_t *rw = cls_rw(cls);
        if (rw->lazyName) return rw->lazyName;
        Class nonmeta = (rw->flags & RW_META) ? rw->nonMetaClass : cls;
        if (nonmeta) {
            const char *n = objc_hook_lazy_name(nonmeta);
            if (n) {
                rw->lazyName = n;
                if (nonmeta != cls && cls_is_realized(nonmeta)) cls_rw(nonmeta)->lazyName = n;
                if (!map_find(&named_classes, n)) map_set(&named_classes, n, nonmeta);
                return n;
            }
        }
    }
    return "<unnamed class>";
}

void class_register_name(Class cls) {
    const class_ro_t *ro = cls_ro(cls);
    if (!ro->name) return;
    objc_map_entry *e = map_find(&named_classes, ro->name);
    if (!e) map_set(&named_classes, ro->name, cls);
    else if (e->value != cls) _objc_inform("libobjc-wasm: class %s is implemented twice; using the first", ro->name);
}

void class_track(Class cls) {
    if (map_find(&known_classes, cls)) return;
    map_set(&known_classes, cls, (void *)1);
    vec_push(&all_classes, cls);
}

/* ---- method lists ---- */

void method_list_fixup(method_list_t *list) {
    if (!list) return;
    if (list->entsizeAndFlags & METHOD_LIST_IS_SMALL)
        _objc_fatal("libobjc-wasm: relative method lists are not supported (entsize flags 0x%x)", list->entsizeAndFlags);
    if ((list->entsizeAndFlags & METHOD_LIST_IS_FIXED_UP) == METHOD_LIST_IS_FIXED_UP) return;
    uint32_t entsize = METHOD_LIST_ENTSIZE(list);
    if (entsize != sizeof(method_t))
        _objc_fatal("libobjc-wasm: unexpected method list entry size %u", entsize);
    for (uint32_t i = 0; i < list->count; i++)
        list->first[i].name = sel_intern((const char *)list->first[i].name, false);
    list->entsizeAndFlags |= METHOD_LIST_IS_FIXED_UP;
}

method_t *method_list_find(method_list_t *list, SEL sel) {
    if (!list) return NULL;
    for (uint32_t i = 0; i < list->count; i++)
        if (list->first[i].name == sel) return &list->first[i];
    return NULL;
}

method_t *class_find_method_here(Class cls, SEL sel) {
    class_rw_t *rw = cls_rw(cls);
    for (uint32_t i = 0; i < rw->methods.count; i++) {
        method_t *m = method_list_find((method_list_t *)rw->methods.items[i], sel);
        if (m) return m;
    }
    return NULL;
}

static method_list_t *method_list_alloc(uint32_t count) {
    method_list_t *l = calloc(1, sizeof(method_list_t) + count * sizeof(method_t));
    if (!l) _objc_fatal("libobjc-wasm: out of memory");
    l->entsizeAndFlags = sizeof(method_t) | METHOD_LIST_IS_FIXED_UP;
    l->count = count;
    return l;
}

static bool is_nsobject_root(Class cls) {
    const class_ro_t *ro = cls_ro(cls);
    return ro->name && strcmp(ro->name, "NSObject") == 0;
}

static void set_custom_rr_recursive(Class cls) {
    class_rw_t *rw = cls_rw(cls);
    if (rw->flags & RW_HAS_CUSTOM_RR) return;
    rw->flags |= RW_HAS_CUSTOM_RR;
    for (Class sub = rw->firstSubclass; sub; sub = cls_rw(sub)->nextSiblingClass)
        set_custom_rr_recursive(sub);
}

/* NSObject's own retain/release are the defaults; any other implementation of the
 * reference-counting selectors makes the class (and its subclasses) custom-RR, which
 * routes objc_retain/objc_release through message sends. */
void class_update_custom_rr(Class cls, method_list_t *list) {
    if (!list || cls_is_meta(cls) || is_nsobject_root(cls)) return;
    for (uint32_t i = 0; i < list->count; i++) {
        SEL s = list->first[i].name;
        if (s == SELS.retain || s == SELS.release || s == SELS.autorelease || s == SELS.retainCount ||
            s == SELS._tryRetain || s == SELS._isDeallocating || s == SELS.retainWeakReference ||
            s == SELS.allowsWeakReference) {
            set_custom_rr_recursive(cls);
            return;
        }
    }
}

/* ---- realization ---- */

static class_ro_t *ro_duplicate(const class_ro_t *ro) {
    size_t size = sizeof(class_ro_t) + ((ro->flags & RO_HAS_SWIFT_INITIALIZER) ? sizeof(void *) : 0);
    class_ro_t *copy = malloc(size);
    if (!copy) _objc_fatal("libobjc-wasm: out of memory");
    memcpy(copy, ro, size);
    return copy;
}

static uint32_t ivar_alignment(const ivar_t *iv) {
    uint32_t a = iv->alignment_raw;
    if (a == (uint32_t)~0) return sizeof(void *);
    return 1u << a;
}

/* Slide the ivars of `cls` so that they start after the superclass's instance size. */
static void reconcile_instance_layout(Class cls, class_rw_t *rw, Class super) {
    const class_ro_t *ro = rw->ro;
    const class_ro_t *sro = cls_ro(super);
    uint32_t superSize = sro->instanceSize;
    if (ro->instanceStart >= superSize) return;

    uint32_t maxAlign = 1;
    if (ro->ivars) {
        for (uint32_t i = 0; i < ro->ivars->count; i++) {
            uint32_t a = ivar_alignment(&ro->ivars->first[i]);
            if (a > maxAlign) maxAlign = a;
        }
    }
    uint32_t start = align_up(superSize, maxAlign);
    int32_t delta = (int32_t)(start - ro->instanceStart);
    class_ro_t *w = ro_duplicate(ro);
    if (w->ivars) {
        for (uint32_t i = 0; i < w->ivars->count; i++) {
            ivar_t *iv = (ivar_t *)&w->ivars->first[i];
            if (iv->offset) *iv->offset += delta;
        }
    }
    w->instanceStart += (uint32_t)delta;
    w->instanceSize += (uint32_t)delta;
    rw->ro = w;
    rw->flags |= RW_RO_OWNED;
    (void)cls;
}

static Class realize_impl(Class cls) {
    if (!cls) return Nil;
    if (cls_is_stub(cls)) _objc_fatal("libobjc-wasm: attempt to realize a Swift stub class %p", (void *)cls);
    if (cls_is_realized(cls)) return cls;

    const class_ro_t *ro = (const class_ro_t *)cls_data(cls);
    bool isMeta = (ro->flags & RO_META) != 0;

    class_rw_t *rw = calloc(1, sizeof *rw);
    if (!rw) _objc_fatal("libobjc-wasm: out of memory");
    rw->flags = RW_REALIZED | (isMeta ? RW_META : 0);
    rw->ro = ro;
    cls->bits = (uintptr_t)rw | (cls->bits & FAST_IS_SWIFT_MASK);
    if (!cls->cache) cls->cache = _objc_empty_cache;

    Class super = cls->superclass;
    if (super) class_realize(super);
    Class meta = cls->isa;
    if (!isMeta && meta) {
        realize_impl(meta);
        cls_rw(meta)->nonMetaClass = cls;
    }

    if (!isMeta && super) reconcile_instance_layout(cls, rw, super);
    ro = rw->ro;

    if (ro->flags & RO_HAS_CXX_STRUCTORS) {
        rw->flags |= RW_HAS_CXX_DTOR;
        if (!(ro->flags & RO_HAS_CXX_DTOR_ONLY)) rw->flags |= RW_HAS_CXX_CTOR;
    }
    if (ro->flags & RO_FORBIDS_ASSOCIATED_OBJECTS) rw->flags |= RW_FORBIDS_ASSOC;
    if (super) {
        uint32_t sflags = cls_rw(super)->flags;
        rw->flags |= sflags & (RW_HAS_CXX_CTOR | RW_HAS_CXX_DTOR | RW_FORBIDS_ASSOC | RW_HAS_CUSTOM_RR);
        rw->nextSiblingClass = cls_rw(super)->firstSubclass;
        cls_rw(super)->firstSubclass = cls;
    }

    if (ro->baseMethodList) {
        method_list_fixup(ro->baseMethodList);
        vec_push(&rw->methods, ro->baseMethodList);
        class_update_custom_rr(cls, ro->baseMethodList);
    }
    if (ro->baseProtocols) {
        protocol_list_canonicalize(ro->baseProtocols);
        vec_push(&rw->protocols, ro->baseProtocols);
    }
    if (ro->baseProperties) vec_push(&rw->properties, ro->baseProperties);

    class_attach_queued_categories(cls);
    return cls;
}

Class class_realize_without_swift(Class cls) { return realize_impl(cls); }

Class class_realize(Class cls) {
    if (!cls || cls_is_realized(cls)) return cls;
    if (cls_is_stub(cls)) _objc_fatal("libobjc-wasm: attempt to realize a Swift stub class %p", (void *)cls);
    if (cls_is_swift(cls)) {
        const class_ro_t *ro = (const class_ro_t *)cls_data(cls);
        Class (*init)(Class, void *) = class_ro_swift_initializer(ro);
        if (init) {
            /* The Swift metadata update callback fixes up the class and calls
             * _objc_realizeClassFromSwift(cls, cls), which realizes it. */
            Class newcls = init(cls, NULL);
            if (newcls != cls)
                _objc_fatal("libobjc-wasm: Swift metadata initializer for %s returned a different class", cls_name(cls));
            if (!cls_is_realized(cls)) realize_impl(cls);
            return cls;
        }
    }
    return realize_impl(cls);
}

/* ---- +initialize ---- */

static IMP find_imp_no_cache(Class cls, SEL sel) {
    for (Class c = cls; c; c = c->superclass) {
        class_realize(c);
        method_t *m = class_find_method_here(c, sel);
        if (m) return m->imp;
    }
    return NULL;
}

void class_initialize_if_needed(Class cls) {
    class_realize(cls);
    class_rw_t *rw = cls_rw(cls);
    if (rw->flags & (RW_INITIALIZED | RW_INITIALIZING)) return;
    rw->flags |= RW_INITIALIZING;
    if (cls->superclass) class_initialize_if_needed(cls->superclass);
    IMP imp = find_imp_no_cache(cls->isa, SELS.initialize);
    if (imp) ((void (*)(id, SEL))imp)((id)cls, SELS.initialize);
    rw->flags = (rw->flags | RW_INITIALIZED) & ~RW_INITIALIZING;
}

/* ---- categories ---- */

static ptr_vec *pending_for(const void *target, bool create) {
    ptr_vec *v = map_get(&pending_categories, target);
    if (!v && create) {
        v = calloc(1, sizeof *v);
        map_set(&pending_categories, target, v);
    }
    return v;
}

static void class_attach_category(Class cls, category_t *cat) {
    class_rw_t *rw = cls_rw(cls);
    if (cat->instanceMethods) {
        method_list_fixup(cat->instanceMethods);
        vec_insert_front(&rw->methods, cat->instanceMethods);
        class_update_custom_rr(cls, cat->instanceMethods);
    }
    if (cat->protocols) {
        protocol_list_canonicalize(cat->protocols);
        vec_push(&rw->protocols, cat->protocols);
    }
    if (cat->instanceProperties) vec_push(&rw->properties, cat->instanceProperties);
    Class meta = cls->isa;
    if (meta && !cls_is_stub(meta)) {
        realize_impl(meta);
        class_rw_t *mrw = cls_rw(meta);
        if (cat->classMethods) {
            method_list_fixup(cat->classMethods);
            vec_insert_front(&mrw->methods, cat->classMethods);
        }
        if (CATEGORY_HAS_CLASS_PROPERTIES(cat) && cat->_classProperties)
            vec_push(&mrw->properties, cat->_classProperties);
        class_flush_caches(meta);
    }
    class_flush_caches(cls);
}

void class_attach_queued_categories(Class cls) {
    ptr_vec *v = pending_for(cls, false);
    if (!v) return;
    map_remove(&pending_categories, cls);
    for (uint32_t i = 0; i < v->count; i++) class_attach_category(cls, (category_t *)v->items[i]);
    vec_free(v);
    free(v);
}

void class_load_category(category_t *cat) {
    Class target = cat->cls;
    if (!target) return;
    if (cls_is_stub(target)) {
        Class real = stub_get_class((struct objc_class_stub *)target);
        if (!real) { vec_push(pending_for(target, true), cat); return; }
        target = real;
    }
    if (cls_is_realized(target)) class_attach_category(target, cat);
    else vec_push(pending_for(target, true), cat);
}

Class stub_get_class(struct objc_class_stub *stub) { return map_get(&stub_classes, stub); }

void stub_set_class(struct objc_class_stub *stub, Class cls) {
    map_set(&stub_classes, stub, cls);
    ptr_vec *v = pending_for(stub, false);
    if (!v) return;
    map_remove(&pending_categories, stub);
    for (uint32_t i = 0; i < v->count; i++) {
        category_t *cat = (category_t *)v->items[i];
        if (cls_is_realized(cls)) class_attach_category(cls, cat);
        else vec_push(pending_for(cls, true), cat);
    }
    vec_free(v);
    free(v);
}

/* ---- +load ---- */

void class_call_load(Class cls) {
    class_realize(cls);
    class_rw_t *rw = cls_rw(cls);
    if (rw->flags & RW_LOADED) return;
    rw->flags |= RW_LOADED;
    if (cls->superclass) class_call_load(cls->superclass);
    method_t *m = class_find_method_here(cls->isa, SELS.load);
    if (m && !vec_contains(&cls_rw(cls->isa)->methods, (void *)0)) {
        /* Only a +load implemented by the class itself (its base list), not by a category. */
        const class_ro_t *mro = cls_ro(cls->isa);
        if (mro->baseMethodList && method_list_find(mro->baseMethodList, SELS.load) == m)
            ((void (*)(id, SEL))m->imp)((id)cls, SELS.load);
    }
}

void category_call_load(category_t *cat) {
    Class target = cat->cls;
    if (!target || cls_is_stub(target)) return;
    class_realize(target);
    method_list_fixup(cat->classMethods);
    method_t *m = method_list_find(cat->classMethods, SELS.load);
    if (m) ((void (*)(id, SEL))m->imp)((id)target, SELS.load);
}

/* ---- Swift SPI ---- */

Class _objc_realizeClassFromSwift(Class cls, void *previously) {
    if (previously && previously != (void *)cls && IS_STUB_CLASS_ISA(((struct objc_class_stub *)previously)->isa))
        stub_set_class((struct objc_class_stub *)previously, cls);
    if (!cls) return Nil;
    class_register_name(cls);
    class_track(cls);
    return realize_impl(cls);
}

Class objc_readClassPair(Class cls, const struct objc_image_info *info) {
    (void)info;
    if (!cls) return Nil;
    if (cls_is_realized(cls)) return cls;
    class_register_name(cls);
    class_track(cls);
    return realize_impl(cls);
}

Class objc_loadClassref(Class *clsref) {
    Class cls = *clsref;
    if (!((uintptr_t)cls & 1)) return cls;
    struct objc_class_stub *stub = (struct objc_class_stub *)((uintptr_t)cls & ~(uintptr_t)1);
    Class real = stub_get_class(stub);
    if (!real) {
        real = stub->initializer((Class)stub, NULL);
        if (!real) _objc_fatal("libobjc-wasm: Swift stub class initializer %p returned nil", (void *)stub);
        if (!stub_get_class(stub)) stub_set_class(stub, real);
    }
    *clsref = real;
    return real;
}

BOOL _objc_supportsLazyRealization(void) { return YES; }

void _objc_setClassCopyFixupHandler(_objc_classCopyFixupHandler newFixupHandler) {
    copy_fixup_handler = newFixupHandler;
}

/* ---- class lookup ---- */

Class objc_getClass(const char *name) {
    if (!name) return Nil;
    Class cls = map_get(&named_classes, name);
    if (cls) return cls;
    cls = objc_hook_lookup_class(name);
    if (cls) {
        class_track(cls);
        if (!map_find(&named_classes, name)) map_set(&named_classes, strdup(name), cls);
    }
    return cls;
}

Class objc_lookUpClass(const char *name) { return objc_getClass(name); }

Class objc_getRequiredClass(const char *name) {
    Class cls = objc_getClass(name);
    if (!cls) _objc_fatal("libobjc-wasm: link error: class '%s' not found.", name);
    return cls;
}

Class objc_getMetaClass(const char *name) {
    Class cls = objc_getClass(name);
    return cls ? cls->isa : Nil;
}

int objc_getClassList(Class *buffer, int bufferCount) {
    int n = (int)all_classes.count;
    if (buffer) {
        for (int i = 0; i < n && i < bufferCount; i++) {
            Class c = (Class)all_classes.items[i];
            class_realize(c);
            buffer[i] = c;
        }
    }
    return n;
}

Class *objc_copyClassList(unsigned int *outCount) {
    unsigned n = all_classes.count;
    Class *list = malloc((n + 1) * sizeof(Class));
    if (!list) _objc_fatal("libobjc-wasm: out of memory");
    for (unsigned i = 0; i < n; i++) { list[i] = (Class)all_classes.items[i]; class_realize(list[i]); }
    list[n] = Nil;
    if (outCount) *outCount = n;
    return list;
}

/* ---- class information ---- */

const char *class_getName(Class cls) { return cls_name(cls); }
BOOL class_isMetaClass(Class cls) { return cls ? cls_is_meta(cls) : NO; }

Class class_getSuperclass(Class cls) {
    if (!cls) return Nil;
    class_realize(cls);
    return cls->superclass;
}

Class class_setSuperclass(Class cls, Class newSuper) {
    class_realize(cls);
    class_realize(newSuper);
    Class old = cls->superclass;
    if (old && cls_is_realized(old)) {
        class_rw_t *orw = cls_rw(old);
        if (orw->firstSubclass == cls) orw->firstSubclass = cls_rw(cls)->nextSiblingClass;
        else for (Class s = orw->firstSubclass; s; s = cls_rw(s)->nextSiblingClass)
            if (cls_rw(s)->nextSiblingClass == cls) { cls_rw(s)->nextSiblingClass = cls_rw(cls)->nextSiblingClass; break; }
    }
    cls->superclass = newSuper;
    cls_rw(cls)->nextSiblingClass = cls_rw(newSuper)->firstSubclass;
    cls_rw(newSuper)->firstSubclass = cls;
    uint32_t inherit = cls_rw(newSuper)->flags & (RW_HAS_CUSTOM_RR | RW_HAS_CXX_CTOR | RW_HAS_CXX_DTOR | RW_FORBIDS_ASSOC);
    cls_rw(cls)->flags |= inherit;

    Class meta = cls->isa, newMeta = newSuper->isa;
    if (meta && newMeta && !cls_is_meta(cls)) {
        realize_impl(meta); realize_impl(newMeta);
        Class oldMeta = meta->superclass;
        if (oldMeta && cls_is_realized(oldMeta)) {
            class_rw_t *orw = cls_rw(oldMeta);
            if (orw->firstSubclass == meta) orw->firstSubclass = cls_rw(meta)->nextSiblingClass;
            else for (Class s = orw->firstSubclass; s; s = cls_rw(s)->nextSiblingClass)
                if (cls_rw(s)->nextSiblingClass == meta) { cls_rw(s)->nextSiblingClass = cls_rw(meta)->nextSiblingClass; break; }
        }
        meta->superclass = newMeta;
        cls_rw(meta)->nextSiblingClass = cls_rw(newMeta)->firstSubclass;
        cls_rw(newMeta)->firstSubclass = meta;
        class_flush_caches(meta);
    }
    class_flush_caches(cls);
    return old;
}

int class_getVersion(Class cls) { if (!cls) return 0; class_realize(cls); return (int)cls_rw(cls)->version; }
void class_setVersion(Class cls, int version) { if (!cls) return; class_realize(cls); cls_rw(cls)->version = (uint32_t)version; }

size_t class_getInstanceSize(Class cls) {
    if (!cls) return 0;
    class_realize(cls);
    return align_up(cls_ro(cls)->instanceSize, sizeof(void *));
}

static Ivar ivar_list_find(const ivar_list_t *list, const char *name) {
    if (!list) return NULL;
    for (uint32_t i = 0; i < list->count; i++)
        if (list->first[i].name && strcmp(list->first[i].name, name) == 0) return (Ivar)&list->first[i];
    return NULL;
}

Ivar class_getInstanceVariable(Class cls, const char *name) {
    if (!cls || !name) return NULL;
    class_realize(cls);
    for (Class c = cls; c; c = c->superclass) {
        class_realize(c);
        Ivar iv = ivar_list_find(cls_ro(c)->ivars, name);
        if (iv) return iv;
    }
    return NULL;
}

Ivar class_getClassVariable(Class cls, const char *name) {
    if (!cls) return NULL;
    class_realize(cls);
    return class_getInstanceVariable(cls->isa, name);
}

Ivar *class_copyIvarList(Class cls, unsigned int *outCount) {
    if (!cls) { if (outCount) *outCount = 0; return NULL; }
    class_realize(cls);
    const ivar_list_t *list = cls_ro(cls)->ivars;
    unsigned n = list ? list->count : 0;
    if (!n) { if (outCount) *outCount = 0; return NULL; }
    Ivar *out = malloc((n + 1) * sizeof(Ivar));
    for (unsigned i = 0; i < n; i++) out[i] = (Ivar)&list->first[i];
    out[n] = NULL;
    if (outCount) *outCount = n;
    return out;
}

static Method search_method(Class cls, SEL sel) {
    for (Class c = cls; c; c = c->superclass) {
        class_realize(c);
        method_t *m = class_find_method_here(c, sel);
        if (m) return (Method)m;
    }
    return NULL;
}

Method class_getInstanceMethod(Class cls, SEL sel) {
    if (!cls || !sel) return NULL;
    class_realize(cls);
    Method m = search_method(cls, sel);
    if (m) return m;
    class_lookup_imp(cls, sel, true);   /* may run +resolveInstanceMethod: */
    return search_method(cls, sel);
}

Method class_getClassMethod(Class cls, SEL sel) {
    if (!cls) return NULL;
    class_realize(cls);
    return class_getInstanceMethod(cls->isa, sel);
}

extern void _objc_msgForward(void);

IMP class_getMethodImplementation(Class cls, SEL sel) {
    if (!cls || !sel) return NULL;
    IMP imp = class_lookup_imp(cls, sel, true);
    return imp ? imp : (IMP)_objc_msgForward;
}

BOOL class_respondsToSelector(Class cls, SEL sel) {
    if (!cls || !sel) return NO;
    return class_lookup_imp(cls, sel, true) != NULL;
}

Method *class_copyMethodList(Class cls, unsigned int *outCount) {
    if (!cls) { if (outCount) *outCount = 0; return NULL; }
    class_realize(cls);
    class_rw_t *rw = cls_rw(cls);
    unsigned n = 0;
    for (uint32_t i = 0; i < rw->methods.count; i++) n += ((method_list_t *)rw->methods.items[i])->count;
    if (!n) { if (outCount) *outCount = 0; return NULL; }
    Method *out = malloc((n + 1) * sizeof(Method));
    unsigned k = 0;
    for (uint32_t i = 0; i < rw->methods.count; i++) {
        method_list_t *l = rw->methods.items[i];
        for (uint32_t j = 0; j < l->count; j++) out[k++] = (Method)&l->first[j];
    }
    out[n] = NULL;
    if (outCount) *outCount = n;
    return out;
}

BOOL class_conformsToProtocol(Class cls, Protocol *protocol) {
    if (!cls || !protocol) return NO;
    class_realize(cls);
    class_rw_t *rw = cls_rw(cls);
    for (uint32_t i = 0; i < rw->protocols.count; i++)
        if (protocol_list_conforms((protocol_list_t *)rw->protocols.items[i], (protocol_t *)protocol)) return YES;
    return NO;
}

Protocol **class_copyProtocolList(Class cls, unsigned int *outCount) {
    if (!cls) { if (outCount) *outCount = 0; return NULL; }
    class_realize(cls);
    class_rw_t *rw = cls_rw(cls);
    unsigned n = 0;
    for (uint32_t i = 0; i < rw->protocols.count; i++) n += ((protocol_list_t *)rw->protocols.items[i])->count;
    if (!n) { if (outCount) *outCount = 0; return NULL; }
    Protocol **out = malloc((n + 1) * sizeof(Protocol *));
    unsigned k = 0;
    for (uint32_t i = 0; i < rw->protocols.count; i++) {
        protocol_list_t *l = rw->protocols.items[i];
        for (uintptr_t j = 0; j < l->count; j++) out[k++] = (Protocol *)protocol_canonical(l->list[j]);
    }
    out[n] = NULL;
    if (outCount) *outCount = n;
    return out;
}

static objc_property_t property_list_find(property_list_t *list, const char *name) {
    if (!list) return NULL;
    for (uint32_t i = 0; i < list->count; i++)
        if (strcmp(list->first[i].name, name) == 0) return (objc_property_t)&list->first[i];
    return NULL;
}

objc_property_t class_getProperty(Class cls, const char *name) {
    if (!cls || !name) return NULL;
    for (Class c = cls; c; c = c->superclass) {
        class_realize(c);
        class_rw_t *rw = cls_rw(c);
        for (uint32_t i = 0; i < rw->properties.count; i++) {
            objc_property_t p = property_list_find((property_list_t *)rw->properties.items[i], name);
            if (p) return p;
        }
    }
    return NULL;
}

objc_property_t *class_copyPropertyList(Class cls, unsigned int *outCount) {
    if (!cls) { if (outCount) *outCount = 0; return NULL; }
    class_realize(cls);
    class_rw_t *rw = cls_rw(cls);
    unsigned n = 0;
    for (uint32_t i = 0; i < rw->properties.count; i++) n += ((property_list_t *)rw->properties.items[i])->count;
    if (!n) { if (outCount) *outCount = 0; return NULL; }
    objc_property_t *out = malloc((n + 1) * sizeof(objc_property_t));
    unsigned k = 0;
    for (uint32_t i = 0; i < rw->properties.count; i++) {
        property_list_t *l = rw->properties.items[i];
        for (uint32_t j = 0; j < l->count; j++) out[k++] = (objc_property_t)&l->first[j];
    }
    out[n] = NULL;
    if (outCount) *outCount = n;
    return out;
}

const uint8_t *class_getIvarLayout(Class cls) { return cls ? cls_ro(cls)->ivarLayout : NULL; }
const uint8_t *class_getWeakIvarLayout(Class cls) { return cls ? cls_ro(cls)->weakIvarLayout : NULL; }

/* ---- modifying classes ---- */

BOOL class_addMethod(Class cls, SEL name, IMP imp, const char *types) {
    if (!cls || !name || !imp) return NO;
    class_realize(cls);
    if (class_find_method_here(cls, name)) return NO;
    method_list_t *l = method_list_alloc(1);
    l->first[0].name = name;
    l->first[0].types = types ? strdup(types) : "";
    l->first[0].imp = imp;
    vec_insert_front(&cls_rw(cls)->methods, l);
    class_update_custom_rr(cls, l);
    class_flush_caches(cls);
    return YES;
}

IMP class_replaceMethod(Class cls, SEL name, IMP imp, const char *types) {
    if (!cls || !name || !imp) return NULL;
    class_realize(cls);
    method_t *m = class_find_method_here(cls, name);
    if (m) {
        IMP old = m->imp;
        m->imp = imp;
        class_flush_caches(cls);
        return old;
    }
    class_addMethod(cls, name, imp, types);
    return NULL;
}

BOOL class_addIvar(Class cls, const char *name, size_t size, uint8_t alignment, const char *types) {
    if (!cls || !name) return NO;
    class_realize(cls);
    class_rw_t *rw = cls_rw(cls);
    if (!(rw->flags & RW_CONSTRUCTED) || cls_is_meta(cls)) return NO;
    if (class_getInstanceVariable(cls, name)) return NO;
    class_ro_t *ro = (class_ro_t *)rw->ro;   /* owned by the runtime for constructed classes */
    uint32_t count = ro->ivars ? ro->ivars->count : 0;
    ivar_list_t *list = calloc(1, sizeof(ivar_list_t) + (count + 1) * sizeof(ivar_t));
    list->entsizeAndFlags = sizeof(ivar_t);
    list->count = count + 1;
    if (ro->ivars) memcpy(list->first, ro->ivars->first, count * sizeof(ivar_t));
    uint32_t align = alignment ? 1u << alignment : sizeof(void *);
    uint32_t offset = align_up(ro->instanceSize, align);
    int32_t *offsetSlot = malloc(sizeof(int32_t));
    *offsetSlot = (int32_t)offset;
    ivar_t *iv = &list->first[count];
    iv->offset = offsetSlot;
    iv->name = strdup(name);
    iv->type = types ? strdup(types) : "";
    iv->alignment_raw = alignment;
    iv->size = (uint32_t)size;
    ro->ivars = list;
    ro->instanceSize = offset + (uint32_t)size;
    return YES;
}

BOOL class_addProtocol(Class cls, Protocol *protocol) {
    if (!cls || !protocol) return NO;
    class_realize(cls);
    if (class_conformsToProtocol(cls, protocol)) return NO;
    protocol_list_t *l = calloc(1, sizeof(protocol_list_t) + 2 * sizeof(protocol_t *));
    l->count = 1;
    l->list[0] = protocol_canonical((protocol_t *)protocol);
    vec_push(&cls_rw(cls)->protocols, l);
    return YES;
}

static char *property_attributes_string(const objc_property_attribute_t *attrs, unsigned count) {
    size_t len = 1;
    for (unsigned i = 0; i < count; i++) len += strlen(attrs[i].name) + strlen(attrs[i].value) + 1;
    char *s = malloc(len), *p = s;
    for (unsigned i = 0; i < count; i++) {
        if (i) *p++ = ',';
        size_t n = strlen(attrs[i].name); memcpy(p, attrs[i].name, n); p += n;
        n = strlen(attrs[i].value); memcpy(p, attrs[i].value, n); p += n;
    }
    *p = 0;
    return s;
}

static BOOL add_property(Class cls, const char *name, const objc_property_attribute_t *attrs, unsigned count, bool replace) {
    if (!cls || !name) return NO;
    class_realize(cls);
    class_rw_t *rw = cls_rw(cls);
    for (uint32_t i = 0; i < rw->properties.count; i++) {
        property_t *p = (property_t *)property_list_find((property_list_t *)rw->properties.items[i], name);
        if (p) {
            if (!replace) return NO;
            p->attributes = property_attributes_string(attrs, count);
            return YES;
        }
    }
    property_list_t *l = calloc(1, sizeof(property_list_t) + sizeof(property_t));
    l->entsizeAndFlags = sizeof(property_t);
    l->count = 1;
    l->first[0].name = strdup(name);
    l->first[0].attributes = property_attributes_string(attrs, count);
    vec_push(&rw->properties, l);
    return YES;
}

BOOL class_addProperty(Class cls, const char *name, const objc_property_attribute_t *attributes, unsigned int attributeCount) {
    return add_property(cls, name, attributes, attributeCount, false);
}

void class_replaceProperty(Class cls, const char *name, const objc_property_attribute_t *attributes, unsigned int attributeCount) {
    add_property(cls, name, attributes, attributeCount, true);
}

void class_setIvarLayout(Class cls, const uint8_t *layout) {
    if (!cls) return;
    class_realize(cls);
    if (cls_rw(cls)->flags & RW_RO_OWNED) ((class_ro_t *)cls_rw(cls)->ro)->ivarLayout = layout;
}

void class_setWeakIvarLayout(Class cls, const uint8_t *layout) {
    if (!cls) return;
    class_realize(cls);
    if (cls_rw(cls)->flags & RW_RO_OWNED) ((class_ro_t *)cls_rw(cls)->ro)->weakIvarLayout = layout;
}

/* ---- dynamic class creation ---- */

static Class alloc_class_object(Class super, size_t extraBytes, bool meta) {
    if (super && cls_is_swift(super) && !meta) {
        const struct swift_class_t *swsuper = (const struct swift_class_t *)super;
        size_t superSize = swsuper->classSize;
        size_t addressPoint = swsuper->classAddressPoint;
        if (superSize && superSize >= addressPoint + sizeof(struct swift_class_t)) {
            char *base = malloc(superSize + extraBytes);
            memcpy(base, (const char *)super - addressPoint, superSize);
            memset(base + superSize, 0, extraBytes);
            struct swift_class_t *sub = (struct swift_class_t *)(base + addressPoint);
            memset(&sub->objc, 0, sizeof(sub->objc));
            sub->classSize = (uint32_t)(superSize + extraBytes);
            return (Class)sub;
        }
    }
    Class cls = calloc(1, sizeof(struct objc_class) + extraBytes);
    return cls;
}

Class objc_allocateClassPair(Class super, const char *name, size_t extraBytes) {
    if (!name || map_find(&named_classes, name)) return Nil;
    if (super) {
        class_realize(super);
        if (cls_is_meta(super)) return Nil;
    }
    Class cls = alloc_class_object(super, extraBytes, false);
    Class meta = alloc_class_object(super ? super->isa : Nil, extraBytes, true);
    if (!cls || !meta) return Nil;

    class_ro_t *ro = calloc(1, sizeof(class_ro_t));
    class_ro_t *mro = calloc(1, sizeof(class_ro_t));
    char *dupname = strdup(name);
    ro->flags = super ? 0 : RO_ROOT;
    ro->instanceStart = super ? cls_ro(super)->instanceSize : sizeof(Class);
    ro->instanceSize = ro->instanceStart;
    ro->name = dupname;
    mro->flags = RO_META | (super ? 0 : RO_ROOT);
    mro->instanceStart = mro->instanceSize = sizeof(struct objc_class);
    mro->name = dupname;

    cls->isa = meta;
    cls->superclass = super;
    cls->cache = _objc_empty_cache;
    cls->bits = (uintptr_t)ro | (super ? (super->bits & FAST_IS_SWIFT_MASK) : 0);
    if (super) {
        meta->isa = super->isa->isa;
        meta->superclass = super->isa;
    } else {
        meta->isa = meta;
        meta->superclass = cls;
    }
    meta->cache = _objc_empty_cache;
    meta->bits = (uintptr_t)mro;

    realize_impl(cls);
    cls_rw(cls)->flags |= RW_CONSTRUCTED | RW_RO_OWNED;
    cls_rw(meta)->flags |= RW_CONSTRUCTED | RW_RO_OWNED;
    if (super && cls_is_swift(super) && copy_fixup_handler) copy_fixup_handler(super, cls);
    return cls;
}

void objc_registerClassPair(Class cls) {
    if (!cls) return;
    class_realize(cls);
    class_rw_t *rw = cls_rw(cls);
    rw->flags = (rw->flags & ~RW_CONSTRUCTED) | RW_REGISTERED;
    cls_rw(cls->isa)->flags = (cls_rw(cls->isa)->flags & ~RW_CONSTRUCTED) | RW_REGISTERED;
    class_register_name(cls);
    class_track(cls);
}

Class objc_duplicateClass(Class original, const char *name, size_t extraBytes) {
    (void)original; (void)name; (void)extraBytes;
    _objc_fatal("libobjc-wasm: objc_duplicateClass is not supported");
}

void objc_disposeClassPair(Class cls) {
    if (!cls) return;
    class_rw_t *rw = cls_rw(cls);
    if (rw->flags & RW_REGISTERED) {
        map_remove(&named_classes, cls_ro(cls)->name);
        vec_remove(&all_classes, cls);
        map_remove(&known_classes, cls);
    }
    /* The class object itself may still be referenced; leak conservatively. */
}

/* ---- instances ---- */

id object_cxx_construct(id obj, Class cls) {
    if (!cls) return obj;
    if (cls->superclass && (cls_rw(cls->superclass)->flags & RW_HAS_CXX_CTOR)) {
        if (!object_cxx_construct(obj, cls->superclass)) return nil;
    }
    const class_ro_t *ro = cls_ro(cls);
    if (ro->flags & RO_HAS_CXX_STRUCTORS && !(ro->flags & RO_HAS_CXX_DTOR_ONLY)) {
        method_t *m = ro->baseMethodList ? method_list_find(ro->baseMethodList, SELS.cxx_construct) : NULL;
        if (m) return ((id (*)(id, SEL))m->imp)(obj, SELS.cxx_construct);
    }
    return obj;
}

void object_cxx_destruct(id obj, Class cls) {
    for (Class c = cls; c; c = c->superclass) {
        if (!cls_is_realized(c)) continue;
        const class_ro_t *ro = cls_ro(c);
        if (!(ro->flags & RO_HAS_CXX_STRUCTORS)) continue;
        method_t *m = ro->baseMethodList ? method_list_find(ro->baseMethodList, SELS.cxx_destruct) : NULL;
        if (m) ((void (*)(id, SEL))m->imp)(obj, SELS.cxx_destruct);
    }
}

id class_createInstance(Class cls, size_t extraBytes) {
    if (!cls) return nil;
    class_realize(cls);
    size_t size = class_getInstanceSize(cls) + extraBytes;
    if (size < sizeof(struct objc_object)) size = sizeof(struct objc_object);
    size_t aligned = (size + 15) & ~(size_t)15;
    void *mem = aligned_alloc(16, aligned);
    if (!mem) return nil;
    memset(mem, 0, aligned);
    id obj = (id)mem;
    obj->isa = cls;
    if (cls_rw(cls)->flags & RW_HAS_CXX_CTOR) {
        if (!object_cxx_construct(obj, cls)) { free(mem); return nil; }
    }
    return obj;
}

id objc_constructInstance(Class cls, void *bytes) {
    if (!cls || !bytes) return nil;
    class_realize(cls);
    id obj = (id)bytes;
    obj->isa = cls;
    if (cls_rw(cls)->flags & RW_HAS_CXX_CTOR) return object_cxx_construct(obj, cls);
    return obj;
}

void *objc_destructInstance(id obj) {
    if (!obj) return NULL;
    Class cls = obj->isa;
    if (cls && !cls_is_stub(cls) && cls_is_realized(cls) && (cls_rw(cls)->flags & RW_HAS_CXX_DTOR))
        object_cxx_destruct(obj, cls);
    assoc_remove_all(obj);
    weak_clear(obj);
    refcount_forget(obj);
    return obj;
}

id object_dispose(id obj) {
    if (!obj) return nil;
    objc_destructInstance(obj);
    free(obj);
    return nil;
}

id object_copy(id obj, size_t size) {
    if (!obj) return nil;
    Class cls = obj->isa;
    size_t instSize = class_getInstanceSize(cls);
    if (size < instSize) size = instSize;
    size_t aligned = (size + 15) & ~(size_t)15;
    void *mem = aligned_alloc(16, aligned);
    if (!mem) return nil;
    memset(mem, 0, aligned);
    memcpy(mem, obj, size);
    return (id)mem;
}

Class object_getClass(id obj) { return obj ? obj->isa : Nil; }

Class object_setClass(id obj, Class cls) {
    if (!obj) return Nil;
    class_realize(cls);
    Class old = obj->isa;
    obj->isa = cls;
    return old;
}

BOOL object_isClass(id obj) { return obj && !cls_is_stub(obj->isa) && cls_is_meta(obj->isa); }
const char *object_getClassName(id obj) { return cls_name(object_getClass(obj)); }

void *object_getIndexedIvars(id obj) {
    if (!obj) return NULL;
    return (char *)obj + class_getInstanceSize(obj->isa);
}

id object_getIvar(id obj, Ivar ivar) {
    if (!obj || !ivar) return nil;
    return *(id *)((char *)obj + ivar_getOffset(ivar));
}

void object_setIvar(id obj, Ivar ivar, id value) {
    if (!obj || !ivar) return;
    *(id *)((char *)obj + ivar_getOffset(ivar)) = value;
}

void object_setIvarWithStrongDefault(id obj, Ivar ivar, id value) {
    if (!obj || !ivar) return;
    objc_storeStrong((id *)((char *)obj + ivar_getOffset(ivar)), value);
}

Ivar object_setInstanceVariable(id obj, const char *name, void *value) {
    if (!obj || !name) return NULL;
    Ivar iv = class_getInstanceVariable(obj->isa, name);
    if (iv) *(void **)((char *)obj + ivar_getOffset(iv)) = value;
    return iv;
}

Ivar object_setInstanceVariableWithStrongDefault(id obj, const char *name, void *value) {
    if (!obj || !name) return NULL;
    Ivar iv = class_getInstanceVariable(obj->isa, name);
    if (iv) objc_storeStrong((id *)((char *)obj + ivar_getOffset(iv)), (id)value);
    return iv;
}

Ivar object_getInstanceVariable(id obj, const char *name, void **outValue) {
    if (!obj || !name) return NULL;
    Ivar iv = class_getInstanceVariable(obj->isa, name);
    if (iv && outValue) *outValue = *(void **)((char *)obj + ivar_getOffset(iv));
    return iv;
}

/* ---- methods, ivars, properties ---- */

SEL method_getName(Method m) { return ((method_t *)m)->name; }
IMP method_getImplementation(Method m) { return ((method_t *)m)->imp; }
const char *method_getTypeEncoding(Method m) { return ((method_t *)m)->types; }
struct objc_method_description *method_getDescription(Method m) { return (struct objc_method_description *)m; }

IMP method_setImplementation(Method m, IMP imp) {
    IMP old = ((method_t *)m)->imp;
    ((method_t *)m)->imp = imp;
    flush_all_caches();
    return old;
}

void method_exchangeImplementations(Method m1, Method m2) {
    if (!m1 || !m2) return;
    IMP t = ((method_t *)m1)->imp;
    ((method_t *)m1)->imp = ((method_t *)m2)->imp;
    ((method_t *)m2)->imp = t;
    flush_all_caches();
}

unsigned int method_getNumberOfArguments(Method m) { return enc_arg_count(((method_t *)m)->types); }
char *method_copyReturnType(Method m) { return enc_copy_nth(((method_t *)m)->types, 0); }
char *method_copyArgumentType(Method m, unsigned int index) { return enc_copy_nth(((method_t *)m)->types, index + 1); }
void method_getReturnType(Method m, char *dst, size_t dst_len) { enc_get_nth(((method_t *)m)->types, 0, dst, dst_len); }
void method_getArgumentType(Method m, unsigned int index, char *dst, size_t dst_len) { enc_get_nth(((method_t *)m)->types, index + 1, dst, dst_len); }

const char *ivar_getName(Ivar v) { return v ? ((ivar_t *)v)->name : NULL; }
const char *ivar_getTypeEncoding(Ivar v) { return v ? ((ivar_t *)v)->type : NULL; }
ptrdiff_t ivar_getOffset(Ivar v) { return (v && ((ivar_t *)v)->offset) ? *((ivar_t *)v)->offset : 0; }

const char *property_getName(objc_property_t p) { return ((property_t *)p)->name; }
const char *property_getAttributes(objc_property_t p) { return ((property_t *)p)->attributes; }

objc_property_attribute_t *property_copyAttributeList(objc_property_t property, unsigned int *outCount) {
    const char *attrs = property_getAttributes(property);
    if (!attrs || !*attrs) { if (outCount) *outCount = 0; return NULL; }
    unsigned n = 1;
    for (const char *p = attrs; *p; p++) if (*p == ',') n++;
    size_t strsize = strlen(attrs) + 2 * n + 1;
    char *mem = malloc((n + 1) * sizeof(objc_property_attribute_t) + strsize);
    objc_property_attribute_t *out = (objc_property_attribute_t *)mem;
    char *s = mem + (n + 1) * sizeof(objc_property_attribute_t);
    unsigned k = 0;
    const char *p = attrs;
    while (*p) {
        const char *end = p;
        if (*p == 'T' || *p == 'V') {
            /* type and ivar values may contain commas inside quotes/braces; take until a top-level comma */
            int depth = 0; bool quoted = false;
            for (end = p; *end; end++) {
                if (*end == '"') quoted = !quoted;
                else if (!quoted && (*end == '{' || *end == '(' || *end == '[')) depth++;
                else if (!quoted && (*end == '}' || *end == ')' || *end == ']')) depth--;
                else if (!quoted && depth == 0 && *end == ',') break;
            }
        } else {
            while (*end && *end != ',') end++;
        }
        out[k].name = s; *s++ = *p; *s++ = 0;
        out[k].value = s;
        size_t vlen = (size_t)(end - p - 1);
        memcpy(s, p + 1, vlen); s += vlen; *s++ = 0;
        k++;
        p = (*end == ',') ? end + 1 : end;
    }
    out[k].name = NULL; out[k].value = NULL;
    if (outCount) *outCount = k;
    return out;
}

char *property_copyAttributeValue(objc_property_t property, const char *attributeName) {
    unsigned n = 0;
    objc_property_attribute_t *list = property_copyAttributeList(property, &n);
    char *result = NULL;
    for (unsigned i = 0; i < n; i++) {
        if (strcmp(list[i].name, attributeName) == 0) { result = strdup(list[i].value); break; }
    }
    free(list);
    return result;
}

/* ---- images ---- */

static const char *image_name = "main";

const char **objc_copyImageNames(unsigned int *outCount) {
    const char **list = malloc(2 * sizeof(char *));
    list[0] = image_name; list[1] = NULL;
    if (outCount) *outCount = 1;
    return list;
}

const char *class_getImageName(Class cls) {
    if (!cls) return NULL;
    const char *n = objc_hook_image_name(cls);
    return n ? n : image_name;
}

const char **objc_copyClassNamesForImage(const char *image, unsigned int *outCount) {
    (void)image;
    unsigned n = all_classes.count;
    const char **list = malloc((n + 1) * sizeof(char *));
    for (unsigned i = 0; i < n; i++) list[i] = cls_name((Class)all_classes.items[i]);
    list[n] = NULL;
    if (outCount) *outCount = n;
    return list;
}
