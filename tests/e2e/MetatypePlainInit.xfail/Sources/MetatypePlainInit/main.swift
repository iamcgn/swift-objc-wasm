// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
// Patch swift/0005 accepts only `@objc dynamic` initializers from a metatype: one dispatched
// through the vtable still needs `required`, as Swift's rule says (ObjCDynamicInit is the
// accepted case).
import ObjectiveC

class Base: NSObject {
    override init() { super.init() }
}
func make<T: Base>(_ type: T.Type) -> T { T() }
print(make(Base.self))
