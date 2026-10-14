/*
 * NSConstantString.m — the class of @"..." literals, without Foundation.
 * Layout matches clang's __NSConstantString_tag: isa, flags, bytes, length.
 */
#import <objc/NSObject.h>
#import <objc/runtime.h>
#include <string.h>
#include <limits.h>

@interface __NSCFConstantString : NSObject {
    int _flags;
    const char *_bytes;
    long _length;
}
@end

@implementation __NSCFConstantString

- (unsigned long)length { return (unsigned long)_length; }
- (const char *)UTF8String { return _bytes; }
- (unsigned short)characterAtIndex:(unsigned long)index { return index < (unsigned long)_length ? (unsigned char)_bytes[index] : 0; }
- (BOOL)isEqual:(id)other {
    if (other == self) return YES;
    if (!other || ![other isKindOfClass:[__NSCFConstantString class]]) return NO;
    __NSCFConstantString *o = other;
    return o->_length == _length && memcmp(o->_bytes, _bytes, (size_t)_length) == 0;
}
- (NSUInteger)hash {
    NSUInteger h = 5381;
    for (long i = 0; i < _length; i++) h = h * 33 + (unsigned char)_bytes[i];
    return h;
}
- (id)copyWithZone:(struct _NSZone *)zone { return self; }
- (id)retain { return self; }
- (oneway void)release {}
- (id)autorelease { return self; }
- (NSUInteger)retainCount { return ULONG_MAX; }
- (unsigned long)_cfTypeID { return 7; }   /* CFString */

@end

/* Clang references this symbol as the isa of every string literal. It must be the
 * class object of __NSCFConstantString. */
extern struct objc_class OBJC_CLASS_$___NSCFConstantString;
__attribute__((alias("OBJC_CLASS_$___NSCFConstantString")))
extern struct objc_class __CFConstantStringClassReference;
