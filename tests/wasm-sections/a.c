// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
static int a_class = 1;
__attribute__((used, section("objc_classlist"))) void *a_entry = &a_class;
