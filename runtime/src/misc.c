// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#include "internal.h"
OBJC_ANCHOR_INIT();
#include <objc/objc-exception.h>
#include <stdio.h>
#include <stdarg.h>

void _objc_fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
    abort();
}

void _objc_inform(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* ---- trapping stand-ins for Darwin dispatch trampolines ---- */

#define TRAP_STUB(name)                                                                   \
    void name(void) {                                                                     \
        _objc_fatal("libobjc-wasm: " #name " was called. WebAssembly type-checks indirect "   \
                    "calls, so callers must use objc_msgLookup and call the returned IMP; "    \
                    "recompile the caller for this runtime.");                                 \
    }

TRAP_STUB(objc_msgSend)
TRAP_STUB(objc_msgSendSuper)
TRAP_STUB(objc_msgSendSuper2)
TRAP_STUB(objc_msgSend_stret)
TRAP_STUB(objc_msgSendSuper_stret)
TRAP_STUB(objc_msgSendSuper2_stret)
TRAP_STUB(objc_msgSend_fpret)
TRAP_STUB(objc_msgSend_fp2ret)
TRAP_STUB(_objc_msgForward)
TRAP_STUB(_objc_msgForward_stret)

/* ---- exceptions: unsupported ---- */

static objc_uncaught_exception_handler uncaught_handler;

void objc_exception_throw(id exception) {
    if (uncaught_handler) uncaught_handler(exception);
    _objc_fatal("libobjc-wasm: Objective-C exceptions are not supported on WebAssembly: uncaught exception %p of class %s",
                (void *)exception, exception ? cls_name(exception->isa) : "nil");
}

void objc_exception_rethrow(void) { _objc_fatal("libobjc-wasm: objc_exception_rethrow is not supported"); }
id objc_begin_catch(void *exc_buf) { (void)exc_buf; _objc_fatal("libobjc-wasm: Objective-C @catch is not supported"); }
void objc_end_catch(void) {}
void objc_terminate(void) { _objc_fatal("libobjc-wasm: objc_terminate"); }

objc_uncaught_exception_handler objc_setUncaughtExceptionHandler(objc_uncaught_exception_handler fn) {
    objc_uncaught_exception_handler old = uncaught_handler;
    uncaught_handler = fn;
    return old;
}

/* ---- language support ---- */

static void (*mutation_handler)(id);

void objc_enumerationMutation(id obj) {
    if (mutation_handler) { mutation_handler(obj); return; }
    _objc_fatal("libobjc-wasm: collection <%s: %p> was mutated while being enumerated", cls_name(obj ? obj->isa : Nil), (void *)obj);
}

void objc_setEnumerationMutationHandler(void (*handler)(id)) { mutation_handler = handler; }
void objc_setForwardHandler(void *fwd, void *fwd_stret) { (void)fwd; (void)fwd_stret; }
