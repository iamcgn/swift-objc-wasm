/*
 * NSObject.m — the root class, as objc4 provides it on Darwin.
 * Compiled without ARC; reference counting goes through the runtime's root functions.
 */
#import <objc/NSObject.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import <objc/objc-internal.h>
#import <objc/Protocol.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

@class NSString;
IMP object_getMethodImplementation(id obj, SEL sel);

@implementation NSObject

+ (void)load {}
+ (void)initialize {}

+ (id)self { return (id)self; }
- (id)self { return self; }

+ (Class)class { return self; }
- (Class)class { return object_getClass(self); }

+ (Class)superclass { return class_getSuperclass(self); }
- (Class)superclass { return class_getSuperclass(object_getClass(self)); }

+ (BOOL)isMemberOfClass:(Class)cls { return object_getClass((id)self) == cls; }
- (BOOL)isMemberOfClass:(Class)cls { return object_getClass(self) == cls; }

+ (BOOL)isKindOfClass:(Class)cls {
    for (Class tcls = object_getClass((id)self); tcls; tcls = class_getSuperclass(tcls))
        if (tcls == cls) return YES;
    return NO;
}
- (BOOL)isKindOfClass:(Class)cls {
    for (Class tcls = object_getClass(self); tcls; tcls = class_getSuperclass(tcls))
        if (tcls == cls) return YES;
    return NO;
}

+ (BOOL)isSubclassOfClass:(Class)cls {
    for (Class tcls = self; tcls; tcls = class_getSuperclass(tcls))
        if (tcls == cls) return YES;
    return NO;
}

+ (BOOL)isAncestorOfObject:(NSObject *)obj {
    for (Class tcls = object_getClass(obj); tcls; tcls = class_getSuperclass(tcls))
        if (tcls == self) return YES;
    return NO;
}

+ (BOOL)instancesRespondToSelector:(SEL)sel { return sel ? class_respondsToSelector(self, sel) : NO; }
+ (BOOL)respondsToSelector:(SEL)sel { return sel ? class_respondsToSelector(object_getClass((id)self), sel) : NO; }
- (BOOL)respondsToSelector:(SEL)sel { return sel ? class_respondsToSelector(object_getClass(self), sel) : NO; }

+ (BOOL)conformsToProtocol:(Protocol *)protocol {
    if (!protocol) return NO;
    for (Class tcls = self; tcls; tcls = class_getSuperclass(tcls))
        if (class_conformsToProtocol(tcls, protocol)) return YES;
    return NO;
}
- (BOOL)conformsToProtocol:(Protocol *)protocol {
    if (!protocol) return NO;
    for (Class tcls = object_getClass(self); tcls; tcls = class_getSuperclass(tcls))
        if (class_conformsToProtocol(tcls, protocol)) return YES;
    return NO;
}

+ (NSUInteger)hash { return _objc_rootHash((id)self); }
- (NSUInteger)hash { return _objc_rootHash(self); }

+ (BOOL)isEqual:(id)obj { return obj == (id)self; }
- (BOOL)isEqual:(id)obj { return obj == self; }

+ (BOOL)isFault { return NO; }
- (BOOL)isFault { return NO; }

+ (BOOL)isProxy { return NO; }
- (BOOL)isProxy { return NO; }

+ (IMP)instanceMethodForSelector:(SEL)sel { return sel ? class_getMethodImplementation(self, sel) : NULL; }
+ (IMP)methodForSelector:(SEL)sel { return sel ? object_getMethodImplementation((id)self, sel) : NULL; }
- (IMP)methodForSelector:(SEL)sel { return sel ? object_getMethodImplementation(self, sel) : NULL; }

+ (BOOL)resolveClassMethod:(SEL)sel { return NO; }
+ (BOOL)resolveInstanceMethod:(SEL)sel { return NO; }

+ (void)doesNotRecognizeSelector:(SEL)sel {
    _objc_fatal("+[%s %s]: unrecognized selector sent to class %p", class_getName(self), sel_getName(sel), (void *)self);
}
- (void)doesNotRecognizeSelector:(SEL)sel {
    _objc_fatal("-[%s %s]: unrecognized selector sent to instance %p", object_getClassName(self), sel_getName(sel), (void *)self);
}

+ (id)performSelector:(SEL)sel { return objc_performSelector((id)self, sel, 0, nil, nil); }
+ (id)performSelector:(SEL)sel withObject:(id)obj { return objc_performSelector((id)self, sel, 1, obj, nil); }
+ (id)performSelector:(SEL)sel withObject:(id)obj1 withObject:(id)obj2 { return objc_performSelector((id)self, sel, 2, obj1, obj2); }
- (id)performSelector:(SEL)sel { return objc_performSelector(self, sel, 0, nil, nil); }
- (id)performSelector:(SEL)sel withObject:(id)obj { return objc_performSelector(self, sel, 1, obj, nil); }
- (id)performSelector:(SEL)sel withObject:(id)obj1 withObject:(id)obj2 { return objc_performSelector(self, sel, 2, obj1, obj2); }

+ (id)forwardingTargetForSelector:(SEL)sel { return nil; }
- (id)forwardingTargetForSelector:(SEL)sel { return nil; }

/* NSInvocation-based forwarding does not exist without Foundation. */
- (void)forwardInvocation:(NSInvocation *)invocation { [self doesNotRecognizeSelector:_cmd]; }
- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel { return nil; }
+ (NSMethodSignature *)instanceMethodSignatureForSelector:(SEL)sel { return nil; }

+ (NSString *)description { return nil; }
- (NSString *)description { return nil; }
+ (NSString *)debugDescription { return [self description]; }
- (NSString *)debugDescription { return [self description]; }

+ (id)new { return [[self alloc] init]; }
+ (id)alloc { return _objc_rootAlloc(self); }
+ (id)allocWithZone:(struct _NSZone *)zone { return _objc_rootAllocWithZone(self, (void *)zone); }

+ (id)init { return (id)self; }
- (id)init { return _objc_rootInit(self); }

+ (void)dealloc {}
- (void)dealloc { _objc_rootDealloc(self); }

+ (void)finalize {}
- (void)finalize { _objc_rootFinalize(self); }

+ (struct _NSZone *)zone { return (struct _NSZone *)_objc_rootZone((id)self); }
- (struct _NSZone *)zone { return (struct _NSZone *)_objc_rootZone(self); }

+ (id)copy { return (id)self; }
+ (id)copyWithZone:(struct _NSZone *)zone { return (id)self; }
- (id)copy { return [(id)self copyWithZone:NULL]; }

+ (id)mutableCopy { return (id)self; }
+ (id)mutableCopyWithZone:(struct _NSZone *)zone { return (id)self; }
- (id)mutableCopy { return [(id)self mutableCopyWithZone:NULL]; }

+ (id)retain { return (id)self; }
- (id)retain { return _objc_rootRetain(self); }

+ (BOOL)_tryRetain { return YES; }
- (BOOL)_tryRetain { return _objc_rootTryRetain(self); }

+ (BOOL)_isDeallocating { return NO; }
- (BOOL)_isDeallocating { return _objc_rootIsDeallocating(self); }

+ (BOOL)allowsWeakReference { return YES; }
+ (BOOL)retainWeakReference { return YES; }
- (BOOL)allowsWeakReference { return !_objc_rootIsDeallocating(self); }
- (BOOL)retainWeakReference { return _objc_rootTryRetain(self); }

+ (oneway void)release {}
- (oneway void)release { _objc_rootRelease(self); }

+ (id)autorelease { return (id)self; }
- (id)autorelease { return _objc_rootAutorelease(self); }

+ (NSUInteger)retainCount { return ULONG_MAX; }
- (NSUInteger)retainCount { return _objc_rootRetainCount(self); }

/* CoreFoundation-style type identification used by the Swift runtime's bridging. */
- (unsigned long)_cfTypeID { return 1; }

@end

/* Swift's -performSelector: patch and our NSObject use this helper. */
IMP object_getMethodImplementation(id obj, SEL sel) {
    return class_getMethodImplementation(object_getClass(obj), sel);
}
