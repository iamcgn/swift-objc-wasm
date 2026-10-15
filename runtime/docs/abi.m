// swift-objc-wasm — Copyright 2026 Corey Earwood and the swift-objc-wasm contributors
// SPDX-License-Identifier: Apache-2.0 WITH Swift-exception
#include <stdint.h>
typedef struct objc_class *Class; typedef struct objc_object { Class isa; } *id; typedef struct objc_selector *SEL;
@protocol NSObject - (int)hash; @optional - (void)opt; @end
@protocol Extra <NSObject> - (void)extra:(id)x; @end
@interface Root <NSObject> { Class isa; int ivar1; } + (id)alloc; - (int)hash; @property int prop; @end
@implementation Root + (id)alloc { return 0; } - (int)hash { return 7; } - (void)opt {} @end
@interface Sub : Root <Extra> { double d; } @end
@implementation Sub - (void)extra:(id)x {} + (void)load {} @end
@interface Sub (Cat) - (float)catMethod:(int)a; @end
@implementation Sub (Cat) - (float)catMethod:(int)a { return a; } @end
int main(){ id s = [Sub alloc]; [s hash]; id str = @"literal"; (void)str; return 0; }
