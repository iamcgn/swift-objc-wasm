// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
import ObjectiveC
import Observation
import CBlocks

var failures = 0
func check(_ cond: Bool, _ what: String, line: Int = #line) { if !cond { print("FAIL \(line): \(what)"); failures += 1 } }

@objc enum Direction: Int { case up = 1, down, left, right }

class Node: NSObject {
    @objc var direction: Direction = .left
    @objc func turn(_ d: Direction) -> Direction { direction = d; return d }
    @objc func partner() -> AnyObject? { self }
    @objc func name() -> NSObject { self }
}

@Observable final class Counter {
    var value = 0
    var label: String { "count \(value)" }
}

let n = Node()
// @objc enum: raw-value backed, usable as an Objective-C method parameter and return.
check(n.turn(.right) == .right && n.direction == .right, "@objc enum round trip through an @objc method")
check(Direction(rawValue: 2) == .down, "@objc enum raw values")
// perform on a method returning an object: the result is the object, retained correctly.
let got = n.perform(#selector(Node.partner))?.takeUnretainedValue()
check(got === n, "perform returning AnyObject yields the object")
check(n.perform(#selector(Node.name))?.takeUnretainedValue() === n, "perform returning NSObject yields the object")
// is NSObject / NSObjectProtocol on Swift classes.
let anyN: Any = n
check(anyN is NSObject, "is NSObject")
check(anyN is NSObjectProtocol, "is NSObjectProtocol")
class Plain {}
check(!(Plain() as Any is NSObject), "a pure Swift class is not an NSObject")
// Blocks: a Swift closure as an Objective-C block, copied by the runtime's _Block_copy.
typealias IntBlock = @convention(block) (Int) -> Int
let base = 40
let block: IntBlock = { $0 + base }
// A @convention(block) value is a pointer to the block object; copy it with the runtime.
let copied = objc_test_block_copy(unsafeBitCast(block, to: UnsafeRawPointer.self))
check(copied != nil, "_Block_copy returns a block")
if let copied {
    let restored = unsafeBitCast(copied, to: IntBlock.self)
    check(restored(2) == 42, "the copied block runs with its captures")
    objc_test_block_release(copied)
}
// @Observable (the macro plugin shipped with the SDK) expands and tracks changes.
let counter = Counter()
var observed = false
withObservationTracking({ _ = counter.value }, onChange: { observed = true })
counter.value = 5
check(observed, "@Observable expanded and observation fired")
check(counter.label == "count 5", "@Observable computed property")
if failures > 0 { print("\(failures) failures"); exit(1) }
print("ObjCMore: ok")
