/*
 * message.h — message dispatch (objc4 API), libobjc-wasm.
 *
 * WebAssembly type-checks every indirect call, so a universal trampoline such as
 * objc_msgSend cannot exist. Compilers targeting this runtime emit
 *
 *     IMP imp = objc_msgLookup(receiver, selector);   // or objc_msgLookupSuper2
 *     ((RET (*)(id, SEL, ...))imp)(receiver, selector, ...);
 *
 * after a nil check of the receiver. The classic objc_msgSend symbols exist only so
 * that stale references link; calling them aborts with a diagnostic.
 */
#ifndef _OBJC_MESSAGE_H_
#define _OBJC_MESSAGE_H_

#include <objc/objc.h>
#include <objc/runtime.h>

/// Specifies the superclass of an instance.
struct objc_super {
    /// Specifies an instance of a class.
    __unsafe_unretained _Nonnull id receiver;

    /// Specifies the particular superclass of the instance to message.
    __unsafe_unretained _Nonnull Class super_class;
};

/* Method lookup. The returned IMP is never NULL: unknown selectors go through
 * forwarding (`forwardingTargetForSelector:`) and then `doesNotRecognizeSelector:`. */
OBJC_EXPORT IMP _Nonnull objc_msgLookup(id _Nullable receiver, SEL _Nonnull sel);
OBJC_EXPORT IMP _Nonnull objc_msgLookupSuper(struct objc_super * _Nonnull super, SEL _Nonnull sel);
OBJC_EXPORT IMP _Nonnull objc_msgLookupSuper2(struct objc_super * _Nonnull super, SEL _Nonnull sel);

/* Trapping stand-ins for the Darwin trampolines. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wincompatible-library-redeclaration"
OBJC_EXPORT void objc_msgSend(void);
OBJC_EXPORT void objc_msgSendSuper(void);
OBJC_EXPORT void objc_msgSendSuper2(void);
OBJC_EXPORT void objc_msgSend_stret(void);
OBJC_EXPORT void objc_msgSendSuper_stret(void);
OBJC_EXPORT void objc_msgSendSuper2_stret(void);
OBJC_EXPORT void objc_msgSend_fpret(void);
OBJC_EXPORT void objc_msgSend_fp2ret(void);
#pragma clang diagnostic pop

/* Typed sends with up to two object arguments, used by -performSelector: and by
 * runtimes that need to call a method whose signature is known only at run time.
 * The method's type encoding selects the call signature. */
OBJC_EXPORT id _Nullable objc_performSelector(id _Nullable self, SEL _Nonnull sel, unsigned int argc, id _Nullable arg1, id _Nullable arg2);

#endif /* _OBJC_MESSAGE_H_ */
