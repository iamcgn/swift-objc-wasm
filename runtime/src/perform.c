// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#include "internal.h"
OBJC_ANCHOR_INIT();

/* Call an IMP whose signature is known only from a type encoding, with up to two
 * object-sized arguments. WebAssembly checks indirect call signatures, so every
 * supported combination of return kind and argument count is spelled out. */

enum ret_kind { RK_VOID, RK_I32, RK_I64, RK_F32, RK_F64, RK_UNSUPPORTED };

static enum ret_kind ret_kind_for(char c) {
    switch (c) {
    case 'v': return RK_VOID;
    case '@': case '#': case ':': case '^': case '*': case 'c': case 'C': case 's': case 'S':
    case 'i': case 'I': case 'l': case 'L': case 'B': case '?':
        return RK_I32;
    case 'q': case 'Q': return RK_I64;
    case 'f': return RK_F32;
    case 'd': return RK_F64;
    default: return RK_UNSUPPORTED;
    }
}

id objc_performSelector(id self, SEL sel, unsigned int argc, id a1, id a2) {
    if (!self) return nil;
    IMP imp = objc_msgLookup(self, sel);
    Method m = class_getInstanceMethod(self->isa, sel);
    const char *types = m ? method_getTypeEncoding(m) : NULL;
    enum ret_kind rk = RK_I32;
    if (types && *types) rk = ret_kind_for(*enc_skip_qualifiers(types));
    if (rk == RK_UNSUPPORTED)
        _objc_fatal("libobjc-wasm: -[%s %s] returns a type (%s) that performSelector: cannot call",
                    cls_name(self->isa), sel_getName(sel), types);
    if (argc > 2) argc = 2;

#define CALL_RET(RT, EXPR)                                                                 \
    do {                                                                                   \
        if (argc == 0) { RT r = ((RT (*)(id, SEL))imp)(self, sel); EXPR; }                 \
        else if (argc == 1) { RT r = ((RT (*)(id, SEL, id))imp)(self, sel, a1); EXPR; }    \
        else { RT r = ((RT (*)(id, SEL, id, id))imp)(self, sel, a1, a2); EXPR; }           \
    } while (0)

    switch (rk) {
    case RK_VOID:
        if (argc == 0) ((void (*)(id, SEL))imp)(self, sel);
        else if (argc == 1) ((void (*)(id, SEL, id))imp)(self, sel, a1);
        else ((void (*)(id, SEL, id, id))imp)(self, sel, a1, a2);
        return nil;
    case RK_I32:
        CALL_RET(uintptr_t, return (id)r);
    case RK_I64:
        CALL_RET(uint64_t, return (id)(uintptr_t)r);
    case RK_F32:
        CALL_RET(float, (void)r; return nil);
    case RK_F64:
        CALL_RET(double, (void)r; return nil);
    default:
        return nil;
    }
#undef CALL_RET
}
