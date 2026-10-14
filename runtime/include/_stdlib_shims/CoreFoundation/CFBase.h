/* CoreFoundation/CFBase.h shim: the types and functions the Swift runtime's
 * bridging code refers to, implemented by libobjc-wasm's CFLite.c. */
#ifndef __COREFOUNDATION_CFBASE__
#define __COREFOUNDATION_CFBASE__
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <objc/objc.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned long CFTypeID;
typedef unsigned long CFHashCode;
typedef unsigned long CFOptionFlags;
typedef long CFIndex;
typedef const void *CFTypeRef;
typedef const struct __CFString *CFStringRef;
typedef struct __CFString *CFMutableStringRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const struct __CFArray *CFArrayRef;
typedef struct __CFError *CFErrorRef;
typedef const struct __CFAllocator *CFAllocatorRef;
typedef struct __CFRuntimeBase {
    uintptr_t _cfisa;
    uint8_t _cfinfo[4];
} CFRuntimeBase;
#define kCFAllocatorDefault ((CFAllocatorRef)0)
#define CF_EXPORT extern
CFTypeID CFGetTypeID(CFTypeRef cf);
CFTypeID CFStringGetTypeID(void);
CFHashCode CFStringHashNSString(id str);
CFHashCode CFStringHashCString(const uint8_t *bytes, CFIndex len);
#ifdef __cplusplus
}
#endif
#endif
