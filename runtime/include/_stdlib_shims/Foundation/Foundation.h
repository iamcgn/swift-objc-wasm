// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
/* Foundation/Foundation.h shim for building the Swift runtime against libobjc-wasm.
 * There is no Foundation here: only the declarations the runtime's Objective-C++
 * sources need to compile. Foundation classes are looked up at run time by name. */
#ifndef _FOUNDATION_SHIM_H_
#define _FOUNDATION_SHIM_H_
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <objc/NSObject.h>
#include <objc/NSObjCRuntime.h>
#include <objc/objc-internal.h>
#include <CoreFoundation/CFBase.h>
#include <TargetConditionals.h>

#define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#define NS_OPTIONS(_type, _name) enum _name : _type _name; enum _name : _type
#define NS_INLINE static inline
#define NS_RETURNS_RETAINED __attribute__((ns_returns_retained))
#define NS_RETURNS_NOT_RETAINED __attribute__((ns_returns_not_retained))
#define NS_SWIFT_NAME(_name) __attribute__((swift_name(#_name)))
#define NS_SWIFT_UNAVAILABLE(_msg) OBJC_SWIFT_UNAVAILABLE(_msg)
#define NS_DESIGNATED_INITIALIZER OBJC_DESIGNATED_INITIALIZER
#define NS_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#define NS_ASSUME_NONNULL_END _Pragma("clang assume_nonnull end")
#define FOUNDATION_EXPORT extern
#define FOUNDATION_EXTERN extern
enum { NSNotFound = NSIntegerMax };

#if __OBJC__
@class NSString, NSArray, NSDictionary, NSError, NSNull, NSNumber, NSSet, NSBundle, NSProxy, NSCoder, NSInvocation, NSMethodSignature, NSException;

typedef NSString *NSErrorDomain;
typedef NSString *NSErrorUserInfoKey;

@protocol NSCopying
- (id)copyWithZone:(NSZone *)zone;
@end

@protocol NSMutableCopying
- (id)mutableCopyWithZone:(NSZone *)zone;
@end

@protocol NSCoding
- (void)encodeWithCoder:(NSCoder *)coder;
- (instancetype)initWithCoder:(NSCoder *)coder;
@end

@protocol NSSecureCoding <NSCoding>
+ (BOOL)supportsSecureCoding;
@end

typedef unsigned short unichar;

/* Declaration only: there is no NSString implementation in libobjc-wasm. The Swift
 * runtime calls these on objects it has already identified as Foundation strings. */
@interface NSString : NSObject <NSCopying, NSMutableCopying>
- (NSUInteger)length;
- (unichar)characterAtIndex:(NSUInteger)index;
- (const char *)UTF8String;
@property (readonly, copy) NSString *decomposedStringWithCanonicalMapping;
+ (instancetype)stringWithUTF8String:(const char *)nullTerminatedCString;
@end

/* Messages the Swift runtime sends to Foundation objects it looked up by name. */
@interface NSObject (FoundationShimSelectors)
- (id)domain;
- (NSInteger)code;
- (id)userInfo;
- (id)localizedDescription;
+ (id)dictionary;
+ (id)null;
- (id)objectForKey:(id)key;
- (BOOL)isNSObject__;
- (BOOL)isNSString__;
@end
#endif

#endif /* _FOUNDATION_SHIM_H_ */
