# Derived from stdlib/public/Darwin/ObjectiveC/CMakeLists.txt at swift-5.2-RELEASE of
# the Swift.org open source project (Copyright (c) 2014 - 2017 Apple Inc. and the Swift
# project authors; Apache License v2.0 with Runtime Library Exception; see
# https://swift.org/LICENSE.txt). Modifications Copyright 2026 Corey Earwood and the
# swift-objc-wasm contributors, under the same license.
# SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
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
