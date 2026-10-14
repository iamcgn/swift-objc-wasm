/*
 * objc.h — basic Objective-C types (objc4 API), libobjc-wasm.
 */
#ifndef _OBJC_OBJC_H_
#define _OBJC_OBJC_H_

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <objc/objc-api.h>

#if !OBJC_TYPES_DEFINED
/// An opaque type that represents an Objective-C class.
typedef struct objc_class *Class;

/// Represents an instance of a class.
struct objc_object {
    Class _Nonnull isa OBJC_ISA_AVAILABILITY;
};

/// A pointer to an instance of a class.
typedef struct objc_object *id;
#endif

/// An opaque type that represents a method selector.
typedef struct objc_selector *SEL;

/// A pointer to the function of a method implementation.
#if !OBJC_OLD_DISPATCH_PROTOTYPES
typedef void (*IMP)(void /* id, SEL, ... */ );
#else
typedef id _Nullable (*IMP)(id _Nonnull, SEL _Nonnull, ...);
#endif

/// BOOL is `signed char`, as on 32-bit Darwin; ObjCBool in Swift wraps Int8.
#define OBJC_BOOL_IS_BOOL 0
#define OBJC_BOOL_DEFINED
typedef signed char BOOL;

#if __has_feature(objc_bool)
#   define YES __objc_yes
#   define NO  __objc_no
#else
#   define YES ((BOOL)1)
#   define NO  ((BOOL)0)
#endif

#ifndef Nil
#   if __has_feature(cxx_nullptr)
#       define Nil nullptr
#   else
#       define Nil ((Class _Nullable)0)
#   endif
#endif

#ifndef nil
#   if __has_feature(cxx_nullptr)
#       define nil nullptr
#   else
#       define nil ((id _Nullable)0)
#   endif
#endif

#ifndef __strong
#   if !__has_feature(objc_arc)
#       define __strong /* empty */
#   endif
#endif

#ifndef __unsafe_unretained
#   if !__has_feature(objc_arc)
#       define __unsafe_unretained /* empty */
#   endif
#endif

#ifndef __autoreleasing
#   if !__has_feature(objc_arc)
#       define __autoreleasing /* empty */
#   endif
#endif

OBJC_EXPORT const char * _Nonnull sel_getName(SEL _Nonnull sel);
OBJC_EXPORT SEL _Nonnull sel_registerName(const char * _Nonnull str);
OBJC_EXPORT const char * _Nonnull object_getClassName(id _Nullable obj);
OBJC_EXPORT void * _Nullable object_getIndexedIvars(id _Nullable obj);
OBJC_EXPORT BOOL sel_isMapped(SEL _Nonnull sel);
OBJC_EXPORT SEL _Nonnull sel_getUid(const char * _Nonnull str);

typedef const void * _Nullable objc_objectptr_t;

#endif /* _OBJC_OBJC_H_ */
