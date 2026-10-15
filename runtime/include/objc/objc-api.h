// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
/*
 * objc-api.h — attribute and availability macros of the objc4 API, for libobjc-wasm.
 * Availability macros are no-ops: there is exactly one runtime version on this platform.
 */
#ifndef _OBJC_OBJC_API_H_
#define _OBJC_OBJC_API_H_

#include <stdint.h>

#ifndef __has_feature
#   define __has_feature(x) 0
#endif
#ifndef __has_extension
#   define __has_extension(x) 0
#endif
#ifndef __has_attribute
#   define __has_attribute(x) 0
#endif

#define OBJC_API_VERSION 2
#define OBJC_NO_GC 1
#define OBJC_NO_GC_API 1
#define OBJC_WASM 1

/* Dispatch prototypes: objc_msgSend is declared as an untyped function, as on Darwin. */
#ifndef OBJC_OLD_DISPATCH_PROTOTYPES
#   define OBJC_OLD_DISPATCH_PROTOTYPES 0
#endif

#if defined(__cplusplus)
#   define OBJC_EXPORT extern "C" __attribute__((visibility("default")))
#   define OBJC_IMPORT extern "C"
#else
#   define OBJC_EXPORT extern __attribute__((visibility("default")))
#   define OBJC_IMPORT extern
#endif
#define OBJC_INLINE static inline

#define OBJC_AVAILABLE(x, i, t, w, b)
#define OBJC_DEPRECATED(...)
#define OBJC_ISA_AVAILABILITY
#define OBJC_ISA_DEPRECATION
#define OBJC_OSX_DEPRECATED_OTHERS_UNAVAILABLE(...)
#define OBJC_OSX_AVAILABLE_OTHERS_UNAVAILABLE(...)
#define OBJC2_UNAVAILABLE
#define OBJC_ARM64_UNAVAILABLE
#define OBJC_GC_UNAVAILABLE
#define OBJC_REFCOUNT_UNAVAILABLE
#define OBJC_HIDE_64

#define OBJC_UNAVAILABLE(_msg) __attribute__((unavailable(_msg)))

/* Darwin Availability.h macros that Objective-C headers and the Swift runtime use. */
#ifndef __OSX_AVAILABLE_STARTING
#   define __OSX_AVAILABLE_STARTING(_osx, _ios)
#   define __OSX_AVAILABLE_BUT_DEPRECATED(_osxIntro, _osxDep, _iosIntro, _iosDep)
#   define __OSX_AVAILABLE_BUT_DEPRECATED_MSG(_osxIntro, _osxDep, _iosIntro, _iosDep, _msg)
#endif
#ifndef __API_AVAILABLE
#   define __API_AVAILABLE(...)
#   define __API_DEPRECATED(...)
#   define __API_UNAVAILABLE(...)
#endif
#ifndef API_AVAILABLE
#   define API_AVAILABLE(...)
#   define API_DEPRECATED(...)
#   define API_UNAVAILABLE(...)
#endif
#ifndef SPI_AVAILABLE
#   define SPI_AVAILABLE(...)
#endif
#define __MAC_10_10 101000
#define __IPHONE_8_0 80000

#if __has_feature(objc_arc)
#   define OBJC_ARC_UNAVAILABLE __attribute__((unavailable("not available in automatic reference counting mode")))
#else
#   define OBJC_ARC_UNAVAILABLE
#endif

#if __has_feature(attribute_availability_swift)
#   define OBJC_SWIFT_UNAVAILABLE(_msg) __attribute__((availability(swift, unavailable, message=_msg)))
#else
#   define OBJC_SWIFT_UNAVAILABLE(_msg)
#endif

#if __has_attribute(objc_root_class)
#   define OBJC_ROOT_CLASS __attribute__((objc_root_class))
#else
#   define OBJC_ROOT_CLASS
#endif

#if __has_attribute(objc_designated_initializer)
#   define OBJC_DESIGNATED_INITIALIZER __attribute__((objc_designated_initializer))
#else
#   define OBJC_DESIGNATED_INITIALIZER
#endif

#if __has_attribute(ns_returns_retained)
#   define OBJC_RETURNS_RETAINED __attribute__((ns_returns_retained))
#else
#   define OBJC_RETURNS_RETAINED
#endif

#if (__cplusplus && __cplusplus >= 201103L && (__has_extension(cxx_strong_enums) || __has_feature(objc_fixed_enum))) || (!__cplusplus && __has_feature(objc_fixed_enum))
#   define OBJC_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#   if __has_feature(objc_fixed_enum) && !__has_feature(objc_arc_fields) && 0
#   endif
#   define OBJC_OPTIONS(_type, _name) enum _name : _type _name; enum _name : _type
#else
#   define OBJC_ENUM(_type, _name) _type _name; enum
#   define OBJC_OPTIONS(_type, _name) _type _name; enum
#endif

#endif /* _OBJC_OBJC_API_H_ */
