/*
 * runtime.h — the Objective-C runtime API (objc4 API surface), libobjc-wasm.
 *
 * Everything declared here exists in the library. Functions that cannot be
 * implemented on WebAssembly (imp_implementationWithBlock, the layout of
 * fragile classes, ...) are declared but abort with a message when called.
 */
#ifndef _OBJC_RUNTIME_H_
#define _OBJC_RUNTIME_H_

#include <objc/objc.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/* Types */

#if !OBJC_TYPES_DEFINED

/// An opaque type that represents a method in a class definition.
typedef struct objc_method *Method;

/// An opaque type that represents an instance variable.
typedef struct objc_ivar *Ivar;

/// An opaque type that represents a category.
typedef struct objc_category *Category;

/// An opaque type that represents an Objective-C declared property.
typedef struct objc_property *objc_property_t;

#endif

#ifdef __OBJC__
@class Protocol;
#else
typedef struct objc_object Protocol;
#endif

/// Defines a method
struct objc_method_description {
    SEL _Nullable name;               /**< The name of the method */
    char * _Nullable types;           /**< The types of the method arguments */
};

/// Defines a property attribute
typedef struct {
    const char * _Nonnull name;           /**< The name of the attribute */
    const char * _Nonnull value;          /**< The value of the attribute (usually empty) */
} objc_property_attribute_t;


/* Functions */

/* Working with Instances */

OBJC_EXPORT id _Nullable object_copy(id _Nullable obj, size_t size) OBJC_ARC_UNAVAILABLE;
OBJC_EXPORT id _Nullable object_dispose(id _Nullable obj) OBJC_ARC_UNAVAILABLE;
OBJC_EXPORT Class _Nullable object_getClass(id _Nullable obj);
OBJC_EXPORT Class _Nullable object_setClass(id _Nullable obj, Class _Nonnull cls);
OBJC_EXPORT BOOL object_isClass(id _Nullable obj);
OBJC_EXPORT id _Nullable object_getIvar(id _Nullable obj, Ivar _Nonnull ivar);
OBJC_EXPORT void object_setIvar(id _Nullable obj, Ivar _Nonnull ivar, id _Nullable value);
OBJC_EXPORT void object_setIvarWithStrongDefault(id _Nullable obj, Ivar _Nonnull ivar, id _Nullable value);
OBJC_EXPORT Ivar _Nullable object_setInstanceVariable(id _Nullable obj, const char * _Nonnull name, void * _Nullable value) OBJC_ARC_UNAVAILABLE;
OBJC_EXPORT Ivar _Nullable object_setInstanceVariableWithStrongDefault(id _Nullable obj, const char * _Nonnull name, void * _Nullable value) OBJC_ARC_UNAVAILABLE;
OBJC_EXPORT Ivar _Nullable object_getInstanceVariable(id _Nullable obj, const char * _Nonnull name, void * _Nullable * _Nullable outValue) OBJC_ARC_UNAVAILABLE;

/* Obtaining Class Definitions */

OBJC_EXPORT Class _Nullable objc_getClass(const char * _Nonnull name);
OBJC_EXPORT Class _Nullable objc_getMetaClass(const char * _Nonnull name);
OBJC_EXPORT Class _Nullable objc_lookUpClass(const char * _Nonnull name);
OBJC_EXPORT Class _Nonnull objc_getRequiredClass(const char * _Nonnull name);
OBJC_EXPORT int objc_getClassList(Class _Nonnull * _Nullable buffer, int bufferCount);
OBJC_EXPORT Class _Nonnull * _Nullable objc_copyClassList(unsigned int * _Nullable outCount);

/* Working with Classes */

OBJC_EXPORT const char * _Nonnull class_getName(Class _Nullable cls);
OBJC_EXPORT BOOL class_isMetaClass(Class _Nullable cls);
OBJC_EXPORT Class _Nullable class_getSuperclass(Class _Nullable cls);
OBJC_EXPORT Class _Nullable class_setSuperclass(Class _Nonnull cls, Class _Nonnull newSuper);
OBJC_EXPORT int class_getVersion(Class _Nullable cls);
OBJC_EXPORT void class_setVersion(Class _Nullable cls, int version);
OBJC_EXPORT size_t class_getInstanceSize(Class _Nullable cls);
OBJC_EXPORT Ivar _Nullable class_getInstanceVariable(Class _Nullable cls, const char * _Nonnull name);
OBJC_EXPORT Ivar _Nullable class_getClassVariable(Class _Nullable cls, const char * _Nonnull name);
OBJC_EXPORT Ivar _Nonnull * _Nullable class_copyIvarList(Class _Nullable cls, unsigned int * _Nullable outCount);
OBJC_EXPORT Method _Nullable class_getInstanceMethod(Class _Nullable cls, SEL _Nonnull name);
OBJC_EXPORT Method _Nullable class_getClassMethod(Class _Nullable cls, SEL _Nonnull name);
OBJC_EXPORT IMP _Nullable class_getMethodImplementation(Class _Nullable cls, SEL _Nonnull name);
OBJC_EXPORT IMP _Nullable class_getMethodImplementation_stret(Class _Nullable cls, SEL _Nonnull name) OBJC_UNAVAILABLE("stret dispatch does not exist on WebAssembly");
OBJC_EXPORT BOOL class_respondsToSelector(Class _Nullable cls, SEL _Nonnull sel);
OBJC_EXPORT Method _Nonnull * _Nullable class_copyMethodList(Class _Nullable cls, unsigned int * _Nullable outCount);
OBJC_EXPORT BOOL class_conformsToProtocol(Class _Nullable cls, Protocol * _Nullable protocol);
OBJC_EXPORT Protocol * __unsafe_unretained _Nonnull * _Nullable class_copyProtocolList(Class _Nullable cls, unsigned int * _Nullable outCount);
OBJC_EXPORT objc_property_t _Nullable class_getProperty(Class _Nullable cls, const char * _Nonnull name);
OBJC_EXPORT objc_property_t _Nonnull * _Nullable class_copyPropertyList(Class _Nullable cls, unsigned int * _Nullable outCount);
OBJC_EXPORT const uint8_t * _Nullable class_getIvarLayout(Class _Nullable cls);
OBJC_EXPORT const uint8_t * _Nullable class_getWeakIvarLayout(Class _Nullable cls);
OBJC_EXPORT BOOL class_addMethod(Class _Nullable cls, SEL _Nonnull name, IMP _Nonnull imp, const char * _Nullable types);
OBJC_EXPORT IMP _Nullable class_replaceMethod(Class _Nullable cls, SEL _Nonnull name, IMP _Nonnull imp, const char * _Nullable types);
OBJC_EXPORT BOOL class_addIvar(Class _Nullable cls, const char * _Nonnull name, size_t size, uint8_t alignment, const char * _Nullable types);
OBJC_EXPORT BOOL class_addProtocol(Class _Nullable cls, Protocol * _Nonnull protocol);
OBJC_EXPORT BOOL class_addProperty(Class _Nullable cls, const char * _Nonnull name, const objc_property_attribute_t * _Nullable attributes, unsigned int attributeCount);
OBJC_EXPORT void class_replaceProperty(Class _Nullable cls, const char * _Nonnull name, const objc_property_attribute_t * _Nullable attributes, unsigned int attributeCount);
OBJC_EXPORT void class_setIvarLayout(Class _Nullable cls, const uint8_t * _Nullable layout);
OBJC_EXPORT void class_setWeakIvarLayout(Class _Nullable cls, const uint8_t * _Nullable layout);

/* Instantiating Classes */

OBJC_EXPORT id _Nullable class_createInstance(Class _Nullable cls, size_t extraBytes) OBJC_ARC_UNAVAILABLE;
OBJC_EXPORT id _Nullable objc_constructInstance(Class _Nullable cls, void * _Nullable bytes) OBJC_ARC_UNAVAILABLE;
OBJC_EXPORT void * _Nullable objc_destructInstance(id _Nullable obj) OBJC_ARC_UNAVAILABLE;

/* Adding Classes */

OBJC_EXPORT Class _Nullable objc_allocateClassPair(Class _Nullable superclass, const char * _Nonnull name, size_t extraBytes);
OBJC_EXPORT void objc_registerClassPair(Class _Nonnull cls);
OBJC_EXPORT Class _Nonnull objc_duplicateClass(Class _Nonnull original, const char * _Nonnull name, size_t extraBytes);
OBJC_EXPORT void objc_disposeClassPair(Class _Nonnull cls);

/* Working with Methods */

OBJC_EXPORT SEL _Nonnull method_getName(Method _Nonnull m);
OBJC_EXPORT IMP _Nonnull method_getImplementation(Method _Nonnull m);
OBJC_EXPORT const char * _Nullable method_getTypeEncoding(Method _Nonnull m);
OBJC_EXPORT unsigned int method_getNumberOfArguments(Method _Nonnull m);
OBJC_EXPORT char * _Nonnull method_copyReturnType(Method _Nonnull m);
OBJC_EXPORT char * _Nullable method_copyArgumentType(Method _Nonnull m, unsigned int index);
OBJC_EXPORT void method_getReturnType(Method _Nonnull m, char * _Nonnull dst, size_t dst_len);
OBJC_EXPORT void method_getArgumentType(Method _Nonnull m, unsigned int index, char * _Nullable dst, size_t dst_len);
OBJC_EXPORT struct objc_method_description * _Nonnull method_getDescription(Method _Nonnull m);
OBJC_EXPORT IMP _Nonnull method_setImplementation(Method _Nonnull m, IMP _Nonnull imp);
OBJC_EXPORT void method_exchangeImplementations(Method _Nonnull m1, Method _Nonnull m2);

/* Working with Instance Variables */

OBJC_EXPORT const char * _Nullable ivar_getName(Ivar _Nonnull v);
OBJC_EXPORT const char * _Nullable ivar_getTypeEncoding(Ivar _Nonnull v);
OBJC_EXPORT ptrdiff_t ivar_getOffset(Ivar _Nonnull v);

/* Working with Properties */

OBJC_EXPORT const char * _Nonnull property_getName(objc_property_t _Nonnull property);
OBJC_EXPORT const char * _Nullable property_getAttributes(objc_property_t _Nonnull property);
OBJC_EXPORT objc_property_attribute_t * _Nullable property_copyAttributeList(objc_property_t _Nonnull property, unsigned int * _Nullable outCount);
OBJC_EXPORT char * _Nullable property_copyAttributeValue(objc_property_t _Nonnull property, const char * _Nonnull attributeName);

/* Working with Protocols */

OBJC_EXPORT Protocol * _Nullable objc_getProtocol(const char * _Nonnull name);
OBJC_EXPORT Protocol * __unsafe_unretained _Nonnull * _Nullable objc_copyProtocolList(unsigned int * _Nullable outCount);
OBJC_EXPORT BOOL protocol_conformsToProtocol(Protocol * _Nullable proto, Protocol * _Nullable other);
OBJC_EXPORT BOOL protocol_isEqual(Protocol * _Nullable proto, Protocol * _Nullable other);
OBJC_EXPORT const char * _Nonnull protocol_getName(Protocol * _Nonnull proto);
OBJC_EXPORT struct objc_method_description protocol_getMethodDescription(Protocol * _Nonnull proto, SEL _Nonnull aSel, BOOL isRequiredMethod, BOOL isInstanceMethod);
OBJC_EXPORT struct objc_method_description * _Nullable protocol_copyMethodDescriptionList(Protocol * _Nonnull proto, BOOL isRequiredMethod, BOOL isInstanceMethod, unsigned int * _Nullable outCount);
OBJC_EXPORT objc_property_t _Nullable protocol_getProperty(Protocol * _Nonnull proto, const char * _Nonnull name, BOOL isRequiredProperty, BOOL isInstanceProperty);
OBJC_EXPORT objc_property_t _Nonnull * _Nullable protocol_copyPropertyList(Protocol * _Nonnull proto, unsigned int * _Nullable outCount);
OBJC_EXPORT objc_property_t _Nonnull * _Nullable protocol_copyPropertyList2(Protocol * _Nonnull proto, unsigned int * _Nullable outCount, BOOL isRequiredProperty, BOOL isInstanceProperty);
OBJC_EXPORT Protocol * __unsafe_unretained _Nonnull * _Nullable protocol_copyProtocolList(Protocol * _Nonnull proto, unsigned int * _Nullable outCount);
OBJC_EXPORT Protocol * _Nullable objc_allocateProtocol(const char * _Nonnull name);
OBJC_EXPORT void objc_registerProtocol(Protocol * _Nonnull proto);
OBJC_EXPORT void protocol_addMethodDescription(Protocol * _Nonnull proto, SEL _Nonnull name, const char * _Nullable types, BOOL isRequiredMethod, BOOL isInstanceMethod);
OBJC_EXPORT void protocol_addProtocol(Protocol * _Nonnull proto, Protocol * _Nonnull addition);
OBJC_EXPORT void protocol_addProperty(Protocol * _Nonnull proto, const char * _Nonnull name, const objc_property_attribute_t * _Nullable attributes, unsigned int attributeCount, BOOL isRequiredProperty, BOOL isInstanceProperty);

/* Working with Libraries */

OBJC_EXPORT const char * _Nonnull * _Nonnull objc_copyImageNames(unsigned int * _Nullable outCount);
OBJC_EXPORT const char * _Nullable class_getImageName(Class _Nullable cls);
OBJC_EXPORT const char * _Nonnull * _Nonnull objc_copyClassNamesForImage(const char * _Nonnull image, unsigned int * _Nullable outCount);

/* Working with Selectors */

OBJC_EXPORT BOOL sel_isEqual(SEL _Nonnull lhs, SEL _Nonnull rhs);

/* Objective-C Language Features */

OBJC_EXPORT void objc_enumerationMutation(id _Nonnull obj);
OBJC_EXPORT void objc_setEnumerationMutationHandler(void (* _Nullable handler)(id _Nonnull));
OBJC_EXPORT void objc_setForwardHandler(void * _Nonnull fwd, void * _Nonnull fwd_stret);

/* Associative References */

#if __cplusplus
enum objc_AssociationPolicy : uintptr_t {
#else
typedef OBJC_ENUM(uintptr_t, objc_AssociationPolicy) {
#endif
    OBJC_ASSOCIATION_ASSIGN = 0,
    OBJC_ASSOCIATION_RETAIN_NONATOMIC = 1,
    OBJC_ASSOCIATION_COPY_NONATOMIC = 3,
    OBJC_ASSOCIATION_RETAIN = 01401,
    OBJC_ASSOCIATION_COPY = 01403
};

OBJC_EXPORT void objc_setAssociatedObject(id _Nonnull object, const void * _Nonnull key, id _Nullable value, objc_AssociationPolicy policy);
OBJC_EXPORT id _Nullable objc_getAssociatedObject(id _Nonnull object, const void * _Nonnull key);
OBJC_EXPORT void objc_removeAssociatedObjects(id _Nonnull object);

/* Hooks for Swift */

typedef BOOL (*objc_hook_getImageName)(Class _Nonnull cls, const char * _Nullable * _Nonnull outImageName);
OBJC_EXPORT void objc_setHook_getImageName(objc_hook_getImageName _Nonnull newValue, objc_hook_getImageName _Nullable * _Nonnull outOldValue);
#define OBJC_SETHOOK_GETIMAGENAME_DEFINED 1

typedef BOOL (*objc_hook_getClass)(const char * _Nonnull name, Class _Nullable * _Nonnull outClass);
OBJC_EXPORT void objc_setHook_getClass(objc_hook_getClass _Nonnull newValue, objc_hook_getClass _Nullable * _Nonnull outOldValue);
#define OBJC_GETCLASSHOOK_DEFINED 1

typedef const char * _Nullable (*objc_hook_lazyClassNamer)(Class _Nonnull cls);
OBJC_EXPORT void objc_setHook_lazyClassNamer(objc_hook_lazyClassNamer _Nonnull newValue, objc_hook_lazyClassNamer _Nullable * _Nonnull outOldValue);
#define OBJC_SETHOOK_LAZYCLASSNAMER_DEFINED 1

/* Swift class realization (as in objc4's runtime.h). */
typedef Class _Nullable (*_objc_swiftMetadataInitializer)(Class _Nonnull cls, void * _Nullable arg);

/// Realize `cls` (which may be in the class list, dynamically created, or the
/// replacement for the stub class `previously`). Categories referencing the stub
/// are attached to `cls`.
OBJC_EXPORT Class _Nullable _objc_realizeClassFromSwift(Class _Nullable cls, void * _Nullable previously);
#define OBJC_REALIZECLASSFROMSWIFT_DEFINED 1

/* Stub classes (Swift). The classref's low bit marks an uninitialized stub. */
OBJC_EXPORT Class _Nullable objc_loadClassref(Class _Nullable * _Nonnull clsref);

/* Weak references (public part) */
OBJC_EXPORT id _Nullable objc_loadWeak(id _Nullable * _Nonnull location);
OBJC_EXPORT id _Nullable objc_storeWeak(id _Nullable * _Nonnull location, id _Nullable obj);

#endif /* _OBJC_RUNTIME_H_ */
