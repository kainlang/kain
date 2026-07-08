// Stub: arm_sve_typeflags.inc - minimal definitions for C-only clang
// This file is included multiple times with different LLVM_GET_SVE_* guards.

#ifdef LLVM_GET_SVE_TYPEFLAGS
// Bitfield mask constants for SVETypeFlags.
// These are static constexpr members of the SVETypeFlags struct.
static constexpr unsigned long long EltTypeMask                = 0x7ULL;
static constexpr unsigned long long MemEltTypeMask              = 0x38ULL;
static constexpr unsigned long long MergeTypeMask               = 0x1C0ULL;
static constexpr unsigned long long SplatOperandMask            = 0x600ULL;
static constexpr unsigned long long IsLoad                      = 0x800ULL;
static constexpr unsigned long long IsStore                     = 0x1000ULL;
static constexpr unsigned long long IsGatherLoad                = 0x2000ULL;
static constexpr unsigned long long IsScatterStore              = 0x4000ULL;
static constexpr unsigned long long IsStructLoad                = 0x8000ULL;
static constexpr unsigned long long IsStructStore               = 0x10000ULL;
static constexpr unsigned long long IsZExtReturn                = 0x20000ULL;
static constexpr unsigned long long IsByteIndexed               = 0x40000ULL;
static constexpr unsigned long long IsOverloadNone              = 0x80000ULL;
static constexpr unsigned long long IsOverloadWhileOrMultiVecCvt = 0x100000ULL;
static constexpr unsigned long long OverloadKindMask            = 0x600000ULL;
static constexpr unsigned long long IsOverloadWhileRW           = 0x200000ULL;
static constexpr unsigned long long IsOverloadFirstandLast      = 0x400000ULL;
static constexpr unsigned long long IsOverloadDefaultAndOp0     = 0x600000ULL;
static constexpr unsigned long long IsPrefetch                  = 0x800000ULL;
static constexpr unsigned long long ReverseCompare              = 0x1000000ULL;
static constexpr unsigned long long IsAppendSVALL               = 0x2000000ULL;
static constexpr unsigned long long IsInsertOp1SVALL            = 0x4000000ULL;
static constexpr unsigned long long IsGatherPrefetch            = 0x8000000ULL;
static constexpr unsigned long long ReverseUSDOT                = 0x10000000ULL;
static constexpr unsigned long long ReverseMergeAnyBinOp        = 0x20000000ULL;
static constexpr unsigned long long ReverseMergeAnyAccOp        = 0x40000000ULL;
static constexpr unsigned long long IsUndef                     = 0x80000000ULL;
static constexpr unsigned long long IsTupleCreate               = 0x100000000ULL;
static constexpr unsigned long long IsTupleGet                  = 0x200000000ULL;
static constexpr unsigned long long IsTupleSet                  = 0x400000000ULL;
static constexpr unsigned long long IsReadZA                    = 0x800000000ULL;
static constexpr unsigned long long IsWriteZA                   = 0x1000000000ULL;
static constexpr unsigned long long SetsFPMR                    = 0x2000000000ULL;
static constexpr unsigned long long IsReductionQV               = 0x4000000000ULL;
#endif

#ifdef LLVM_GET_SVE_ELTTYPES
  EltType_Undefined,
  EltType_Int8,
  EltType_Int16,
  EltType_Int32,
  EltType_Int64,
  EltType_Float16,
  EltType_Float32,
  EltType_Float64,
  EltType_Bfloat16,
#endif

#ifdef LLVM_GET_SVE_MEMELTTYPES
  MemEltType_Undefined,
  MemEltType_Int8,
  MemEltType_Int16,
  MemEltType_Int32,
  MemEltType_Int64,
  MemEltType_Float16,
  MemEltType_Float32,
  MemEltType_Float64,
  MemEltType_Bfloat16,
#endif

#ifdef LLVM_GET_SVE_MERGETYPES
  MergeType_Undefined,
  MergeType_MergeAny,
  MergeType_MergeZero,
  MergeType_MergeAnyFP,
  MergeType_MergeZeroFP,
#endif
