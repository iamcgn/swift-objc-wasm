# Plan: Objective-C runtime + interop-enabled Swift toolchain for WebAssembly

## Context

`~/swiftui-to-html` (github.com/iamcgn/swiftui-web) must build unmodified upstream iOS apps for wasm. The pinned SwiftUI-2048 app fails on one file, `Apps/upstream/SwiftUI-2048/SwiftUI2048/AppDelegate.swift`: `@UIApplicationMain`, `class AppDelegate: UIResponder, UIApplicationDelegate`, `@objc func newGame(_:)`, `#selector(newGame(_:))`, `UIKeyCommand(input:modifierFlags:action:)`, `override func buildMenu(with: UIMenuBuilder)`. `Docs/decisions/0021-real-apps.md:127-151` keeps the barrier open and names route 1: an ObjC runtime for wasm plus a Swift toolchain built with interop on. The acceptance standard forbids touching upstream sources.

The swift.org thread "Cross-platform Objective-C Interop with GNUStep feasibility discussion" (and its follow-up proposal) lays out three models; the user wants the one that keeps the **Darwin model** (every Swift class is an ObjC class, NSObject root, same stdlib bridging paths, `-enable-objc-interop`), not the "foreign class" model.

### Verified facts (this machine: Swift 6.3.3 via swiftly, SDK `swift-6.3.3-RELEASE_wasm`, `wasm32-unknown-wasip1`)

- `@objc` → "Objective-C interoperability is disabled"; `#selector` → "can only be used with the Objective-C runtime". `EnableObjCInterop` defaults to `isOSDarwin() && !Embedded` (`lib/Frontend/CompilerInvocation.cpp`).
- `-Xfrontend -enable-objc-interop` aborts swift-frontend even on `-typecheck`: `report_fatal_error("Objective-C support is unimplemented for object file format")`. That string is **clang's** `CGObjCCommonMac::GetSectionName` (`clang/lib/CodeGen/CGObjCMac.cpp`), whose `Wasm` case is fatal while `ELF` returns `Section.substr(2)`. Swift's own `IRGenModule::GetObjCSectionName` (`lib/IRGen/GenDecl.cpp`) at tag `swift-6.3.3-RELEASE` already treats Wasm like ELF, and `emitGlobalLists` emits ObjC lists whenever interop is on.
- clang with `-fobjc-runtime=ios-13` on `x86_64-unknown-linux-gnu` emits Apple objc4-format metadata into plain sections `objc_classlist`, `objc_classrefs`, `objc_selrefs` and calls `objc_msgSend`. Apple-ABI codegen on non-Mach-O already exists; wasm only needs to join the ELF case.
- `-fobjc-runtime=gnustep-2.x` is rejected by the 6.3.3 clang driver on wasm (upstream merged it Aug 2026, llvm/llvm-project#215562, not in swiftlang's clang-21 fork). The GNUstep route also has an unresolved blocker: Swift embeds a 5-word objc4 `objc_class` in every class metadata; libobjc2's class is ~20 words (gnustep/libobjc2#306).
- Message sends: Swift IRGen (`lib/IRGen/GenObjC.cpp`, `getObjCMethodCallee`) and clang `EmitMessageSend` bitcast `objc_msgSend`/`objc_msgSendSuper2` to the message signature. On wasm `call_indirect` type-checks the callee, so that traps. Dispatch must become `imp = objc_msgLookup(receiver, sel); imp(receiver, sel, args...)` (objc4 already exports `objc_msgLookup`/`objc_msgLookupSuper2` under those names).
- Bundled WASI sysroot: single-threaded wasi-libc (no pthread stubs), libc++abi without exceptions. `_Block_copy` is defined only in `libCoreFoundation.a` / `lib_CFXMLInterface.a`, **not** in `libswiftCore.a`; the project's wasm build links neither (WebFoundation uses FoundationEssentials only).
- wasm stdlib build: `utils/build-script --build-wasm-stdlib` → `utils/swift_build_support/swift_build_support/products/{wasmstdlib,wasmswiftsdk,wasisysroot}.py`; preset `wasm_stdlib,macos` at `utils/build-presets.ini:3362`. Interop switch: top-level `CMakeLists.txt:699-709` option `SWIFT_STDLIB_ENABLE_OBJC_INTEROP` (default TRUE only for Darwin SDK names); `stdlib/cmake/modules/SwiftSource.cmake:627` adds `-disable-objc-interop` when off (nothing adds `-enable-objc-interop` when on); `cmake/modules/AddSwift.cmake:385` only ever adds `-DSWIFT_OBJC_INTEROP=0`. ObjC++ runtime sources: `stdlib/public/runtime/CMakeLists.txt:26-31` (`ErrorObject.mm`, `SwiftObject.mm`, `SwiftValue.mm`, `ReflectionMirrorObjC.mm`, `ObjCRuntimeGetImageNameFromClass.mm`) plus `stdlib/public/stubs/{SwiftNativeNSXXXBase.mm.gyb, SwiftNativeNSObject.mm, FoundationHelpers.mm, OptionalBridgingHelper.mm}`; these `#import <Foundation/Foundation.h>` / `<CoreFoundation/CoreFoundation.h>` / `<TargetConditionals.h>` / `<objc/objc-internal.h>` / `malloc/malloc.h`, but their uses are thin (`objc_lookUpClass("NSString"...)`, `CFStringGetTypeID` via `dlsym`).
- The `ObjectiveC` Swift overlay is gone from swiftlang/swift; last copy is `stdlib/public/Darwin/ObjectiveC/ObjectiveC.swift` at tag `swift-5.2`/`5.3-RELEASE` (~250 lines: `ObjCBool`, `Selector`, `NSZone`, `autoreleasepool`, `NSObject: Equatable, Hashable`). `#selector` maps `SEL` to `ObjectiveC.Selector`; `@objc` additionally requires a module literally named `Foundation` to be loaded (or `-disable-objc-attr-requires-foundation-module`).
- Host: Apple Silicon, 10 cores, 16 GB RAM, 94 GB free. cmake present; ninja not installed. WasmKit 0.1.6 ships in the toolchain (`usr/bin/wasmkit`) and is the test runner. `gh` authenticated. Tags `swift-6.3.3-RELEASE` exist in swiftlang/swift and swiftlang/llvm-project.

## Approach

Keep the Darwin model end to end and keep both compilers emitting **Apple objc4-format metadata**. The new piece is **libobjc-wasm**, a small ObjC runtime for wasm that implements the objc4 ABI (class_t/class_ro_t/class_rw_t, method/ivar/protocol/category lists, selector refs, image info) and the objc4 API subset that Swift's runtime/stdlib and clang codegen need, plus NSObject (as objc4 ships it) and a "Foundation-lite" just large enough for the Swift runtime's thin references. Static-link only, single-threaded, raw `isa`, no tagged pointers, no exceptions. Compiler patches stay tiny (section names, lookup-based dispatch, retain flags), which matters because they must be rebased on every Swift release.

Why not GNUstep/libobjc2: needs a new GNUstep metadata emitter in Swift IRGen, the class-layout conflict (#306) is unresolved, and the 6.3.3 clang fork lacks the wasm GNUstep driver support.

**Scoped out of v1:** ObjC exceptions (`objc_exception_throw` aborts), tagged pointers, non-pointer isa, KVO/KVC, `NSInvocation`, categories on Swift classes from ObjC source, `objc_msgSend` from JS/C, dynamic libraries, threads/`objc_sync` semantics, NSString/NSArray/NSDictionary/NSError bridging (`String as NSString` is a documented compile error; v1.5 adds `String: _ObjectiveCBridgeable` in WebFoundation over Foundation-lite's `NSString`), `Foundation`/`XCTest`/`Testing` in the new SDK, Linux-host toolset (CI follow-up).

## Deliverables and layout

This repository (`swift-objc-wasm`):
```
README.md
runtime/                       libobjc-wasm (C + a few .m, CMake, tests on WasmKit)
patches/swift/*.patch          git format-patch vs swift-6.3.3-RELEASE
patches/llvm-project/*.patch   same, vs swiftlang/llvm-project swift-6.3.3-RELEASE
toolchain/{checkout.sh,build-host-compiler.sh,build-wasm-stdlib.sh,rebase.sh}
sdk/{assemble-artifactbundle.sh,toolset.json,swift-sdk.json,info.json}
tests/{wasm-sections/,e2e/}
```
Sources at `~/Development/swift-source/` (standard `update-checkout` layout). Output: artifactbundle `swift-6.3.3-RELEASE_wasm-objc` whose `toolset.json` points at the patched `swift-frontend`/`swiftc`/`clang`/`wasm-ld`, so the stock swiftly toolchain stays untouched.

## Phase 0 — Workspace, checkouts, baseline compiler build (2–3 days)

1. `brew install ninja`. `git init` the repo above.
2. `mkdir ~/Development/swift-source && cd $_ && git clone https://github.com/swiftlang/swift && ./swift/utils/update-checkout --clone --tag swift-6.3.3-RELEASE` (~10 GB).
3. Host compiler build, pinned to the same SDK `scripts/env.sh` uses (`DEVELOPER_DIR=/Library/Developer/CommandLineTools`, `SDKROOT=.../MacOSX26.sdk`):
   ```
   ./swift/utils/build-script --release --assertions --build-subdir objc-wasm \
     --skip-ios --skip-tvos --skip-watchos --skip-xros --skip-build-benchmarks \
     --skip-test-swift --skip-test-cmark \
     --build-swift-dynamic-stdlib=0 --build-swift-dynamic-sdk-overlay=0 \
     --build-swift-static-stdlib=0 --build-swift-static-sdk-overlay=0 \
     --llvm-targets-to-build="AArch64;X86;WebAssembly" \
     --extra-cmake-options="-DLLVM_PARALLEL_LINK_JOBS=1 -DSWIFT_PARALLEL_LINK_JOBS=1" --jobs 6
   ```
   Expect 2–4 h; link jobs capped for 16 GB. Outputs `build/objc-wasm/llvm-macosx-arm64/bin/clang`, `build/objc-wasm/swift-macosx-arm64/bin/swift-frontend`, `lib/swift/host/compiler/*.dylib`. Later iterations use `ninja -C ... swift-frontend` / `clang`.
4. Link experiment `tests/wasm-sections/`: two C objects each placing a `__attribute__((used))` pointer in section `objc_classlist`, a third declaring the empty `"R"` section + `extern __start_objc_classlist/__stop_objc_classlist` (copy the idiom from `stdlib/public/runtime/SwiftRT-ELF-WASM.cpp`), link with `wasm-ld --gc-sections`, run on WasmKit, expect count 2. This gates the metadata-discovery design.

Exit: unpatched build reproduces stock `-emit-ir` for a wasm Swift file; experiment prints 2.

## Phase 1 — Compiler patches (4 days)

### clang (`llvm-project/clang/lib/CodeGen/CGObjCMac.cpp`)
1. `CGObjCCommonMac::GetSectionName`: move `case llvm::Triple::Wasm:` next to `ELF` (`return Section.substr(2).str()`). This alone removes the fatal error for clang and swift-frontend.
2. `CGObjCNonFragileABIMac::AddModuleClassList` and the other `setSection(GetSectionName("__objc_*list"...))` sites (`protolist`, `imageinfo`, `nlclslist`, `catlist`, `catlist2`, `nlcatlist`): on `isOSBinFormatWasm()` use `CGM.addUsedGlobal` instead of `addCompilerUsedGlobal` (only `llvm.used` gets `WASM_SEG_FLAG_RETAIN`; Swift's `emitGlobalList` already uses `addUsedGlobal`).
3. `CGObjCCommonMac::EmitMessageSend`: on Wasm, no messenger bitcast. After `GetSelector`, emit `imp = objc_msgLookup(receiver, sel)` (or `objc_msgLookupSuper2(objc_super*, sel)` for super), bitcast `imp` to `MSI.MessengerType`, call it. Force `RequiresNullCheck` when the receiver can be nil so `NullReturnState` zero-fills and skips the lookup (a generic nil IMP can't match arbitrary signatures). Add `getMessageLookupFn`/`getMessageLookupSuperFn` to `ObjCNonFragileABITypesHelper`. Ignore stret/fpret selection on Wasm.
4. `isVTableDispatchedSelector` → `false` on Wasm (no `objc_msgSend_fixup` path).
5. Pass `-fobjc-runtime=ios-7.0` explicitly everywhere (runtime CMake, toolset) rather than patching the WebAssembly driver default; ClangImporter already uses `ios-7.0` for non-Darwin.
6. Verify with `-S -emit-llvm` on `wasm32-unknown-wasip1 -fobjc-runtime=ios-7.0`: sections `objc_classlist/objc_selrefs/objc_classrefs/objc_imageinfo/objc_protolist`, lists in `llvm.used`, every send is `objc_msgLookup` + indirect call, nil-check blocks present.

### Swift
1. `include/swift/Runtime/RuntimeFunctions.def`: add `ObjCMsgLookup` (`objc_msgLookup`, returns `PtrTy`, args `(ObjCPtrTy, ObjCSELTy)`), `ObjCMsgLookupSuper` (Peer) and `ObjCMsgLookupSuper2` (Super) with `(ObjCSuperPtrTy, ObjCSELTy)`.
2. `lib/IRGen/GenObjC.cpp` `getObjCMethodCallee`: on `Triple.isOSBinFormatWasm()`, compute receiver and selector, call the matching lookup, bitcast the IMP to the method signature's pointer type, build `FunctionPointer(Kind::Function, impValue, PointerAuthInfo(), sig)` instead of `forDirect(messenger)`. No stret branch; no nil check (SIL guarantees non-null `self` for `objc_method` applies).
3. `lib/IRGen/SwiftTargetInfo.cpp` `configureWasm32`: `ObjCUseStret = false` (reserved-bit masks already 0 → no tagged pointers / isa masking).
4. Leave the `EnableObjCInterop` default; the SDK's toolset passes `-Xfrontend -enable-objc-interop`.
5. Confirm `-lobjc` appears in a wasm object's autolink entries (`IRGenModule.cpp` registers `LinkLibrary{"objc"}` when interop is on).
6. Verify: `swift-frontend -target wasm32-unknown-wasip1 -enable-objc-interop -emit-ir -parse-stdlib` on a class + `@objc` method + send shows `objc_classlist` globals in `llvm.used`, `objc_msgLookup` calls, thunks with `(ptr self, ptr _cmd, ...)`; `-typecheck` no longer aborts. Run `lit` on `test/IRGen/objc_*.swift` for Darwin to prove no regression.

Exit: both compilers produce wasm objects for ObjC and `@objc` Swift and `wasm-ld` links them against a stub `libobjc.a`.

## Phase 2 — libobjc-wasm runtime (10–15 days; overlaps Phase 1 once the clang patch lands)

Layout: `runtime/include/objc/{objc.h,runtime.h,message.h,NSObject.h,NSObjCRuntime.h,objc-api.h,objc-internal.h,module.modulemap}`; `runtime/include/_stdlib_shims/{Foundation/Foundation.h,CoreFoundation/CoreFoundation.h,TargetConditionals.h,malloc/malloc.h}` (stdlib build only, not installed); `runtime/src/{abi.h,image.c,class.c,selector.c,dispatch.c,protocol.c,category.c,refcount.c,weak.c,assoc.c,autorelease.c,hooks.c,perform.c}`; `runtime/src/foundation-lite/{NSObject.m,NSString.m,NSConstantString.m,NSNull.m,CFLite.c}`; `runtime/src/blocks/` (from swift-corelibs-foundation `Sources/BlocksRuntime`, every export `__attribute__((weak))` so `-lCoreFoundation` doesn't collide); `runtime/tests/*.m` + `run-tests.sh` (WasmKit); `runtime/cmake/wasm32-wasip1.toolchain.cmake` (patched clang + the artifactbundle's `WASI.sdk`, `-fobjc-runtime=ios-7.0`, MRR).

Work items:
1. **ABI structs** (`abi.h`): `objc_class {isa, superclass, cache, vtable, data}` with `data` = `class_ro_t*` | flags (`FAST_IS_SWIFT_STABLE = 1<<1`; replaced by `class_rw_t*` on realization like objc4), `class_ro_t` (10-field 32-bit layout clang emits), `method_t {SEL, types, IMP}` + `method_list_t` (absolute entries only; assert on the relative-list flag `0x80000000` — neither clang's non-Darwin `ios-7.0` nor Swift IRGen emits relative lists here), `ivar_t`/`ivar_list_t`, `property_t`, `protocol_t`, `category_t`, `objc_super`, `objc_image_info`. Export `_objc_empty_cache` and `_objc_empty_vtable` as real globals.
2. **Image loading** (`image.c`): constructor at priority 101 (must precede Swift's `swift_image_constructor`; verify order) walks `__start_/__stop_` of `objc_classlist`, `objc_nlclslist`, `objc_catlist`, `objc_protolist`, `objc_selrefs`, `objc_classrefs`, `objc_superrefs`, `objc_imageinfo`, `objc_stublist`, `objc_clsrolist`, `objc_protorefs`; declares the empty `"R"` sections as `SwiftRT-ELF-WASM.cpp` does. Interns every `objc_selrefs` slot and `method_t.name` to the canonical `SEL` (unique `const char*`, objc4 semantics) so `#selector` and thunks agree. Non-lazy classes realized eagerly, others on first lookup/message.
3. **Class realization** (`class.c`): `class_rw_t` with flattened method lists (categories win), ivar offset slide, metaclass realization, `objc_readClassPair`, `_objc_realizeClassFromSwift`, `objc_loadClassref` + stub classes (`isa` 1..15 → initializer), `_objc_setClassCopyFixupHandler`, `objc_allocateClassPair/registerClassPair/disposeClassPair`, `class_add{Method,Ivar,Protocol,Property}`, `class_setSuperclass` (for `SwiftNativeNS*Base +load`), the `class_*`/`object_*`/`objc_getClass`-family getters, `objc_constructInstance/destructInstance`, `class_createInstance`, `objc_allocWithZone`, `_objc_supportsLazyRealization`, `objc_isUniquelyReferenced`. **Every SPI the Swift runtime probes with `&sym` must be a strong definition** (`objc_setHook_getClass`, `objc_setHook_getImageName`, `objc_setHook_lazyClassNamer`, `_objc_realizeClassFromSwift`, `_objc_supportsLazyRealization`, `objc_isUniquelyReferenced`): an undefined weak function on wasm resolves to a non-null trap stub.
4. **Dispatch** (`dispatch.c`): `objc_msgLookup`, `objc_msgLookupSuper`, `objc_msgLookupSuper2`; per-class open-addressing cache keyed by SEL, flushed on method add / category attach; unknown selector → `forwardingTargetForSelector:` then `doesNotRecognizeSelector:` → abort naming class and selector. Export `objc_msgSend`, `objc_msgSendSuper2`, `objc_msgSend_stret` as **trapping symbols with a clear message** so any leftover reference is diagnosable. `method_*`, `class_replaceMethod`.
5. **Typed perform** (`perform.c`): `id objc_performSelector(id, SEL, unsigned argc, id a1, id a2)` reads the type encoding and calls with the right wasm signature (`v`/`@`/`#`/`c`/`B`/`i`/`l`/`q`/`f`/`d` returns, `@` args). Used by `-[NSObject performSelector:...]`, the `SwiftObject.mm` patch, and UIKitWeb's target-action. This is what makes a Void action like `newGame(_:)` callable via `perform`.
6. **Refcounting** (`refcount.c`): full ARC entry-point set (`objc_retain/release/autorelease/retainAutorelease*/autoreleaseReturnValue/unsafeClaimAutoreleasedReturnValue/storeStrong/retainBlock/alloc/allocWithZone/_objc_root*`). `FAST_IS_SWIFT_STABLE` set → forward to `swift_retain/swift_release` (extern, from libswiftCore.a); else side-table refcount keyed by address; dealloc → `objc_destructInstance` + `free`. `objc_setProperty*/getProperty/copyStruct/enumerationMutation`, `objc_sync_enter/exit` no-ops.
7. **Weak / assoc / pools** (`weak.c`, `assoc.c`, `autorelease.c`, `hooks.c`): weak side table (`objc_initWeak/storeWeak/loadWeak/loadWeakRetained/destroyWeak/copyWeak/moveWeak/*OrNil`), associated objects, a page-stack autorelease pool, `objc_setHook_*`.
8. **Protocols** (`protocol.c`): registration from `objc_protolist`, `objc_getProtocol/allocateProtocol/registerProtocol`, `protocol_*`, `class_conformsToProtocol` including category protocols.
9. **Foundation-lite**: `NSObject` root class + protocol per objc4 `NSObject.mm` (alloc/init/new/copy/dealloc/retain/release/autorelease/retainCount/isEqual/hash/class/superclass/isKindOfClass/isMemberOfClass/respondsToSelector/conformsToProtocol/methodForSelector/performSelector family via `objc_performSelector`/description/debugDescription/doesNotRecognizeSelector/forwardingTargetForSelector/zone/_isDeallocating/+initialize/+load); `NSZone` + `malloc_default_zone` stub; `NSCopying`/`NSMutableCopying`; minimal `NSString` (UTF-8: `length`, `characterAtIndex:`, `UTF8String`, `isEqual:`, `hash`, `copyWithZone:`, `initWithUTF8String:`, `stringWithUTF8String:`); `__NSCFConstantString` + `__CFConstantStringClassReference` for `@""` literals; `NSNull`; `CFLite.c` exporting `CFStringGetTypeID/CFGetTypeID/CFStringHashNSString/CFStringHashCString`. No NSArray/NSDictionary/NSError (the Swift runtime's `objc_lookUpClass` of them returns nil and degrades gracefully).
10. **Headers**: objc4 API surface the Swift runtime and ClangImporter need (`BOOL` as `signed char`, `OBJC_BOOL_IS_BOOL 0`, nullability/`NS_ENUM`/`instancetype` macros, `objc-internal.h` with the SPIs and `OBJC_*_DEFINED` macros), `module.modulemap` `module ObjectiveC { ... }`. Test early: `swift-frontend -typecheck -enable-objc-interop -I include` of `import ObjectiveC`.
11. **Tests** on WasmKit: class registration across two objects; categories; selector uniquing; sends with int/float/struct/sret returns; super sends; nil receiver zero-fill; `respondsToSelector`; `performSelector` on a `void` method; weak/assoc/autorelease; protocol conformance; `+load/+initialize` order; `objc_allocateClassPair`; stub-class `objc_loadClassref`; a `--trace-dispatch` env knob.

Exit: all runtime tests pass; `nm libobjc.a` exports every symbol referenced by the Swift runtime/stdlib (enumerate by grepping `objc_`/`class_`/`object_`/`sel_`/`protocol_` across `stdlib/public/runtime`, `stdlib/public/stubs`, `stdlib/public/core`, `lib/IRGen` of the checkout).

## Phase 3 — Rebuild the wasm stdlib with interop; ObjectiveC overlay; artifactbundle (5–8 days)

Swift-tree patches (`patches/swift/`):
1. `cmake/modules/AddSwift.cmake:385`: emit `-DSWIFT_OBJC_INTEROP=1` when `SWIFT_STDLIB_ENABLE_OBJC_INTEROP` (it is otherwise inferred from `__APPLE__`).
2. `stdlib/cmake/modules/AddSwiftStdlib.cmake` (`_add_target_variant_c_compile_flags` / `_add_target_variant_swift_compile_flags`): for `SDK STREQUAL "WASI"` with interop on, C/ObjC flags `-fobjc-runtime=ios-7.0 -DSWIFT_CLASS_IS_SWIFT_MASK=2ULL -I${SWIFT_WASI_OBJC_INCLUDE_DIR} -I${SWIFT_WASI_OBJC_SHIMS_DIR}` and Swift flags `-Xfrontend -enable-objc-interop`. (IRGen sets `class_ro_t` flag bit 2 as the is-Swift bit off-Darwin; the runtime's non-Apple default mask must be checked and pinned explicitly.) New cache vars `SWIFT_WASI_OBJC_INCLUDE_DIR`, `SWIFT_WASI_OBJC_SHIMS_DIR`, `SWIFT_WASI_OBJC_LIB`.
3. `stdlib/public/runtime/SwiftObject.mm` `-performSelector:` family (casts `objc_msgSend`): `#if defined(__wasm__)` → `objc_performSelector(...)`.
4. `stdlib/public/stubs/FoundationHelpers.mm`: `#if defined(__wasm__)` bind `CFGetTypeID` etc. directly (wasi-libc `dlsym` returns NULL).
5. `ObjCRuntimeGetImageNameFromClass.mm`: expect to compile with the `TargetConditionals.h` shim; otherwise guard with `#if __APPLE__`.
6. New `stdlib/public/ObjectiveC/{CMakeLists.txt,ObjectiveC.swift}`: port of the `swift-5.3-RELEASE` overlay (`ObjCBool` over `Int8`, `Selector`, `NSZone`, `autoreleasepool`, `YES/NO`, `_convertBoolToObjCBool/_convertObjCBoolToBool`, `NSObject: Equatable, Hashable`; drop `CVarArg`; add `NSObject: CustomStringConvertible` via `NSString.UTF8String` since no Foundation). Built `IS_SDK_OVERLAY` only when `SWIFT_STDLIB_ENABLE_OBJC_INTEROP AND NOT APPLE`; hook into `stdlib/public/CMakeLists.txt`.
7. `utils/swift_build_support/swift_build_support/products/wasmstdlib.py`: define `SWIFT_STDLIB_ENABLE_OBJC_INTEROP:BOOL=TRUE` and the `SWIFT_WASI_OBJC_*` paths from env `SWIFT_OBJC_WASM_RUNTIME`.
8. `stdlib/public/Resources/wasi/static-executable-args.lnk`: add `-lobjc` (belt and braces).

Build: Phase 0 command + `--build-wasm-stdlib --skip-test-wasm-stdlib`. If the `swift-sdk-generator` route (`wasmswiftsdk.py`) fights back, fall back to `sdk/assemble-artifactbundle.sh`: copy the stock bundle to `swift-6.3.3-RELEASE_wasm-objc.artifactbundle`, replace `swift_static/wasi/*` with the new install, add `libobjc.a` + `objc/` headers + modulemap, rebuild **FoundationEssentials/_FoundationCShims/_FoundationCollections** against the new stdlib (`_build_foundation` in `wasmswiftsdk.py` shows the CMake invocation; swift-foundation has no `_runtime(_ObjC)` branches), and **drop** `libFoundation.a`, FoundationXML/Internationalization, CoreFoundation, XCTest and Testing from v1 (ABI-mismatched with the new stdlib; the project's wasm path uses none of them).

Toolset: `swift.xctoolchain/usr/bin/` gets **copies** (not symlinks; swift-driver and swift-frontend locate siblings by real path) of patched `swift-frontend` (+ hardlinks `swift`, `swift-autolink-extract`, `swift-symbolgraph-extract`), stock `swift-driver` as `swiftc`, patched `clang`/`clang++`, stock `wasm-ld`, `lld`, `llvm-ar`, `llvm-ranlib`; `usr/lib/swift/host` → the build's compiler libs plus `plugins/` from the stock toolchain (macro plugins; same swift-syntax tag). `toolset.json`:
```json
{"schemaVersion":"1.0","rootPath":"swift.xctoolchain/usr/bin",
 "swiftCompiler":{"path":"swiftc","extraCLIOptions":["-static-stdlib","-Xfrontend","-enable-objc-interop"]},
 "cCompiler":{"path":"clang","extraCLIOptions":["-fobjc-runtime=ios-7.0"]},
 "cxxCompiler":{"path":"clang++","extraCLIOptions":["-fobjc-runtime=ios-7.0"]},
 "linker":{"path":"wasm-ld"}}
```
`swift sdk install <bundle>`; `swift sdk list` shows `swift-6.3.3-RELEASE_wasm-objc`.

Exit: `swift build --swift-sdk swift-6.3.3-RELEASE_wasm-objc` of a Foundation-free hello world links and runs on WasmKit; `Swift.swiftinterface` flags show `-enable-objc-interop`; `nm libswiftCore.a` shows `SwiftObject`, `__SwiftValue` metaclasses; `@Observable` expands under the new SDK.

## Phase 4 — End-to-end Swift tests (3 days)

`tests/e2e/` SwiftPM packages, `swift build --swift-sdk ... && wasmkit run`, asserting stdout: `@objc` method + `#selector` + `perform(_:with:)` on Void and `AnyObject`-returning methods; `responds(to:)`; `NSObject` subclass overriding `isEqual`/`hash`/`description`; `@objc protocol` with optional requirements and `AnyObject` dynamic lookup; `as? NSObjectProtocol`, `is NSObject`; `@objc enum`; `Selector("newGame:") == #selector(...)`; `NSStringFromClass` via `class_getName`; Swift class instantiated from ObjC (`objc_getClass("App.Foo")` → alloc/init) and the reverse; weak refs to ObjC-class objects; `@convention(block)` copied via `_Block_copy`; one package linking `-lCoreFoundation` to prove the weak Blocks runtime doesn't collide; `String as NSString` → documented compile error. Curated pass list from `swift/test/Interpreter/objc_*` that need no Foundation, under the wasm lit config.

## Phase 5 — swiftui-to-html changes (4–6 days)

1. `scripts/build-wasm.sh` and `scripts/check-apps.sh`: SDK name `swift-$(cat .swift-version)-RELEASE_wasm-objc` (env override `SWIFTUIWEB_WASM_SDK`). New `Docs/decisions/0023-objc-runtime-wasm.md`; update the SwiftUI-2048 row in `0021-real-apps.md`.
2. `Packages/WebGraphics/Sources/WebFoundation/Responder.swift`: the stand-in `Selector` (56–65) is already `#if !canImport(ObjectiveC)`; gate the `NSObjectProtocol`/`NSObject` block (118–145) as `#if os(WASI) && !canImport(ObjectiveC)`; add `#if canImport(ObjectiveC) @_exported import ObjectiveC` at module top (as Apple's Foundation re-exports it, so `#selector` resolves in files that only `import UIKit`). Add `NSStringFromClass`/`NSClassFromString` over `class_getName`/`objc_getClass`. Audit the 10 `canImport(ObjectiveC)` sites (`Lifecycle.swift:128`, `UIPasteboard.swift:201`, `DragDrop.swift:41/68/130`, `CommandNodes.swift:106/117`, `DateFormattingTests.swift:179/202`, `LinuxCompatibilityTests.swift:81`): where they mean "Apple's Foundation", rewrite as `canImport(Darwin)`.
3. `Packages/Shadows/Sources/Foundation`: `@_exported import ObjectiveC` on wasm so an app's `import Foundation` satisfies `@objc`'s module requirement (fallback: `-Xfrontend -disable-objc-attr-requires-foundation-module` in the toolset).
4. `Packages/UIKitWeb/Sources/UIKitWebCore/Events/Keyboard.swift`: `UIKeyCommand` gains `action: Selector?` and the Apple initializers `init(input:modifierFlags:action: Selector)` and the full `init(title:image:action:input:...)`; keep the closure form for fixtures. Dispatch: walk the responder chain (first responder → ... → window → application → delegate) with `responds(to:)`, invoke via `perform(selector, with: sender)` (`UIApplication.sendAction(_:to:from:for:)` semantics).
5. `Views/UIResponder.swift`: `open class UIResponder: NSObject` (today a root class; `AppDelegate: UIResponder` with `@objc` members needs an ObjC-visible root); add `open func buildMenu(with:)`, `canPerformAction(_:withSender:)`, `target(forAction:withSender:)`. New `Containers/MenuBuilder.swift`: `UIMenuBuilder` protocol + concrete builder (`remove(menu:)`, `replaceChildren(ofMenu:from:)`, `insertChild`, `menu(for:)`), `UIMenu.Identifier` statics `.file/.edit/.format/.view/.window/.help`. Build the menu once at launch (`UIKitScene.launchApplication`) by calling `buildMenu(with:)` on the delegate when it is a `UIResponder`; feed the built key commands into the existing matching in `Keyboard.swift`.
6. `App/UIWindow.swift:213` `UIApplicationDelegate`: drop the `init()` requirement. Add `UIApplicationMain(_:_:_:_:)` in `Packages/UIKitWeb/Sources/UIKit/UIKit.swift` (what `@UIApplicationMain` expands to), resolving the delegate class with `NSClassFromString` and instantiating via `(cls as! NSObject.Type).init()`; keep `UIApplicationDelegate.main()` for `@main` apps; both route into `UIKitLaunch.launch`.
7. `withTransaction`/`withAnimation` isolation (sw-isolation-fidelity, `0021-real-apps.md:173-177`) is on the acceptance path: upstream's nonisolated `@objc func newGame` calls `withTransaction`. Make them nonisolated as Apple's are.
8. Verify: `scripts/check-apps.sh` (upstream hashes intact) then `scripts/build-wasm.sh Apps --debug --product SwiftUI2048`; ⌘N starts a new game in the Playwright harness; existing wasm examples still build; macOS native builds unaffected.

## Phase 6 — Upstream and maintenance (2–3 days + ongoing)

- File swiftlang issues/PRs: clang `GetSectionName` Wasm + lookup dispatch + `llvm.used` lists (upstream merged GNUstep-on-wasm in Aug 2026, so there is appetite); Swift `GenObjC.cpp` wasm dispatch + `SwiftTargetInfo`; the stdlib build/runtime hygiene patches (`-DSWIFT_OBJC_INTEROP=1` explicit, `SWIFT_CLASS_IS_SWIFT_MASK`, `performSelector` via lookup, dlsym-free `FoundationHelpers`); the non-Darwin `ObjectiveC` overlay (hardest to land; keep in-tree). Report the `-typecheck` abort as the first bug, as decision 0021 asks.
- Patches kept as `git format-patch` series against the release tag; `toolchain/rebase.sh` applies them to the next `swift-6.x-RELEASE` tag; GitHub Actions on a Linux host builds a Linux-host toolset for a second artifactbundle for the project's CI.

## Phase 7 — Release on GitHub: licensing, attribution, publishing (3–4 days)

The repo has no LICENSE, NOTICE or file headers yet. The inventory below is what a scan on
2026-10-15 found (verbatim-line comparison against `apple-oss-distributions/objc4`, the
swift-5.2-RELEASE overlay, swift-corelibs-libdispatch's BlocksRuntime and the macOS SDK's
`objc/` headers). Nothing in it is a blocker; two items need a fix before the first push and
one needs the user's decision.

### 7.1 Provenance inventory

| Component | Origin | Upstream license | State | Action |
|---|---|---|---|---|
| `runtime/src/*.c`, `internal.h`, `map.c`, `encoding.c`, `perform.c`, … | Written here | — | 0 verbatim lines from objc4 (3 trivial statements in `class.c`, 1 signature in `refcount.c`) | Our license + header |
| `runtime/src/abi.h` | Written here; struct layouts are the objc4 ABI as clang/IRGen emit it (`objc-runtime-new.h`) | APSL 2.0 for objc4 | 7 of 63 lines coincide (field names) | Keep; name objc4 in NOTICE as the ABI reference |
| `runtime/include/objc/*.h` | Written here following objc4's public API | APSL 2.0 for objc4 | API declarations and macros coincide where the API dictates them; **16 doc-comment lines are verbatim objc4 text** (`objc.h` 5, `runtime.h` 8, `message.h` 3) | **Rewrite the 16 comments** before publishing; keep declarations; NOTICE states the API follows objc4 and no objc4 implementation code is used |
| `runtime/src/foundation-lite/NSObject.m`, `Protocol.m`, `NSConstantString.m`, `CFLite.c` | Written here; `NSObject.m` mirrors objc4's `NSObject.mm` method set | APSL 2.0 for objc4 | 10 of 90 lines coincide, all method signatures | Keep; same NOTICE statement |
| `runtime/src/blocks/*`, `runtime/include/Block.h`, `runtime/include/Block/Block.h` | Byte-identical copies of `swift-corelibs-libdispatch/src/BlocksRuntime` (checkout `swift-6.3-DEVELOPMENT-SNAPSHOT-2026-06-07-a`) | Apache-2.0 with Runtime Library Exception, Apple headers intact | Compliant as is | Record origin and commit in THIRD_PARTY_NOTICES; never edit without a "modified" line |
| `runtime/overlay/ObjectiveC.swift` | Port of `stdlib/public/Darwin/ObjectiveC/ObjectiveC.swift` at swift-5.2-RELEASE; 31 of 35 substantive lines verbatim | Apache-2.0 with Runtime Library Exception | **Apple's copyright/license header was dropped** | **Restore the Apple header** and add a "modified for libobjc-wasm" line; same for `runtime/overlay/CMakeLists.txt` (derived from the 5.2 CMakeLists) |
| `runtime/docs/overlay/*-5.2.*` | Verbatim Apple files kept as reference | Apache-2.0 with RLE, headers intact | Compliant | Drop them and cite the tag instead (they are reproducible from git), or keep and list in THIRD_PARTY_NOTICES |
| `patches/swift/*.patch`, `patches/llvm-project/*.patch`, `toolchain/apply-*.py` | Our hunks plus upstream context lines and embedded upstream snippets | swiftlang/swift: Apache-2.0 with RLE; swiftlang/llvm-project: Apache-2.0 WITH LLVM-exception | Derivative works of the upstream files | `patches/README.md`: base tags, upstream licenses, and that our hunks are offered under the upstream license of each tree (needed for Phase 6 upstreaming anyway) |
| `runtime/include/_stdlib_shims/*` | Written here; declare Apple API names (`CFBase` types, `TargetConditionals` macros) only | — | Original | Our license + header |
| `tests/`, `sdk/`, `toolchain/*.sh`, `runtime/docs/abi.m`, `swiftmeta.swift`, `README.md`, `PLAN.md` | Written here | — | Original; `PLAN.md` carries one personal absolute path | Our license; make the path relative |
| SDK artifactbundle (release asset, 2.3 GB uncompressed) | Redistributes: Swift toolchain binaries and stdlib (Apache-2.0 with RLE), clang/lld and compiler-rt builtins (Apache-2.0 WITH LLVM-exception), wasi-libc (Apache-2.0 WITH LLVM-exception / Apache-2.0 / MIT, at the user's choice), libc++ and libc++abi (Apache-2.0 WITH LLVM-exception), swift-foundation (Apache-2.0), swift-foundation-icu (Unicode-3.0 / ICU), swift-collections (Apache-2.0), swift-syntax macro plugins (Apache-2.0 with RLE), plus libobjc-wasm and the overlay | as listed | The stock swift.org bundle ships no license files; ours should | `LICENSE.txt` and `THIRD_PARTY_NOTICES.md` at the bundle root, with the license texts that require reproduction (Apache-2.0, LLVM exception, Unicode/ICU, MIT) |

No binaries or build outputs are in git history (largest blob 41 KB). No secrets or emails are in tracked files. The swiftui-web branch (25 files vs `main`, unmerged) adds nothing copied from third parties; its existing `LICENSE`/`NOTICE` need no change.

### 7.2 Decisions for the user (defaults in bold)

5. **Repo license: Apache-2.0 with the Swift Runtime Library Exception** (the swift.org `LICENSE.txt` form, SPDX `Apache-2.0 WITH Swift-exception`). `libobjc.a` and the overlay are statically linked into every consumer's app; plain Apache-2.0 would oblige every app to carry our NOTICE, which is what the exception exists to waive. It also matches every Apache file we carry, so the repo stays single-license apart from upstream notices. Alternative: MIT for original files (simpler text, but the repo is mixed-license anyway and consumers lose the exception's clarity).
6. **objc4 header text: rewrite the 16 verbatim comments** rather than placing `objc.h`, `runtime.h` and `message.h` under APSL 2.0. APSL would bring its modification and deployment duties into three headers that every consumer includes, and APSL is not GPL-compatible. Declarations stay: they are the API, and reproducing an API's declarations for compatibility is the position every alternative runtime takes.
7. **Copyright line**: "Copyright 2026 <holder> and the swift-objc-wasm contributors" — the user's name or handle is needed.
8. **Reference copies under `runtime/docs/overlay/`: drop** (cite the tag).
9. **Release form: source tag plus the artifactbundle as a release asset**, macOS arm64 host only for now (Phase 6's Linux-host toolset remains a CI follow-up). GitHub caps an asset at 2 GiB: the bundle must be compressed (`tar.gz`, as `swift sdk install` expects) and, if still over, host binaries stripped; measure before tagging. Alternative: source only at first, consumers build with the scripts (2–4 h).

### 7.3 Steps, in order

1. **License files** at the repo root: `LICENSE.txt` (Apache-2.0 text + the Runtime Library Exception, verbatim from swift.org), `NOTICE.txt` (project name, copyright line, one paragraph on objc4 as the ABI/API reference with no objc4 code, the trademark line: Apple, Objective-C, Swift are Apple trademarks; not affiliated with or endorsed by Apple), `THIRD_PARTY_NOTICES.md` (every row above with URL, tag or commit, license, and the license texts).
2. **Fix the two defects**: restore Apple's header on `runtime/overlay/ObjectiveC.swift` and `CMakeLists.txt` with a modification line; rewrite the 16 objc4 doc comments (`objc.h`, `runtime.h`, `message.h`). Rerun the verbatim-line scan afterwards and keep it as `tests/provenance-check.sh` (clones objc4 shallow into a scratch dir for comparison only; objc4 is never vendored).
3. **File headers**: 67 of 74 original source files have none. Add a two-line header (`// swift-objc-wasm` / `// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception`) to every original `.c/.h/.m/.swift/.py/.sh/.cmake/CMakeLists.txt`; leave the BlocksRuntime copies untouched.
4. **`patches/README.md`**: base tags (`swift-6.3.3-RELEASE` of both trees), what each patch does, upstream license per tree, and that the hunks are offered under those licenses (Swift and LLVM accept contributions under their own license without a CLA, which Phase 6 relies on).
5. **Scrub**: `PLAN.md` path to relative; `git grep` for `/Users/`, emails, tokens (clean today); decide whether the `Claude-Session` commit trailers may go public (they are private links; harmless, but the history is rewritten now or never).
6. **README for strangers**: prerequisites (macOS arm64, Xcode CLT with the macOS 26 SDK, swiftly 6.3.3, ninja, 16 GB, ~60 GB disk), build time, the exact checkout tag, the status table and limits, how to verify (`runtime/tests/run.sh`, `tests/e2e/run.sh`, `tests/compiler-checks.sh`), where to report problems, link to swiftui-web's decision 0023 for the consumer side.
7. **Clean-commit verification** on the commit to be tagged: runtime tests, e2e, compiler checks, and the swiftui-web hash/probe chain against a bundle assembled from that commit. Record the outcomes in README as done for the acceptance rerun.
8. **Publish the source**: `gh repo create iamcgn/swift-objc-wasm --public` with description and topics (swift, objective-c, webassembly, wasi, objc-runtime), push `main`, tag `v0.1.0`. Enable issues; no CI yet.
9. **Publish the SDK asset**: assemble the bundle from the tagged commit, add `LICENSE.txt` + `THIRD_PARTY_NOTICES.md` + a `VERSION` file (tag, commit, patch-series sha256) at the bundle root, `tar.gz`, `shasum -a 256`, `gh release create v0.1.0` with notes (what works, limits, host requirements). README gets `swift sdk install <asset url> --checksum <sha256>`.
10. **Consumer side (swiftui-web)**: after the asset exists, decision 0023 drops "the SDK is local"; add the CI step pinned to the release URL and checksum; merge `objc-runtime-wasm` by PR. These are the steps the acceptance phase deliberately left undone and need the user's go-ahead at that point.
11. **Phase 6 upstream filings** reference the public repo and tag.

### 7.4 Done when

- `LICENSE.txt`, `NOTICE.txt`, `THIRD_PARTY_NOTICES.md` exist; every original file carries the SPDX header; every third-party file carries its original header; the provenance scan reports 0 verbatim objc4 comment lines.
- The public repo and `v0.1.0` tag exist, the release asset installs with `swift sdk install … --checksum` on a second machine, and `tests/e2e/run.sh` passes against it.
- README states what was verified on the tagged commit.

## Hardest risks and mitigations

| Risk | Mitigation |
|---|---|
| `call_indirect` signature mismatch between Swift `@objc` thunks and callers (clang, runtime, `perform`) | Both compilers derive the foreign signature from clang's wasm32 ABI lowering; Phase 4 covers every return kind (void, scalar, `BOOL`/signed char, small struct, sret, float); typed `objc_performSelector`; `objc_msgSend` symbols trap loudly |
| Swift runtime's Foundation/CF header and class dependence | Shim headers + Foundation-lite; four small Swift-tree hunks; grep the link map for every `objc_lookUpClass("NS...")` path exercised |
| Weak-symbol probes (`&objc_setHook_*`) resolve to trap stubs on wasm | Every SPI strongly defined in libobjc; e2e test exercises `NSClassFromString` of a Swift class |
| wasm-ld dropping metadata | `llvm.used` → RETAIN flag; `__start_/__stop_` only for C-identifier names (all `objc_*` names qualify); Phase 0 experiment gates; libobjc declares the empty `"R"` sections |
| Constructor ordering (ObjC image registration vs `swift_image_constructor`; `+load` needing Swift metadata) | Register at priority 101, realize lazily; `SwiftNativeNS*Base +load` only calls `objc_lookUpClass`/`class_setSuperclass`; test a Swift class referenced from an ObjC class ref at startup |
| is-Swift bit / `class_ro_t` layout disagreement | Pin `SWIFT_CLASS_IS_SWIFT_MASK`; runtime unit test asserts `sizeof(class_ro_t)==40` on wasm32 and that `OBJC_CLASS_$_SwiftObject` realizes |
| 16 GB host build | `--jobs 6`, link jobs 1, no macOS stdlib, no debug info; keep `build/objc-wasm` across phases; iterate with ninja targets |
| Patched frontend vs stock host libs / macro plugins | Ship the build's own `lib/swift/host/compiler`; plugins from stock (same swift-syntax tag); verify `@Observable` |
| FoundationEssentials / Testing / XCTest ABI with the new stdlib | Rebuild FoundationEssentials; omit Foundation/XCTest/Testing in v1; the project's wasm path uses none |
| `@objc` needs a module named `Foundation`; `#selector` needs `ObjectiveC` loaded | Shadows `Foundation` + WebFoundation re-export; toolset fallback flag |
| JavaScriptKit `PackageToJS` with a custom toolset | It shells out to `swift build --swift-sdk`; verify `swift package ... js` once in Phase 5 |
| Blocks runtime duplicate symbols | Weak definitions in libobjc; e2e test links `-lCoreFoundation` |

## Effort

Phase 0: 2–3 days · Phase 1: 4 · Phase 2: 10–15 · Phase 3: 5–8 · Phase 4: 3 · Phase 5: 4–6 · Phase 6: 2–3 + ongoing · Phase 7: 3–4. Total ≈ 33–46 engineer-days; Phases 1 and 2 overlap once the clang patch lands.

## Open decisions (defaults assumed unless changed; 5–9 are in Phase 7.2)

1. **Build location**: host compiler built on this Mac (needed anyway for the macOS toolset); Linux-host variant deferred to CI.
2. **Route**: objc4-ABI runtime (above) rather than GNUstep libobjc2. Switching later is possible since Phase 0/1 clang work (section names) is shared.
3. **Bridging**: `String`/`Array` bridging deferred to v1.5; acceptance app needs only `AnyObject?`.
4. **SDK contents**: Foundation/XCTest/Testing dropped from the interop SDK in v1; the project's test suites keep running on the stock SDK.
   Decided 2026-10-15: v1 ships neither corelibs Foundation nor CoreFoundation; v1.5 adds targeted bridging over Foundation-lite's `NSString` in WebFoundation; the eventual target is corelibs Foundation rebuilt with interop on (its own `NSObject` and bridging replaced by the runtime's), as a separate decision with its own budget.
