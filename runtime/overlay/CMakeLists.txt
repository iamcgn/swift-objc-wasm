# The ObjectiveC overlay for platforms using libobjc-wasm (WebAssembly).
add_swift_target_library(swiftObjectiveC ${SWIFT_STDLIB_LIBRARY_BUILD_TYPES} IS_SDK_OVERLAY
  ObjectiveC.swift

  SWIFT_COMPILE_FLAGS
    ${SWIFT_RUNTIME_SWIFT_COMPILE_FLAGS}
    ${SWIFT_STANDARD_LIBRARY_SWIFT_FLAGS}
    "-Xfrontend" "-disable-objc-attr-requires-foundation-module"
  LINK_FLAGS "${SWIFT_RUNTIME_SWIFT_LINK_FLAGS}"
  TARGET_SDKS WASI
  INSTALL_IN_COMPONENT sdk-overlay)
