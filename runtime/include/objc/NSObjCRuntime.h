// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
/*
 * NSObjCRuntime.h — NSInteger/NSUInteger (objc4 copy), libobjc-wasm.
 */
#ifndef _OBJC_NSOBJCRUNTIME_H_
#define _OBJC_NSOBJCRUNTIME_H_

#include <stdint.h>
#include <limits.h>

#if __LP64__ || NS_BUILD_32_LIKE_64
typedef long NSInteger;
typedef unsigned long NSUInteger;
#else
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif

#define NSIntegerMax    LONG_MAX
#define NSIntegerMin    LONG_MIN
#define NSUIntegerMax   ULONG_MAX

#define NSINTEGER_DEFINED 1

#endif /* _OBJC_NSOBJCRUNTIME_H_ */
