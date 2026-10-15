// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
/*
 * objc-exception.h — exception entry points (objc4 API), libobjc-wasm.
 * Objective-C exceptions are not supported on this platform: throwing aborts.
 */
#ifndef _OBJC_EXCEPTION_H_
#define _OBJC_EXCEPTION_H_

#include <objc/objc.h>

OBJC_EXPORT void objc_exception_throw(id _Nullable exception) __attribute__((noreturn));
OBJC_EXPORT void objc_exception_rethrow(void) __attribute__((noreturn));
OBJC_EXPORT id _Nullable objc_begin_catch(void * _Nullable exc_buf);
OBJC_EXPORT void objc_end_catch(void);
OBJC_EXPORT void objc_terminate(void) __attribute__((noreturn));

typedef void (*objc_uncaught_exception_handler)(id _Null_unspecified exception);
OBJC_EXPORT objc_uncaught_exception_handler _Nonnull
objc_setUncaughtExceptionHandler(objc_uncaught_exception_handler _Nullable fn);

#endif /* _OBJC_EXCEPTION_H_ */
