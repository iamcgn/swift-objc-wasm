/* Core runtime test with hand-built objc4 metadata (no Objective-C compiler needed). */
#include <objc/runtime.h>
#include <objc/message.h>
#include <objc/objc-internal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/abi.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define SEL_(s) sel_registerName(s)
#define SEND0(ret, obj, s) ((ret (*)(id, SEL))objc_msgLookup((id)(obj), SEL_(s)))((id)(obj), SEL_(s))
#define SEND1(ret, obj, s, T, a) ((ret (*)(id, SEL, T))objc_msgLookup((id)(obj), SEL_(s)))((id)(obj), SEL_(s), a)

extern uintptr_t _objc_empty_cache[];
extern struct objc_class TRoot_class, TRoot_meta, TSub_class, TSub_meta;

/* ---- TRoot ---- */
struct TRoot_ivars { Class isa; int value; };
static int TRoot_initialize_count, TRoot_load_count, TRoot_dealloc_count, Cat_load_count;

static void TRoot_initialize(id self, SEL _cmd) { (void)self; (void)_cmd; TRoot_initialize_count++; }
static void TRoot_load(id self, SEL _cmd) { (void)self; (void)_cmd; TRoot_load_count++; }
static id TRoot_alloc(id self, SEL _cmd) { (void)_cmd; return class_createInstance((Class)self, 0); }
static id TRoot_init(id self, SEL _cmd) { (void)_cmd; ((struct TRoot_ivars *)self)->value = 41; return self; }
static int TRoot_value(id self, SEL _cmd) { (void)_cmd; return ((struct TRoot_ivars *)self)->value; }
static void TRoot_setValue(id self, SEL _cmd, int v) { (void)_cmd; ((struct TRoot_ivars *)self)->value = v; }
static void TRoot_dealloc(id self, SEL _cmd) { (void)_cmd; TRoot_dealloc_count++; _objc_rootDealloc(self); }
static const char *TRoot_describe(id self, SEL _cmd) { (void)self; (void)_cmd; return "root"; }
static int resolved_imp(id self, SEL _cmd) { (void)self; (void)_cmd; return 99; }
static BOOL TRoot_resolveInstanceMethod(id self, SEL _cmd, SEL sel) {
    (void)_cmd;
    if (strcmp(sel_getName(sel), "resolved") == 0) { class_addMethod((Class)self, sel, (IMP)resolved_imp, "i16@0:8"); return YES; }
    return NO;
}

static struct { uint32_t e, c; method_t m[5]; } TRoot_imethods = { 12, 5, {
    { (SEL)"init", "@16@0:8", (IMP)TRoot_init },
    { (SEL)"value", "i16@0:8", (IMP)TRoot_value },
    { (SEL)"setValue:", "v20@0:8i16", (IMP)TRoot_setValue },
    { (SEL)"dealloc", "v16@0:8", (IMP)TRoot_dealloc },
    { (SEL)"describe", "*16@0:8", (IMP)TRoot_describe },
}};
static struct { uint32_t e, c; method_t m[4]; } TRoot_cmethods = { 12, 4, {
    { (SEL)"initialize", "v16@0:8", (IMP)TRoot_initialize },
    { (SEL)"load", "v16@0:8", (IMP)TRoot_load },
    { (SEL)"alloc", "@16@0:8", (IMP)TRoot_alloc },
    { (SEL)"resolveInstanceMethod:", "c24@0:8:16", (IMP)TRoot_resolveInstanceMethod },
}};
static int32_t TRoot_value_offset = 4;
static struct { uint32_t e, c; ivar_t v[1]; } TRoot_ivars_list = { 20, 1, { { &TRoot_value_offset, "value", "i", 2, 4 } } };
static struct { uint32_t e, c; property_t p[1]; } TRoot_props = { 8, 1, { { "value", "Ti,N,V_value" } } };
static class_ro_t TRoot_ro = { RO_ROOT, 0, 8, NULL, "TRoot", (method_list_t *)&TRoot_imethods, NULL, (ivar_list_t *)&TRoot_ivars_list, NULL, (property_list_t *)&TRoot_props };
static class_ro_t TRoot_meta_ro = { RO_META | RO_ROOT, 20, 20, NULL, "TRoot", (method_list_t *)&TRoot_cmethods, NULL, NULL, NULL, NULL };
struct objc_class TRoot_meta = { &TRoot_meta, &TRoot_class, _objc_empty_cache, NULL, (uintptr_t)&TRoot_meta_ro };
struct objc_class TRoot_class = { &TRoot_meta, NULL, _objc_empty_cache, NULL, (uintptr_t)&TRoot_ro };

/* ---- TSub : TRoot <TProto>, with a double ivar whose static offset is stale ---- */
struct TSub_ivars { struct TRoot_ivars base; double d; };
static int32_t TSub_d_offset = 4;   /* compiled against a smaller TRoot: must slide to 8 */
static const char *TSub_describe(id self, SEL _cmd) { (void)self; (void)_cmd; return "sub"; }
static const char *TSub_superDescribe(id self, SEL _cmd) {
    (void)_cmd;
    struct objc_super s = { self, &TSub_class };
    return ((const char *(*)(struct objc_super *, SEL))objc_msgLookupSuper2(&s, SEL_("describe")))(&s, SEL_("describe"));
}
static double TSub_d(id self, SEL _cmd) { (void)_cmd; return *(double *)((char *)self + TSub_d_offset); }
static void TSub_setD(id self, SEL _cmd, double v) { (void)_cmd; *(double *)((char *)self + TSub_d_offset) = v; }

static struct { uint32_t e, c; method_t m[4]; } TSub_imethods = { 12, 4, {
    { (SEL)"describe", "*16@0:8", (IMP)TSub_describe },
    { (SEL)"superDescribe", "*16@0:8", (IMP)TSub_superDescribe },
    { (SEL)"d", "d16@0:8", (IMP)TSub_d },
    { (SEL)"setD:", "v24@0:8d16", (IMP)TSub_setD },
}};
static struct { uint32_t e, c; ivar_t v[1]; } TSub_ivars_list = { 20, 1, { { &TSub_d_offset, "d", "d", 3, 8 } } };
static struct { uint32_t e, c; method_t m[1]; } TProto_methods = { 12, 1, { { (SEL)"describe", "*16@0:8", NULL } } };
static protocol_t TProto = { NULL, "TProto", NULL, (method_list_t *)&TProto_methods, NULL, NULL, NULL, NULL, 52, 0, NULL, NULL, NULL };
static struct { uintptr_t c; protocol_t *l[2]; } TSub_protocols = { 1, { &TProto, NULL } };
static class_ro_t TSub_ro = { 0, 4, 12, NULL, "TSub", (method_list_t *)&TSub_imethods, (protocol_list_t *)&TSub_protocols, (ivar_list_t *)&TSub_ivars_list, NULL, NULL };
static class_ro_t TSub_meta_ro = { RO_META, 20, 20, NULL, "TSub", NULL, NULL, NULL, NULL, NULL };
struct objc_class TSub_meta = { &TRoot_meta, &TRoot_meta, _objc_empty_cache, NULL, (uintptr_t)&TSub_meta_ro };
struct objc_class TSub_class = { &TSub_meta, &TRoot_class, _objc_empty_cache, NULL, (uintptr_t)&TSub_ro };

/* ---- category TSub (Cat) ---- */
static int Cat_catValue(id self, SEL _cmd) { (void)self; (void)_cmd; return 7; }
static int Cat_classValue(id self, SEL _cmd) { (void)self; (void)_cmd; return 8; }
static void Cat_load(id self, SEL _cmd) { (void)self; (void)_cmd; Cat_load_count++; }
static struct { uint32_t e, c; method_t m[1]; } Cat_imethods = { 12, 1, { { (SEL)"catValue", "i16@0:8", (IMP)Cat_catValue } } };
static struct { uint32_t e, c; method_t m[2]; } Cat_cmethods = { 12, 2, { { (SEL)"classValue", "i16@0:8", (IMP)Cat_classValue }, { (SEL)"load", "v16@0:8", (IMP)Cat_load } } };
static category_t TSub_cat = { "Cat", &TSub_class, (method_list_t *)&Cat_imethods, (method_list_t *)&Cat_cmethods, NULL, NULL, NULL, 32 };

/* ---- metadata sections ---- */
__attribute__((used, section("objc_classlist"))) static void *classlist[] = { &TRoot_class, &TSub_class };
__attribute__((used, section("objc_nlclslist"))) static void *nlclslist[] = { &TRoot_class };
__attribute__((used, section("objc_catlist"))) static void *catlist[] = { &TSub_cat };
__attribute__((used, section("objc_nlcatlist"))) static void *nlcatlist[] = { &TSub_cat };
__attribute__((used, section("objc_protolist"))) static void *protolist[] = { &TProto };
__attribute__((used, section("objc_selrefs"))) static SEL selref_describe = (SEL)"describe";

static int added_imp(id self, SEL _cmd) { (void)self; (void)_cmd; return 123; }
static int extra_get(id self, SEL _cmd) { (void)_cmd; return *(int *)((char *)self + 16); }

int main(void) {
    CHECK(TRoot_load_count == 1);
    CHECK(Cat_load_count == 1);

    Class root = objc_getClass("TRoot"), sub = objc_getClass("TSub");
    CHECK(root == &TRoot_class && sub == &TSub_class);
    CHECK(class_getSuperclass(sub) == root);
    CHECK(class_getSuperclass(root) == Nil);
    CHECK(objc_getMetaClass("TSub") == &TSub_meta);
    CHECK(class_isMetaClass(&TSub_meta) && !class_isMetaClass(sub));
    CHECK(strcmp(class_getName(sub), "TSub") == 0);

    /* selectors */
    CHECK(selref_describe == SEL_("describe"));
    CHECK(sel_isEqual(SEL_("value"), SEL_("value")));
    CHECK(strcmp(sel_getName(SEL_("setD:")), "setD:") == 0);

    /* layout: ivar slide */
    CHECK(class_getInstanceSize(root) == 8);
    CHECK(class_getInstanceSize(sub) == 16);
    CHECK(TSub_d_offset == 8);
    CHECK(ivar_getOffset(class_getInstanceVariable(sub, "d")) == 8);
    CHECK(ivar_getOffset(class_getInstanceVariable(sub, "value")) == 4);

    /* +initialize and messaging */
    CHECK(TRoot_initialize_count == 0);
    id obj = SEND0(id, sub, "alloc");
    CHECK(TRoot_initialize_count == 2);   /* TRoot, then inherited for TSub */
    CHECK(obj && object_getClass(obj) == sub);
    obj = SEND0(id, obj, "init");
    CHECK(SEND0(int, obj, "value") == 41);
    SEND1(void, obj, "setValue:", int, 5);
    CHECK(SEND0(int, obj, "value") == 5);
    SEND1(void, obj, "setD:", double, 2.5);
    CHECK(SEND0(double, obj, "d") == 2.5);
    CHECK(((struct TSub_ivars *)obj)->d == 2.5);
    CHECK(strcmp(SEND0(const char *, obj, "describe"), "sub") == 0);
    CHECK(strcmp(SEND0(const char *, obj, "superDescribe"), "root") == 0);
    CHECK(SEND0(int, obj, "catValue") == 7);
    CHECK(SEND0(int, sub, "classValue") == 8);
    CHECK(objc_msgLookup(nil, SEL_("value")) != NULL);

    /* responds / resolve / add */
    CHECK(class_respondsToSelector(sub, SEL_("value")));
    CHECK(!class_respondsToSelector(sub, SEL_("nope")));
    CHECK(class_respondsToSelector(root, SEL_("resolved")));
    CHECK(SEND0(int, obj, "resolved") == 99);
    CHECK(class_addMethod(sub, SEL_("added"), (IMP)added_imp, "i16@0:8"));
    CHECK(!class_addMethod(sub, SEL_("added"), (IMP)added_imp, "i16@0:8"));
    CHECK(SEND0(int, obj, "added") == 123);
    CHECK(class_getInstanceMethod(sub, SEL_("value")) == class_getInstanceMethod(root, SEL_("value")));
    CHECK(class_getClassMethod(sub, SEL_("alloc")) != NULL);

    /* protocols */
    Protocol *proto = objc_getProtocol("TProto");
    CHECK(proto == (Protocol *)&TProto);
    CHECK(class_conformsToProtocol(sub, proto));
    CHECK(!class_conformsToProtocol(root, proto));
    CHECK(protocol_getMethodDescription(proto, SEL_("describe"), YES, YES).name == SEL_("describe"));
    CHECK(strcmp(protocol_getName(proto), "TProto") == 0);

    /* method / property introspection */
    Method m = class_getInstanceMethod(sub, SEL_("setD:"));
    CHECK(method_getNumberOfArguments(m) == 3);
    char *rt = method_copyReturnType(m); CHECK(strcmp(rt, "v") == 0); free(rt);
    char *at = method_copyArgumentType(m, 2); CHECK(strcmp(at, "d") == 0); free(at);
    unsigned n = 0; Method *ms = class_copyMethodList(sub, &n); CHECK(n == 6); free(ms);
    objc_property_t prop = class_getProperty(sub, "value");
    CHECK(prop && strcmp(property_getName(prop), "value") == 0);
    objc_property_attribute_t *attrs = property_copyAttributeList(prop, &n);
    CHECK(n == 3 && strcmp(attrs[0].name, "T") == 0 && strcmp(attrs[0].value, "i") == 0 && strcmp(attrs[2].value, "_value") == 0);
    free(attrs);
    char *v = property_copyAttributeValue(prop, "V"); CHECK(strcmp(v, "_value") == 0); free(v);

    /* perform */
    CHECK(objc_performSelector(obj, SEL_("value"), 0, nil, nil) == (id)5);
    objc_performSelector(obj, SEL_("setValue:"), 1, (id)77, nil);
    CHECK(SEND0(int, obj, "value") == 77);
    CHECK(objc_performSelector(nil, SEL_("value"), 0, nil, nil) == nil);

    /* refcounting, weak, dealloc */
    CHECK(_objc_rootRetainCount(obj) == 1);
    CHECK(objc_retain(obj) == obj);
    CHECK(_objc_rootRetainCount(obj) == 2);
    objc_release(obj);
    CHECK(_objc_rootRetainCount(obj) == 1);
    id weak = nil;
    objc_storeWeak(&weak, obj);
    CHECK(weak == obj);
    id loaded = objc_loadWeakRetained(&weak);
    CHECK(loaded == obj && _objc_rootRetainCount(obj) == 2);
    objc_release(loaded);
    CHECK(TRoot_dealloc_count == 0);
    objc_release(obj);
    CHECK(TRoot_dealloc_count == 1);
    CHECK(weak == nil);

    /* associated objects */
    id a = SEND0(id, root, "alloc"), b = SEND0(id, root, "alloc");
    static char key;
    objc_setAssociatedObject(a, &key, b, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    CHECK(_objc_rootRetainCount(b) == 2);
    CHECK(objc_getAssociatedObject(a, &key) == b);
    objc_setAssociatedObject(a, &key, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    CHECK(_objc_rootRetainCount(b) == 1);
    objc_setAssociatedObject(a, &key, b, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    objc_release(a);
    CHECK(TRoot_dealloc_count == 2 && _objc_rootRetainCount(b) == 1);

    /* autorelease pools */
    void *pool = objc_autoreleasePoolPush();
    objc_autorelease(objc_retain(b));
    CHECK(_objc_rootRetainCount(b) == 2);
    void *inner = objc_autoreleasePoolPush();
    objc_autorelease(objc_retain(b));
    (void)inner;
    objc_autoreleasePoolPop(pool);
    CHECK(_objc_rootRetainCount(b) == 1);
    objc_release(b);
    CHECK(TRoot_dealloc_count == 3);

    /* dynamic classes */
    Class dyn = objc_allocateClassPair(sub, "TDyn", 0);
    CHECK(dyn != Nil);
    CHECK(class_addIvar(dyn, "extra", 4, 2, "i"));
    CHECK(class_addMethod(dyn, SEL_("extra"), (IMP)extra_get, "i16@0:8"));
    objc_registerClassPair(dyn);
    CHECK(objc_getClass("TDyn") == dyn);
    CHECK(class_getInstanceSize(dyn) == 20);
    CHECK(ivar_getOffset(class_getInstanceVariable(dyn, "extra")) == 16);
    id d = SEND0(id, SEND0(id, dyn, "alloc"), "init");
    *(int *)((char *)d + 16) = 31;
    CHECK(SEND0(int, d, "extra") == 31);
    CHECK(SEND0(int, d, "value") == 41);
    CHECK(strcmp(SEND0(const char *, d, "describe"), "sub") == 0);
    CHECK(class_getSuperclass(object_getClass(d)) == sub);
    objc_release(d);

    CHECK(objc_getClassList(NULL, 0) == 3);
    CHECK(objc_getRequiredClass("TRoot") == root);
    CHECK(objc_lookUpClass("Missing") == Nil);

    if (failures) { printf("%d failures\n", failures); return 1; }
    printf("test_core: ok\n");
    return 0;
}
