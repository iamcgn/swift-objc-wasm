// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
import ObjectiveC
@objc protocol Greeter { func greet() ; @objc optional func optional() }
class Base: NSObject {
  @objc func newGame(_ sender: AnyObject?) {}
  @objc var count: Int = 0
}
final class Sub: Base, Greeter {
  func greet() {}
  override func newGame(_ sender: AnyObject?) {}
}
class Generic<T>: NSObject { var t: T? }
public func run() { let s = Sub(); s.newGame(nil); _ = s.perform(#selector(Base.newGame(_:)), with: nil); _ = s.perform(#selector(Base.newGame(_:)), with: nil) ; let g = Generic<Int>(); _ = g.t }
