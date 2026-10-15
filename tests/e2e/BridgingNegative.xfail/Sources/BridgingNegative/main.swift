// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
import ObjectiveC
// v1 of the interop SDK has no Foundation, so String does not bridge to NSString: this must
// fail to compile (an @objc method cannot take a String). The runner expects the failure.
class Talker: NSObject {
    @objc func say(_ s: String) {}
}
print("BridgingNegative compiled, which it must not")
