//===- Attributes.inc - Generated tablegen for attributes --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all the LLVM attributes.  Generated from Attributes.td
// by llvm-tblgen.  Hand-adapted for the llvm-kain stripped build.
//
//===----------------------------------------------------------------------===//

#ifdef GET_ATTR_ENUM
// Enum attributes (simple boolean flags) - in .td definition order
      AllocAlign,
      AllocatedPointer,
      AlwaysInline,
      Builtin,
      NoUndef,
      Cold,
      Convergent,
      Hot,
      DisableSanitizerInstrumentation,
      Flatten,
      FnRetThunkExtern,
      HybridPatchable,
      InlineHint,
      InReg,
      JumpTable,
      MinSize,
      Naked,
      Nest,
      NoAlias,
      NoBuiltin,
      NoCallback,
      NoCreateUndefOrPoison,
      NoDivergenceSource,
      NoDuplicate,
      NoExt,
      NoFree,
      DeadOnUnwind,
      NoImplicitFloat,
      NoInline,
      NoOutline,
      NoIPA,
      NonLazyBind,
      NoMerge,
      NonNull,
      NoRecurse,
      NoRedZone,
      NoReturn,
      NoSync,
      NoCfCheck,
      NoProfile,
      SkipProfile,
      NoUnwind,
      NoSanitizeBounds,
      NoSanitizeCoverage,
      NullPointerIsValid,
      OptimizeForDebugging,
      OptForFuzzing,
      OptimizeForSize,
      OptimizeNone,
      ReadNone,
      ReadOnly,
      Returned,
      ImmArg,
      ReturnsTwice,
      SafeStack,
      ShadowCallStack,
      SExt,
      Speculatable,
      StackProtect,
      StackProtectReq,
      StackProtectStrong,
      StrictFP,
      SanitizeAddress,
      SanitizeThread,
      SanitizeType,
      SanitizeMemory,
      SanitizeHWAddress,
      SanitizeMemTag,
      SanitizeNumericalStability,
      SanitizeRealtime,
      SanitizeRealtimeBlocking,
      SanitizeAllocToken,
      SpeculativeLoadHardening,
      SwiftError,
      SwiftSelf,
      SwiftAsync,
      WillReturn,
      Writable,
      WriteOnly,
      ZExt,
      MustProgress,
      PresplitCoroutine,
      CoroDestroyOnlyWhenComplete,
      CoroElideSafe,
      // Sentinel: first and last enum attributes
      FirstEnumAttr = AllocAlign,
      LastEnumAttr = CoroElideSafe,

// Int attributes (carry an integer value) - in .td definition order
      Alignment,
      AllocKind,
      AllocSize,
      Dereferenceable,
      DereferenceableOrNull,
      Memory,
      NoFPClass,
      Captures,
      DeadOnReturn,
      StackAlignment,
      UWTable,
      VScaleRange,
      DenormalFPEnv,
      // Sentinel: first and last int attributes
      FirstIntAttr = Alignment,
      LastIntAttr = DenormalFPEnv,

// Type attributes (carry a Type*) - in .td definition order
      ByVal,
      ByRef,
      ElementType,
      InAlloca,
      Preallocated,
      StructRet,
      // Sentinel: first and last type attributes
      FirstTypeAttr = ByVal,
      LastTypeAttr = StructRet,

// ConstantRange attributes (carry a ConstantRange)
      Range,
      // Sentinel: first and last constant range attributes
      FirstConstantRangeAttr = Range,
      LastConstantRangeAttr = Range,

// ConstantRangeList attributes (carry ArrayRef<ConstantRange>)
      Initializes,
      // Sentinel: first and last constant range list attributes
      FirstConstantRangeListAttr = Initializes,
      LastConstantRangeListAttr = Initializes,
#endif  // GET_ATTR_ENUM

#ifdef GET_ATTR_NAMES
ATTRIBUTE_ENUM(AllocAlign, allocalign)
ATTRIBUTE_ENUM(AllocatedPointer, allocptr)
ATTRIBUTE_ENUM(AlwaysInline, alwaysinline)
ATTRIBUTE_ENUM(Builtin, builtin)
ATTRIBUTE_ENUM(NoUndef, noundef)
ATTRIBUTE_ENUM(Cold, cold)
ATTRIBUTE_ENUM(Convergent, convergent)
ATTRIBUTE_ENUM(Hot, hot)
ATTRIBUTE_ENUM(DisableSanitizerInstrumentation, disable_sanitizer_instrumentation)
ATTRIBUTE_ENUM(Flatten, flatten)
ATTRIBUTE_ENUM(FnRetThunkExtern, fn_ret_thunk_extern)
ATTRIBUTE_ENUM(HybridPatchable, hybrid_patchable)
ATTRIBUTE_ENUM(InlineHint, inlinehint)
ATTRIBUTE_ENUM(InReg, inreg)
ATTRIBUTE_ENUM(JumpTable, jumptable)
ATTRIBUTE_ENUM(MinSize, minsize)
ATTRIBUTE_ENUM(Naked, naked)
ATTRIBUTE_ENUM(Nest, nest)
ATTRIBUTE_ENUM(NoAlias, noalias)
ATTRIBUTE_ENUM(NoBuiltin, nobuiltin)
ATTRIBUTE_ENUM(NoCallback, nocallback)
ATTRIBUTE_ENUM(NoCreateUndefOrPoison, nocreateundeforpoison)
ATTRIBUTE_ENUM(NoDivergenceSource, nodivergencesource)
ATTRIBUTE_ENUM(NoDuplicate, noduplicate)
ATTRIBUTE_ENUM(NoExt, noext)
ATTRIBUTE_ENUM(NoFree, nofree)
ATTRIBUTE_ENUM(DeadOnUnwind, dead_on_unwind)
ATTRIBUTE_ENUM(NoImplicitFloat, noimplicitfloat)
ATTRIBUTE_ENUM(NoInline, noinline)
ATTRIBUTE_ENUM(NoOutline, nooutline)
ATTRIBUTE_ENUM(NoIPA, noipa)
ATTRIBUTE_ENUM(NonLazyBind, nonlazybind)
ATTRIBUTE_ENUM(NoMerge, nomerge)
ATTRIBUTE_ENUM(NonNull, nonnull)
ATTRIBUTE_ENUM(NoRecurse, norecurse)
ATTRIBUTE_ENUM(NoRedZone, noredzone)
ATTRIBUTE_ENUM(NoReturn, noreturn)
ATTRIBUTE_ENUM(NoSync, nosync)
ATTRIBUTE_ENUM(NoCfCheck, nocf_check)
ATTRIBUTE_ENUM(NoProfile, noprofile)
ATTRIBUTE_ENUM(SkipProfile, skipprofile)
ATTRIBUTE_ENUM(NoUnwind, nounwind)
ATTRIBUTE_ENUM(NoSanitizeBounds, nosanitize_bounds)
ATTRIBUTE_ENUM(NoSanitizeCoverage, nosanitize_coverage)
ATTRIBUTE_ENUM(NullPointerIsValid, null_pointer_is_valid)
ATTRIBUTE_ENUM(OptimizeForDebugging, optdebug)
ATTRIBUTE_ENUM(OptForFuzzing, optforfuzzing)
ATTRIBUTE_ENUM(OptimizeForSize, optsize)
ATTRIBUTE_ENUM(OptimizeNone, optnone)
ATTRIBUTE_ENUM(ReadNone, readnone)
ATTRIBUTE_ENUM(ReadOnly, readonly)
ATTRIBUTE_ENUM(Returned, returned)
ATTRIBUTE_ENUM(ImmArg, immarg)
ATTRIBUTE_ENUM(ReturnsTwice, returns_twice)
ATTRIBUTE_ENUM(SafeStack, safestack)
ATTRIBUTE_ENUM(ShadowCallStack, shadowcallstack)
ATTRIBUTE_ENUM(SExt, signext)
ATTRIBUTE_ENUM(Speculatable, speculatable)
ATTRIBUTE_ENUM(StackProtect, ssp)
ATTRIBUTE_ENUM(StackProtectReq, sspreq)
ATTRIBUTE_ENUM(StackProtectStrong, sspstrong)
ATTRIBUTE_ENUM(StrictFP, strictfp)
ATTRIBUTE_ENUM(SanitizeAddress, sanitize_address)
ATTRIBUTE_ENUM(SanitizeThread, sanitize_thread)
ATTRIBUTE_ENUM(SanitizeType, sanitize_type)
ATTRIBUTE_ENUM(SanitizeMemory, sanitize_memory)
ATTRIBUTE_ENUM(SanitizeHWAddress, sanitize_hwaddress)
ATTRIBUTE_ENUM(SanitizeMemTag, sanitize_memtag)
ATTRIBUTE_ENUM(SanitizeNumericalStability, sanitize_numerical_stability)
ATTRIBUTE_ENUM(SanitizeRealtime, sanitize_realtime)
ATTRIBUTE_ENUM(SanitizeRealtimeBlocking, sanitize_realtime_blocking)
ATTRIBUTE_ENUM(SanitizeAllocToken, sanitize_alloc_token)
ATTRIBUTE_ENUM(SpeculativeLoadHardening, speculative_load_hardening)
ATTRIBUTE_ENUM(SwiftError, swifterror)
ATTRIBUTE_ENUM(SwiftSelf, swiftself)
ATTRIBUTE_ENUM(SwiftAsync, swiftasync)
ATTRIBUTE_ENUM(WillReturn, willreturn)
ATTRIBUTE_ENUM(Writable, writable)
ATTRIBUTE_ENUM(WriteOnly, writeonly)
ATTRIBUTE_ENUM(ZExt, zeroext)
ATTRIBUTE_ENUM(MustProgress, mustprogress)
ATTRIBUTE_ENUM(PresplitCoroutine, presplitcoroutine)
ATTRIBUTE_ENUM(CoroDestroyOnlyWhenComplete, coro_only_destroy_when_complete)
ATTRIBUTE_ENUM(CoroElideSafe, coro_elide_safe)
ATTRIBUTE_ENUM(Alignment, align)
ATTRIBUTE_ENUM(AllocKind, allockind)
ATTRIBUTE_ENUM(AllocSize, allocsize)
ATTRIBUTE_ENUM(Dereferenceable, dereferenceable)
ATTRIBUTE_ENUM(DereferenceableOrNull, dereferenceable_or_null)
ATTRIBUTE_ENUM(Memory, memory)
ATTRIBUTE_ENUM(NoFPClass, nofpclass)
ATTRIBUTE_ENUM(Captures, captures)
ATTRIBUTE_ENUM(DeadOnReturn, dead_on_return)
ATTRIBUTE_ENUM(StackAlignment, alignstack)
ATTRIBUTE_ENUM(UWTable, uwtable)
ATTRIBUTE_ENUM(VScaleRange, vscale_range)
ATTRIBUTE_ENUM(DenormalFPEnv, denormal_fpenv)
ATTRIBUTE_ENUM(ByVal, byval)
ATTRIBUTE_ENUM(ByRef, byref)
ATTRIBUTE_ENUM(ElementType, elementtype)
ATTRIBUTE_ENUM(InAlloca, inalloca)
ATTRIBUTE_ENUM(Preallocated, preallocated)
ATTRIBUTE_ENUM(StructRet, sret)
ATTRIBUTE_ENUM(Range, range)
ATTRIBUTE_ENUM(Initializes, initializes)
#endif  // GET_ATTR_NAMES

#ifdef GET_ATTR_PROP_TABLE
// Property table indexed by (AttrKind - 1).
// Properties: FnAttr(1), ParamAttr(2), RetAttr(4)
// Intersection: IntersectPreserve(0<<3=0), IntersectAnd(1<<3=8), IntersectMin(2<<3=16), IntersectCustom(3<<3=24)
// Combined bitmask: FnAttr|ParamAttr|RetAttr|IntersectMask
//
// Enum attributes
static constexpr std::array<unsigned, 106> AttrPropTable = {{
  /*  1: AllocAlign=FnAttr|IntersectAnd */ 1|8,
  /*  2: AllocatedPointer=FnAttr|IntersectAnd */ 1|8,
  /*  3: AlwaysInline=FnAttr|IntersectPreserve */ 1|0,
  /*  4: Builtin=FnAttr|IntersectPreserve */ 1|0,
  /*  5: NoUndef=ParamAttr|RetAttr|IntersectAnd */ 2|4|8,
  /*  6: Cold=FnAttr|IntersectAnd */ 1|8,
  /*  7: Convergent=FnAttr|IntersectPreserve */ 1|0,
  /*  8: Hot=FnAttr|IntersectAnd */ 1|8,
  /*  9: DisableSanitizerInstrumentation=FnAttr|IntersectPreserve */ 1|0,
  /* 10: Flatten=FnAttr|IntersectPreserve */ 1|0,
  /* 11: FnRetThunkExtern=FnAttr|IntersectPreserve */ 1|0,
  /* 12: HybridPatchable=FnAttr|IntersectPreserve */ 1|0,
  /* 13: InlineHint=FnAttr|IntersectAnd */ 1|8,
  /* 14: InReg=ParamAttr|RetAttr|IntersectPreserve */ 2|4|0,
  /* 15: JumpTable=FnAttr|IntersectPreserve */ 1|0,
  /* 16: MinSize=FnAttr|IntersectPreserve */ 1|0,
  /* 17: Naked=FnAttr|IntersectPreserve */ 1|0,
  /* 18: Nest=ParamAttr|IntersectPreserve */ 2|0,
  /* 19: NoAlias=ParamAttr|RetAttr|IntersectAnd */ 2|4|8,
  /* 20: NoBuiltin=FnAttr|IntersectPreserve */ 1|0,
  /* 21: NoCallback=FnAttr|IntersectAnd */ 1|8,
  /* 22: NoCreateUndefOrPoison=FnAttr|IntersectAnd */ 1|8,
  /* 23: NoDivergenceSource=FnAttr|IntersectAnd */ 1|8,
  /* 24: NoDuplicate=FnAttr|IntersectPreserve */ 1|0,
  /* 25: NoExt=ParamAttr|RetAttr|IntersectPreserve */ 2|4|0,
  /* 26: NoFree=FnAttr|ParamAttr|IntersectAnd */ 1|2|8,
  /* 27: DeadOnUnwind=ParamAttr|IntersectAnd */ 2|8,
  /* 28: NoImplicitFloat=FnAttr|IntersectPreserve */ 1|0,
  /* 29: NoInline=FnAttr|IntersectPreserve */ 1|0,
  /* 30: NoOutline=FnAttr|IntersectPreserve */ 1|0,
  /* 31: NoIPA=FnAttr|IntersectPreserve */ 1|0,
  /* 32: NonLazyBind=FnAttr|IntersectPreserve */ 1|0,
  /* 33: NoMerge=FnAttr|IntersectPreserve */ 1|0,
  /* 34: NonNull=ParamAttr|RetAttr|IntersectAnd */ 2|4|8,
  /* 35: NoRecurse=FnAttr|IntersectAnd */ 1|8,
  /* 36: NoRedZone=FnAttr|IntersectPreserve */ 1|0,
  /* 37: NoReturn=FnAttr|IntersectAnd */ 1|8,
  /* 38: NoSync=FnAttr|IntersectAnd */ 1|8,
  /* 39: NoCfCheck=FnAttr|IntersectPreserve */ 1|0,
  /* 40: NoProfile=FnAttr|IntersectPreserve */ 1|0,
  /* 41: SkipProfile=FnAttr|IntersectPreserve */ 1|0,
  /* 42: NoUnwind=FnAttr|IntersectAnd */ 1|8,
  /* 43: NoSanitizeBounds=FnAttr|IntersectPreserve */ 1|0,
  /* 44: NoSanitizeCoverage=FnAttr|IntersectPreserve */ 1|0,
  /* 45: NullPointerIsValid=FnAttr|IntersectPreserve */ 1|0,
  /* 46: OptimizeForDebugging=FnAttr|IntersectPreserve */ 1|0,
  /* 47: OptForFuzzing=FnAttr|IntersectPreserve */ 1|0,
  /* 48: OptimizeForSize=FnAttr|IntersectPreserve */ 1|0,
  /* 49: OptimizeNone=FnAttr|IntersectPreserve */ 1|0,
  /* 50: ReadNone=ParamAttr|IntersectAnd */ 2|8,
  /* 51: ReadOnly=ParamAttr|IntersectAnd */ 2|8,
  /* 52: Returned=ParamAttr|IntersectAnd */ 2|8,
  /* 53: ImmArg=ParamAttr|IntersectPreserve */ 2|0,
  /* 54: ReturnsTwice=FnAttr|IntersectPreserve */ 1|0,
  /* 55: SafeStack=FnAttr|IntersectPreserve */ 1|0,
  /* 56: ShadowCallStack=FnAttr|IntersectPreserve */ 1|0,
  /* 57: SExt=ParamAttr|RetAttr|IntersectPreserve */ 2|4|0,
  /* 58: Speculatable=FnAttr|IntersectAnd */ 1|8,
  /* 59: StackProtect=FnAttr|IntersectPreserve */ 1|0,
  /* 60: StackProtectReq=FnAttr|IntersectPreserve */ 1|0,
  /* 61: StackProtectStrong=FnAttr|IntersectPreserve */ 1|0,
  /* 62: StrictFP=FnAttr|IntersectPreserve */ 1|0,
  /* 63: SanitizeAddress=FnAttr|IntersectPreserve */ 1|0,
  /* 64: SanitizeThread=FnAttr|IntersectPreserve */ 1|0,
  /* 65: SanitizeType=FnAttr|IntersectPreserve */ 1|0,
  /* 66: SanitizeMemory=FnAttr|IntersectPreserve */ 1|0,
  /* 67: SanitizeHWAddress=FnAttr|IntersectPreserve */ 1|0,
  /* 68: SanitizeMemTag=FnAttr|IntersectPreserve */ 1|0,
  /* 69: SanitizeNumericalStability=FnAttr|IntersectPreserve */ 1|0,
  /* 70: SanitizeRealtime=FnAttr|IntersectPreserve */ 1|0,
  /* 71: SanitizeRealtimeBlocking=FnAttr|IntersectPreserve */ 1|0,
  /* 72: SanitizeAllocToken=FnAttr|IntersectPreserve */ 1|0,
  /* 73: SpeculativeLoadHardening=FnAttr|IntersectPreserve */ 1|0,
  /* 74: SwiftError=ParamAttr|IntersectPreserve */ 2|0,
  /* 75: SwiftSelf=ParamAttr|IntersectPreserve */ 2|0,
  /* 76: SwiftAsync=ParamAttr|IntersectPreserve */ 2|0,
  /* 77: WillReturn=FnAttr|IntersectAnd */ 1|8,
  /* 78: Writable=ParamAttr|IntersectAnd */ 2|8,
  /* 79: WriteOnly=ParamAttr|IntersectAnd */ 2|8,
  /* 80: ZExt=ParamAttr|RetAttr|IntersectPreserve */ 2|4|0,
  /* 81: MustProgress=FnAttr|IntersectAnd */ 1|8,
  /* 82: PresplitCoroutine=FnAttr|IntersectPreserve */ 1|0,
  /* 83: CoroDestroyOnlyWhenComplete=FnAttr|IntersectPreserve */ 1|0,
  /* 84: CoroElideSafe=FnAttr|IntersectPreserve */ 1|0,
  /* 85: Alignment=ParamAttr|RetAttr|IntersectCustom */ 2|4|24,
  /* 86: AllocKind=FnAttr|IntersectPreserve */ 1|0,
  /* 87: AllocSize=FnAttr|IntersectPreserve */ 1|0,
  /* 88: Dereferenceable=ParamAttr|RetAttr|IntersectMin */ 2|4|16,
  /* 89: DereferenceableOrNull=ParamAttr|RetAttr|IntersectMin */ 2|4|16,
  /* 90: Memory=FnAttr|IntersectCustom */ 1|24,
  /* 91: NoFPClass=ParamAttr|RetAttr|IntersectCustom */ 2|4|24,
  /* 92: Captures=ParamAttr|IntersectCustom */ 2|24,
  /* 93: DeadOnReturn=ParamAttr|IntersectMin */ 2|16,
  /* 94: StackAlignment=FnAttr|ParamAttr|RetAttr|IntersectPreserve */ 1|2|4|0,
  /* 95: UWTable=FnAttr|IntersectPreserve */ 1|0,
  /* 96: VScaleRange=FnAttr|IntersectPreserve */ 1|0,
  /* 97: DenormalFPEnv=FnAttr|IntersectPreserve */ 1|0,
  /* 98: ByVal=ParamAttr|IntersectPreserve */ 2|0,
  /* 99: ByRef=ParamAttr|IntersectPreserve */ 2|0,
  /*100: ElementType=RetAttr|ParamAttr|IntersectPreserve */ 4|2|0,
  /*101: InAlloca=ParamAttr|IntersectPreserve */ 2|0,
  /*102: Preallocated=FnAttr|ParamAttr|IntersectPreserve */ 1|2|0,
  /*103: StructRet=ParamAttr|IntersectPreserve */ 2|0,
  /*104: Range=ParamAttr|RetAttr|IntersectCustom */ 2|4|24,
  /*105: Initializes=ParamAttr|IntersectPreserve */ 2|0,
}};
#endif  // GET_ATTR_PROP_TABLE

#ifdef GET_ATTR_COMPAT_FUNC
// Stub: no attribute compatibility functions needed for Kain's stripped LLVM.
// These functions check which attributes are compatible for inlining/merging.
static bool hasCompatibleFnAttrs(const Function &Caller, const Function &Callee) {
  // Conservative stub: always return true (all attributes compatible).
  // Real implementation would check each attribute's merge rules.
  (void)Caller;
  (void)Callee;
  return true;
}

static void mergeFnAttrs(Function &Caller, const Function &Callee) {
  // Conservative stub: do nothing.
  (void)Caller;
  (void)Callee;
}
#endif  // GET_ATTR_COMPAT_FUNC
