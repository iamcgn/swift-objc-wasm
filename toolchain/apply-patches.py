#!/usr/bin/env python3
"""Apply the libobjc-wasm compiler patches to the swift and llvm-project checkouts.

Idempotent: each edit is skipped when its marker text is already present.
Usage: apply-patches.py [SWIFT_SOURCE]   (default ~/Development/swift-source)
"""
import os, sys

root = os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else "~/Development/swift-source")
MARK = "libobjc-wasm"

def patch(path, edits):
    p = os.path.join(root, path)
    s = open(p).read()
    changed = False
    for old, new, marker in edits:
        if marker in s:
            continue
        if old not in s:
            sys.exit(f"{path}: anchor not found:\n{old[:200]}")
        s = s.replace(old, new, 1)
        changed = True
    if changed:
        open(p, "w").write(s)
    print(("patched " if changed else "already patched ") + path)

# ---------------------------------------------------------------- clang
CG = "llvm-project/clang/lib/CodeGen/CGObjCMac.cpp"
patch(CG, [
# 1. Section names: WebAssembly uses plain names like ELF.
("""  case llvm::Triple::ELF:
    assert(Section.starts_with("__") && "expected the name to begin with __");
    return Section.substr(2).str();
  case llvm::Triple::COFF:
    assert(Section.starts_with("__") && "expected the name to begin with __");
    return ("." + Section.substr(2) + "$B").str();
  case llvm::Triple::Wasm:
  case llvm::Triple::GOFF:""",
"""  case llvm::Triple::ELF:
  case llvm::Triple::Wasm: // libobjc-wasm: wasm-ld synthesizes __start_/__stop_
    assert(Section.starts_with("__") && "expected the name to begin with __");
    return Section.substr(2).str();
  case llvm::Triple::COFF:
    assert(Section.starts_with("__") && "expected the name to begin with __");
    return ("." + Section.substr(2) + "$B").str();
  case llvm::Triple::GOFF:""",
"case llvm::Triple::Wasm: // libobjc-wasm"),

# 2. Lookup functions next to the messengers.
("""  llvm::FunctionCallee getSendFn2(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperFn2() : getMessageSendFn();
  }
""",
"""  llvm::FunctionCallee getSendFn2(bool IsSuper) const {
    return IsSuper ? getMessageSendSuperFn2() : getMessageSendFn();
  }

  /// IMP objc_msgLookup(id, SEL) and IMP objc_msgLookupSuper2(struct objc_super *, SEL)
  ///
  /// libobjc-wasm: WebAssembly type-checks indirect calls, so a universal
  /// messenger cannot exist; the IMP is looked up and called directly.
  llvm::FunctionCallee getMessageLookupFn() const {
    llvm::Type *params[] = {ObjectPtrTy, SelectorPtrTy};
    return CGM.CreateRuntimeFunction(
        llvm::FunctionType::get(Int8PtrTy, params, false), "objc_msgLookup");
  }
  llvm::FunctionCallee getMessageLookupSuperFn2() const {
    llvm::Type *params[] = {SuperPtrTy, SelectorPtrTy};
    return CGM.CreateRuntimeFunction(
        llvm::FunctionType::get(Int8PtrTy, params, false),
        "objc_msgLookupSuper2");
  }
""",
"getMessageLookupFn() const"),

# 3. EmitMessageSend: lookup-based dispatch with a nil check on wasm.
("""  bool ReceiverCanBeNull =
      canMessageReceiverBeNull(CGF, Method, IsSuper, ClassReceiver, Arg0);

  bool RequiresNullCheck = false;
  bool RequiresSelValue = true;

  llvm::FunctionCallee Fn = nullptr;
  if (Method && Method->isDirectMethod()) {""",
"""  bool ReceiverCanBeNull =
      canMessageReceiverBeNull(CGF, Method, IsSuper, ClassReceiver, Arg0);

  bool RequiresNullCheck = false;
  bool RequiresSelValue = true;

  // libobjc-wasm: on WebAssembly the messenger is replaced by a method lookup
  // followed by a direct call with the message's own signature. A nil receiver
  // must then be skipped before the call, whatever the return type.
  const bool UseMsgLookup = CGM.getTriple().isOSBinFormatWasm() &&
                            !(Method && Method->isDirectMethod());

  llvm::FunctionCallee Fn = nullptr;
  if (UseMsgLookup) {
    if (ReceiverCanBeNull)
      RequiresNullCheck = true;
  } else if (Method && Method->isDirectMethod()) {""",
"const bool UseMsgLookup"),

("""  // Cast function to proper signature
  llvm::Constant *BitcastFn = cast<llvm::Constant>(
      CGF.Builder.CreateBitCast(Fn.getCallee(), MSI.MessengerType));

  // We don't need to emit a null check to zero out an indirect result if the
  // result is ignored.
  if (Return.isUnused())
    RequiresNullCheck = false;
""",
"""  // Cast function to proper signature
  llvm::Constant *BitcastFn =
      UseMsgLookup ? nullptr
                   : cast<llvm::Constant>(CGF.Builder.CreateBitCast(
                         Fn.getCallee(), MSI.MessengerType));

  // We don't need to emit a null check to zero out an indirect result if the
  // result is ignored. (Not on WebAssembly: the call itself must be skipped.)
  if (Return.isUnused() && !UseMsgLookup)
    RequiresNullCheck = false;
""",
"UseMsgLookup ? nullptr"),

("""  llvm::CallBase *CallSite;
  CGCallee Callee = CGCallee::forDirect(BitcastFn);
  RValue rvalue =
      CGF.EmitCall(MSI.CallInfo, Callee, Return, ActualArgs, &CallSite);
""",
"""  llvm::CallBase *CallSite;
  CGCallee Callee;
  if (UseMsgLookup) {
    llvm::Value *LookupArgs[] = {Arg0, SelValue};
    llvm::Value *Imp = CGF.EmitNounwindRuntimeCall(
        IsSuper ? ObjCTypes.getMessageLookupSuperFn2()
                : ObjCTypes.getMessageLookupFn(),
        LookupArgs);
    if (IsSuper) {
      // The IMP takes the receiver itself, not the objc_super struct.
      Address SuperAddr(Arg0, ObjCTypes.SuperTy, CGF.getPointerAlign());
      llvm::Value *Receiver =
          CGF.Builder.CreateLoad(CGF.Builder.CreateStructGEP(SuperAddr, 0));
      ActualArgs[0] = CallArg(RValue::get(Receiver), Arg0Ty);
    }
    Callee = CGCallee(CGCalleeInfo(), Imp, CGPointerAuthInfo());
  } else {
    Callee = CGCallee::forDirect(BitcastFn);
  }
  RValue rvalue =
      CGF.EmitCall(MSI.CallInfo, Callee, Return, ActualArgs, &CallSite);
""",
"Callee = CGCallee(CGCalleeInfo(), Imp, CGPointerAuthInfo());"),

# 4. Metadata lists must survive wasm-ld --gc-sections: only llvm.used is retained.
("""static Qualifiers::GC GetGCAttrTypeForType(ASTContext &Ctx, QualType FQT,
                                           bool pointee = false) {""",
"""/// libobjc-wasm: wasm-ld only keeps data segments of globals in llvm.used, so
/// Objective-C metadata that the runtime discovers through section bounds is
/// marked used rather than compiler-used on WebAssembly.
static void addCompilerUsedOrRetainedGlobal(CodeGen::CodeGenModule &CGM,
                                            llvm::GlobalValue *GV) {
  if (CGM.getTriple().isOSBinFormatWasm())
    CGM.addUsedGlobal(GV);
  else
    CGM.addCompilerUsedGlobal(GV);
}

static Qualifiers::GC GetGCAttrTypeForType(ASTContext &Ctx, QualType FQT,
                                           bool pointee = false) {""",
"addCompilerUsedOrRetainedGlobal(CodeGen::CodeGenModule &CGM"),

# 5. No vtable (objc_msgSend_fixup) dispatch on wasm.
("""bool CGObjCNonFragileABIMac::isVTableDispatchedSelector(Selector Sel) {
  // At various points we've experimented with using vtable-based
  // dispatch for all methods.
  switch (CGM.getCodeGenOpts().getObjCDispatchMethod()) {""",
"""bool CGObjCNonFragileABIMac::isVTableDispatchedSelector(Selector Sel) {
  // libobjc-wasm: there are no objc_msgSend_fixup trampolines.
  if (CGM.getTriple().isOSBinFormatWasm())
    return false;

  // At various points we've experimented with using vtable-based
  // dispatch for all methods.
  switch (CGM.getCodeGenOpts().getObjCDispatchMethod()) {""",
"there are no objc_msgSend_fixup trampolines"),
])

# Route every compiler-used metadata global through the helper (after its definition).
p = os.path.join(root, CG)
s = open(p).read()
head, sep, tail = s.partition("static Qualifiers::GC GetGCAttrTypeForType(")
if "CGM.addCompilerUsedGlobal(" in tail:
    tail = tail.replace("CGM.addCompilerUsedGlobal(", "addCompilerUsedOrRetainedGlobal(CGM, ")
    open(p, "w").write(head + sep + tail)
    print("patched " + CG + " (compiler-used globals)")

# ---------------------------------------------------------------- swift
patch("swift/include/swift/Runtime/RuntimeFunctions.def", [
("""FUNCTION(ObjCMsgSend, objc2, objc_msgSend,""",
"""// libobjc-wasm: lookup-based dispatch for targets without messenger trampolines.
FUNCTION(ObjCMsgLookup, objc2, objc_msgLookup,
         C_CC, AlwaysAvailable,
         RETURNS(PtrTy), ARGS(ObjCPtrTy, ObjCSELTy), ATTRS(NoUnwind),
         EFFECT(RuntimeEffect::ObjectiveC),
         UNKNOWN_MEMEFFECTS)
FUNCTION(ObjCMsgLookupSuper, objc2, objc_msgLookupSuper,
         C_CC, AlwaysAvailable,
         RETURNS(PtrTy), ARGS(ObjCSuperPtrTy, ObjCSELTy), ATTRS(NoUnwind),
         EFFECT(RuntimeEffect::ObjectiveC),
         UNKNOWN_MEMEFFECTS)
FUNCTION(ObjCMsgLookupSuper2, objc2, objc_msgLookupSuper2,
         C_CC, AlwaysAvailable,
         RETURNS(PtrTy), ARGS(ObjCSuperPtrTy, ObjCSELTy), ATTRS(NoUnwind),
         EFFECT(RuntimeEffect::ObjectiveC),
         UNKNOWN_MEMEFFECTS)

FUNCTION(ObjCMsgSend, objc2, objc_msgSend,""",
"FUNCTION(ObjCMsgLookup, objc2, objc_msgLookup,"),
])

patch("swift/lib/IRGen/SwiftTargetInfo.cpp", [
("""static void configureWasm32(IRGenModule &IGM, const llvm::Triple &triple,
                            SwiftTargetInfo &target) {
  target.LeastValidPointerValue =
    SWIFT_ABI_WASM32_LEAST_VALID_POINTER;
}""",
"""static void configureWasm32(IRGenModule &IGM, const llvm::Triple &triple,
                            SwiftTargetInfo &target) {
  target.LeastValidPointerValue =
    SWIFT_ABI_WASM32_LEAST_VALID_POINTER;
  // libobjc-wasm: messages are dispatched by looking up the IMP, which is then
  // called with the method's own signature; there are no stret messengers.
  target.ObjCUseStret = false;
}""",
"target.ObjCUseStret = false;\n}"),
])

patch("swift/lib/IRGen/GenObjC.cpp", [
("""  auto kind = methodInfo.getMessageKind();

  Signature sig = IGF.IGM.getSignature(info.OrigFnType);
  bool indirectResult =
    sig.getForeignInfo().ClangInfo->getReturnInfo().isIndirect();
  if (kind != ObjCMessageKind::Normal) {
    sig.setType(getMsgSendSuperTy(IGF.IGM, sig.getType(), indirectResult));
  }

  // Create the appropriate messenger function.
  // FIXME: this needs to be target-specific.  Ask Clang for it!
  llvm::Constant *messenger = [&]() -> llvm::Constant* {""",
"""  auto kind = methodInfo.getMessageKind();

  // libobjc-wasm: WebAssembly type-checks indirect calls, so the IMP is looked
  // up (objc_msgLookup / objc_msgLookupSuper2) and called with the method's own
  // signature, receiving `self` rather than an objc_super struct.
  const bool useMsgLookup = IGF.IGM.Triple.isOSBinFormatWasm();

  Signature sig = IGF.IGM.getSignature(info.OrigFnType);
  bool indirectResult =
    sig.getForeignInfo().ClangInfo->getReturnInfo().isIndirect();
  if (kind != ObjCMessageKind::Normal && !useMsgLookup) {
    sig.setType(getMsgSendSuperTy(IGF.IGM, sig.getType(), indirectResult));
  }

  // Create the appropriate messenger function.
  // FIXME: this needs to be target-specific.  Ask Clang for it!
  llvm::Constant *messenger = useMsgLookup ? nullptr : [&]() -> llvm::Constant* {""",
"const bool useMsgLookup = IGF.IGM.Triple.isOSBinFormatWasm();"),

("""  }();

  messenger = llvm::ConstantExpr::getBitCast(messenger, IGF.IGM.PtrTy);

  // super.constructor references an instance method (even though the""",
"""  }();

  if (messenger)
    messenger = llvm::ConstantExpr::getBitCast(messenger, IGF.IGM.PtrTy);

  // super.constructor references an instance method (even though the""",
"  if (messenger)\n    messenger = llvm::ConstantExpr::getBitCast"),

("""  // Compute the selector.
  Selector selector(method);
  llvm::Value *selectorValue = IGF.emitObjCSelectorRefLoad(selector.str());

  auto fn =
      FunctionPointer::forDirect(FunctionPointer::Kind::Function, messenger,
                                 /*secondaryValue*/ nullptr, sig,
                                 /*useSignature*/ true);
  return Callee(std::move(info), fn, receiverValue, selectorValue);""",
"""  // Compute the selector.
  Selector selector(method);
  llvm::Value *selectorValue = IGF.emitObjCSelectorRefLoad(selector.str());

  if (useMsgLookup) {
    FunctionPointer lookup = [&]() -> FunctionPointer {
      switch (kind) {
      case ObjCMessageKind::Normal:
        return IGF.IGM.getObjCMsgLookupFunctionPointer();
      case ObjCMessageKind::Peer:
        return IGF.IGM.getObjCMsgLookupSuperFunctionPointer();
      case ObjCMessageKind::Super:
        return IGF.IGM.getObjCMsgLookupSuper2FunctionPointer();
      }
      llvm_unreachable("unhandled kind");
    }();
    llvm::Value *imp =
        IGF.Builder.CreateCall(lookup, {receiverValue, selectorValue});
    auto impFn = FunctionPointer::createUnsigned(
        FunctionPointer::Kind::Function, imp, sig, /*useSignature*/ true);
    // The IMP takes the receiver itself, even for super sends.
    return Callee(std::move(info), impFn, selfValue, selectorValue);
  }

  auto fn =
      FunctionPointer::forDirect(FunctionPointer::Kind::Function, messenger,
                                 /*secondaryValue*/ nullptr, sig,
                                 /*useSignature*/ true);
  return Callee(std::move(info), fn, receiverValue, selectorValue);""",
"IGF.IGM.getObjCMsgLookupFunctionPointer()"),
])

patch("swift/lib/IRGen/IRGenSIL.cpp", [
("""  llvm::Constant *messenger = IGM.getObjCMsgSendFn();
  llvm::Type *argTys[] = {
    IGM.ObjCPtrTy,
    IGM.Int8PtrTy,
    IGM.Int8PtrTy,
  };
  auto respondsToSelectorTy = llvm::FunctionType::get(IGM.Int1Ty, argTys,
                                                      /*isVarArg*/ false);
  messenger = llvm::ConstantExpr::getBitCast(messenger, IGM.PtrTy);
  llvm::CallInst *call = Builder.IRBuilderBase::CreateCall(
      respondsToSelectorTy, messenger, {object, respondsToSelector, loadSel});""",
"""  llvm::Type *argTys[] = {
    IGM.ObjCPtrTy,
    IGM.Int8PtrTy,
    IGM.Int8PtrTy,
  };
  auto respondsToSelectorTy = llvm::FunctionType::get(IGM.Int1Ty, argTys,
                                                      /*isVarArg*/ false);
  llvm::Value *messenger;
  if (IGM.Triple.isOSBinFormatWasm()) {
    // libobjc-wasm: look the IMP up and call it with the right signature.
    messenger = Builder.CreateCall(IGM.getObjCMsgLookupFunctionPointer(),
                                   {object, respondsToSelector});
  } else {
    messenger =
        llvm::ConstantExpr::getBitCast(IGM.getObjCMsgSendFn(), IGM.PtrTy);
  }
  llvm::CallInst *call = Builder.CreateCall(
      respondsToSelectorTy, messenger, {object, respondsToSelector, loadSel});""",
"IGM.getObjCMsgLookupFunctionPointer(),\n                                   {object, respondsToSelector}"),
])
patch("swift/lib/IRGen/GenKeyPath.cpp", [('      if (declRef.isForeign) {\n        assert(IGM.ObjCInterop && "foreign keypath component w/o objc interop?!");\n        idKind = KeyPathComponentHeader::Pointer;\n        // FIXME: In non-JIT mode, ideally we would just refer to the selector', '      if (declRef.isForeign && IGM.getOptions().CompactAbsoluteFunctionPointer) {\n        // libobjc-wasm: WebAssembly cannot store a relative reference to a\n        // function in data. Refer to the selector reference slot instead; the\n        // runtime loads the uniqued selector through it at instantiation time.\n        assert(IGM.ObjCInterop && "foreign keypath component w/o objc interop?!");\n        idKind = KeyPathComponentHeader::Pointer;\n        idValue = IGM.getAddrOfObjCSelectorRef(declRef);\n        idResolution = KeyPathComponentHeader::IndirectPointer;\n      } else if (declRef.isForeign) {\n        assert(IGM.ObjCInterop && "foreign keypath component w/o objc interop?!");\n        idKind = KeyPathComponentHeader::Pointer;\n        // FIXME: In non-JIT mode, ideally we would just refer to the selector', "libobjc-wasm: WebAssembly cannot store a relative reference")])
patch("swift/lib/SILGen/SILGenFunction.cpp", [('  case ArtificialMainKind::UIApplicationMain: {\n    // Emit a UIKit main.\n    // return UIApplicationMain(C_ARGC, C_ARGV, nil, ClassName);\n\n    auto *mainClass = cast<NominalTypeDecl>(mainDecl);\n\n    CanType NSStringTy = SGM.Types.getNSStringType();', '  case ArtificialMainKind::UIApplicationMain: {\n    // Emit a UIKit main.\n    // return UIApplicationMain(C_ARGC, C_ARGV, nil, ClassName);\n\n    auto *mainClass = cast<NominalTypeDecl>(mainDecl);\n\n    // libobjc-wasm: without Foundation\'s NSString there is no C UIApplicationMain\n    // to call. Call a Swift one from the UIKit module instead:\n    //   UIApplicationMain(_: Int32, _: argv, _: String?, _: String?) -> Int32\n    // with the class name from the module\'s NSStringFromClass(_: AnyClass) -> String.\n    if (!SGM.Types.getNSStringType()) {\n      ASTContext &ctx = getASTContext();\n      ModuleDecl *UIKitSwift = ctx.getLoadedModule(ctx.getIdentifier("UIKit"));\n      assert(UIKitSwift && "couldn\'t find a UIKit module for @UIApplicationMain");\n      auto lookupFunc = [&](StringRef name, unsigned numParams) -> FuncDecl * {\n        SmallVector<ValueDecl *, 4> found;\n        UIKitSwift->lookupQualified(UIKitSwift,\n                                    DeclNameRef(ctx.getIdentifier(name)),\n                                    SourceLoc(), NL_QualifiedDefault, found);\n        for (auto *vd : found)\n          if (auto *fd = dyn_cast<FuncDecl>(vd))\n            if (fd->getClangNode().isNull() &&\n                fd->getParameters()->size() == numParams)\n              return fd;\n        return nullptr;\n      };\n      FuncDecl *mainFn = lookupFunc("UIApplicationMain", 4);\n      FuncDecl *nameFn = lookupFunc("NSStringFromClass", 1);\n      assert(mainFn && nameFn &&\n             "UIKit lacks a Swift UIApplicationMain or NSStringFromClass");\n\n      // NSStringFromClass(Self.self as AnyClass)\n      CanType mainClassTy =\n          mainClass->getDeclaredInterfaceType()->getCanonicalType();\n      CanType metaTy =\n          CanMetatypeType::get(mainClassTy, MetatypeRepresentation::Thick);\n      CanType anyClassTy = CanExistentialMetatypeType::get(\n          ctx.getAnyObjectType(), MetatypeRepresentation::Thick);\n      auto conformances = collectExistentialConformances(metaTy, anyClassTy);\n      SILValue meta = B.createMetatype(\n          mainClass, SILType::getPrimitiveObjectType(metaTy));\n      meta = B.createInitExistentialMetatype(\n          mainClass, meta, SILType::getPrimitiveObjectType(anyClassTy),\n          conformances);\n      ManagedValue metaArg[] = {\n          ManagedValue::forObjectRValueWithoutOwnership(meta)};\n      ManagedValue name =\n          std::move(emitApplyOfLibraryIntrinsic(mainClass, nameFn,\n                                                SubstitutionMap(), metaArg,\n                                                SGFContext()))\n              .getAsSingleValue(*this, mainClass);\n\n      CanType optStringTy =\n          OptionalType::get(ctx.getStringDecl()->getDeclaredInterfaceType())\n              ->getCanonicalType();\n      auto &optStringTL = getTypeLowering(optStringTy);\n      ManagedValue optName = getOptionalSomeValue(mainClass, name, optStringTL);\n      ManagedValue nilName = emitManagedRValueWithCleanup(\n          getOptionalNoneValue(mainClass, optStringTL));\n\n      ManagedValue mainArgs[] = {\n          ManagedValue::forObjectRValueWithoutOwnership(argc),\n          ManagedValue::forObjectRValueWithoutOwnership(argv), nilName,\n          optName};\n      (void)emitApplyOfLibraryIntrinsic(mainClass, mainFn, SubstitutionMap(),\n                                        mainArgs, SGFContext());\n\n      SILValue r = B.createIntegerLiteral(\n          mainClass, SILType::getBuiltinIntegerType(32, ctx), 0);\n      auto rType = F.getConventions().getSingleSILResultType(\n          B.getTypeExpansionContext());\n      if (r->getType() != rType)\n        r = B.createStruct(mainClass, rType, r);\n      Cleanups.emitCleanupsForReturn(mainClass, NotForUnwind);\n      B.createReturn(mainClass, r);\n      return;\n    }\n\n    CanType NSStringTy = SGM.Types.getNSStringType();', "libobjc-wasm: without Foundation")])
print("done")
