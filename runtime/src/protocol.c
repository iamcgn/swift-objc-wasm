#include "internal.h"
OBJC_ANCHOR_INIT();

objc_map named_protocols;
static ptr_vec all_protocols;
#define PROTOCOL_UNDER_CONSTRUCTION (1u << 28)

void protocol_table_init(void) { map_init(&named_protocols, true); }

static Class protocol_class(void) {
    static Class cls;
    if (!cls) cls = map_get(&named_classes, "Protocol");
    return cls;
}

static const char *proto_name(const protocol_t *p) {
    if (PROTOCOL_HAS_DEMANGLED_NAME(p) && p->demangledName) return p->demangledName;
    return p->mangledName;
}

void protocol_fixup(protocol_t *p) {
    if (!p) return;
    if (!(p->flags & PROTOCOL_FIXED_UP_1)) {
        method_list_fixup(p->instanceMethods);
        method_list_fixup(p->classMethods);
        method_list_fixup(p->optionalInstanceMethods);
        method_list_fixup(p->optionalClassMethods);
        p->flags |= PROTOCOL_FIXED_UP_1;
    }
    if (!p->isa) p->isa = protocol_class();
}

void protocol_register(protocol_t *p) {
    if (!p || !p->mangledName) return;
    protocol_fixup(p);
    if (!map_find(&named_protocols, p->mangledName)) {
        map_set(&named_protocols, p->mangledName, p);
        vec_push(&all_protocols, p);
    }
}

protocol_t *protocol_canonical(protocol_t *p) {
    if (!p) return NULL;
    protocol_t *c = map_get(&named_protocols, p->mangledName);
    if (c) return c;
    protocol_register(p);
    return p;
}

void protocol_list_canonicalize(protocol_list_t *list) {
    if (!list) return;
    for (uintptr_t i = 0; i < list->count; i++) list->list[i] = protocol_canonical(list->list[i]);
}

Protocol *objc_getProtocol(const char *name) {
    if (!name) return NULL;
    protocol_t *p = map_get(&named_protocols, name);
    if (p) protocol_fixup(p);
    return (Protocol *)p;
}

Protocol **objc_copyProtocolList(unsigned int *outCount) {
    unsigned n = all_protocols.count;
    Protocol **list = malloc((n + 1) * sizeof(Protocol *));
    for (unsigned i = 0; i < n; i++) list[i] = (Protocol *)all_protocols.items[i];
    list[n] = NULL;
    if (outCount) *outCount = n;
    return list;
}

const char *protocol_getName(Protocol *proto) { return proto ? proto_name((protocol_t *)proto) : "nil"; }

BOOL protocol_isEqual(Protocol *a, Protocol *b) {
    if (a == b) return YES;
    if (!a || !b) return NO;
    return strcmp(((protocol_t *)a)->mangledName, ((protocol_t *)b)->mangledName) == 0;
}

BOOL protocol_conformsToProtocol(Protocol *proto, Protocol *other) {
    if (!proto || !other) return NO;
    if (protocol_isEqual(proto, other)) return YES;
    protocol_t *p = (protocol_t *)proto;
    if (!p->protocols) return NO;
    for (uintptr_t i = 0; i < p->protocols->count; i++)
        if (protocol_conformsToProtocol((Protocol *)p->protocols->list[i], other)) return YES;
    return NO;
}

bool protocol_list_conforms(protocol_list_t *list, protocol_t *target) {
    if (!list) return false;
    for (uintptr_t i = 0; i < list->count; i++)
        if (protocol_conformsToProtocol((Protocol *)list->list[i], (Protocol *)target)) return true;
    return false;
}

static method_list_t **select_list(protocol_t *p, BOOL required, BOOL instance) {
    if (required) return instance ? &p->instanceMethods : &p->classMethods;
    return instance ? &p->optionalInstanceMethods : &p->optionalClassMethods;
}

static method_t *protocol_find_method(protocol_t *p, SEL sel, BOOL required, BOOL instance, bool recurse) {
    protocol_fixup(p);
    method_t *m = method_list_find(*select_list(p, required, instance), sel);
    if (m || !recurse || !p->protocols) return m;
    for (uintptr_t i = 0; i < p->protocols->count; i++) {
        m = protocol_find_method(p->protocols->list[i], sel, required, instance, true);
        if (m) return m;
    }
    return NULL;
}

struct objc_method_description protocol_getMethodDescription(Protocol *proto, SEL sel, BOOL required, BOOL instance) {
    struct objc_method_description d = { NULL, NULL };
    if (!proto || !sel) return d;
    method_t *m = protocol_find_method((protocol_t *)proto, sel, required, instance, true);
    if (m) { d.name = m->name; d.types = (char *)m->types; }
    return d;
}

struct objc_method_description *protocol_copyMethodDescriptionList(Protocol *proto, BOOL required, BOOL instance, unsigned int *outCount) {
    if (!proto) { if (outCount) *outCount = 0; return NULL; }
    protocol_t *p = (protocol_t *)proto;
    protocol_fixup(p);
    method_list_t *l = *select_list(p, required, instance);
    unsigned n = l ? l->count : 0;
    if (!n) { if (outCount) *outCount = 0; return NULL; }
    struct objc_method_description *out = malloc((n + 1) * sizeof *out);
    for (unsigned i = 0; i < n; i++) { out[i].name = l->first[i].name; out[i].types = (char *)l->first[i].types; }
    out[n].name = NULL; out[n].types = NULL;
    if (outCount) *outCount = n;
    return out;
}

static property_list_t *select_props(protocol_t *p, BOOL required, BOOL instance) {
    if (!required) return NULL;
    if (instance) return p->instanceProperties;
    return PROTOCOL_HAS_CLASS_PROPERTIES(p) ? p->classProperties : NULL;
}

objc_property_t protocol_getProperty(Protocol *proto, const char *name, BOOL required, BOOL instance) {
    if (!proto || !name) return NULL;
    protocol_t *p = (protocol_t *)proto;
    property_list_t *l = select_props(p, required, instance);
    if (l) for (uint32_t i = 0; i < l->count; i++)
        if (strcmp(l->first[i].name, name) == 0) return (objc_property_t)&l->first[i];
    if (p->protocols) for (uintptr_t i = 0; i < p->protocols->count; i++) {
        objc_property_t r = protocol_getProperty((Protocol *)p->protocols->list[i], name, required, instance);
        if (r) return r;
    }
    return NULL;
}

objc_property_t *protocol_copyPropertyList2(Protocol *proto, unsigned int *outCount, BOOL required, BOOL instance) {
    if (!proto) { if (outCount) *outCount = 0; return NULL; }
    property_list_t *l = select_props((protocol_t *)proto, required, instance);
    unsigned n = l ? l->count : 0;
    if (!n) { if (outCount) *outCount = 0; return NULL; }
    objc_property_t *out = malloc((n + 1) * sizeof(objc_property_t));
    for (unsigned i = 0; i < n; i++) out[i] = (objc_property_t)&l->first[i];
    out[n] = NULL;
    if (outCount) *outCount = n;
    return out;
}

objc_property_t *protocol_copyPropertyList(Protocol *proto, unsigned int *outCount) {
    return protocol_copyPropertyList2(proto, outCount, YES, YES);
}

Protocol **protocol_copyProtocolList(Protocol *proto, unsigned int *outCount) {
    if (!proto) { if (outCount) *outCount = 0; return NULL; }
    protocol_t *p = (protocol_t *)proto;
    unsigned n = p->protocols ? (unsigned)p->protocols->count : 0;
    if (!n) { if (outCount) *outCount = 0; return NULL; }
    Protocol **out = malloc((n + 1) * sizeof(Protocol *));
    for (unsigned i = 0; i < n; i++) out[i] = (Protocol *)protocol_canonical(p->protocols->list[i]);
    out[n] = NULL;
    if (outCount) *outCount = n;
    return out;
}

/* ---- dynamic protocols ---- */

Protocol *objc_allocateProtocol(const char *name) {
    if (!name || map_find(&named_protocols, name)) return NULL;
    protocol_t *p = calloc(1, sizeof *p);
    p->mangledName = strdup(name);
    p->size = sizeof(protocol_t);
    p->flags = PROTOCOL_UNDER_CONSTRUCTION | PROTOCOL_FIXED_UP_1;
    return (Protocol *)p;
}

void objc_registerProtocol(Protocol *proto) {
    if (!proto) return;
    protocol_t *p = (protocol_t *)proto;
    if (!(p->flags & PROTOCOL_UNDER_CONSTRUCTION)) return;
    p->flags &= ~PROTOCOL_UNDER_CONSTRUCTION;
    p->isa = protocol_class();
    if (!map_find(&named_protocols, p->mangledName)) {
        map_set(&named_protocols, p->mangledName, p);
        vec_push(&all_protocols, p);
    }
}

static method_list_t *method_list_append(method_list_t *l, SEL name, const char *types) {
    uint32_t n = l ? l->count : 0;
    method_list_t *nl = malloc(sizeof(method_list_t) + (n + 1) * sizeof(method_t));
    nl->entsizeAndFlags = sizeof(method_t) | METHOD_LIST_IS_FIXED_UP;
    nl->count = n + 1;
    if (l) memcpy(nl->first, l->first, n * sizeof(method_t));
    nl->first[n].name = name;
    nl->first[n].types = types ? strdup(types) : "";
    nl->first[n].imp = NULL;
    return nl;
}

void protocol_addMethodDescription(Protocol *proto, SEL name, const char *types, BOOL required, BOOL instance) {
    if (!proto || !name) return;
    protocol_t *p = (protocol_t *)proto;
    if (!(p->flags & PROTOCOL_UNDER_CONSTRUCTION)) return;
    method_list_t **slot = select_list(p, required, instance);
    *slot = method_list_append(*slot, name, types);
}

void protocol_addProtocol(Protocol *proto, Protocol *addition) {
    if (!proto || !addition) return;
    protocol_t *p = (protocol_t *)proto;
    if (!(p->flags & PROTOCOL_UNDER_CONSTRUCTION)) return;
    uintptr_t n = p->protocols ? p->protocols->count : 0;
    protocol_list_t *nl = calloc(1, sizeof(protocol_list_t) + (n + 2) * sizeof(protocol_t *));
    nl->count = n + 1;
    if (p->protocols) memcpy(nl->list, p->protocols->list, n * sizeof(protocol_t *));
    nl->list[n] = protocol_canonical((protocol_t *)addition);
    p->protocols = nl;
}

void protocol_addProperty(Protocol *proto, const char *name, const objc_property_attribute_t *attributes, unsigned int attributeCount, BOOL required, BOOL instance) {
    if (!proto || !name || !required || !instance) return;
    protocol_t *p = (protocol_t *)proto;
    if (!(p->flags & PROTOCOL_UNDER_CONSTRUCTION)) return;
    uint32_t n = p->instanceProperties ? p->instanceProperties->count : 0;
    property_list_t *nl = calloc(1, sizeof(property_list_t) + (n + 1) * sizeof(property_t));
    nl->entsizeAndFlags = sizeof(property_t);
    nl->count = n + 1;
    if (p->instanceProperties) memcpy(nl->first, p->instanceProperties->first, n * sizeof(property_t));
    size_t len = 1;
    for (unsigned i = 0; i < attributeCount; i++) len += strlen(attributes[i].name) + strlen(attributes[i].value) + 1;
    char *s = malloc(len), *q = s;
    for (unsigned i = 0; i < attributeCount; i++) {
        if (i) *q++ = ',';
        strcpy(q, attributes[i].name); q += strlen(attributes[i].name);
        strcpy(q, attributes[i].value); q += strlen(attributes[i].value);
    }
    *q = 0;
    nl->first[n].name = strdup(name);
    nl->first[n].attributes = s;
    p->instanceProperties = nl;
}
