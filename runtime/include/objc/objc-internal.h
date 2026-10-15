/*
 * objc-internal.h — SPI and ARC entry points (objc4 API surface), libobjc-wasm.
 */
#ifndef _OBJC_INTERNAL_H_
#define _OBJC_INTERNAL_H_

#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>

/* Image info, as emitted in objc_imageinfo by the compiler (defined by clients such
 * as the Swift runtime; this runtime discovers metadata through section bounds). */
struct objc_image_info;

/* Class registration from Swift. */
OBJC_EXPORT Class _Nullable objc_readClassPair(Class _Nonnull cls, const struct objc_image_info * _Nullable info);
#define OBJC_READCLASSPAIR_DEFINED 1

OBJC_EXPORT BOOL _objc_supportsLazyRealization(void);
#define OBJC_SUPPORTSLAZYREALIZATION_DEFINED 1

OBJC_EXPORT BOOL objc_isUniquelyReferenced(id _Nullable obj);
#define OBJC_ISUNIQUELYREFERENCED_DEFINED 1

typedef void (*_objc_classCopyFixupHandler)(Class _Nonnull oldClass, Class _Nonnull newClass);
OBJC_EXPORT void _objc_setClassCopyFixupHandler(_objc_classCopyFixupHandler _Nonnull newFixupHandler);

/* Root implementations of NSObject's reference counting and allocation. */
OBJC_EXPORT id _Nonnull _objc_rootRetain(id _Nonnull obj);
OBJC_EXPORT void _objc_rootRelease(id _Nonnull obj);
OBJC_EXPORT bool _objc_rootReleaseWasZero(id _Nonnull obj);
OBJC_EXPORT bool _objc_rootTryRetain(id _Nonnull obj);
OBJC_EXPORT bool _objc_rootIsDeallocating(id _Nonnull obj);
OBJC_EXPORT id _Nonnull _objc_rootAutorelease(id _Nonnull obj);
OBJC_EXPORT uintptr_t _objc_rootRetainCount(id _Nonnull obj);
OBJC_EXPORT id _Nullable _objc_rootAlloc(Class _Nonnull cls);
OBJC_EXPORT id _Nullable _objc_rootAllocWithZone(Class _Nonnull cls, void * _Nullable zone);
OBJC_EXPORT id _Nonnull _objc_rootInit(id _Nonnull obj);
OBJC_EXPORT void _objc_rootDealloc(id _Nonnull obj);
OBJC_EXPORT void _objc_rootFinalize(id _Nonnull obj);
OBJC_EXPORT void * _Nullable _objc_rootZone(id _Nonnull obj);
OBJC_EXPORT uintptr_t _objc_rootHash(id _Nonnull obj);
OBJC_EXPORT void _objc_setBadAllocHandler(id _Nullable (* _Nonnull newHandler)(Class _Nullable isa));

/* Allocation entry points the compiler may emit. */
OBJC_EXPORT id _Nullable objc_alloc(Class _Nullable cls);
OBJC_EXPORT id _Nullable objc_allocWithZone(Class _Nullable cls);
OBJC_EXPORT id _Nullable objc_alloc_init(Class _Nullable cls);
OBJC_EXPORT id _Nullable objc_opt_new(Class _Nullable cls);
OBJC_EXPORT id _Nullable objc_opt_self(id _Nullable obj);
OBJC_EXPORT Class _Nullable objc_opt_class(id _Nullable obj);
OBJC_EXPORT BOOL objc_opt_isKindOfClass(id _Nullable obj, Class _Nullable cls);
OBJC_EXPORT BOOL objc_opt_respondsToSelector(id _Nullable obj, SEL _Nullable sel);

/* ARC runtime. */
OBJC_EXPORT id _Nullable objc_retain(id _Nullable obj);
OBJC_EXPORT void objc_release(id _Nullable obj);
OBJC_EXPORT id _Nullable objc_autorelease(id _Nullable obj);
OBJC_EXPORT id _Nullable objc_retainAutorelease(id _Nullable obj);
OBJC_EXPORT id _Nullable objc_retainAutoreleaseReturnValue(id _Nullable obj);
OBJC_EXPORT id _Nullable objc_autoreleaseReturnValue(id _Nullable obj);
OBJC_EXPORT id _Nullable objc_retainAutoreleasedReturnValue(id _Nullable obj);
OBJC_EXPORT id _Nullable objc_unsafeClaimAutoreleasedReturnValue(id _Nullable obj);
OBJC_EXPORT id _Nullable objc_retainBlock(id _Nullable obj);
OBJC_EXPORT void objc_storeStrong(id _Nullable * _Nonnull location, id _Nullable obj);
OBJC_EXPORT void objc_release_x(id _Nullable obj);

OBJC_EXPORT void * _Nonnull objc_autoreleasePoolPush(void);
OBJC_EXPORT void objc_autoreleasePoolPop(void * _Nonnull context);

OBJC_EXPORT id _Nullable objc_initWeak(id _Nullable * _Nonnull location, id _Nullable val);
OBJC_EXPORT id _Nullable objc_initWeakOrNil(id _Nullable * _Nonnull location, id _Nullable val);
OBJC_EXPORT id _Nullable objc_storeWeakOrNil(id _Nullable * _Nonnull location, id _Nullable obj);
OBJC_EXPORT id _Nullable objc_loadWeakRetained(id _Nullable * _Nonnull location);
OBJC_EXPORT void objc_destroyWeak(id _Nullable * _Nonnull location);
OBJC_EXPORT void objc_copyWeak(id _Nullable * _Nonnull to, id _Nullable * _Nonnull from);
OBJC_EXPORT void objc_moveWeak(id _Nullable * _Nonnull to, id _Nullable * _Nonnull from);

OBJC_EXPORT int objc_sync_enter(id _Nullable obj);
OBJC_EXPORT int objc_sync_exit(id _Nullable obj);

/* Property helpers the compiler emits for @synthesize. */
OBJC_EXPORT id _Nullable objc_getProperty(id _Nullable self, SEL _Nonnull _cmd, ptrdiff_t offset, BOOL atomic);
OBJC_EXPORT void objc_setProperty(id _Nullable self, SEL _Nonnull _cmd, ptrdiff_t offset, id _Nullable newValue, BOOL atomic, signed char shouldCopy);
OBJC_EXPORT void objc_setProperty_atomic(id _Nullable self, SEL _Nonnull _cmd, id _Nullable newValue, ptrdiff_t offset);
OBJC_EXPORT void objc_setProperty_nonatomic(id _Nullable self, SEL _Nonnull _cmd, id _Nullable newValue, ptrdiff_t offset);
OBJC_EXPORT void objc_setProperty_atomic_copy(id _Nullable self, SEL _Nonnull _cmd, id _Nullable newValue, ptrdiff_t offset);
OBJC_EXPORT void objc_setProperty_nonatomic_copy(id _Nullable self, SEL _Nonnull _cmd, id _Nullable newValue, ptrdiff_t offset);
OBJC_EXPORT void objc_copyStruct(void * _Nonnull dest, const void * _Nonnull src, ptrdiff_t size, BOOL atomic, BOOL hasStrong);
OBJC_EXPORT void objc_copyCppObjectAtomic(void * _Nonnull dest, const void * _Nonnull src, void (* _Nonnull copyHelper)(void * _Nonnull dest, const void * _Nonnull source));

/* Diagnostics. */
OBJC_EXPORT void _objc_fatal(const char * _Nonnull fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
OBJC_EXPORT void _objc_inform(const char * _Nonnull fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* _OBJC_INTERNAL_H_ */
