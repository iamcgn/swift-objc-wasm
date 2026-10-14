/* Objective-C level runtime test: needs a clang that targets this runtime on wasm. */
#import <objc/NSObject.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import <objc/objc-internal.h>
#import <objc/Protocol.h>
#import <Block/Block.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int deallocs, initializes, loads;

@protocol Greeter
- (int)greet:(int)x;
@optional
- (void)optionalThing;
@end

@interface Base : NSObject <Greeter> {
    int _value;
}
@property (nonatomic) int value;
@property (nonatomic, retain) id child;
+ (instancetype)make;
- (int)greet:(int)x;
- (double)half:(double)d;
- (long long)wide:(long long)w;
- (struct { int a; int b; })pair;
@end

@implementation Base
@synthesize value = _value;
@synthesize child;
+ (void)load { loads++; }
+ (void)initialize { initializes++; }
+ (instancetype)make { return [[[self alloc] init] autorelease]; }
- (instancetype)init { if ((self = [super init])) _value = 10; return self; }
- (int)greet:(int)x { return x + _value; }
- (double)half:(double)d { return d / 2; }
- (long long)wide:(long long)w { return w * 2; }
- (struct { int a; int b; })pair { struct { int a; int b; } p = { 1, 2 }; return p; }
- (void)dealloc { deallocs++; [child release]; [super dealloc]; }
- (NSUInteger)hash { return 42; }
- (BOOL)isEqual:(id)other { return [other isKindOfClass:[Base class]] && [(Base *)other value] == _value; }
@end

@interface Sub : Base
- (int)greet:(int)x;
@end

@implementation Sub
- (int)greet:(int)x { return [super greet:x] * 2; }
- (void)optionalThing {}
@end

@interface Base (Extras)
- (int)extra;
+ (int)classExtra;
@end

@implementation Base (Extras)
- (int)extra { return _value + 1; }
+ (int)classExtra { return 99; }
@end

@interface Weird : NSObject
@end
@implementation Weird
+ (BOOL)resolveInstanceMethod:(SEL)sel {
    if (sel == @selector(dynamic)) { class_addMethod(self, sel, imp_dynamic, "i16@0:8"); return YES; }
    return NO;
}
static int imp_dynamic(id self, SEL _cmd) { return 777; }
@end

int main(void) {
    CHECK(loads == 1);
    CHECK(initializes == 0);
    Base *b = [[Base alloc] init];
    CHECK(initializes == 1);
    CHECK(b != nil && [b isKindOfClass:[NSObject class]] && [b isMemberOfClass:[Base class]]);
    CHECK([b greet:5] == 15);
    CHECK([b half:3.0] == 1.5);
    CHECK([b wide:1LL << 40] == 1LL << 41);
    CHECK([b pair].b == 2);
    CHECK([b extra] == 11);
    CHECK([Base classExtra] == 99);
    CHECK(b.value == 10);
    b.value = 20;
    CHECK(b.value == 20 && [b greet:0] == 20);
    CHECK([b respondsToSelector:@selector(greet:)]);
    CHECK(![b respondsToSelector:@selector(optionalThing)]);
    CHECK([b conformsToProtocol:@protocol(Greeter)]);
    CHECK([Base conformsToProtocol:@protocol(Greeter)]);
    CHECK([b hash] == 42);
    CHECK([b isEqual:[[[Base alloc] init] autorelease]] == NO);   /* 20 != 10 */
    CHECK(strcmp(class_getName([b class]), "Base") == 0);
    CHECK(strcmp(object_getClassName(b), "Base") == 0);
    CHECK(objc_getClass("Sub") == [Sub class]);
    CHECK([[Sub class] superclass] == [Base class]);
    CHECK(protocol_isEqual(@protocol(Greeter), objc_getProtocol("Greeter")));

    Sub *s = [Sub make];
    CHECK(initializes == 2);
    CHECK([s greet:1] == 22);
    CHECK([s respondsToSelector:@selector(optionalThing)]);
    CHECK([s performSelector:@selector(greet:) withObject:(id)4] == (id)28);
    CHECK([s performSelector:@selector(optionalThing)] == nil);

    /* nil receivers */
    Base *n = nil;
    CHECK([n greet:3] == 0);
    CHECK([n half:2.0] == 0.0);
    CHECK([n pair].a == 0);
    CHECK([n child] == nil);

    /* dynamic resolution */
    Weird *w = [[Weird alloc] init];
    CHECK([w respondsToSelector:@selector(dynamic)]);
    CHECK((int)(intptr_t)[w performSelector:@selector(dynamic)] == 777);
    [w release];

    /* retain/release, properties, weak */
    Base *child = [[Base alloc] init];
    CHECK([child retainCount] == 1);
    b.child = child;
    CHECK([child retainCount] == 2);
    __weak Base *weak = nil;
    objc_storeWeak((id *)&weak, child);
    CHECK(weak == child);
    [child release];
    CHECK(deallocs == 0);
    [b release];
    CHECK(deallocs == 2);
    CHECK(weak == nil);

    /* constant strings and blocks */
    id str = @"hello";
    CHECK([str length] == 5 && strcmp([str UTF8String], "hello") == 0);
    CHECK([str isEqual:@"hello"] && ![str isEqual:@"world"]);
    int captured = 5;
    int (^block)(int) = ^(int x) { return x + captured; };
    int (^copied)(int) = Block_copy(block);
    CHECK(copied(1) == 6);
    Block_release(copied);

    /* class pairs from code */
    Class dyn = objc_allocateClassPair([Sub class], "Dyn", 0);
    objc_registerClassPair(dyn);
    id d = [[dyn alloc] init];
    CHECK([d greet:1] == 22 && [d isKindOfClass:[Base class]]);
    [d release];

    void *pool = objc_autoreleasePoolPush();
    [Base make];
    objc_autoreleasePoolPop(pool);

    if (failures) { printf("%d failures\n", failures); return 1; }
    printf("test_objc: ok\n");
    return 0;
}
