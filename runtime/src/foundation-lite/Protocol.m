// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#import <objc/Protocol.h>
#import <objc/runtime.h>

@implementation Protocol

- (BOOL)conformsTo:(Protocol *)aProtocolObj { return protocol_conformsToProtocol(self, aProtocolObj); }

- (const char *)name { return protocol_getName(self); }

- (BOOL)isEqual:(id)obj {
    if (self == obj) return YES;
    if (!obj || ![obj isKindOfClass:[Protocol class]]) return NO;
    return protocol_isEqual(self, obj);
}

- (NSUInteger)hash { return 23; }

- (id)retain { return self; }
- (oneway void)release {}
- (id)autorelease { return self; }
- (NSUInteger)retainCount { return 1; }

@end
