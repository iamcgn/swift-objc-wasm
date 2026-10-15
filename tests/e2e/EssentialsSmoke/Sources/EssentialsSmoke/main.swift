// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
import FoundationEssentials
import ObjectiveC

var failures = 0
func check(_ cond: Bool, _ what: String, line: Int = #line) { if !cond { print("FAIL \(line): \(what)"); failures += 1 } }

struct Payload: Codable, Equatable { var name: String; var when: Date; var url: URL; var values: [Int] }

class Model: NSObject {
    @objc dynamic var title: Int = 1
    @objc func bump(_ sender: AnyObject?) { count += 1 }
    var count = 0
}

let p = Payload(name: "wasm", when: Date(timeIntervalSince1970: 1_700_000_000), url: URL(string: "https://example.com/a?b=1")!, values: [1, 2, 3])
let data = try JSONEncoder().encode(p)
let back = try JSONDecoder().decode(Payload.self, from: data)
check(back == p, "JSON round trip")
check(String(decoding: data, as: UTF8.self).contains("\"name\":\"wasm\""), "JSON text")
check(p.url.host == "example.com", "URL parsing")
check(UUID().uuidString.count == 36, "UUID")
check("a,b,c".split(separator: ",").count == 3, "String ops")
let m = Model()
m.perform(#selector(Model.bump(_:)), with: nil)
check(m.count == 1, "perform on NSObject subclass with Essentials linked")
check(m.title == 1, "@objc dynamic Int property on Swift class")
m.title = 2
check(m.title == 2, "@objc dynamic Int property set")
let anyM: AnyObject = m
check(anyM.bump != nil, "dynamic lookup")
if failures > 0 { print("\(failures) failures"); exit(1) }
print("EssentialsSmoke: ok")
