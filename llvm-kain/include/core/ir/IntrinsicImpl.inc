// llvm-kain stub: IntrinsicImpl.inc
// Each section is guarded against multiple inclusion via #ifndef sentinels.

#ifdef GET_INTRINSIC_NAME_TABLE
#ifndef INTRINSIC_NAME_TABLE_DEFINED
#define INTRINSIC_NAME_TABLE_DEFINED
static constexpr unsigned IntrinsicNameOffsetTableSize = 9170;
static const unsigned IntrinsicNameOffsetTable[IntrinsicNameOffsetTableSize] = {};
static constexpr StringTable IntrinsicNameTable("");
#endif
#endif

#ifdef GET_INTRINSIC_IITINFO
#ifndef INTRINSIC_IITINFO_DEFINED
#define INTRINSIC_IITINFO_DEFINED
  IIT_Done = 0,
  IIT_SCALABLE_VEC = 63,
  IIT_I1 = 11, IIT_I2 = 12, IIT_I4 = 13, IIT_I8 = 14,
  IIT_I16 = 15, IIT_I32 = 16, IIT_I64 = 17, IIT_I128 = 18,
  IIT_F16 = 19, IIT_F32 = 20, IIT_F64 = 21, IIT_F128 = 22,
  IIT_BF16 = 23, IIT_PPCF128 = 24,
  IIT_V1 = 25, IIT_V2 = 26, IIT_V3 = 27, IIT_V4 = 28,
  IIT_V6 = 29, IIT_V8 = 30, IIT_V10 = 31, IIT_V16 = 32,
  IIT_V32 = 33, IIT_V64 = 34, IIT_V128 = 35,
  IIT_V256 = 36, IIT_V512 = 37, IIT_V1024 = 38,
  IIT_V2048 = 39, IIT_V4096 = 40,
  IIT_PTR = 41, IIT_PTR_AS = 42, IIT_STRUCT = 43,
  IIT_METADATA = 44, IIT_TOKEN = 45, IIT_MMX = 46, IIT_AMX = 47,
  IIT_VARARG = 48,
  IIT_ANY = 49, IIT_MATCH = 50,
  IIT_EXTEND_ARG = 51, IIT_TRUNC_ARG = 52,
  IIT_SAME_VEC_WIDTH_ARG = 53, IIT_VEC_ELEMENT = 54,
  IIT_SUBDIVIDE2_ARG = 55, IIT_SUBDIVIDE4_ARG = 56,
  IIT_ONE_NTH_ELTS_VEC_ARG = 57,
  IIT_VEC_OF_ANYPTRS_TO_ELT = 58,
  IIT_VEC_OF_BITCASTS_TO_INT = 59,
  IIT_EXTERNREF = 60, IIT_FUNCREF = 61,
  IIT_AARCH64_SVCOUNT = 62,
#endif
#endif

#ifdef GET_INTRINSIC_GENERATOR_GLOBAL
#ifndef INTRINSIC_GENERATOR_GLOBAL_DEFINED
#define INTRINSIC_GENERATOR_GLOBAL_DEFINED
using FixedEncodingTy = uint64_t;
static constexpr FixedEncodingTy IIT_Table[1] = {0};
static constexpr unsigned char IIT_LongEncodingTable[1] = {0};
#endif
#endif

#ifdef GET_INTRINSIC_OVERLOAD_TABLE
#endif

#ifdef GET_INTRINSIC_SCALARIZABLE_TABLE
#endif

#ifdef GET_INTRINSIC_PRETTY_PRINT_TABLE
#endif

#ifdef GET_INTRINSIC_TARGET_DATA
#ifndef INTRINSIC_TARGET_DATA_DEFINED
#define INTRINSIC_TARGET_DATA_DEFINED
struct IntrinsicTargetInfo {
  StringRef Name;
  unsigned Count;
  unsigned Offset;
};
static IntrinsicTargetInfo TargetInfos[1] = {{StringRef(), 0, 0}};
#endif
#endif

#ifdef GET_INTRINSIC_ATTRIBUTES
#endif

#ifdef GET_LLVM_INTRINSIC_FOR_CLANG_BUILTIN
#endif

#ifdef GET_LLVM_INTRINSIC_FOR_MS_BUILTIN
#endif

#ifdef GET_INTRINSIC_PRETTY_PRINT_ARGUMENTS
#endif
