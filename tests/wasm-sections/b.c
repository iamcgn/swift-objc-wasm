// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
static int b_class = 2;
__attribute__((used, section("objc_classlist"))) void *b_entry = &b_class;
/* a section nothing references, to confirm --gc-sections still runs */
__attribute__((section("unused_section"))) void *unused_entry = &b_class;
