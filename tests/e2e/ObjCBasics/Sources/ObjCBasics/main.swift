// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
import ObjectiveC

var failures = 0
func check(_ cond: Bool, _ what: String, file: String = #file, line: Int = #line) {
    if !cond { print("FAIL \(line): \(what)"); failures += 1 }
}

@objc protocol Greeter {
    func greet() -> Int
    @objc optional func optionalGreet() -> Int
}

class Base: NSObject {
    var count = 0
    @objc func newGame(_ sender: AnyObject?) { count += 1 }
    @objc func describe() -> Int { 1 }
    @objc var flag: Bool = false
    override var hash: Int { 7 }
    override func isEqual(_ object: Any?) -> Bool { (object as? Base) != nil }
}

final class Sub: Base, Greeter {
    override func describe() -> Int { super.describe() + 10 }
    func greet() -> Int { 42 }
    func optionalGreet() -> Int { 43 }
}

class Plain {
    var value = 5
    @objc func ping() -> Int { value }
}

let s = Sub()
check(s.describe() == 11, "override + super send")
check(s.responds(to: #selector(Base.newGame(_:))), "responds(to:)")
check(!s.responds(to: Selector(("nope"))), "does not respond")
s.perform(#selector(Base.newGame(_:)), with: nil)
check(s.count == 1, "perform Void method")
// `perform` on a method returning a scalar yields the raw return value in the pointer slot,
// as on Darwin; it is not an object and must not be retained (no takeUnretainedValue here).
let scalarResult = s.perform(#selector(Base.describe))
check(scalarResult.map { unsafeBitCast($0, to: Int.self) } == 11, "perform returning a scalar yields the value (11) in the result slot")
check(#selector(Base.newGame(_:)) == Selector("newGame:"), "selector interning")
check(String(_sel: #selector(Base.describe)) == "describe", "selector name")
s.flag = true
check(s.flag, "objc property")
check(s.hash == 7 && s.hashValue == 7, "hash override")
check(s == Sub(), "isEqual via ==")
check(type(of: s) == Sub.self, "type(of:)")
check(s is NSObjectProtocol, "NSObjectProtocol cast")
check((s as AnyObject) is Greeter, "@objc protocol cast")
let g: Greeter = s
check(g.greet() == 42 && g.optionalGreet?() == 43, "protocol dispatch with optional")
let any: AnyObject = s
check(any.greet?() == 42, "AnyObject dynamic lookup")
check(any.describe?() == 11, "AnyObject dynamic lookup of @objc method")
let subName = String(cString: class_getName(Sub.self))
check(subName.hasSuffix("Sub"), "class_getName")
let p = Plain()
check(p.ping() == 5, "@objc on pure Swift class")
let obj: AnyObject = p
check(obj.ping?() == 5, "dynamic lookup on SwiftObject-rooted class")
var weak: Base? = Base()
weak var w = weak
check(w != nil, "weak ref alive")
weak = nil
check(w == nil, "weak ref cleared")
let cls: AnyClass? = objc_getClass(subName) as? AnyClass
check(cls === Sub.self, "objc_getClass round trip")
if let t = cls as? NSObject.Type { check(t.init() is Sub, "instantiate class from runtime") } else { check(false, "class as NSObject.Type") }
check(autoreleasepool { 3 } == 3, "autoreleasepool")
if failures > 0 { print("\(failures) failures"); exit(1) }
print("ObjCBasics: ok")
