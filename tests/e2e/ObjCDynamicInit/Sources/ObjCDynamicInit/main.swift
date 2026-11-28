// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
// Patch swift/0005: a metatype value constructs with an `@objc dynamic` initializer, which is
// sent as a message and so runs the dynamic class's override (Runestone's `View()` on a
// UIView-constrained generic, SwiftyFORM's `T()` on a view controller type).
import ObjectiveC

var failures = 0
func check(_ cond: Bool, _ what: String, line: Int = #line) { if !cond { print("FAIL \(line): \(what)"); failures += 1 } }

// A designated `@objc dynamic init()` and a subclass with its own.
class Base: NSObject {
    var made = "Base"
    @objc dynamic override init() { super.init() }
}
class Sub: Base {
    let extra: Int
    override init() { extra = 7; super.init(); made = "Sub" }
}
func make<T: Base>(_ type: T.Type) -> T { T() }
let s = make(Sub.self)
check(type(of: s) == Sub.self && s.made == "Sub" && s.extra == 7, "T() runs the subclass's init()")
let erased: Base.Type = Sub.self
check(erased.init().made == "Sub", "init() through an erased metatype")

// A convenience `@objc dynamic init()` that delegates to a designated initializer the subclass
// overrides (UIView's init() over init(frame:)).
class Box: NSObject {
    var size: Int
    var kind = "Box"
    init(size: Int) { self.size = size; super.init() }
    @objc dynamic convenience override init() { self.init(size: 1) }
}
class BigBox: Box {
    override init(size: Int) { super.init(size: size * 10); kind = "BigBox" }
}
func build<T: Box>(_ type: T.Type) -> T { T() }
let b = build(BigBox.self)
check(type(of: b) == BigBox.self && b.size == 10 && b.kind == "BigBox", "convenience init() reaches the subclass's designated init")

if failures > 0 { print("\(failures) failures"); exit(1) }
