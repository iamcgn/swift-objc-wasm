#!/usr/bin/env python3
"""Apply the libobjc-wasm standard-library build patches to the swift checkout.
Idempotent. Usage: apply-stdlib-patches.py [SWIFT_SOURCE]"""
import os, sys, shutil

root = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else "~/Development/swift-source")
here = os.path.dirname(os.path.abspath(__file__))
repo = os.path.dirname(here)

def patch(path, edits):
    p = os.path.join(root, path)
    s = open(p).read()
    changed = False
    for old, new, marker in edits:
        if marker in s:
            continue
        if old not in s:
            sys.exit(f"{path}: anchor not found:\n{old[:300]}")
        s = s.replace(old, new, 1)
        changed = True
    if changed:
        open(p, "w").write(s)
    print(("patched " if changed else "already patched ") + path)

# C/C++/ObjC++ flags for the WASI runtime build with interop on.
patch("swift/stdlib/cmake/modules/AddSwiftStdlib.cmake", [
("""  if(NOT SWIFT_STDLIB_ENABLE_OBJC_INTEROP)
    list(APPEND result "-DSWIFT_OBJC_INTEROP=0")
  endif()
""",
"""  if(NOT SWIFT_STDLIB_ENABLE_OBJC_INTEROP)
    list(APPEND result "-DSWIFT_OBJC_INTEROP=0")
  elseif("${CFLAGS_SDK}" STREQUAL "WASI")
    # libobjc-wasm: Objective-C interop on WebAssembly. The is-Swift bit the
    # compiler emits off-Darwin is 2; the runtime's non-Apple default is 1.
    list(APPEND result "-DSWIFT_OBJC_INTEROP=1" "-DSWIFT_CLASS_IS_SWIFT_MASK=2ULL"
         "-fobjc-runtime=ios-7.0")
    if(SWIFT_WASI_OBJC_INCLUDE_DIR)
      list(APPEND result "-I${SWIFT_WASI_OBJC_INCLUDE_DIR}")
    endif()
    if(SWIFT_WASI_OBJC_SHIMS_DIR)
      list(APPEND result "-I${SWIFT_WASI_OBJC_SHIMS_DIR}")
    endif()
  endif()
""",
'list(APPEND result "-DSWIFT_OBJC_INTEROP=1"'),
])

# Swift flags: the frontend defaults interop off for non-Darwin triples.
patch("swift/stdlib/cmake/modules/SwiftSource.cmake", [
("""  if(NOT SWIFT_STDLIB_ENABLE_OBJC_INTEROP)
    list(APPEND swift_flags "-Xfrontend" "-disable-objc-interop")
  endif()
""",
"""  if(NOT SWIFT_STDLIB_ENABLE_OBJC_INTEROP)
    list(APPEND swift_flags "-Xfrontend" "-disable-objc-interop")
  elseif("${SWIFTFILE_SDK}" STREQUAL "WASI")
    # libobjc-wasm: Objective-C interop on WebAssembly.
    list(APPEND swift_flags "-Xfrontend" "-enable-objc-interop"
         "-Xfrontend" "-disable-objc-attr-requires-foundation-module"
         "-Xfrontend" "-validate-tbd-against-ir=none")   # TBDGen omits ObjC class symbols off Mach-O
    if(SWIFT_WASI_OBJC_INCLUDE_DIR)
      list(APPEND swift_flags "-Xcc" "-I${SWIFT_WASI_OBJC_INCLUDE_DIR}" "-I${SWIFT_WASI_OBJC_INCLUDE_DIR}")
    endif()
  endif()
""",
'list(APPEND swift_flags "-Xfrontend" "-enable-objc-interop"'),
])

# SwiftObject's -performSelector: family casts objc_msgSend.
patch("swift/stdlib/public/runtime/SwiftObject.mm", [
("""- (id)performSelector:(SEL)aSelector {
  return ((id(*)(id, SEL))objc_msgSend)(self, aSelector);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object {
  return ((id(*)(id, SEL, id))objc_msgSend)(self, aSelector, object);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object1
                                     withObject:(id)object2 {
  return ((id(*)(id, SEL, id, id))objc_msgSend)(self, aSelector, object1,
                                                                 object2);
}
""",
"""#if defined(__wasm__)
// libobjc-wasm: WebAssembly type-checks indirect calls; the runtime provides a
// typed perform that selects the call signature from the method's encoding.
extern "C" id objc_performSelector(id self, SEL sel, unsigned int argc, id arg1, id arg2);

- (id)performSelector:(SEL)aSelector {
  return objc_performSelector(self, aSelector, 0, nil, nil);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object {
  return objc_performSelector(self, aSelector, 1, object, nil);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object1
                                     withObject:(id)object2 {
  return objc_performSelector(self, aSelector, 2, object1, object2);
}
#else
- (id)performSelector:(SEL)aSelector {
  return ((id(*)(id, SEL))objc_msgSend)(self, aSelector);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object {
  return ((id(*)(id, SEL, id))objc_msgSend)(self, aSelector, object);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object1
                                     withObject:(id)object2 {
  return ((id(*)(id, SEL, id, id))objc_msgSend)(self, aSelector, object1,
                                                                 object2);
}
#endif
""",
"objc_performSelector(self, aSelector, 0, nil, nil)"),
])

# FoundationHelpers: no dlsym and no dyld on WebAssembly.
patch("swift/stdlib/public/stubs/FoundationHelpers.mm", [
("""static void _initializeBridgingFunctionsImpl(void *ctxt) {
  auto getStringTypeID =
    (CFTypeID(*)(void))
    dlsym(RTLD_DEFAULT, "CFStringGetTypeID");
  assert(getStringTypeID);
  _CFStringTypeID = getStringTypeID();
  
  _CFGetTypeID = (CFTypeID(*)(CFTypeRef obj))dlsym(RTLD_DEFAULT, "CFGetTypeID");
  _CFStringHashNSString = (CFHashCode(*)(id))dlsym(RTLD_DEFAULT,
                                                   "CFStringHashNSString");
  _CFStringHashCString = (CFHashCode(*)(const uint8_t *, CFIndex))dlsym(
                                                   RTLD_DEFAULT,
                                                   "CFStringHashCString");
}
""",
"""static void _initializeBridgingFunctionsImpl(void *ctxt) {
#if defined(__wasm__)
  // libobjc-wasm: static linking, no dlsym; CoreFoundation-lite is linked in.
  _CFStringTypeID = CFStringGetTypeID();
  _CFGetTypeID = &CFGetTypeID;
  _CFStringHashNSString = &CFStringHashNSString;
  _CFStringHashCString = &CFStringHashCString;
#else
  auto getStringTypeID =
    (CFTypeID(*)(void))
    dlsym(RTLD_DEFAULT, "CFStringGetTypeID");
  assert(getStringTypeID);
  _CFStringTypeID = getStringTypeID();
  
  _CFGetTypeID = (CFTypeID(*)(CFTypeRef obj))dlsym(RTLD_DEFAULT, "CFGetTypeID");
  _CFStringHashNSString = (CFHashCode(*)(id))dlsym(RTLD_DEFAULT,
                                                   "CFStringHashNSString");
  _CFStringHashCString = (CFHashCode(*)(const uint8_t *, CFIndex))dlsym(
                                                   RTLD_DEFAULT,
                                                   "CFStringHashCString");
#endif
}
""",
"_CFGetTypeID = &CFGetTypeID;"),
("""__swift_uint8_t
_swift_stdlib_dyld_is_objc_constant_string(const void *addr) {
  return (SWIFT_RUNTIME_WEAK_CHECK(_dyld_is_objc_constant)
          && SWIFT_RUNTIME_WEAK_USE(_dyld_is_objc_constant(dyld_objc_string_kind, addr))) ? 1 : 0;
}
""",
"""__swift_uint8_t
_swift_stdlib_dyld_is_objc_constant_string(const void *addr) {
#if defined(__wasm__)
  return 0;   // libobjc-wasm: no dyld
#else
  return (SWIFT_RUNTIME_WEAK_CHECK(_dyld_is_objc_constant)
          && SWIFT_RUNTIME_WEAK_USE(_dyld_is_objc_constant(dyld_objc_string_kind, addr))) ? 1 : 0;
#endif
}
""",
"return 0;   // libobjc-wasm: no dyld"),
])

# The ObjectiveC overlay for WASI.
dst = os.path.join(root, "swift/stdlib/public/ObjectiveC")
os.makedirs(dst, exist_ok=True)
for f in ("ObjectiveC.swift", "CMakeLists.txt"):
    shutil.copyfile(os.path.join(repo, "runtime/overlay", f), os.path.join(dst, f))
print("installed swift/stdlib/public/ObjectiveC")

patch("swift/stdlib/public/CMakeLists.txt", [
("""if(SWIFT_BUILD_SDK_OVERLAY OR (SWIFT_BUILD_TEST_SUPPORT_MODULES AND SWIFT_BUILD_DYNAMIC_SDK_OVERLAY_default))
  add_subdirectory(Platform)
endif()
""",
"""if(SWIFT_BUILD_SDK_OVERLAY OR (SWIFT_BUILD_TEST_SUPPORT_MODULES AND SWIFT_BUILD_DYNAMIC_SDK_OVERLAY_default))
  add_subdirectory(Platform)
  # libobjc-wasm: the ObjectiveC overlay for WebAssembly with interop enabled.
  if(SWIFT_STDLIB_ENABLE_OBJC_INTEROP AND "WASI" IN_LIST SWIFT_SDKS)
    add_subdirectory(ObjectiveC)
  endif()
endif()
""",
"add_subdirectory(ObjectiveC)"),
])

# Static executables link libobjc.
patch("swift/stdlib/public/Resources/wasi/static-executable-args.lnk", [
("-lswiftSwiftOnoneSupport\n", "-lswiftSwiftOnoneSupport\n-lobjc\n", "-lobjc\n"),
])

# build-script: enable interop for the wasm stdlib when SWIFT_OBJC_WASM_RUNTIME is set.
patch("swift/utils/swift_build_support/swift_build_support/products/wasmstdlib.py", [
("""    def add_extra_cmake_options(self):
        self.cmake_options.define('SWIFT_THREADING_PACKAGE:STRING', 'none')
""",
"""    def add_extra_cmake_options(self):
        self.cmake_options.define('SWIFT_THREADING_PACKAGE:STRING', 'none')
        # libobjc-wasm: Objective-C interop, when the runtime's install dir is given.
        objc_runtime = os.environ.get('SWIFT_OBJC_WASM_RUNTIME')
        if objc_runtime:
            self.cmake_options.define('SWIFT_STDLIB_ENABLE_OBJC_INTEROP:BOOL', 'TRUE')
            self.cmake_options.define('SWIFT_WASI_OBJC_INCLUDE_DIR:PATH',
                                      os.path.join(objc_runtime, 'include'))
            self.cmake_options.define('SWIFT_WASI_OBJC_SHIMS_DIR:PATH',
                                      os.path.join(objc_runtime, 'include', '_stdlib_shims'))
            self.cmake_options.define('SWIFT_WASI_OBJC_LIB:FILEPATH',
                                      os.path.join(objc_runtime, 'lib', 'libobjc.a'))
""",
"SWIFT_OBJC_WASM_RUNTIME"),
])
print("done")
