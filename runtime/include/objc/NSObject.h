// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
/*
 * NSObject.h — the NSObject protocol and root class (objc4 API), libobjc-wasm.
 * As on Darwin, the root class is provided by the Objective-C runtime library.
 */
#ifndef _OBJC_NSOBJECT_H_
#define _OBJC_NSOBJECT_H_

#if __OBJC__

#include <objc/objc.h>
#include <objc/NSObjCRuntime.h>

@class NSString, NSMethodSignature, NSInvocation;
@class Protocol;

typedef struct _NSZone NSZone;

@protocol NSObject

- (BOOL)isEqual:(id _Nullable)object;
@property (readonly) NSUInteger hash;

@property (readonly) Class _Nonnull superclass;
- (Class _Nonnull)class OBJC_SWIFT_UNAVAILABLE("use 'type(of: anObject)' instead");
- (instancetype _Nonnull)self;

- (id _Nullable)performSelector:(SEL _Nonnull)aSelector;
- (id _Nullable)performSelector:(SEL _Nonnull)aSelector withObject:(id _Nullable)object;
- (id _Nullable)performSelector:(SEL _Nonnull)aSelector withObject:(id _Nullable)object1 withObject:(id _Nullable)object2;

- (BOOL)isProxy;

- (BOOL)isKindOfClass:(Class _Nonnull)aClass;
- (BOOL)isMemberOfClass:(Class _Nonnull)aClass;
- (BOOL)conformsToProtocol:(Protocol * _Nonnull)aProtocol;

- (BOOL)respondsToSelector:(SEL _Nonnull)aSelector;

- (instancetype _Nonnull)retain OBJC_ARC_UNAVAILABLE;
- (oneway void)release OBJC_ARC_UNAVAILABLE;
- (instancetype _Nonnull)autorelease OBJC_ARC_UNAVAILABLE;
- (NSUInteger)retainCount OBJC_ARC_UNAVAILABLE;

- (struct _NSZone * _Nullable)zone OBJC_ARC_UNAVAILABLE;

@property (readonly, copy) NSString * _Nonnull description;
@optional
@property (readonly, copy) NSString * _Nonnull debugDescription;

@end


OBJC_ROOT_CLASS
@interface NSObject <NSObject> {
    Class _Nonnull isa OBJC_ISA_AVAILABILITY;
}

+ (void)load;

+ (void)initialize;
- (instancetype _Nonnull)init OBJC_DESIGNATED_INITIALIZER;

+ (instancetype _Nonnull)new OBJC_SWIFT_UNAVAILABLE("use object initializers instead");
+ (instancetype _Nonnull)allocWithZone:(struct _NSZone * _Nullable)zone OBJC_SWIFT_UNAVAILABLE("use object initializers instead");
+ (instancetype _Nonnull)alloc OBJC_SWIFT_UNAVAILABLE("use object initializers instead");
- (void)dealloc OBJC_SWIFT_UNAVAILABLE("use 'deinit' to define a de-initializer");

- (id _Nonnull)copy;
- (id _Nonnull)mutableCopy;

+ (id _Nonnull)copyWithZone:(struct _NSZone * _Nullable)zone OBJC_ARC_UNAVAILABLE;
+ (id _Nonnull)mutableCopyWithZone:(struct _NSZone * _Nullable)zone OBJC_ARC_UNAVAILABLE;

+ (BOOL)instancesRespondToSelector:(SEL _Nonnull)aSelector;
+ (BOOL)conformsToProtocol:(Protocol * _Nonnull)protocol;
- (IMP _Nonnull)methodForSelector:(SEL _Nonnull)aSelector;
+ (IMP _Nonnull)instanceMethodForSelector:(SEL _Nonnull)aSelector;
- (void)doesNotRecognizeSelector:(SEL _Nonnull)aSelector;

- (id _Nullable)forwardingTargetForSelector:(SEL _Nonnull)aSelector;
- (void)forwardInvocation:(NSInvocation * _Nonnull)anInvocation OBJC_SWIFT_UNAVAILABLE("");
- (NSMethodSignature * _Nullable)methodSignatureForSelector:(SEL _Nonnull)aSelector OBJC_SWIFT_UNAVAILABLE("");

+ (NSMethodSignature * _Nullable)instanceMethodSignatureForSelector:(SEL _Nonnull)aSelector OBJC_SWIFT_UNAVAILABLE("");

- (BOOL)allowsWeakReference OBJC_ARC_UNAVAILABLE;
- (BOOL)retainWeakReference OBJC_ARC_UNAVAILABLE;

+ (BOOL)isSubclassOfClass:(Class _Nonnull)aClass;

+ (BOOL)resolveClassMethod:(SEL _Nonnull)sel;
+ (BOOL)resolveInstanceMethod:(SEL _Nonnull)sel;

+ (NSUInteger)hash;
+ (Class _Nonnull)superclass;
+ (Class _Nonnull)class OBJC_SWIFT_UNAVAILABLE("use 'aClass.self' instead");
+ (NSString * _Nonnull)description;
+ (NSString * _Nonnull)debugDescription;

@end

#endif /* __OBJC__ */

#endif /* _OBJC_NSOBJECT_H_ */
