//===--- ObjectiveC.swift - The ObjectiveC overlay for libobjc-wasm -------===//
//
// Port of the historical Swift overlay (stdlib/public/Darwin/ObjectiveC at
// swift-5.2-RELEASE) for platforms whose Objective-C runtime is libobjc-wasm.
// The compiler looks up `ObjCBool`, `Selector` and `NSZone` in this module.
//
//===----------------------------------------------------------------------===//

@_exported
import ObjectiveC

//===----------------------------------------------------------------------===//
// Objective-C Primitive Types
//===----------------------------------------------------------------------===//

/// The Objective-C BOOL type.
///
/// On WebAssembly, as on 32-bit Darwin, Objective-C's BOOL type is a "signed
/// char". The Clang importer imports it as ObjCBool.
@frozen
public struct ObjCBool : ExpressibleByBooleanLiteral {
  @usableFromInline var _value: Int8

  @_transparent
  init(_ value: Int8) {
    self._value = value
  }

  @_transparent
  public init(_ value: Bool) {
    self._value = value ? 1 : 0
  }

  /// The value of `self`, expressed as a `Bool`.
  @_transparent
  public var boolValue: Bool {
    return _value != 0
  }

  /// Create an instance initialized to `value`.
  @_transparent
  public init(booleanLiteral value: Bool) {
    self.init(value)
  }
}

extension ObjCBool : CustomReflectable {
  /// Returns a mirror that reflects `self`.
  public var customMirror: Mirror {
    return Mirror(reflecting: boolValue)
  }
}

extension ObjCBool : CustomStringConvertible {
  /// A textual representation of `self`.
  public var description: String {
    return self.boolValue.description
  }
}

// Functions used by the compiler to convert between Bool and ObjCBool.
@_transparent
public // COMPILER_INTRINSIC
func _convertBoolToObjCBool(_ x: Bool) -> ObjCBool {
  return ObjCBool(x)
}

@_transparent
public // COMPILER_INTRINSIC
func _convertObjCBoolToBool(_ x: ObjCBool) -> Bool {
  return x.boolValue
}

/// The Objective-C SEL type.
///
/// The Clang importer imports SEL as Selector. A selector is the interned,
/// canonical name of a method; equality is pointer equality.
@frozen
public struct Selector : ExpressibleByStringLiteral {
  var ptr: OpaquePointer

  /// Create a selector from a string.
  public init(_ str : String) {
    ptr = str.withCString { sel_registerName($0).ptr }
  }

  /// Create an instance initialized to `value`.
  public init(stringLiteral value: String) {
    self = sel_registerName(value)
  }
}

extension Selector: Equatable, Hashable {
  // The implementations for `==` and `hash(into:)` are synthesized by the
  // compiler and use the value of `ptr`.
}

extension Selector : CustomStringConvertible {
  /// A textual representation of `self`.
  public var description: String {
    return String(_sel: self)
  }
}

extension String {
  /// Construct the C string representation of an Objective-C selector.
  public init(_sel: Selector) {
    self = String(cString: sel_getName(_sel))
  }
}

extension Selector : CustomReflectable {
  /// Returns a mirror that reflects `self`.
  public var customMirror: Mirror {
    return Mirror(reflecting: String(_sel: self))
  }
}

//===----------------------------------------------------------------------===//
// NSZone
//===----------------------------------------------------------------------===//

@frozen
public struct NSZone {
  var pointer: OpaquePointer
}

//===----------------------------------------------------------------------===//
// @autoreleasepool substitute
//===----------------------------------------------------------------------===//

public func autoreleasepool<Result>(
  invoking body: () throws -> Result
) rethrows -> Result {
  let pool = objc_autoreleasePoolPush()
  defer {
    objc_autoreleasePoolPop(pool)
  }
  return try body()
}

//===----------------------------------------------------------------------===//
// Mark YES and NO unavailable.
//===----------------------------------------------------------------------===//

@available(*, unavailable, message: "Use 'Bool' value 'true' instead")
public var YES: ObjCBool {
  fatalError("can't retrieve unavailable property")
}
@available(*, unavailable, message: "Use 'Bool' value 'false' instead")
public var NO: ObjCBool {
  fatalError("can't retrieve unavailable property")
}

//===----------------------------------------------------------------------===//
// NSObject
//===----------------------------------------------------------------------===//

extension NSObject : Equatable, Hashable {
  /// Returns a Boolean value indicating whether two values are
  /// equal. `NSObject` implements this by calling `lhs.isEqual(rhs)`.
  public static func == (lhs: NSObject, rhs: NSObject) -> Bool {
    return lhs.isEqual(rhs)
  }

  /// The hash value; `NSObject` implements this by returning `self.hash`.
  @nonobjc
  public var hashValue: Int {
    return hash
  }

  /// Hashes the essential components of this value by feeding them into the
  /// given hasher; NSObject implements this by feeding `self.hash`.
  public func hash(into hasher: inout Hasher) {
    hasher.combine(self.hash)
  }

  public func _rawHashValue(seed: Int) -> Int {
    return self.hash._rawHashValue(seed: seed)
  }
}
