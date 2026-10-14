/*
 * abi.h — the objc4 "non-fragile" (ObjC 2) metadata ABI as emitted by clang
 * (-fobjc-runtime=ios-7.0) and by Swift IRGen, for 32-bit WebAssembly.
 *
 * Reference dumps: runtime/docs/abi_i386.ll (clang) and runtime/docs/swiftmeta.ll (Swift).
 */
#ifndef LIBOBJC_WASM_ABI_H
#define LIBOBJC_WASM_ABI_H

#include <stdint.h>
#include <stddef.h>
#include <objc/objc.h>

#if __LP64__
#error "libobjc-wasm implements the 32-bit layouts only"
#endif

typedef struct objc_class *Class_t;

/* struct objc_class — the five-word prefix shared with Swift class metadata.
 * `bits` is a class_ro_t pointer OR-ed with the is-Swift bit until realization,
 * then a class_rw_t pointer OR-ed with the same bit. */
struct objc_class {
    struct objc_class *isa;
    struct objc_class *superclass;
    void *cache;              /* _objc_empty_cache; unused here */
    void *vtable;             /* _objc_empty_vtable (NULL) */
    uintptr_t bits;
};

#define FAST_IS_SWIFT_LEGACY  ((uintptr_t)1)
#define FAST_IS_SWIFT_STABLE  ((uintptr_t)2)
#define FAST_IS_SWIFT_MASK    (FAST_IS_SWIFT_LEGACY | FAST_IS_SWIFT_STABLE)
#define FAST_DATA_MASK        (~(uintptr_t)3)

/* Swift class metadata continues after the objc_class prefix. Only the Flags word
 * is of interest to the runtime (UsesSwiftRefcounting). */
struct swift_class_t {
    struct objc_class objc;
    uint32_t flags;           /* swift::ClassFlags */
    uint32_t instanceAddressPoint;
    uint32_t instanceSize;
    uint16_t instanceAlignMask;
    uint16_t reserved;
    uint32_t classSize;
    uint32_t classAddressPoint;
    const void *description;
    /* ... vtable, field offsets */
};
#define SWIFT_CLASS_FLAG_IS_PRE_STABLE_ABI     0x1u
#define SWIFT_CLASS_FLAG_USES_SWIFT_REFCOUNTING 0x2u

/* Swift stub class (objc_stublist entries point at `isa`). */
struct objc_class_stub {
    uintptr_t isa;            /* 1..15 */
    Class (*initializer)(Class cls, void *arg);
};
#define IS_STUB_CLASS_ISA(isa) ((uintptr_t)(isa) >= 1 && (uintptr_t)(isa) <= 15)

/* method_t / method_list_t (absolute, "big" methods only). */
typedef struct method_t {
    SEL name;
    const char *types;
    IMP imp;
} method_t;

typedef struct method_list_t {
    uint32_t entsizeAndFlags;
    uint32_t count;
    method_t first[];
} method_list_t;

#define METHOD_LIST_FLAG_MASK       0xffff0003u
#define METHOD_LIST_IS_SMALL        0x80000000u   /* relative method lists: unsupported */
#define METHOD_LIST_IS_FIXED_UP     0x3u           /* runtime-private flag bits */
#define METHOD_LIST_ENTSIZE(l)      ((l)->entsizeAndFlags & ~METHOD_LIST_FLAG_MASK)

typedef struct ivar_t {
    int32_t *offset;          /* NULL for class ivars; the runtime slides *offset */
    const char *name;
    const char *type;
    uint32_t alignment_raw;   /* log2(alignment); ~0 means pointer alignment */
    uint32_t size;
} ivar_t;

typedef struct ivar_list_t {
    uint32_t entsizeAndFlags;
    uint32_t count;
    ivar_t first[];
} ivar_list_t;

typedef struct property_t {
    const char *name;
    const char *attributes;
} property_t;

typedef struct property_list_t {
    uint32_t entsizeAndFlags;
    uint32_t count;
    property_t first[];
} property_list_t;

struct protocol_t;

typedef struct protocol_list_t {
    uintptr_t count;
    struct protocol_t *list[];   /* count entries, then a NULL terminator */
} protocol_list_t;

typedef struct protocol_t {
    struct objc_class *isa;
    const char *mangledName;
    protocol_list_t *protocols;
    method_list_t *instanceMethods;
    method_list_t *classMethods;
    method_list_t *optionalInstanceMethods;
    method_list_t *optionalClassMethods;
    property_list_t *instanceProperties;
    uint32_t size;            /* sizeof(protocol_t) = 52 on 32-bit */
    uint32_t flags;
    /* Fields below exist only when size says so. */
    const char **extendedMethodTypes;
    const char *demangledName;
    property_list_t *classProperties;
} protocol_t;

#define PROTOCOL_FIXED_UP_2     (1u << 31)
#define PROTOCOL_FIXED_UP_1     (1u << 30)
#define PROTOCOL_IS_CANONICAL   (1u << 29)
#define PROTOCOL_HAS_EXTENDED_METHOD_TYPES(p) ((p)->size >= offsetof(protocol_t, extendedMethodTypes) + sizeof(void *))
#define PROTOCOL_HAS_DEMANGLED_NAME(p)        ((p)->size >= offsetof(protocol_t, demangledName) + sizeof(void *))
#define PROTOCOL_HAS_CLASS_PROPERTIES(p)      ((p)->size >= offsetof(protocol_t, classProperties) + sizeof(void *))

typedef struct category_t {
    const char *name;
    struct objc_class *cls;      /* may point at a Swift stub class */
    method_list_t *instanceMethods;
    method_list_t *classMethods;
    protocol_list_t *protocols;
    property_list_t *instanceProperties;
    property_list_t *_classProperties;
    uint32_t size;
} category_t;
#define CATEGORY_HAS_CLASS_PROPERTIES(c) ((c)->size >= offsetof(category_t, _classProperties) + sizeof(void *))

/* class_ro_t: read-only class data emitted by the compiler. */
typedef struct class_ro_t {
    uint32_t flags;
    uint32_t instanceStart;
    uint32_t instanceSize;
    const uint8_t *ivarLayout;      /* or Class for Swift specialized-generic metaclasses */
    const char *name;
    method_list_t *baseMethodList;
    protocol_list_t *baseProtocols;
    const ivar_list_t *ivars;
    const uint8_t *weakIvarLayout;
    property_list_t *baseProperties;
    /* Present when flags & RO_HAS_SWIFT_INITIALIZER: */
    /* Class (*swiftMetadataInitializer)(Class cls, void *arg); */
} class_ro_t;

#define RO_META                 (1u << 0)
#define RO_ROOT                 (1u << 1)
#define RO_HAS_CXX_STRUCTORS    (1u << 2)
#define RO_HIDDEN               (1u << 4)
#define RO_EXCEPTION            (1u << 5)
#define RO_HAS_SWIFT_INITIALIZER (1u << 6)
#define RO_IS_ARC               (1u << 7)
#define RO_HAS_CXX_DTOR_ONLY    (1u << 8)
#define RO_HAS_WEAK_WITHOUT_ARC (1u << 9)
#define RO_FORBIDS_ASSOCIATED_OBJECTS (1u << 10)
#define RO_FROM_BUNDLE          (1u << 29)
#define RO_FUTURE               (1u << 30)
#define RO_REALIZED             (1u << 31)

static inline Class (*class_ro_swift_initializer(const class_ro_t *ro))(Class, void *) {
    if (!(ro->flags & RO_HAS_SWIFT_INITIALIZER)) return NULL;
    return *(Class (*const *)(Class, void *))((const char *)ro + sizeof(class_ro_t));
}

/* Message send metadata. */
struct objc_super_t {
    id receiver;
    struct objc_class *super_class;
};

/* Constant strings: @"..." literal layout (clang __NSConstantString_tag). */
struct __NSConstantString_tag {
    struct objc_class *isa;      /* &__CFConstantStringClassReference */
    int flags;                   /* 0x7c8 for UTF-8/ASCII literals */
    const char *str;
    long length;
};

#endif /* LIBOBJC_WASM_ABI_H */
