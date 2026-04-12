/*
** mruby amalgamated header
** Generated from mruby source files
**
** This file is auto-generated. Do not edit directly.
*/

#ifndef MRUBY_AMALGAM_H
#define MRUBY_AMALGAM_H

#ifdef __cplusplus
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>


/* ======== mruby.h ======== */
/*
** mruby - An embeddable Ruby implementation
**
** Copyright (c) mruby developers 2010-
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: https://www.opensource.org/licenses/mit-license.php ]
*/

/**
 * @file mruby.h
 */

#define MRUBY_H

#ifdef __cplusplus
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
#ifndef UINTPTR_MAX
#error Must be placed `#include <mruby.h>` before `#include <stdint.h>`
#endif
#ifndef SIZE_MAX
#ifdef __SIZE_MAX__
#define SIZE_MAX __SIZE_MAX__
#else
#define SIZE_MAX std::numeric_limits<size_t>::max()
#endif
#endif
#endif

#ifdef _MSC_VER
# define __func__ __FUNCTION__
#endif

#ifdef MRB_DEBUG
#include <assert.h>
#define mrb_assert(p) assert(p)
#define mrb_assert_int_fit(t1,n,t2,max) assert((n)>=0 && ((sizeof(n)<=sizeof(t2))||(n<=(t1)(max))))
#else
#define mrb_assert(p) ((void)0)
#define mrb_assert_int_fit(t1,n,t2,max) ((void)0)
#endif

#if (defined __cplusplus && __cplusplus >= 201703L)
# define mrb_static_assert(...) static_assert(__VA_ARGS__)
# define mrb_static_assert1(exp) static_assert(exp)
# define mrb_static_assert2(exp, str) static_assert(exp, str)
#elif (defined __cplusplus && __cplusplus >= 201103L) || \
    (defined _MSC_VER) || \
    (defined __GXX_EXPERIMENTAL_CXX0X__)  /* for old G++/Clang++ */
# define mrb_static_assert2(exp, str) static_assert(exp, str)
#elif defined __STDC_VERSION__ && \
        ((__STDC_VERSION__ >= 201112L) || \
         (defined __GNUC__ && __GNUC__ * 100 + __GNUC_MINOR__ >= 406))
# define mrb_static_assert2(exp, str) _Static_assert(exp, str)
#else
# /* alternative implementation of static_assert() */
# define _mrb_static_assert_cat0(a, b) a##b
# define _mrb_static_assert_cat(a, b) _mrb_static_assert_cat0(a, b)
# ifdef __COUNTER__
#  define _mrb_static_assert_id(prefix) _mrb_static_assert_cat(prefix, __COUNTER__)
# else
#  define _mrb_static_assert_id(prefix) _mrb_static_assert_cat(prefix, __LINE__)
# endif
# define mrb_static_assert2(exp, str) \
   struct _mrb_static_assert_id(_mrb_static_assert_) { char x[(exp) ? 1 : -1]; }
#endif

#ifndef mrb_static_assert
# define mrb_static_assert1(exp) mrb_static_assert2(exp, #exp)
# define mrb_static_assert_expand(...) __VA_ARGS__ /* for MSVC behaviour - https://stackoverflow.com/q/5530505 */
# define mrb_static_assert_selector(a, b, name, ...) name
/**
 * The `mrb_static_assert()` macro function takes one or two arguments.
 *
 *      !!!c
 *      mrb_static_assert(expect_condition);
 *      mrb_static_assert(expect_condition, error_message);
 */
# define mrb_static_assert(...) \
    mrb_static_assert_expand(mrb_static_assert_selector(__VA_ARGS__, mrb_static_assert2, mrb_static_assert1, _)(__VA_ARGS__))
#endif

#define mrb_static_assert_powerof2(num) mrb_static_assert((num) > 0 && (num) == ((num) & -(num)), "need power of 2 for " #num)


/* ======== mrbconf.h (inlined) ======== */
/*
** mrbconf.h - mruby core configuration
**
** See Copyright Notice in mruby.h
*/

#define MRUBYCONF_H

/* architecture selection: */
/* specify -DMRB_32BIT or -DMRB_64BIT to override */
#if !defined(MRB_32BIT) && !defined(MRB_64BIT)
#if UINT64_MAX == SIZE_MAX
#define MRB_64BIT
#else
#define MRB_32BIT
#endif
#endif

#if defined(MRB_32BIT) && defined(MRB_64BIT)
#error Cannot build for 32 and 64-bit architecture at the same time
#endif

/* configuration options: */
/* add -DMRB_USE_FLOAT32 to use float instead of double for floating-point numbers */
//#define MRB_USE_FLOAT32

/* exclude floating-point numbers */
//#define MRB_NO_FLOAT

/* obsolete configuration */
#if defined(MRB_USE_FLOAT)
# define MRB_USE_FLOAT32
#endif

/* obsolete configuration */
#if defined(MRB_WITHOUT_FLOAT)
# define MRB_NO_FLOAT
#endif

#if defined(MRB_USE_FLOAT32) && defined(MRB_NO_FLOAT)
#error Cannot define MRB_USE_FLOAT32 and MRB_NO_FLOAT at the same time
#endif

/* define on big endian machines; used by MRB_NAN_BOXING, etc. */
#ifndef MRB_ENDIAN_BIG
# if (defined(BYTE_ORDER) && defined(BIG_ENDIAN) && BYTE_ORDER == BIG_ENDIAN) || \
     (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#  define MRB_ENDIAN_BIG
# endif
#endif

/* represent mrb_value in boxed double; conflict with MRB_USE_FLOAT32 and MRB_NO_FLOAT */
//#define MRB_NAN_BOXING

/* represent mrb_value as a word (natural unit of data for the processor) */
//#define MRB_WORD_BOXING

/* represent mrb_value as a struct; occupies 2 words */
//#define MRB_NO_BOXING

/* if no specific boxing type is chosen */
#if !defined(MRB_NAN_BOXING) && !defined(MRB_WORD_BOXING) && !defined(MRB_NO_BOXING)
# define MRB_WORD_BOXING
#endif

/* if defined mruby does not inline float values in word boxing;
   all floats are heap-allocated as RFloat objects */
//#define MRB_WORDBOX_NO_INLINE_FLOAT

/* obsolete configuration */
#if defined(MRB_WORDBOX_NO_FLOAT_TRUNCATE)
# define MRB_WORDBOX_NO_INLINE_FLOAT
#endif

/* add -DMRB_INT32 to use 32-bit integer for mrb_int; conflict with MRB_INT64;
   Default for 32-bit CPU mode. */
//#define MRB_INT32

/* add -DMRB_INT64 to use 64-bit integer for mrb_int; conflict with MRB_INT32;
   Default for 64-bit CPU mode (unless using MRB_NAN_BOXING). */
//#define MRB_INT64

/* if no specific integer type is chosen */
#if !defined(MRB_INT32) && !defined(MRB_INT64)
# if defined(MRB_64BIT) && !defined(MRB_NAN_BOXING)
/* Use 64-bit integers on 64-bit architecture (without MRB_NAN_BOXING) */
#  define MRB_INT64
# else
/* Otherwise use 32-bit integers */
#  define MRB_INT32
# endif
#endif

/* MRB_INT64 on 32-bit with word/NaN boxing causes alignment issues
   for heap-allocated RInteger (int64_t needs 8-byte alignment but
   GC heap slots may not guarantee it); use MRB_NO_BOXING instead */
#if defined(MRB_INT64) && defined(MRB_32BIT) && !defined(MRB_NO_BOXING)
#error "MRB_INT64 on 32-bit requires MRB_NO_BOXING"
#endif

/* call malloc_trim(0) from mrb_full_gc() */
//#define MRB_USE_MALLOC_TRIM

/* string class to handle UTF-8 encoding */
//#define MRB_UTF8_STRING

/* maximum length of strings */
/* the default value is 1MB */
/* set this value to zero to skip the check */
//#define MRB_STR_LENGTH_MAX 1048576

/* maximum length of arrays */
/* the default value is 2**17 entries */
/* set this value to zero to skip the check */
//#define MRB_ARY_LENGTH_MAX 131072

/* argv max size in mrb_funcall */
//#define MRB_FUNCALL_ARGC_MAX 16

/* number of object per heap page */
//#define MRB_HEAP_PAGE_SIZE 1024

/* define if your platform does not support etext, edata */
//#define MRB_NO_DEFAULT_RO_DATA_P

/* define if your platform supports etext, edata */
//#define MRB_USE_RO_DATA_P_ETEXT
/* use MRB_USE_ETEXT_RO_DATA_P by default on Linux */
#if (defined(__linux__) && !defined(__KERNEL__))
#define MRB_USE_ETEXT_RO_DATA_P
#endif

/* you can provide and use mrb_ro_data_p() for your platform.
   prototype is `mrb_bool mrb_ro_data_p(const char *ptr)` */
//#define MRB_USE_CUSTOM_RO_DATA_P

/* turn off generational GC by default */
//#define MRB_GC_TURN_OFF_GENERATIONAL

/* initial size of khash table bucket */
//#define KHASH_INITIAL_SIZE 32

/* allocated memory address alignment */
//#define POOL_ALIGNMENT 4

/* page size of memory pool */
//#define POOL_PAGE_SIZE 16000

/* arena size */
//#define MRB_GC_ARENA_SIZE 100

/* fixed size GC arena */
//#define MRB_GC_FIXED_ARENA

/* state atexit stack size */
//#define MRB_FIXED_STATE_ATEXIT_STACK_SIZE 5

/* fixed size state atexit stack */
//#define MRB_FIXED_STATE_ATEXIT_STACK

/* -DMRB_NO_XXXX to drop following features */
//#define MRB_NO_STDIO /* use of stdio */

/* -DMRB_USE_XXXX to enable following features */
//#define MRB_USE_DEBUG_HOOK /* hooks for debugger */
//#define MRB_USE_ALL_SYMBOLS /* Symbol.all_symbols */

/* Symbol table configuration */
/* Threshold for switching from linear search to hash table */
#ifndef MRB_SYMBOL_LINEAR_THRESHOLD
#define MRB_SYMBOL_LINEAR_THRESHOLD 256
#endif

/* obsolete configurations */
#if defined(DISABLE_STDIO) || defined(MRB_DISABLE_STDIO)
# define MRB_NO_STDIO
#endif
#if defined(MRB_DISABLE_DIRECT_THREADING) || defined(MRB_NO_DIRECT_THREADING)
# define MRB_USE_VM_SWITCH_DISPATCH
#endif
#if defined(ENABLE_DEBUG) || defined(MRB_ENABLE_DEBUG_HOOK)
# define MRB_USE_DEBUG_HOOK
#endif
#ifdef MRB_ENABLE_ALL_SYMBOLS
# define MRB_USE_ALL_SYMBOLS
#endif
#ifdef MRB_ENABLE_CXX_ABI
# define MRB_USE_CXX_ABI
#endif
#ifdef MRB_ENABLE_CXX_EXCEPTION
# define MRB_USE_CXX_EXCEPTION
#endif

/* end of configuration */

#ifndef MRB_NO_STDIO
# include <stdio.h>
#endif

/*
** mruby tuning profiles
**/

/* A profile for micro controllers */
#if defined(MRB_CONSTRAINED_BASELINE_PROFILE)
# ifndef MRB_NO_METHOD_CACHE
#  define MRB_NO_METHOD_CACHE
# endif

# ifndef KHASH_INITIAL_SIZE
#  define KHASH_INITIAL_SIZE 16
# endif

# ifndef MRB_HEAP_PAGE_SIZE
#  define MRB_HEAP_PAGE_SIZE 256
# endif

/* A profile for default mruby */
#elif defined(MRB_BASELINE_PROFILE)

/* A profile for desktop computers or workstations; rich memory! */
#elif defined(MRB_MAIN_PROFILE)
# ifndef MRB_METHOD_CACHE_SIZE
#  define MRB_METHOD_CACHE_SIZE (1<<10)
# endif

# ifndef MRB_HEAP_PAGE_SIZE
#  define MRB_HEAP_PAGE_SIZE 4096
# endif

/* A profile for server; mruby vm is long life */
#elif defined(MRB_HIGH_PROFILE)
# ifndef MRB_METHOD_CACHE_SIZE
#  define MRB_METHOD_CACHE_SIZE (1<<12)
# endif

# ifndef MRB_HEAP_PAGE_SIZE
#  define MRB_HEAP_PAGE_SIZE 4096
# endif
#endif




/* ======== mruby/common.h (inlined) ======== */
/**
** @file common.h - mruby common platform definition"
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_COMMON_H

#ifdef __APPLE__
  #ifndef __TARGETCONDITIONALS__
  #include "TargetConditionals.h"
  #endif
#endif

#ifdef __cplusplus
#ifdef MRB_USE_CXX_ABI
#define MRB_BEGIN_DECL
#define MRB_END_DECL
#else
# define MRB_BEGIN_DECL extern "C" {
# define MRB_END_DECL }
#endif
#else
/** Start declarations in C mode */
# define MRB_BEGIN_DECL
/** End declarations in C mode */
# define MRB_END_DECL
#endif

#include <sys/types.h>
#if defined _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

/**
 * Shared compiler macros
 */
MRB_BEGIN_DECL

/** Declare a function that never returns. */
#if defined __STDC_VERSION__ && __STDC_VERSION__ >= 201112L
# define mrb_noreturn _Noreturn
#elif defined __GNUC__ && !defined __STRICT_ANSI__
# define mrb_noreturn __attribute__((noreturn))
#elif defined _MSC_VER
# define mrb_noreturn __declspec(noreturn)
#else
# define mrb_noreturn
#endif

/** Mark a function as deprecated. */
#if defined __GNUC__ && !defined __STRICT_ANSI__
# define mrb_deprecated __attribute__((deprecated))
#elif defined _MSC_VER
# define mrb_deprecated __declspec(deprecated)
#else
# define mrb_deprecated
#endif

/** Branch prediction hints for optimization. */
#if defined(__GNUC__) || defined(__clang__)
# define mrb_likely(x)   __builtin_expect(!!(x), 1)
# define mrb_unlikely(x) __builtin_expect(!!(x), 0)
#else
# define mrb_likely(x)   (x)
# define mrb_unlikely(x) (x)
#endif

/** Declare a type or object as an alignment requirement. */
#ifndef mrb_alignas
# if defined(__cplusplus) && __cplusplus >= 201103L
#  // https://en.cppreference.com/w/cpp/language/alignas
#  define mrb_alignas(n) alignas(n)
# elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  // https://en.cppreference.com/w/c/language/_Alignas
#  define mrb_alignas(n) _Alignas(n)
# elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
#  // https://learn.microsoft.com/en-us/cpp/cpp/align-cpp?view=msvc-170
#  define mrb_alignas(n) __declspec(align(n))
# elif defined(__GNUC__) || defined(__clang__)
#  // https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-aligned-type-attribute
#  define mrb_alignas(n) __attribute__((aligned(n)))
# else
#  // `mrb_alignas` defined as dummy. If necessary, send issues to https://github.com/mruby/mruby .
#  define mrb_alignas(n)
# endif
#endif

/** Declare a function as always inlined. */
#if defined _MSC_VER && _MSC_VER < 1900
# ifndef __cplusplus
#  define inline __inline
# endif
#endif
#define MRB_INLINE static inline

/** Declare a public mruby API function. */
#ifndef MRB_API
#if defined(MRB_BUILD_AS_DLL)
#if defined(MRB_CORE) || defined(MRB_LIB)
# define MRB_API __declspec(dllexport)
#else
# define MRB_API __declspec(dllimport)
#endif
#else
# define MRB_API extern
#endif
#endif

/** Declare mingw versions */
#if defined(__MINGW32__) || defined(__MINGW64__)
# include <_mingw.h>
# if defined(__MINGW64_VERSION_MAJOR)
#  define MRB_MINGW64_VERSION  (__MINGW64_VERSION_MAJOR * 1000 + __MINGW64_VERSION_MINOR)
# elif defined(__MINGW32_MAJOR_VERSION)
#  define MRB_MINGW32_VERSION  (__MINGW32_MAJOR_VERSION * 1000 + __MINGW32_MINOR_VERSION)
# endif
# if defined(__MINGW32__) && !defined(__MINGW64__)
#   define MRB_MINGW32_LEGACY
# endif
#endif

MRB_END_DECL



/* ======== mruby/value.h (inlined) ======== */
/**
** @file mruby/value.h - mruby value definitions
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_VALUE_H

// #include "common.h" - in amalgam

/*
 * mruby Value definition functions and macros.
 */
MRB_BEGIN_DECL

/**
 * mruby Symbol.
 * @class mrb_sym
 *
 * You can create an mrb_sym by simply using mrb_str_intern() or mrb_intern_cstr()
 */
typedef uint32_t mrb_sym;

/**
 * mruby Boolean.
 * @class mrb_bool
 *
 *
 * Used internally to represent boolean. Can be TRUE or FALSE.
 * Not to be confused with Ruby's boolean classes, which can be
 * obtained using mrb_false_value() and mrb_true_value()
 */
#if defined(__cplusplus) || (defined(__bool_true_false_are_defined) && __bool_true_false_are_defined)
typedef bool mrb_bool;

# ifndef FALSE
#  define FALSE false
# endif
# ifndef TRUE
#  define TRUE true
# endif
#else
# if __STDC_VERSION__ >= 199901L
typedef _Bool mrb_bool;
# else
typedef uint8_t mrb_bool;
# endif

# ifndef FALSE
#  define FALSE 0
# endif
# ifndef TRUE
#  define TRUE 1
# endif
#endif

struct mrb_state;

#if defined _MSC_VER && _MSC_VER < 1800
# define PRIo64 "llo"
# define PRId64 "lld"
# define PRIu64 "llu"
# define PRIx64 "llx"
# define PRIo16 "ho"
# define PRId16 "hd"
# define PRIu16 "hu"
# define PRIx16 "hx"
# define PRIo32 "o"
# define PRId32 "d"
# define PRIu32 "u"
# define PRIx32 "x"
#else
# include <inttypes.h>
#endif

#if defined(MRB_INT64)
  typedef int64_t mrb_int;
  typedef uint64_t mrb_uint;
# define MRB_INT_BIT 64
# define MRB_INT_MIN INT64_MIN
# define MRB_INT_MAX INT64_MAX
# define MRB_PRIo PRIo64
# define MRB_PRId PRId64
# define MRB_PRIx PRIx64
#else
  typedef int32_t mrb_int;
  typedef uint32_t mrb_uint;
# define MRB_INT_BIT 32
# define MRB_INT_MIN INT32_MIN
# define MRB_INT_MAX INT32_MAX
# define MRB_PRIo PRIo32
# define MRB_PRId PRId32
# define MRB_PRIx PRIx32
#endif

#define MRB_FLAGS_MASK(shift, width)    (~(~0U << (width)) << (shift))
#define MRB_FLAGS_GET(b, s, w)          (((b) >> (s)) & MRB_FLAGS_MASK(0, w))
#define MRB_FLAGS_SET(b, s, w, n)       ((b) = MRB_FLAGS_ZERO(b, s, w) | MRB_FLAGS_MAKE(s, w, n))
#define MRB_FLAGS_ZERO(b, s, w)         ((b) & ~MRB_FLAGS_MASK(s, w))
#define MRB_FLAGS_MAKE(s, w, n)         (((n) & MRB_FLAGS_MASK(0, w)) << (s))
#define MRB_FLAG_ON(b, s)               ((b) |= MRB_FLAGS_MASK(s, 1))
#define MRB_FLAG_OFF(b, s)              ((b) &= ~MRB_FLAGS_MASK(s, 1))
#define MRB_FLAG_CHECK(b, s)            (!!((b) & MRB_FLAGS_MASK(s, 1)))

MRB_API mrb_bool mrb_read_int(const char *p, const char *e, char **endp, mrb_int *np);
/* obsolete; do not use mrb_int_read() */
MRB_API mrb_int mrb_int_read(const char*, const char*, char**);
#ifndef MRB_NO_FLOAT
MRB_API mrb_bool mrb_read_float(const char *p, char **endp, double *fp);
/* obsolete; do not use mrb_float_read() */
MRB_API double mrb_float_read(const char *p, char **endp);
#ifdef MRB_USE_FLOAT32
  typedef float mrb_float;
#else
  typedef double mrb_float;
#endif
#endif

#if defined _MSC_VER && _MSC_VER < 1900
MRB_API int mrb_msvc_vsnprintf(char *s, size_t n, const char *format, va_list arg);
MRB_API int mrb_msvc_snprintf(char *s, size_t n, const char *format, ...);
# define vsnprintf(s, n, format, arg) mrb_msvc_vsnprintf(s, n, format, arg)
# define snprintf(s, n, format, ...) mrb_msvc_snprintf(s, n, format, __VA_ARGS__)
# if _MSC_VER < 1800 && !defined MRB_NO_FLOAT
#  define isfinite(n) _finite(n)
#  define isnan _isnan
#  define isinf(n) (!_finite(n) && !_isnan(n))
#  define signbit(n) (_copysign(1.0, (n)) < 0.0)
static const unsigned int IEEE754_INFINITY_BITS_SINGLE = 0x7F800000;
#  define INFINITY (*(float*)&IEEE754_INFINITY_BITS_SINGLE)
#  define NAN ((float)(INFINITY - INFINITY))
# endif
#endif

#define MRB_VTYPE_FOREACH(f) \
    /* mrb_vtype */     /* C type */        /* Ruby class */ \
  f(MRB_TT_FALSE,       void,               "false") \
  f(MRB_TT_TRUE,        void,               "true") \
  f(MRB_TT_SYMBOL,      void,               "Symbol") \
  f(MRB_TT_UNDEF,       void,               "undefined") \
  f(MRB_TT_FREE,        void,               "free") \
  f(MRB_TT_FLOAT,       struct RFloat,      "Float") \
  f(MRB_TT_INTEGER,     struct RInteger,    "Integer") \
  f(MRB_TT_CPTR,        struct RCptr,       "cptr") \
  f(MRB_TT_OBJECT,      struct RObject,     "Object") \
  f(MRB_TT_CLASS,       struct RClass,      "Class") \
  f(MRB_TT_MODULE,      struct RClass,      "Module") \
  f(MRB_TT_SCLASS,      struct RClass,      "SClass") \
  f(MRB_TT_HASH,        struct RHash,       "Hash") \
  f(MRB_TT_CDATA,       struct RData,       "C data") \
  f(MRB_TT_EXCEPTION,   struct RException,  "Exception") \
  f(MRB_TT_ICLASS,      struct RClass,      "iClass") \
  f(MRB_TT_PROC,        struct RProc,       "Proc") \
  f(MRB_TT_ARRAY,       struct RArray,      "Array") \
  f(MRB_TT_STRING,      struct RString,     "String") \
  f(MRB_TT_RANGE,       struct RRange,      "Range") \
  f(MRB_TT_ENV,         struct REnv,        "env") \
  f(MRB_TT_FIBER,       struct RFiber,      "Fiber") \
  f(MRB_TT_STRUCT,      struct RArray,      "Struct") \
  f(MRB_TT_ISTRUCT,     struct RIStruct,    "istruct") \
  f(MRB_TT_BREAK,       struct RBreak,      "break") \
  f(MRB_TT_COMPLEX,     struct RComplex,    "Complex") \
  f(MRB_TT_RATIONAL,    struct RRational,   "Rational") \
  f(MRB_TT_BIGINT,      struct RBigint,     "Integer") \
  f(MRB_TT_BACKTRACE,   struct RBacktrace,  "backtrace") \
  f(MRB_TT_SET,         struct RSet,        "Set")

enum mrb_vtype {
#define MRB_VTYPE_DEFINE(tt, type, name) tt,
  MRB_VTYPE_FOREACH(MRB_VTYPE_DEFINE)
#undef MRB_VTYPE_DEFINE
  MRB_TT_MAXDEFINE
};

/* obsolete name for MRB_TT_CDATA */
#define MRB_TT_DATA MRB_TT_CDATA

#define MRB_VTYPE_TYPEOF(tt) MRB_TYPEOF_##tt

#define MRB_VTYPE_TYPEDEF(tt, type, name) typedef type MRB_VTYPE_TYPEOF(tt);
MRB_VTYPE_FOREACH(MRB_VTYPE_TYPEDEF)
#undef MRB_VTYPE_TYPEDEF

/* for compatibility */
#define MRB_TT_FIXNUM MRB_TT_INTEGER


/* ======== mruby/object.h (inlined) ======== */
/**
** @file mruby/object.h - mruby object definition
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_OBJECT_H

#define MRB_OBJECT_HEADER \
  struct RClass *c;       \
  enum mrb_vtype tt:8;    \
  unsigned int gc_color:3; \
  unsigned int frozen:1;  \
  uint32_t flags:20

#define MRB_FLAG_TEST(obj, flag) ((obj)->flags & (flag))

struct RBasic {
  MRB_OBJECT_HEADER;
};
#define mrb_basic_ptr(v) ((struct RBasic*)(mrb_ptr(v)))

#define MRB_OBJ_IS_FROZEN 1
#define mrb_frozen_p(o) ((o)->frozen)

/* Object shape flag -- when set, obj->iv is shaped, not iv_tbl* */
/* Bit 5: avoids conflict with MRB_INSTANCE_TT_MASK (bits 0-4);
   but conflicts with MRB_HASH_AR_EA_N_USED on 32-bit, so the
   predicate must also check tt to avoid false positives */
#define MRB_FL_OBJ_SHAPED (1 << 5)
#define MRB_OBJ_SHAPED_P(o) ((o)->tt == MRB_TT_OBJECT && ((o)->flags & MRB_FL_OBJ_SHAPED))

struct RObject {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
};
#define mrb_obj_ptr(v)   ((struct RObject*)(mrb_ptr(v)))

#define mrb_special_const_p(x) mrb_immediate_p(x)

struct RFiber {
  MRB_OBJECT_HEADER;
  struct mrb_context *cxt;
};

#define mrb_static_assert_object_size(st) \
  mrb_static_assert(sizeof(st) <= sizeof(void*) * 5, \
                    #st " size must be within 5 words")



#ifdef MRB_DOCUMENTATION_BLOCK

/**
 * @abstract
 * mruby value boxing.
 *
 * Actual implementation depends on configured boxing type.
 *
 * @see mruby/boxing_word.h Word boxing representation (Default)
 * @see mruby/boxing_no.h No boxing representation
 * @see mruby/boxing_nan.h Boxed double representation
 */
typedef void mrb_value;

#endif

#if defined(MRB_WORD_BOXING) || (defined(MRB_NAN_BOXING) && defined(MRB_64BIT))
struct RCptr {
  MRB_OBJECT_HEADER;
  void *p;
};
#endif

#if defined(MRB_NAN_BOXING)

/* ======== mruby/boxing_nan.h (inlined) ======== */
/**
** @file mruby/boxing_nan.h - nan boxing mrb_value definition
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_BOXING_NAN_H

#ifdef MRB_USE_FLOAT32
# error ---->> MRB_NAN_BOXING and MRB_USE_FLOAT32 conflict <<----
#endif

#ifdef MRB_NO_FLOAT
# error ---->> MRB_NAN_BOXING and MRB_NO_FLOAT conflict <<----
#endif

#define MRB_FIXNUM_MIN INT32_MIN
#define MRB_FIXNUM_MAX INT32_MAX

enum mrb_nanbox_tt_inline {
  MRB_NANBOX_TT_OBJECT = 0,
  MRB_NANBOX_TT_INTEGER = 1,
  MRB_NANBOX_TT_MISC = 2,
  MRB_NANBOX_TT_CPTR = 3,
};

/* value representation by nan-boxing:
 *   float : SEEEEEEE EEEEFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF
 *   +/-inf: S1111111 11110000 00000000 00000000 00000000 00000000 00000000 00000000
 *   nan   : 01111111 11111000 00000000 00000000 00000000 00000000 00000000 00000000
 *   int   : 01111111 11111001 00000000 00000000 IIIIIIII IIIIIIII IIIIIIII IIIIIIII
 *   sym   : 01111111 11111110 00000000 00TTTTTT SSSSSSSS SSSSSSSS SSSSSSSS SSSSSSSS
 *   misc  : 01111111 11111110 00000000 00TTTTTT 00000000 00000000 00000000 0000MMMM
 *   object: 01111111 11111100 PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPP00
 *   cptr  : 01111111 11111111 PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP
 * Stored as O = R + 0x8004000000000000, retrieved as R = O - 0x8004000000000000.
 * This makes pointers have all zeros in the top 32 bits.
 */
typedef struct mrb_value {
  uint64_t u;
} mrb_value;

static inline mrb_float
mrb_nan_boxing_value_float(mrb_value v)
{
  union {
    mrb_float f;
    uint64_t u;
  } x;
  x.u = v.u - 0x8004000000000000;
  return x.f;
}

#define SET_FLOAT_VALUE(mrb,r,f) do { \
  union { \
    mrb_float f; \
    uint64_t u; \
  } float_uint_union; \
  if ((f) != (f)) { /* NaN */ \
    float_uint_union.u = 0x7ff8000000000000UL; \
  } \
  else { \
    float_uint_union.f = (f); \
  } \
  r.u = float_uint_union.u + 0x8004000000000000; \
} while(0)

#define mrb_float_p(o) (((uint64_t)((o).u)&0xfffc000000000000) != 0)

struct RInteger {
  MRB_OBJECT_HEADER;
  mrb_int i;
};

#define mrb_nb_tt(o) ((enum mrb_nanbox_tt_inline)((uint32_t)((o).u>>48)&3))

MRB_INLINE enum mrb_vtype
mrb_type(mrb_value o)
{
  if (mrb_float_p(o)) return MRB_TT_FLOAT;

  int64_t u = o.u;
  switch (mrb_nb_tt(o)) {
  case MRB_NANBOX_TT_OBJECT: {
    if (u == 0) return MRB_TT_FALSE;
    return ((struct RBasic*)(uintptr_t)u)->tt;
  }
  case MRB_NANBOX_TT_INTEGER:
    return MRB_TT_INTEGER;
  case MRB_NANBOX_TT_MISC:
    return (enum mrb_vtype)((uint32_t)(o.u >> 32) & 0x1f);
  case MRB_NANBOX_TT_CPTR:
    return MRB_TT_CPTR;
  default:
    /* never happen */
    return MRB_TT_FLOAT;
  }
}

MRB_INLINE enum mrb_vtype
mrb_unboxed_type(mrb_value o)
{
  if (!mrb_float_p(o) && mrb_nb_tt(o) == MRB_NANBOX_TT_OBJECT && o.u != 0) {
    return ((struct RBasic*)(uintptr_t)o.u)->tt;
  } else {
    return MRB_TT_FALSE;
  }
}

#define NANBOX_SET_MISC_VALUE(r,t,i) NANBOX_SET_VALUE(r, MRB_NANBOX_TT_MISC, ((uint64_t)(t)<<32) | (i))

#define mrb_float(o) mrb_nan_boxing_value_float(o)
#ifdef MRB_INT64
/*
#ifdef MRB_32BIT
#define mrb_fixnum(o)  ((mrb_int)((intptr_t)0xffffffffffff&((o).u))|(((o).u & 0x800000000000)?0xffff000000000000:0))
#else
#define mrb_fixnum(o)  ((mrb_int)(int32_t)((o).u))
#endif
*/

#define mrb_fixnum(o)  ((mrb_int)(int32_t)((o).u))

static inline mrb_int
mrb_nan_boxing_value_int(mrb_value v)
{
  uint64_t u = v.u;
  if (mrb_nb_tt(v)==MRB_NANBOX_TT_OBJECT) {
    struct RInteger *p = (struct RInteger*)(uintptr_t)u;
    return p->i;
  }
  return mrb_fixnum(v);
}
#define mrb_integer(o) mrb_nan_boxing_value_int(o)
#else
#define mrb_fixnum(o)  ((mrb_int)(((uintptr_t)0xffffffff)&((o).u)))
#define mrb_integer(o) mrb_fixnum(o)
#endif
#define mrb_symbol(o)  ((mrb_sym)((uintptr_t)0xffffffff)&((o).u))
#define mrb_ptr(o)     ((void*)(uintptr_t)(o).u)
#define mrb_cptr(o)    ((void*)(uintptr_t)(0xffffffffffffULL&(o).u))

#define NANBOX_SET_VALUE(o, tt, v) do { \
  (o).u = ((uint64_t)(tt)<<48) | ((uint64_t)(v)); \
} while (0)

#define SET_NIL_VALUE(r) ((r).u = 0)
#define SET_FALSE_VALUE(r) NANBOX_SET_MISC_VALUE(r, MRB_TT_FALSE, 1)
#define SET_TRUE_VALUE(r) NANBOX_SET_MISC_VALUE(r, MRB_TT_TRUE, 1)
#define SET_BOOL_VALUE(r,b) NANBOX_SET_MISC_VALUE(r, (b) ? MRB_TT_TRUE : MRB_TT_FALSE, 1)
#ifdef MRB_INT64
MRB_API mrb_value mrb_boxing_int_value(struct mrb_state*, mrb_int);
#define SET_INT_VALUE(mrb, r, n) ((r) = mrb_boxing_int_value(mrb, n))
#else
#define SET_INT_VALUE(mrb, r, n) SET_FIXNUM_VALUE(r, n)
#endif
#define SET_FIXNUM_VALUE(r,n) NANBOX_SET_VALUE(r, MRB_NANBOX_TT_INTEGER, (uint32_t)(n))
#define SET_SYM_VALUE(r,v) NANBOX_SET_MISC_VALUE(r, MRB_TT_SYMBOL, (uint32_t)(v))
#define SET_OBJ_VALUE(r,v) do {(r).u = (uint64_t)(uintptr_t)(v);} while (0)
#define SET_CPTR_VALUE(mrb,r,v) NANBOX_SET_VALUE(r, MRB_NANBOX_TT_CPTR, (uint64_t)(uintptr_t)(v) & 0x0000ffffffffffffULL)
#define SET_UNDEF_VALUE(r) NANBOX_SET_MISC_VALUE(r, MRB_TT_UNDEF, 4)

#define mrb_immediate_p(o) ((mrb_float_p(o) || mrb_nb_tt(o) != MRB_NANBOX_TT_OBJECT) || (o).u == 0)
#define mrb_nil_p(o)  ((o).u == 0)
#define mrb_false_p(o) (mrb_type(o) == MRB_TT_FALSE || (o).u == 0)
#define mrb_fixnum_p(o) (!mrb_float_p(o) && mrb_nb_tt(o)==MRB_NANBOX_TT_INTEGER)


#elif defined(MRB_WORD_BOXING)

/* ======== mruby/boxing_word.h (inlined) ======== */
/**
** @file mruby/boxing_word.h - word boxing mrb_value definition
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_BOXING_WORD_H

#if defined(MRB_32BIT) && !defined(MRB_USE_FLOAT32) && !defined(MRB_WORDBOX_NO_INLINE_FLOAT)
# define MRB_WORDBOX_NO_INLINE_FLOAT
#endif

#ifndef MRB_NO_FLOAT
struct RFloat {
  MRB_OBJECT_HEADER;
#ifdef MRB_WORDBOX_NO_INLINE_FLOAT
  /* avoid 8-byte alignment on 32-bit; use memcpy-based accessors */
  char f[sizeof(mrb_float)];
#else
  mrb_float f;
#endif
};

#include <string.h>

static inline mrb_float
mrb_rfloat_value(const struct RFloat *p)
{
#ifdef MRB_WORDBOX_NO_INLINE_FLOAT
  mrb_float f;
  memcpy(&f, p->f, sizeof(mrb_float));
  return f;
#else
  return p->f;
#endif
}

static inline void
mrb_rfloat_set(struct RFloat *p, mrb_float f)
{
#ifdef MRB_WORDBOX_NO_INLINE_FLOAT
  memcpy(p->f, &f, sizeof(mrb_float));
#else
  p->f = f;
#endif
}
#endif

struct RInteger {
  MRB_OBJECT_HEADER;
  mrb_int i;
};

enum mrb_special_consts {
  MRB_Qnil    =  0,
  MRB_Qfalse  =  4,
  MRB_Qtrue   = 12,
  MRB_Qundef  = 20,
};

#if defined(MRB_64BIT) && defined(MRB_INT32)
#define MRB_FIXNUM_SHIFT        0
#else
#define MRB_FIXNUM_SHIFT        WORDBOX_FIXNUM_SHIFT
#endif
#define MRB_SYMBOL_SHIFT        WORDBOX_SYMBOL_SHIFT

#if defined(MRB_64BIT) && defined(MRB_INT64)
# define MRB_FIXNUM_MIN (INT64_MIN>>MRB_FIXNUM_SHIFT)
# define MRB_FIXNUM_MAX (INT64_MAX>>MRB_FIXNUM_SHIFT)
#else
# define MRB_FIXNUM_MIN (INT32_MIN>>MRB_FIXNUM_SHIFT)
# define MRB_FIXNUM_MAX (INT32_MAX>>MRB_FIXNUM_SHIFT)
#endif

#define WORDBOX_FIXNUM_BIT_POS  1
#define WORDBOX_FIXNUM_SHIFT    WORDBOX_FIXNUM_BIT_POS
#define WORDBOX_FIXNUM_FLAG     (1 << (WORDBOX_FIXNUM_BIT_POS - 1))
#define WORDBOX_FIXNUM_MASK     ((1 << WORDBOX_FIXNUM_BIT_POS) - 1)

#if defined(MRB_WORDBOX_NO_INLINE_FLOAT) || defined(MRB_NO_FLOAT)
/* floats are allocated in heaps */
#define WORDBOX_IMMEDIATE_MASK  0x03
#define WORDBOX_SYMBOL_BIT_POS  2
#define WORDBOX_SYMBOL_SHIFT    WORDBOX_SYMBOL_BIT_POS
#define WORDBOX_SYMBOL_FLAG     (1 << (WORDBOX_SYMBOL_BIT_POS - 1))
#define WORDBOX_SYMBOL_MASK     ((1 << WORDBOX_SYMBOL_BIT_POS) - 1)
#else
#define WORDBOX_FLOAT_FLAG      2
#define WORDBOX_FLOAT_MASK      3
#if defined(MRB_64BIT)
#define WORDBOX_SYMBOL_SHIFT    32
#else  /* MRB_32BIT */
#define WORDBOX_SYMBOL_SHIFT    5
#endif
#define WORDBOX_SYMBOL_FLAG     0x1c
#define WORDBOX_SYMBOL_MASK     0x1f
#endif

#ifndef WORDBOX_IMMEDIATE_MASK
#define WORDBOX_IMMEDIATE_MASK  0x07
#endif

#define WORDBOX_SET_SHIFT_VALUE(o,n,v) \
  ((o).w = (((uintptr_t)(v)) << WORDBOX_##n##_SHIFT) | WORDBOX_##n##_FLAG)
#define WORDBOX_SHIFT_VALUE_P(o,n) \
  (((o).w & WORDBOX_##n##_MASK) == WORDBOX_##n##_FLAG)
#define WORDBOX_OBJ_TYPE_P(o,n) \
  (!mrb_immediate_p(o) && mrb_val_union(o).bp->tt == MRB_TT_##n)

/*
 * mrb_value representation:
 *
 * 64-bit word with inline float (rotation encoding, lossless):
 *   nil   : ...0000 0000 (all bits are 0)
 *   false : ...0000 0100 (mrb_fixnum(v) != 0)
 *   true  : ...0000 1100
 *   undef : ...0001 0100
 *   symbol: ...0001 1100 (use only upper 32-bit as symbol value with MRB_64BIT)
 *   fixnum: ...IIII III1
 *   float : ...FFFF FF10 (rotl64(float64-ADDEND, 3); exponent [-255,+256])
 *   object: ...PPPP P000
 *   (floats outside inline range are heap-allocated as RFloat)
 *
 * 64-bit word with inline float32 (MRB_USE_FLOAT32):
 *   float : ...FFFF FF10 (float32 shifted left by 2)
 *   (other values same as above)
 *
 * word boxing without inline float (MRB_WORDBOX_NO_INLINE_FLOAT):
 *   nil   : ...0000 0000 (all bits are 0)
 *   false : ...0000 0100 (mrb_fixnum(v) != 0)
 *   true  : ...0000 1100
 *   undef : ...0001 0100
 *   fixnum: ...IIII III1
 *   symbol: ...SSSS SS10
 *   object: ...PPPP PP00 (any bits are 1)
 */
typedef struct mrb_value {
  uintptr_t w;
} mrb_value;

union mrb_value_ {
  void *p;
  struct RBasic *bp;
#ifndef MRB_NO_FLOAT
  struct RFloat *fp;
#if !defined(MRB_WORDBOX_NO_INLINE_FLOAT) && defined(MRB_USE_FLOAT32)
  mrb_float f;
#endif
#endif
  struct RInteger *ip;
  struct RCptr *vp;
  uintptr_t w;
  mrb_value value;
};

mrb_static_assert(sizeof(mrb_value) == sizeof(union mrb_value_));

static inline union mrb_value_
mrb_val_union(mrb_value v)
{
  union mrb_value_ x;
  x.value = v;
  return x;
}

MRB_API mrb_value mrb_word_boxing_cptr_value(struct mrb_state*, void*);
#ifndef MRB_NO_FLOAT
MRB_API mrb_value mrb_word_boxing_float_value(struct mrb_state*, mrb_float);
#endif
MRB_API mrb_value mrb_boxing_int_value(struct mrb_state*, mrb_int);

#if WORDBOX_IMMEDIATE_MASK == 0x3
#define mrb_immediate_p(o) ((o).w & WORDBOX_IMMEDIATE_MASK || (o).w <= MRB_Qundef)
#else
#define mrb_immediate_p(o) ((o).w & WORDBOX_IMMEDIATE_MASK || (o).w == MRB_Qnil)
#endif

#define mrb_ptr(o)     mrb_val_union(o).p
#define mrb_cptr(o)    mrb_val_union(o).vp->p
#ifndef MRB_NO_FLOAT
#ifndef MRB_WORDBOX_NO_INLINE_FLOAT
MRB_API mrb_float mrb_word_boxing_value_float(mrb_value v);
#define mrb_float(o) mrb_word_boxing_value_float(o)
#else
#define mrb_float(o) mrb_rfloat_value(mrb_val_union(o).fp)
#endif
#endif
#define mrb_fixnum(o)  (mrb_int)(((intptr_t)(o).w) >> WORDBOX_FIXNUM_SHIFT)
MRB_INLINE mrb_int
mrb_integer_func(mrb_value o) {
  if (mrb_immediate_p(o)) return mrb_fixnum(o);
  return mrb_val_union(o).ip->i;
}
#define mrb_integer(o) mrb_integer_func(o)
#define mrb_symbol(o)  (mrb_sym)(((o).w) >> WORDBOX_SYMBOL_SHIFT)
#define mrb_bool(o)    (((o).w & ~(uintptr_t)MRB_Qfalse) != 0)

#define mrb_fixnum_p(o) WORDBOX_SHIFT_VALUE_P(o, FIXNUM)
#define mrb_integer_p(o) (WORDBOX_SHIFT_VALUE_P(o, FIXNUM)||WORDBOX_OBJ_TYPE_P(o, INTEGER))
#define mrb_symbol_p(o) WORDBOX_SHIFT_VALUE_P(o, SYMBOL)
#define mrb_undef_p(o) ((o).w == MRB_Qundef)
#define mrb_nil_p(o)  ((o).w == MRB_Qnil)
#define mrb_false_p(o) ((o).w == MRB_Qfalse)
#define mrb_true_p(o)  ((o).w == MRB_Qtrue)
#ifndef MRB_NO_FLOAT
#ifdef MRB_WORDBOX_NO_INLINE_FLOAT
#define mrb_float_p(o) WORDBOX_OBJ_TYPE_P(o, FLOAT)
#elif defined(MRB_USE_FLOAT32) && defined(MRB_64BIT)
#define mrb_float_p(o) WORDBOX_SHIFT_VALUE_P(o, FLOAT)
#else
/* rotation encoding: most floats inline, edge cases on heap */
#define mrb_float_p(o) (WORDBOX_SHIFT_VALUE_P(o, FLOAT) || WORDBOX_OBJ_TYPE_P(o, FLOAT))
#endif
#else
#define mrb_float_p(o) FALSE
#endif
#define mrb_array_p(o) WORDBOX_OBJ_TYPE_P(o, ARRAY)
#define mrb_string_p(o) WORDBOX_OBJ_TYPE_P(o, STRING)
#define mrb_hash_p(o) WORDBOX_OBJ_TYPE_P(o, HASH)
#define mrb_cptr_p(o) WORDBOX_OBJ_TYPE_P(o, CPTR)
#define mrb_exception_p(o) WORDBOX_OBJ_TYPE_P(o, EXCEPTION)
#define mrb_free_p(o) WORDBOX_OBJ_TYPE_P(o, FREE)
#define mrb_object_p(o) WORDBOX_OBJ_TYPE_P(o, OBJECT)
#define mrb_class_p(o) WORDBOX_OBJ_TYPE_P(o, CLASS)
#define mrb_module_p(o) WORDBOX_OBJ_TYPE_P(o, MODULE)
#define mrb_iclass_p(o) WORDBOX_OBJ_TYPE_P(o, ICLASS)
#define mrb_sclass_p(o) WORDBOX_OBJ_TYPE_P(o, SCLASS)
#define mrb_proc_p(o) WORDBOX_OBJ_TYPE_P(o, PROC)
#define mrb_range_p(o) WORDBOX_OBJ_TYPE_P(o, RANGE)
#define mrb_env_p(o) WORDBOX_OBJ_TYPE_P(o, ENV)
#define mrb_data_p(o) WORDBOX_OBJ_TYPE_P(o, DATA)
#define mrb_fiber_p(o) WORDBOX_OBJ_TYPE_P(o, FIBER)
#define mrb_istruct_p(o) WORDBOX_OBJ_TYPE_P(o, ISTRUCT)
#define mrb_break_p(o) WORDBOX_OBJ_TYPE_P(o, BREAK)

#ifndef MRB_NO_FLOAT
#define SET_FLOAT_VALUE(mrb,r,v) ((r) = mrb_word_boxing_float_value(mrb, v))
#endif
#define SET_CPTR_VALUE(mrb,r,v) ((r) = mrb_word_boxing_cptr_value(mrb, v))
#define SET_UNDEF_VALUE(r) ((r).w = MRB_Qundef)
#define SET_NIL_VALUE(r) ((r).w = MRB_Qnil)
#define SET_FALSE_VALUE(r) ((r).w = MRB_Qfalse)
#define SET_TRUE_VALUE(r) ((r).w = MRB_Qtrue)
#define SET_BOOL_VALUE(r,b) ((b) ? SET_TRUE_VALUE(r) : SET_FALSE_VALUE(r))
#define SET_INT_VALUE(mrb,r,n) ((r) = mrb_boxing_int_value(mrb, n))
#define SET_FIXNUM_VALUE(r,n) WORDBOX_SET_SHIFT_VALUE(r, FIXNUM, n)
#define SET_SYM_VALUE(r,n) WORDBOX_SET_SHIFT_VALUE(r, SYMBOL, n)
#define SET_OBJ_VALUE(r,v) ((r).w = (uintptr_t)(v))

MRB_INLINE enum mrb_vtype
mrb_type(mrb_value o)
{
  return !mrb_bool(o)    ? MRB_TT_FALSE :
         mrb_true_p(o)   ? MRB_TT_TRUE :
         mrb_fixnum_p(o) ? MRB_TT_INTEGER :
         mrb_symbol_p(o) ? MRB_TT_SYMBOL :
         mrb_undef_p(o)  ? MRB_TT_UNDEF :
         mrb_float_p(o)  ? MRB_TT_FLOAT :
         mrb_val_union(o).bp->tt;
}

MRB_INLINE enum mrb_vtype
mrb_unboxed_type(mrb_value o)
{
  if (mrb_nil_p(o)) {
    return MRB_TT_FALSE;
  } else if ((o.w & WORDBOX_IMMEDIATE_MASK) == 0) {
    return mrb_val_union(o).bp->tt;
  } else {
    return MRB_TT_FALSE;
  }
}



#else

/* ======== mruby/boxing_no.h (inlined) ======== */
/**
** @file mruby/boxing_no.h - unboxed mrb_value definition
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_BOXING_NO_H

#define MRB_FIXNUM_SHIFT 0
#define MRB_SYMBOL_SHIFT 0
#define MRB_FIXNUM_MIN MRB_INT_MIN
#define MRB_FIXNUM_MAX MRB_INT_MAX

union mrb_value_union {
#ifndef MRB_NO_FLOAT
  mrb_float f;
#endif
  void *p;
  mrb_int i;
  mrb_sym sym;
};

typedef struct mrb_value {
  union mrb_value_union value;
  enum mrb_vtype tt;
} mrb_value;

#define mrb_ptr(o)          (o).value.p
#define mrb_cptr(o)         mrb_ptr(o)
#ifndef MRB_NO_FLOAT
#define mrb_float(o)        (o).value.f
#endif
#define mrb_fixnum(o)       (o).value.i
#define mrb_integer(o)      mrb_fixnum(o)
#define mrb_symbol(o)       (o).value.sym
#define mrb_type(o)         (o).tt
#define mrb_unboxed_type(o) (o).tt

#define BOXNO_SET_VALUE(o, ttt, attr, v) do {\
  (o).tt = ttt;\
  (o).attr = v;\
} while (0)

#define SET_NIL_VALUE(r) BOXNO_SET_VALUE(r, MRB_TT_FALSE, value.i, 0)
#define SET_FALSE_VALUE(r) BOXNO_SET_VALUE(r, MRB_TT_FALSE, value.i, 1)
#define SET_TRUE_VALUE(r) BOXNO_SET_VALUE(r, MRB_TT_TRUE, value.i, 1)
#define SET_BOOL_VALUE(r,b) BOXNO_SET_VALUE(r, b ? MRB_TT_TRUE : MRB_TT_FALSE, value.i, 1)
#define SET_INT_VALUE(mrb,r,n) BOXNO_SET_VALUE(r, MRB_TT_INTEGER, value.i, (n))
#define SET_FIXNUM_VALUE(r,n) BOXNO_SET_VALUE(r, MRB_TT_INTEGER, value.i, (n))
#ifndef MRB_NO_FLOAT
#define SET_FLOAT_VALUE(mrb,r,v) BOXNO_SET_VALUE(r, MRB_TT_FLOAT, value.f, (v))
#endif
#define SET_SYM_VALUE(r,v) BOXNO_SET_VALUE(r, MRB_TT_SYMBOL, value.sym, (v))
#define SET_OBJ_VALUE(r,v) BOXNO_SET_VALUE(r, (((struct RObject*)(v))->tt), value.p, (v))
#define SET_CPTR_VALUE(mrb,r,v) BOXNO_SET_VALUE(r, MRB_TT_CPTR, value.p, v)
#define SET_UNDEF_VALUE(r) BOXNO_SET_VALUE(r, MRB_TT_UNDEF, value.i, 0)


#endif

#if INTPTR_MAX < MRB_INT_MAX
  typedef intptr_t mrb_ssize;
# define MRB_SSIZE_MAX INTPTR_MAX
#else
  typedef mrb_int mrb_ssize;
# define MRB_SSIZE_MAX MRB_INT_MAX
#endif

#ifndef mrb_immediate_p
#define mrb_immediate_p(o) (mrb_type(o) <= MRB_TT_CPTR)
#endif
#ifndef mrb_integer_p
#define mrb_integer_p(o) (mrb_type(o) == MRB_TT_INTEGER)
#endif
#ifndef mrb_fixnum_p
#define mrb_fixnum_p(o) mrb_integer_p(o)
#endif
#ifndef mrb_symbol_p
#define mrb_symbol_p(o) (mrb_type(o) == MRB_TT_SYMBOL)
#endif
#ifndef mrb_undef_p
#define mrb_undef_p(o) (mrb_type(o) == MRB_TT_UNDEF)
#endif
#ifndef mrb_nil_p
#define mrb_nil_p(o)  (mrb_type(o) == MRB_TT_FALSE && !mrb_fixnum(o))
#endif
#ifndef mrb_false_p
#define mrb_false_p(o) (mrb_type(o) == MRB_TT_FALSE && !!mrb_fixnum(o))
#endif
#ifndef mrb_true_p
#define mrb_true_p(o)  (mrb_type(o) == MRB_TT_TRUE)
#endif
#ifndef mrb_float_p
#ifndef MRB_NO_FLOAT
#define mrb_float_p(o) (mrb_type(o) == MRB_TT_FLOAT)
#else
#define mrb_float_p(o) FALSE
#endif
#endif
#ifndef mrb_array_p
#define mrb_array_p(o) (mrb_type(o) == MRB_TT_ARRAY)
#endif
#ifndef mrb_string_p
#define mrb_string_p(o) (mrb_type(o) == MRB_TT_STRING)
#endif
#ifndef mrb_hash_p
#define mrb_hash_p(o) (mrb_type(o) == MRB_TT_HASH)
#endif
#ifndef mrb_cptr_p
#define mrb_cptr_p(o) (mrb_type(o) == MRB_TT_CPTR)
#endif
#ifndef mrb_exception_p
#define mrb_exception_p(o) (mrb_type(o) == MRB_TT_EXCEPTION)
#endif
#ifndef mrb_free_p
#define mrb_free_p(o) (mrb_type(o) == MRB_TT_FREE)
#endif
#ifndef mrb_object_p
#define mrb_object_p(o) (mrb_type(o) == MRB_TT_OBJECT)
#endif
#ifndef mrb_class_p
#define mrb_class_p(o) (mrb_type(o) == MRB_TT_CLASS)
#endif
#ifndef mrb_module_p
#define mrb_module_p(o) (mrb_type(o) == MRB_TT_MODULE)
#endif
#ifndef mrb_iclass_p
#define mrb_iclass_p(o) (mrb_type(o) == MRB_TT_ICLASS)
#endif
#ifndef mrb_sclass_p
#define mrb_sclass_p(o) (mrb_type(o) == MRB_TT_SCLASS)
#endif
#ifndef mrb_proc_p
#define mrb_proc_p(o) (mrb_type(o) == MRB_TT_PROC)
#endif
#ifndef mrb_range_p
#define mrb_range_p(o) (mrb_type(o) == MRB_TT_RANGE)
#endif
#ifndef mrb_env_p
#define mrb_env_p(o) (mrb_type(o) == MRB_TT_ENV)
#endif
#ifndef mrb_data_p
#define mrb_data_p(o) (mrb_type(o) == MRB_TT_CDATA)
#endif
#ifndef mrb_fiber_p
#define mrb_fiber_p(o) (mrb_type(o) == MRB_TT_FIBER)
#endif
#ifndef mrb_istruct_p
#define mrb_istruct_p(o) (mrb_type(o) == MRB_TT_ISTRUCT)
#endif
#ifndef mrb_break_p
#define mrb_break_p(o) (mrb_type(o) == MRB_TT_BREAK)
#endif
#ifndef mrb_bool
#define mrb_bool(o)   (mrb_type(o) != MRB_TT_FALSE)
#endif
#define mrb_test(o)   mrb_bool(o)
#ifndef mrb_bigint_p
#ifdef MRB_USE_BIGINT
#define mrb_bigint_p(o) (mrb_type(o) == MRB_TT_BIGINT)
#else
#define mrb_bigint_p(o) FALSE
#endif
#endif

/**
 * Returns a float in Ruby.
 *
 * Takes a float and boxes it into an mrb_value
 */
#ifndef MRB_NO_FLOAT
MRB_INLINE mrb_value
mrb_float_value(struct mrb_state *mrb, mrb_float f)
{
  mrb_value v;
  (void) mrb;
  SET_FLOAT_VALUE(mrb, v, f);
  return v;
}
#endif

MRB_INLINE mrb_value
mrb_cptr_value(struct mrb_state *mrb, void *p)
{
  mrb_value v;
  (void) mrb;
  SET_CPTR_VALUE(mrb,v,p);
  return v;
}

/**
 * Returns an integer in Ruby.
 */
MRB_INLINE mrb_value
mrb_int_value(struct mrb_state *mrb, mrb_int i)
{
  mrb_value v;
  SET_INT_VALUE(mrb, v, i);
  return v;
}

MRB_INLINE mrb_value
mrb_fixnum_value(mrb_int i)
{
  mrb_value v;
  SET_FIXNUM_VALUE(v, i);
  return v;
}

MRB_INLINE mrb_value
mrb_symbol_value(mrb_sym i)
{
  mrb_value v;
  SET_SYM_VALUE(v, i);
  return v;
}

MRB_INLINE mrb_value
mrb_obj_value(void *p)
{
  mrb_value v;
  SET_OBJ_VALUE(v, (struct RBasic*)p);
  return v;
}

/**
 * Get a nil mrb_value object.
 *
 * @return
 *      nil mrb_value object reference.
 */
MRB_INLINE mrb_value
mrb_nil_value(void)
{
  mrb_value v;
  SET_NIL_VALUE(v);
  return v;
}

/**
 * Returns false in Ruby.
 */
MRB_INLINE mrb_value
mrb_false_value(void)
{
  mrb_value v;
  SET_FALSE_VALUE(v);
  return v;
}

/**
 * Returns true in Ruby.
 */
MRB_INLINE mrb_value
mrb_true_value(void)
{
  mrb_value v;
  SET_TRUE_VALUE(v);
  return v;
}

MRB_INLINE mrb_value
mrb_bool_value(mrb_bool boolean)
{
  mrb_value v;
  SET_BOOL_VALUE(v, boolean);
  return v;
}

MRB_INLINE mrb_value
mrb_undef_value(void)
{
  mrb_value v;
  SET_UNDEF_VALUE(v);
  return v;
}

#if defined(MRB_USE_CUSTOM_RO_DATA_P)
/* If you define `MRB_USE_CUSTOM_RO_DATA_P`, you must implement `mrb_ro_data_p()`. */
mrb_bool mrb_ro_data_p(const char *p);
#elif !defined(MRB_NO_DEFAULT_RO_DATA_P)
#if defined(MRB_USE_ETEXT_RO_DATA_P)
#define MRB_LINK_TIME_RO_DATA_P
extern char etext, edata;
static inline mrb_bool
mrb_ro_data_p(const char *p)
{
  return &etext < p && p < &edata;
}
#elif defined(__APPLE__)
#define MRB_LINK_TIME_RO_DATA_P
#include <mach-o/getsect.h>
#include <crt_externs.h> // for _NSGetMachExecuteHeader
static inline mrb_bool
mrb_ro_data_p(const char *p)
{
#ifdef __LP64__
  struct mach_header_64 *mhp;
#else
  struct mach_header *mhp;
#endif
  mhp = _NSGetMachExecuteHeader();
  unsigned long textsize;
  char *text = (char*)getsegmentdata(mhp, SEG_TEXT, &textsize);
  return text <= p && p < text + textsize;
}
#endif  /* Linux or macOS */
#endif  /* MRB_NO_DEFAULT_RO_DATA_P */
#ifndef MRB_LINK_TIME_RO_DATA_P
# define mrb_ro_data_p(p) FALSE
#endif

MRB_END_DECL



/* ======== mruby/gc.h (inlined) ======== */
/**
** @file mruby/gc.h - garbage collector for mruby
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_GC_H

// #include "common.h" - in amalgam

/**
 * Uncommon memory management stuffs.
 */
MRB_BEGIN_DECL


struct mrb_state;

#define MRB_EACH_OBJ_OK 0
#define MRB_EACH_OBJ_BREAK 1
typedef int (mrb_each_object_callback)(struct mrb_state *mrb, struct RBasic *obj, void *data);
void mrb_objspace_each_objects(struct mrb_state *mrb, mrb_each_object_callback *callback, void *data);
size_t mrb_objspace_page_slot_size(void);
MRB_API void mrb_free_context(struct mrb_state *mrb, struct mrb_context *c);

#ifndef MRB_GC_ARENA_SIZE
#define MRB_GC_ARENA_SIZE 100
#endif

#ifndef MRB_GRAY_STACK_SIZE
#define MRB_GRAY_STACK_SIZE 1024
#endif

typedef enum {
  MRB_GC_STATE_ROOT = 0,
  MRB_GC_STATE_MARK,
  MRB_GC_STATE_SWEEP
} mrb_gc_state;

typedef struct mrb_gc {
  struct mrb_heap_page *heaps;     /* all heaps pages */
  struct mrb_heap_page *free_heaps;/* heaps for allocation */
  struct mrb_heap_page *sweeps;    /* page where sweep starts */
  struct mrb_heap_region *regions;  /* contiguous heap regions */
  struct RBasic *gray_stack[MRB_GRAY_STACK_SIZE]; /* stack of gray objects */
  size_t gray_stack_top;           /* top index of gray stack */
  mrb_bool gray_overflow:1;        /* gray stack overflowed; needs heap rescan */
  size_t live;                     /* count of live objects */
  size_t live_after_mark;          /* old generation objects */
  size_t threshold;                /* threshold to start GC */
  size_t oldgen_threshold;         /* threshold to kick major GC */
  mrb_gc_state state;              /* current state of gc */
  int interval_ratio;
  int step_ratio;
  int current_white_part :2;       /* make white object by white_part */
  mrb_bool iterating     :1;       /* currently iterating over objects */
  mrb_bool disabled      :1;       /* GC disabled */
  mrb_bool generational  :1;       /* generational GC mode */
  mrb_bool full          :1;       /* major GC mode */
  mrb_bool out_of_memory :1;       /* out-of-memory error occurred */

#ifdef MRB_GC_FIXED_ARENA
  struct RBasic *arena[MRB_GC_ARENA_SIZE]; /* GC protection array */
#else
  struct RBasic **arena;                   /* GC protection array */
  int arena_capa;                          /* size of protection array */
#endif
  int arena_idx;
} mrb_gc;

MRB_API mrb_bool mrb_object_dead_p(struct mrb_state *mrb, struct RBasic *object);
MRB_API int mrb_gc_add_region(struct mrb_state *mrb, void *start, size_t size);

#define MRB_GC_RED 7

MRB_END_DECL



/* ======== mruby/version.h (inlined) ======== */
/**
** @file mruby/version.h - mruby version definition
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_VERSION_H

// #include "common.h" - in amalgam

/**
 * mruby version definition macros
 */
MRB_BEGIN_DECL

/*
 * A passed in expression.
 */
#define MRB_STRINGIZE0(expr) #expr

/*
 * Passes in an expression to MRB_STRINGIZE0.
 */
#define MRB_STRINGIZE(expr) MRB_STRINGIZE0(expr)

/*
 * The version of Ruby used by mruby.
 */
#define MRUBY_RUBY_VERSION "4.0"

/*
 * Ruby engine.
 */
#define MRUBY_RUBY_ENGINE  "mruby"

/*
 * Major release version number.
 */
#define MRUBY_RELEASE_MAJOR 4

/*
 * Minor release version number.
 */
#define MRUBY_RELEASE_MINOR 0

/*
 * Tiny release version number.
 */
#define MRUBY_RELEASE_TEENY 0

/*
 * Patch level.
 */
#define MRUBY_PATCHLEVEL -1

/*
 * Patch level string. (optional)
 */
#define MRUBY_PATCHLEVEL_STR ""

#ifndef MRUBY_PATCHLEVEL_STR
# if MRUBY_PATCHLEVEL < 0
#   define MRUBY_PATCHLEVEL_STR "dev"
# else
#   define MRUBY_PATCHLEVEL_STR "p"MRB_STRINGIZE(MRUBY_PATCHLEVEL)
# endif
#endif

/*
 * The mruby version.
 */
#define MRUBY_VERSION MRB_STRINGIZE(MRUBY_RELEASE_MAJOR) "." MRB_STRINGIZE(MRUBY_RELEASE_MINOR) "." MRB_STRINGIZE(MRUBY_RELEASE_TEENY)

/*
 * Release number.
 */
#define MRUBY_RELEASE_NO (MRUBY_RELEASE_MAJOR * 100 * 100 + MRUBY_RELEASE_MINOR * 100 + MRUBY_RELEASE_TEENY)

/*
 * Release year.
 */
#define MRUBY_RELEASE_YEAR 2025

/*
 * Release month.
 */
#define MRUBY_RELEASE_MONTH 4

/*
 * Release day.
 */
#define MRUBY_RELEASE_DAY 20

/*
 * Release date as a string.
 */
#define MRUBY_RELEASE_DATE    \
  MRUBY_RELEASE_YEAR_STR "-"  \
  MRUBY_RELEASE_MONTH_STR "-" \
  MRUBY_RELEASE_DAY_STR
#define MRUBY_RELEASE_YEAR_STR MRB_STRINGIZE(MRUBY_RELEASE_YEAR)
#if MRUBY_RELEASE_MONTH < 10
#define MRUBY_RELEASE_MONTH_STR "0" MRB_STRINGIZE(MRUBY_RELEASE_MONTH)
#else
#define MRUBY_RELEASE_MONTH_STR MRB_STRINGIZE(MRUBY_RELEASE_MONTH)
#endif
#if MRUBY_RELEASE_DAY < 10
#define MRUBY_RELEASE_DAY_STR "0" MRB_STRINGIZE(MRUBY_RELEASE_DAY)
#else
#define MRUBY_RELEASE_DAY_STR MRB_STRINGIZE(MRUBY_RELEASE_DAY)
#endif

/*
 * The year mruby was first created.
 */
#define MRUBY_BIRTH_YEAR 2010

/*
 * mruby's authors.
 */
#define MRUBY_AUTHOR "mruby developers"

/*
 * mruby's version, and release date.
 */
#define MRUBY_DESCRIPTION     \
  "mruby " MRUBY_VERSION      \
  MRUBY_PATCHLEVEL_STR        \
  " (" MRUBY_RELEASE_DATE ")" \

/*
 * mruby's copyright information.
 */
#define MRUBY_COPYRIGHT                \
  "mruby - Copyright (c) "             \
  MRB_STRINGIZE(MRUBY_BIRTH_YEAR)"-"   \
  MRB_STRINGIZE(MRUBY_RELEASE_YEAR)" " \
  MRUBY_AUTHOR                         \

MRB_END_DECL



#ifndef MRB_NO_FLOAT
#include <math.h>
#include <float.h>
#ifndef FLT_EPSILON
#define FLT_EPSILON (1.19209290e-07f)
#endif
#ifndef DBL_EPSILON
#define DBL_EPSILON ((double)2.22044604925031308085e-16L)
#endif
#ifndef LDBL_EPSILON
#define LDBL_EPSILON (1.08420217248550443401e-19L)
#endif

#ifdef MRB_USE_FLOAT32
#define MRB_FLOAT_EPSILON FLT_EPSILON
#else
#define MRB_FLOAT_EPSILON DBL_EPSILON
#endif
#endif

/**
 * mruby C API entry point
 */
MRB_BEGIN_DECL

typedef uint8_t mrb_code;

/**
 * \class mrb_aspec
 *
 * Specifies the number of arguments a function takes
 *
 * Example: `MRB_ARGS_REQ(2) | MRB_ARGS_OPT(1)` for a method that expects 2..3 arguments
 */
typedef uint32_t mrb_aspec;

typedef struct mrb_irep mrb_irep;

struct mrb_state;

#ifndef MRB_FIXED_STATE_ATEXIT_STACK_SIZE
#define MRB_FIXED_STATE_ATEXIT_STACK_SIZE 5
#endif

typedef struct {
  uint8_t n:4;                  /* (15=*) c=n|nk<<4 */
  uint8_t nk:4;                 /* (15=*) */
  uint8_t cci;                  /* called from C function */
  uint8_t vis;                  /* 5(ZERO):1(separate module):2(method visibility) */
                                /* under 3-bit flags are copied to env, and after that, env takes precedence */
  mrb_sym mid;
  const struct RProc *proc;
  struct RProc *blk;
  mrb_value *stack;
  const mrb_code *pc;           /* current address on iseq of this proc */
  union {
    struct REnv *env;
    struct RClass *target_class;
    const void *keep_context;   /* if NULL, it means that the fiber has switched; for internal use */
  } u;
} mrb_callinfo;

enum mrb_fiber_state {
  MRB_FIBER_CREATED = 0,
  MRB_FIBER_RUNNING,
  MRB_FIBER_RESUMED,
  MRB_FIBER_SUSPENDED,
  MRB_FIBER_TRANSFERRED,
  MRB_FIBER_TERMINATED,
};

/* Task context status aliases */
#define MRB_TASK_CREATED MRB_FIBER_CREATED
#define MRB_TASK_STOPPED MRB_FIBER_TERMINATED

struct mrb_context {
  struct mrb_context *prev;

  mrb_value *stbase, *stend;              /* stack of virtual machine */

  mrb_callinfo *ci;
  mrb_callinfo *cibase, *ciend;

  enum mrb_fiber_state status : 4;
  mrb_bool vmexec : 1;
  struct RFiber *fib;
};

#ifdef MRB_METHOD_CACHE_SIZE
# undef MRB_NO_METHOD_CACHE
mrb_static_assert_powerof2(MRB_METHOD_CACHE_SIZE);
#else
/* default method cache size: 256 */
/* cache size needs to be power of 2 */
# define MRB_METHOD_CACHE_SIZE (1<<8)
#endif

/**
 * Function pointer type for a function callable by mruby.
 *
 * The arguments to the function are stored on the mrb_state. To get them see mrb_get_args
 *
 * @param mrb The mruby state
 * @param self The self object
 * @return [mrb_value] The function's return value
 */
typedef mrb_value (*mrb_func_t)(struct mrb_state *mrb, mrb_value self);

typedef struct {
  uint32_t flags;                       /* method flags (no symbol packed) */

  union {
    const struct RProc *proc;
    mrb_func_t func;
  } as;
} mrb_method_t;

#ifndef MRB_NO_METHOD_CACHE
struct mrb_cache_entry {
  struct RClass *c, *c0;
  mrb_sym mid;
  mrb_method_t m;
};
#endif

struct mrb_jmpbuf;

typedef void (*mrb_atexit_func)(struct mrb_state*);

#ifdef MRB_USE_TASK_SCHEDULER
struct mrb_task;

typedef struct mrb_task_state {
  struct mrb_task *queues[4];      /* Task queues (dormant, ready, waiting, suspended) */
  volatile uint32_t tick;           /* Current tick count */
  volatile uint32_t wakeup_tick;    /* Next wakeup tick */
  volatile mrb_bool switching;      /* Context switch pending flag */
  struct mrb_task *main_task;       /* Main task wrapper for root context */
  uint8_t scheduler_lock;           /* Lock counter for synchronous execution */
} mrb_task_state;
#endif

typedef struct mrb_state {
  struct mrb_jmpbuf *jmp;

  struct mrb_context *c;
  struct mrb_context *root_c;
  struct iv_tbl *globals;                 /* global variable table */

  struct RObject *exc;                    /* exception */

  struct RObject *top_self;
  struct RClass *object_class;            /* Object class */
  struct RClass *class_class;
  struct RClass *module_class;
  struct RClass *proc_class;
  struct RClass *string_class;
  struct RClass *array_class;
  struct RClass *hash_class;
  struct RClass *range_class;

#ifndef MRB_NO_FLOAT
  struct RClass *float_class;
#endif
  struct RClass *integer_class;
  struct RClass *true_class;
  struct RClass *false_class;
  struct RClass *nil_class;
  struct RClass *symbol_class;
  struct RClass *kernel_module;

  mrb_gc gc;

  mrb_bool bootstrapping;

#ifndef MRB_NO_METHOD_CACHE
  struct mrb_cache_entry cache[MRB_METHOD_CACHE_SIZE];
#endif

  mrb_sym symidx;
  const char **symtbl;
  size_t symcapa;
  struct mrb_sym_hash_table *symhash;
  void *sym_pool;
#ifndef MRB_USE_ALL_SYMBOLS
  char symbuf[8];                         /* buffer for small symbol names */
#endif

#ifdef MRB_USE_DEBUG_HOOK
  void (*code_fetch_hook)(struct mrb_state* mrb, const struct mrb_irep *irep, const mrb_code *pc, mrb_value *regs);
  void (*debug_op_hook)(struct mrb_state* mrb, const struct mrb_irep *irep, const mrb_code *pc, mrb_value *regs);
#endif

#ifdef MRB_BYTECODE_DECODE_OPTION
  mrb_code (*bytecode_decoder)(struct mrb_state* mrb, mrb_code code);
#endif

  struct RClass *eException_class;
  struct RClass *eStandardError_class;
  struct RObject *nomem_err;              /* pre-allocated NoMemoryError */
  struct RObject *stack_err;              /* pre-allocated SystemStackError */
#ifdef MRB_GC_FIXED_ARENA
  struct RObject *arena_err;              /* pre-allocated arena overflow error */
#endif

  struct mrb_mt_rom_list *rom_mt;  /* heap-allocated ROM wrappers (freed at close) */

  struct mrb_iv_shape *root_shape; /* root of IV shape tree */

  void *ud; /* auxiliary data */

#ifdef MRB_FIXED_STATE_ATEXIT_STACK
  mrb_atexit_func atexit_stack[MRB_FIXED_STATE_ATEXIT_STACK_SIZE];
#else
  mrb_atexit_func *atexit_stack;
#endif
  uint16_t atexit_stack_len;

#ifdef MRB_USE_TASK_SCHEDULER
  mrb_task_state task;                    /* Task scheduler state */
#endif
} mrb_state;

/**
 * Defines a new class.
 *
 * If you're creating a gem it may look something like this:
 *
 *      !!!c
 *      void mrb_example_gem_init(mrb_state* mrb) {
 *          struct RClass *example_class;
 *          example_class = mrb_define_class(mrb, "Example_Class", mrb->object_class);
 *      }
 *
 *      void mrb_example_gem_final(mrb_state* mrb) {
 *          //free(TheAnimals);
 *      }
 *
 * @param mrb The current mruby state.
 * @param name The name of the defined class.
 * @param super The new class parent.
 * @return [struct RClass *] Reference to the newly defined class.
 * @see mrb_define_class_under
 */
MRB_API struct RClass *mrb_define_class(mrb_state *mrb, const char *name, struct RClass *super);
MRB_API struct RClass *mrb_define_class_id(mrb_state *mrb, mrb_sym name, struct RClass *super);

/**
 * Defines a new module.
 *
 * @param mrb The current mruby state.
 * @param name The name of the module.
 * @return [struct RClass *] Reference to the newly defined module.
 */
MRB_API struct RClass *mrb_define_module(mrb_state *mrb, const char *name);
MRB_API struct RClass *mrb_define_module_id(mrb_state *mrb, mrb_sym name);

/**
 * Returns the singleton class of an object.
 *
 * Raises a `TypeError` exception for immediate values.
 */
MRB_API mrb_value mrb_singleton_class(mrb_state *mrb, mrb_value val);

/**
 * Returns the singleton class of an object.
 *
 * Returns `NULL` for immediate values,
 */
MRB_API struct RClass *mrb_singleton_class_ptr(mrb_state *mrb, mrb_value val);

/**
 * Include a module in another class or module.
 * Equivalent to:
 *
 *   module B
 *     include A
 *   end
 * @param mrb The current mruby state.
 * @param cla A reference to module or a class.
 * @param included A reference to the module to be included.
 */
MRB_API void mrb_include_module(mrb_state *mrb, struct RClass *cla, struct RClass *included);

/**
 * Prepends a module in another class or module.
 *
 * Equivalent to:
 *  module B
 *    prepend A
 *  end
 * @param mrb The current mruby state.
 * @param cla A reference to module or a class.
 * @param prepended A reference to the module to be prepended.
 */
MRB_API void mrb_prepend_module(mrb_state *mrb, struct RClass *cla, struct RClass *prepended);

/**
 * Defines a global function in Ruby.
 *
 * If you're creating a gem it may look something like this
 *
 * Example:
 *
 *     mrb_value example_method(mrb_state* mrb, mrb_value self)
 *     {
 *          puts("Executing example command!");
 *          return self;
 *     }
 *
 *     void mrb_example_gem_init(mrb_state* mrb)
 *     {
 *           mrb_define_method(mrb, mrb->kernel_module, "example_method", example_method, MRB_ARGS_NONE());
 *     }
 *
 * @param mrb The mruby state reference.
 * @param cla The class pointer where the method will be defined.
 * @param name The name of the method being defined.
 * @param func The function pointer to the method definition.
 * @param aspec The method parameters declaration.
 */

MRB_API void mrb_define_method(mrb_state *mrb, struct RClass *cla, const char *name, mrb_func_t func, mrb_aspec aspec);
MRB_API void mrb_define_method_id(mrb_state *mrb, struct RClass *c, mrb_sym mid, mrb_func_t func, mrb_aspec aspec);
MRB_API void mrb_define_private_method(mrb_state *mrb, struct RClass *cla, const char *name, mrb_func_t func, mrb_aspec aspec);
MRB_API void mrb_define_private_method_id(mrb_state *mrb, struct RClass *c, mrb_sym mid, mrb_func_t func, mrb_aspec aspec);

/**
 * Defines a class method.
 *
 * Example:
 *
 *     # Ruby style
 *     class Foo
 *       def Foo.bar
 *       end
 *     end
 *     // C style
 *     mrb_value bar_method(mrb_state* mrb, mrb_value self){
 *       return mrb_nil_value();
 *     }
 *     void mrb_example_gem_init(mrb_state* mrb){
 *       struct RClass *foo;
 *       foo = mrb_define_class(mrb, "Foo", mrb->object_class);
 *       mrb_define_class_method(mrb, foo, "bar", bar_method, MRB_ARGS_NONE());
 *     }
 * @param mrb The mruby state reference.
 * @param cla The class where the class method will be defined.
 * @param name The name of the class method being defined.
 * @param fun The function pointer to the class method definition.
 * @param aspec The method parameters declaration.
 */
MRB_API void mrb_define_class_method(mrb_state *mrb, struct RClass *cla, const char *name, mrb_func_t fun, mrb_aspec aspec);
MRB_API void mrb_define_class_method_id(mrb_state *mrb, struct RClass *cla, mrb_sym name, mrb_func_t fun, mrb_aspec aspec);

/**
 * Defines a singleton method
 *
 * @see mrb_define_class_method
 */
MRB_API void mrb_define_singleton_method(mrb_state *mrb, struct RObject *cla, const char *name, mrb_func_t fun, mrb_aspec aspec);
MRB_API void mrb_define_singleton_method_id(mrb_state *mrb, struct RObject *cla, mrb_sym name, mrb_func_t fun, mrb_aspec aspec);

/**
 *  Defines a module function.
 *
 * Example:
 *
 *        # Ruby style
 *        module Foo
 *          def Foo.bar
 *          end
 *        end
 *        // C style
 *        mrb_value bar_method(mrb_state* mrb, mrb_value self){
 *          return mrb_nil_value();
 *        }
 *        void mrb_example_gem_init(mrb_state* mrb){
 *          struct RClass *foo;
 *          foo = mrb_define_module(mrb, "Foo");
 *          mrb_define_module_function(mrb, foo, "bar", bar_method, MRB_ARGS_NONE());
 *        }
 *  @param mrb The mruby state reference.
 *  @param cla The module where the module function will be defined.
 *  @param name The name of the module function being defined.
 *  @param fun The function pointer to the module function definition.
 *  @param aspec The method parameters declaration.
 */
MRB_API void mrb_define_module_function(mrb_state *mrb, struct RClass *cla, const char *name, mrb_func_t fun, mrb_aspec aspec);
MRB_API void mrb_define_module_function_id(mrb_state *mrb, struct RClass *cla, mrb_sym name, mrb_func_t fun, mrb_aspec aspec);

/**
 *  Defines a constant.
 *
 * Example:
 *
 *          # Ruby style
 *          class ExampleClass
 *            AGE = 22
 *          end
 *          // C style
 *          #include <stdio.h>
 *          #include <mruby.h>
 *
 *          void
 *          mrb_example_gem_init(mrb_state* mrb){
 *            mrb_define_const(mrb, mrb->kernel_module, "AGE", mrb_fixnum_value(22));
 *          }
 *
 *          mrb_value
 *          mrb_example_gem_final(mrb_state* mrb){
 *          }
 *  @param mrb The mruby state reference.
 *  @param cla A class or module the constant is defined in.
 *  @param name The name of the constant being defined.
 *  @param val The value for the constant.
 */
MRB_API void mrb_define_const(mrb_state* mrb, struct RClass* cla, const char *name, mrb_value val);
MRB_API void mrb_define_const_id(mrb_state* mrb, struct RClass* cla, mrb_sym name, mrb_value val);

/**
 * Undefines a method.
 *
 * Example:
 *
 *     # Ruby style
 *
 *     class ExampleClassA
 *       def example_method
 *         "example"
 *       end
 *     end
 *     ExampleClassA.new.example_method # => example
 *
 *     class ExampleClassB < ExampleClassA
 *       undef_method :example_method
 *     end
 *
 *     ExampleClassB.new.example_method # => undefined method 'example_method' for ExampleClassB (NoMethodError)
 *
 *     // C style
 *     #include <stdio.h>
 *     #include <mruby.h>
 *
 *     mrb_value
 *     mrb_example_method(mrb_state *mrb){
 *       return mrb_str_new_lit(mrb, "example");
 *     }
 *
 *     void
 *     mrb_example_gem_init(mrb_state* mrb){
 *       struct RClass *example_class_a;
 *       struct RClass *example_class_b;
 *       struct RClass *example_class_c;
 *
 *       example_class_a = mrb_define_class(mrb, "ExampleClassA", mrb->object_class);
 *       mrb_define_method(mrb, example_class_a, "example_method", mrb_example_method, MRB_ARGS_NONE());
 *       example_class_b = mrb_define_class(mrb, "ExampleClassB", example_class_a);
 *       example_class_c = mrb_define_class(mrb, "ExampleClassC", example_class_b);
 *       mrb_undef_method(mrb, example_class_c, "example_method");
 *     }
 *
 *     mrb_example_gem_final(mrb_state* mrb){
 *     }
 * @param mrb The mruby state reference.
 * @param cla The class the method will be undefined from.
 * @param name The name of the method to be undefined.
 */
MRB_API void mrb_undef_method(mrb_state *mrb, struct RClass *cla, const char *name);
MRB_API void mrb_undef_method_id(mrb_state*, struct RClass*, mrb_sym);

/**
 * Undefine a class method.
 * Example:
 *
 *      # Ruby style
 *      class ExampleClass
 *        def self.example_method
 *          "example"
 *        end
 *      end
 *
 *     ExampleClass.example_method
 *
 *     // C style
 *     #include <stdio.h>
 *     #include <mruby.h>
 *
 *     mrb_value
 *     mrb_example_method(mrb_state *mrb){
 *       return mrb_str_new_lit(mrb, "example");
 *     }
 *
 *     void
 *     mrb_example_gem_init(mrb_state* mrb){
 *       struct RClass *example_class;
 *       example_class = mrb_define_class(mrb, "ExampleClass", mrb->object_class);
 *       mrb_define_class_method(mrb, example_class, "example_method", mrb_example_method, MRB_ARGS_NONE());
 *       mrb_undef_class_method(mrb, example_class, "example_method");
 *      }
 *
 *      void
 *      mrb_example_gem_final(mrb_state* mrb){
 *      }
 * @param mrb The mruby state reference.
 * @param cls A class the class method will be undefined from.
 * @param name The name of the class method to be undefined.
 */
MRB_API void mrb_undef_class_method(mrb_state *mrb, struct RClass *cls, const char *name);
MRB_API void mrb_undef_class_method_id(mrb_state *mrb, struct RClass *cls, mrb_sym name);

/**
 * Initialize a new object instance of c class.
 *
 * Example:
 *
 *     # Ruby style
 *     class ExampleClass
 *     end
 *
 *     p ExampleClass # => #<ExampleClass:0x9958588>
 *     // C style
 *     #include <stdio.h>
 *     #include <mruby.h>
 *
 *     void
 *     mrb_example_gem_init(mrb_state* mrb) {
 *       struct RClass *example_class;
 *       mrb_value obj;
 *       example_class = mrb_define_class(mrb, "ExampleClass", mrb->object_class); # => class ExampleClass; end
 *       obj = mrb_obj_new(mrb, example_class, 0, NULL); # => ExampleClass.new
 *       mrb_p(mrb, obj); // => Kernel#p
 *      }
 * @param mrb The current mruby state.
 * @param c Reference to the class of the new object.
 * @param argc Number of arguments in argv
 * @param argv Array of mrb_value to initialize the object
 * @return [mrb_value] The newly initialized object
 */
MRB_API mrb_value mrb_obj_new(mrb_state *mrb, struct RClass *c, mrb_int argc, const mrb_value *argv);

/** @see mrb_obj_new */
MRB_INLINE mrb_value mrb_class_new_instance(mrb_state *mrb, mrb_int argc, const mrb_value *argv, struct RClass *c)
{
  return mrb_obj_new(mrb,c,argc,argv);
}

/**
 * Creates a new instance of Class, Class.
 *
 * Example:
 *
 *      void
 *      mrb_example_gem_init(mrb_state* mrb) {
 *        struct RClass *example_class;
 *
 *        mrb_value obj;
 *        example_class = mrb_class_new(mrb, mrb->object_class);
 *        obj = mrb_obj_new(mrb, example_class, 0, NULL); // => #<#<Class:0x9a945b8>:0x9a94588>
 *        mrb_p(mrb, obj); // => Kernel#p
 *       }
 *
 * @param mrb The current mruby state.
 * @param super The super class or parent.
 * @return [struct RClass *] Reference to the new class.
 */
MRB_API struct RClass * mrb_class_new(mrb_state *mrb, struct RClass *super);

/**
 * Creates a new module, Module.
 *
 * Example:
 *      void
 *      mrb_example_gem_init(mrb_state* mrb) {
 *        struct RClass *example_module;
 *
 *        example_module = mrb_module_new(mrb);
 *      }
 *
 * @param mrb The current mruby state.
 * @return [struct RClass *] Reference to the new module.
 */
MRB_API struct RClass * mrb_module_new(mrb_state *mrb);

/**
 * Returns an mrb_bool. True if class was defined, and false if the class was not defined.
 *
 * Example:
 *     void
 *     mrb_example_gem_init(mrb_state* mrb) {
 *       struct RClass *example_class;
 *       mrb_bool cd;
 *
 *       example_class = mrb_define_class(mrb, "ExampleClass", mrb->object_class);
 *       cd = mrb_class_defined(mrb, "ExampleClass");
 *
 *       // If mrb_class_defined returns TRUE then puts "True"
 *       // If mrb_class_defined returns FALSE then puts "False"
 *       if (cd) {
 *         puts("True");
 *       }
 *       else {
 *         puts("False");
 *       }
 *      }
 *
 * @param mrb The current mruby state.
 * @param name A string representing the name of the class.
 * @return [mrb_bool] A boolean value.
 */
MRB_API mrb_bool mrb_class_defined(mrb_state *mrb, const char *name);
MRB_API mrb_bool mrb_class_defined_id(mrb_state *mrb, mrb_sym name);

/**
 * Gets a class.
 * @param mrb The current mruby state.
 * @param name The name of the class.
 * @return [struct RClass *] A reference to the class.
*/
MRB_API struct RClass* mrb_class_get(mrb_state *mrb, const char *name);
MRB_API struct RClass* mrb_class_get_id(mrb_state *mrb, mrb_sym name);

/**
 * Gets a exception class.
 * @param mrb The current mruby state.
 * @param name The name of the class.
 * @return [struct RClass *] A reference to the class.
*/
MRB_API struct RClass* mrb_exc_get_id(mrb_state *mrb, mrb_sym name);
#define mrb_exc_get(mrb, name) mrb_exc_get_id(mrb, mrb_intern_cstr(mrb, name))

/**
 * Returns an mrb_bool. True if inner class was defined, and false if the inner class was not defined.
 *
 * Example:
 *     void
 *     mrb_example_gem_init(mrb_state* mrb) {
 *       struct RClass *example_outer, *example_inner;
 *       mrb_bool cd;
 *
 *       example_outer = mrb_define_module(mrb, "ExampleOuter");
 *
 *       example_inner = mrb_define_class_under(mrb, example_outer, "ExampleInner", mrb->object_class);
 *       cd = mrb_class_defined_under(mrb, example_outer, "ExampleInner");
 *
 *       // If mrb_class_defined_under returns TRUE then puts "True"
 *       // If mrb_class_defined_under returns FALSE then puts "False"
 *       if (cd) {
 *         puts("True");
 *       }
 *       else {
 *         puts("False");
 *       }
 *      }
 *
 * @param mrb The current mruby state.
 * @param outer The name of the outer class.
 * @param name A string representing the name of the inner class.
 * @return [mrb_bool] A boolean value.
 */
MRB_API mrb_bool mrb_class_defined_under(mrb_state *mrb, struct RClass *outer, const char *name);
MRB_API mrb_bool mrb_class_defined_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name);

/**
 * Gets a child class.
 * @param mrb The current mruby state.
 * @param outer The name of the parent class.
 * @param name The name of the class.
 * @return [struct RClass *] A reference to the class.
*/
MRB_API struct RClass * mrb_class_get_under(mrb_state *mrb, struct RClass *outer, const char *name);
MRB_API struct RClass * mrb_class_get_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name);

/**
 * Gets a module.
 * @param mrb The current mruby state.
 * @param name The name of the module.
 * @return [struct RClass *] A reference to the module.
*/
MRB_API struct RClass * mrb_module_get(mrb_state *mrb, const char *name);
MRB_API struct RClass * mrb_module_get_id(mrb_state *mrb, mrb_sym name);

/**
 * Gets a module defined under another module.
 * @param mrb The current mruby state.
 * @param outer The name of the outer module.
 * @param name The name of the module.
 * @return [struct RClass *] A reference to the module.
*/
MRB_API struct RClass * mrb_module_get_under(mrb_state *mrb, struct RClass *outer, const char *name);
MRB_API struct RClass * mrb_module_get_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name);

/* a function to raise NotImplementedError with current method name */
MRB_API void mrb_notimplement(mrb_state*);
/* a function to be replacement of unimplemented method */
MRB_API mrb_value mrb_notimplement_m(mrb_state*, mrb_value);
/* just return it self */
MRB_API mrb_value mrb_obj_itself(mrb_state*, mrb_value);

/**
 * Duplicate an object.
 *
 * Equivalent to:
 *   Object#dup
 * @param mrb The current mruby state.
 * @param obj Object to be duplicate.
 * @return [mrb_value] The newly duplicated object.
 */
MRB_API mrb_value mrb_obj_dup(mrb_state *mrb, mrb_value obj);

/**
 * Returns true if obj responds to the given method. If the method was defined for that
 * class it returns true, it returns false otherwise.
 *
 *      Example:
 *      # Ruby style
 *      class ExampleClass
 *        def example_method
 *        end
 *      end
 *
 *      ExampleClass.new.respond_to?(:example_method) # => true
 *
 *      // C style
 *      void
 *      mrb_example_gem_init(mrb_state* mrb) {
 *        struct RClass *example_class;
 *        mrb_sym mid;
 *        mrb_bool obj_resp;
 *
 *        example_class = mrb_define_class(mrb, "ExampleClass", mrb->object_class);
 *        mrb_define_method(mrb, example_class, "example_method", exampleMethod, MRB_ARGS_NONE());
 *        mid = mrb_intern_str(mrb, mrb_str_new_lit(mrb, "example_method" ));
 *        obj_resp = mrb_obj_respond_to(mrb, example_class, mid); // => TRUE (true in Ruby world)
 *
 *        // If mrb_obj_respond_to returns TRUE then puts "True"
 *        // If mrb_obj_respond_to returns FALSE then puts "False"
 *        if (obj_resp) {
 *          puts("True");
 *        }
 *        else {
 *          puts("False");
 *        }
 *      }
 *
 * @param mrb The current mruby state.
 * @param c A reference to a class.
 * @param mid A symbol referencing a method id.
 * @return [mrb_bool] A boolean value.
 */
MRB_API mrb_bool mrb_obj_respond_to(mrb_state *mrb, struct RClass* c, mrb_sym mid);

/**
 * Defines a new class under a given module
 *
 * @param mrb The current mruby state.
 * @param outer Reference to the module under which the new class will be defined
 * @param name The name of the defined class
 * @param super The new class parent
 * @return [struct RClass *] Reference to the newly defined class
 * @see mrb_define_class
 */
MRB_API struct RClass* mrb_define_class_under(mrb_state *mrb, struct RClass *outer, const char *name, struct RClass *super);
MRB_API struct RClass* mrb_define_class_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name, struct RClass *super);

MRB_API struct RClass* mrb_define_module_under(mrb_state *mrb, struct RClass *outer, const char *name);
MRB_API struct RClass* mrb_define_module_under_id(mrb_state *mrb, struct RClass *outer, mrb_sym name);

/**
 * Function requires n arguments.
 *
 * @param n
 *      The number of required arguments.
 */
#define MRB_ARGS_REQ(n)     ((mrb_aspec)((n)&0x1f) << 18)

/**
 * Function takes n optional arguments
 *
 * @param n
 *      The number of optional arguments.
 */
#define MRB_ARGS_OPT(n)     ((mrb_aspec)((n)&0x1f) << 13)

/**
 * Function takes n1 mandatory arguments and n2 optional arguments
 *
 * @param n1
 *      The number of required arguments.
 * @param n2
 *      The number of optional arguments.
 */
#define MRB_ARGS_ARG(n1,n2)   (MRB_ARGS_REQ(n1)|MRB_ARGS_OPT(n2))

/** rest argument */
#define MRB_ARGS_REST()     ((mrb_aspec)(1 << 12))

/** required arguments after rest */
#define MRB_ARGS_POST(n)    ((mrb_aspec)((n)&0x1f) << 7)

/** keyword arguments (n of keys, kdict) */
#define MRB_ARGS_KEY(n1,n2) ((mrb_aspec)((((n1)&0x1f) << 2) | ((n2)?(1<<1):0)))

/**
 * Function takes a block argument
 */
#define MRB_ARGS_BLOCK()    ((mrb_aspec)1)

/**
 * Function does not accept a block (&nil)
 */
#define MRB_ARGS_NOBLOCK()  ((mrb_aspec)(1 << 23))

/**
 * Function accepts any number of arguments
 */
#define MRB_ARGS_ANY()      MRB_ARGS_REST()

/**
 * Function accepts no arguments
 */
#define MRB_ARGS_NONE()     ((mrb_aspec)0)

/**
 * Format specifiers for {mrb_get_args} function
 *
 * Must be a C string composed of the following format specifiers:
 *
 * | char | Ruby type      | C types           | Notes                                              |
 * |:----:|----------------|-------------------|----------------------------------------------------|
 * | `o`  | {Object}       | {mrb_value}       | Could be used to retrieve any type of argument     |
 * | `C`  | {Class}/{Module} | {mrb_value}     | when `!` follows, the value may be `nil`           |
 * | `S`  | {String}       | {mrb_value}       | when `!` follows, the value may be `nil`           |
 * | `A`  | {Array}        | {mrb_value}       | when `!` follows, the value may be `nil`           |
 * | `H`  | {Hash}         | {mrb_value}       | when `!` follows, the value may be `nil`           |
 * | `s`  | {String}       | const char *, {mrb_int} | Receive two arguments; `s!` gives (`NULL`,`0`) for `nil` |
 * | `z`  | {String}       | const char *      | `NULL` terminated string; `z!` gives `NULL` for `nil` |
 * | `a`  | {Array}        | const {mrb_value} *, {mrb_int} | Receive two arguments; `a!` gives (`NULL`,`0`) for `nil` |
 * | `c`  | {Class}/{Module} | strcut RClass * | `c!` gives `NULL` for `nil`                        |
 * | `f`  | {Integer}/{Float} | {mrb_float}    |                                                    |
 * | `i`  | {Integer}/{Float} | {mrb_int}      |                                                    |
 * | `b`  | boolean        | {mrb_bool}        |                                                    |
 * | `n`  | {String}/{Symbol} | {mrb_sym}         |                                                    |
 * | `d`  | data           | void *, {mrb_data_type} const | 2nd argument will be used to check data type so it won't be modified; when `!` follows, the value may be `nil` |
 * | `I`  | inline struct  | void *, struct RClass | `I!` gives `NULL` for `nil`                    |
 * | `&`  | block          | {mrb_value}       | &! raises exception if no block given.             |
 * | `*`  | rest arguments | const {mrb_value} *, {mrb_int} | Receive the rest of arguments as an array; `*!` avoid copy of the stack.  |
 * | `\|` | optional     |                   | After this spec following specs would be optional. |
 * | `?`  | optional given | {mrb_bool}        | `TRUE` if preceding argument is given. Used to check optional argument is given. |
 * | `:`  | keyword args   | {mrb_kwargs} const | Get keyword arguments. @see mrb_kwargs |
 *
 * @see mrb_get_args
 *
 * Immediately after format specifiers it can add format modifiers:
 *
 * | char | Notes                                                                                   |
 * |:----:|-----------------------------------------------------------------------------------------|
 * | `!`  | Switch to the alternate mode; The behaviour changes depending on the format specifier   |
 * | `+`  | Request a not frozen object; However, except nil value                                  |
 */
typedef const char *mrb_args_format;

/**
 * Get keyword arguments by `mrb_get_args()` with `:` specifier.
 *
 * `mrb_kwargs::num` indicates that the total number of keyword values.
 *
 * `mrb_kwargs::required` indicates that the specified number of keywords starting from the beginning of the `mrb_sym` array are required.
 *
 * `mrb_kwargs::table` accepts a `mrb_sym` array of C.
 *
 * `mrb_kwargs::values` is an object array of C, and the keyword argument corresponding to the `mrb_sym` array is assigned.
 * Note that `undef` is assigned if there is no keyword argument corresponding over `mrb_kwargs::required` to `mrb_kwargs::num`.
 *
 * `mrb_kwargs::rest` is the remaining keyword argument that can be accepted as `**rest` in Ruby.
 * If `NULL` is specified, `ArgumentError` is raised when there is an undefined keyword.
 *
 * Examples:
 *
 *      // def method(a: 1, b: 2)
 *
 *      mrb_int kw_num = 2;
 *      mrb_int kw_required = 0;
 *      mrb_sym kw_names[] = { mrb_intern_lit(mrb, "a"), mrb_intern_lit(mrb, "b") };
 *      mrb_value kw_values[kw_num];
 *      mrb_kwargs kwargs = { kw_num, kw_required, kw_names, kw_values, NULL };
 *
 *      mrb_get_args(mrb, ":", &kwargs);
 *      if (mrb_undef_p(kw_values[0])) { kw_values[0] = mrb_fixnum_value(1); }
 *      if (mrb_undef_p(kw_values[1])) { kw_values[1] = mrb_fixnum_value(2); }
 *
 *
 *      // def method(str, x:, y: 2, z: "default string", **opts)
 *
 *      mrb_value str, kw_rest;
 *      uint32_t kw_num = 3;
 *      uint32_t kw_required = 1;
 *      // `MRB_SYM()` is available via `mruby.h` (which includes `mruby/presym.h`).
 *      // If the usage of `MRB_SYM()` is not desired, replace it with `mrb_intern_lit()`.
 *      mrb_sym kw_names[] = { MRB_SYM(x), MRB_SYM(y), MRB_SYM(z) };
 *      mrb_value kw_values[kw_num];
 *      mrb_kwargs kwargs = { kw_num, kw_required, kw_names, kw_values, &kw_rest };
 *
 *      mrb_get_args(mrb, "S:", &str, &kwargs);
 *      // or: mrb_get_args(mrb, ":S", &kwargs, &str);
 *      if (mrb_undef_p(kw_values[1])) { kw_values[1] = mrb_fixnum_value(2); }
 *      if (mrb_undef_p(kw_values[2])) { kw_values[2] = mrb_str_new_cstr(mrb, "default string"); }
 */
typedef struct mrb_kwargs mrb_kwargs;

struct mrb_kwargs
{
  mrb_int num;                  /* number of keyword arguments */
  mrb_int required;             /* number of required keyword arguments */
  const mrb_sym *table;         /* C array of symbols for keyword names */
  mrb_value *values;            /* keyword argument values */
  mrb_value *rest;              /* keyword rest (dict) */
};

/**
 * Retrieve arguments from mrb_state.
 *
 * @param mrb The current mruby state.
 * @param format is a list of format specifiers
 * @param ... The passing variadic arguments must be a pointer of retrieving type.
 * @return the number of arguments retrieved.
 * @see mrb_args_format
 * @see mrb_kwargs
 */
MRB_API mrb_int mrb_get_args(mrb_state *mrb, mrb_args_format format, ...);

/**
 * Array version of mrb_get_args()
 *
 * @param ptr Array of void*, in the same order as the varargs version.
 */
MRB_API mrb_int mrb_get_args_a(mrb_state *mrb, mrb_args_format format, void** ptr);

MRB_INLINE mrb_sym
mrb_get_mid(mrb_state *mrb) /* get method symbol */
{
  return mrb->c->ci->mid;
}

/**
 * Retrieve number of arguments from mrb_state.
 *
 * Correctly handles *splat arguments.
 */
MRB_API mrb_int mrb_get_argc(mrb_state *mrb);

/**
 * Retrieve an array of arguments from mrb_state.
 *
 * Correctly handles *splat arguments.
 */
MRB_API const mrb_value *mrb_get_argv(mrb_state *mrb);

/**
 * Retrieve the first and only argument from mrb_state.
 * Raises ArgumentError unless the number of arguments is exactly one.
 *
 * Correctly handles *splat arguments.
 */
MRB_API mrb_value mrb_get_arg1(mrb_state *mrb);

/**
 * Check if a block argument is given from mrb_state.
 */
MRB_API mrb_bool mrb_block_given_p(mrb_state *mrb);

/* `strlen` for character string literals (use with caution or `strlen` instead)
    Adjacent string literals are concatenated in C/C++ in translation phase 6.
    If `lit` is not one, the compiler will report a syntax error:
     MSVC: "error C2143: syntax error : missing ')' before 'string'"
     GCC:  "error: expected ')' before string constant"
*/
#define mrb_strlen_lit(lit) (sizeof(lit "") - 1)

/**
 * Call existing Ruby functions.
 *
 * Example:
 *
 *      #include <stdio.h>
 *      #include <mruby.h>
 *      #include <mruby/compile.h>
 *
 *      int
 *      main()
 *      {
 *        mrb_int i = 99;
 *        mrb_state *mrb = mrb_open();
 *
 *        if (!mrb) { }
 *        FILE *fp = fopen("test.rb","r");
 *        mrb_value obj = mrb_load_file(mrb,fp);
 *        mrb_funcall(mrb, obj, "method_name", 1, mrb_fixnum_value(i));
 *        mrb_funcall_id(mrb, obj, MRB_SYM(method_name), 1, mrb_fixnum_value(i));
 *        fclose(fp);
 *        mrb_close(mrb);
 *      }
 *
 * @param mrb The current mruby state.
 * @param val A reference to an mruby value.
 * @param name The name of the method.
 * @param argc The number of arguments the method has.
 * @param ... Variadic values(not type safe!).
 * @return [mrb_value] mruby function value.
 */
MRB_API mrb_value mrb_funcall(mrb_state *mrb, mrb_value val, const char *name, mrb_int argc, ...);
MRB_API mrb_value mrb_funcall_id(mrb_state *mrb, mrb_value val, mrb_sym mid, mrb_int argc, ...);
/**
 * Call existing Ruby functions. This is basically the type safe version of mrb_funcall.
 *
 *      #include <stdio.h>
 *      #include <mruby.h>
 *      #include <mruby/compile.h>
 *      int
 *      main()
 *      {
 *        mrb_state *mrb = mrb_open();
 *        mrb_value obj = mrb_fixnum_value(1);
 *
 *        if (!mrb) { }
 *
 *        FILE *fp = fopen("test.rb","r");
 *        mrb_value obj = mrb_load_file(mrb,fp);
 *        mrb_funcall_argv(mrb, obj, MRB_SYM(method_name), 1, &obj); // Calling Ruby function from test.rb.
 *        fclose(fp);
 *        mrb_close(mrb);
 *       }
 * @param mrb The current mruby state.
 * @param val A reference to an mruby value.
 * @param name_sym The symbol representing the method.
 * @param argc The number of arguments the method has.
 * @param obj Pointer to the object.
 * @return [mrb_value] mrb_value mruby function value.
 * @see mrb_funcall
 */
MRB_API mrb_value mrb_funcall_argv(mrb_state *mrb, mrb_value val, mrb_sym name, mrb_int argc, const mrb_value *argv);
/**
 * Call existing Ruby functions with a block.
 */
MRB_API mrb_value mrb_funcall_with_block(mrb_state *mrb, mrb_value val, mrb_sym name, mrb_int argc, const mrb_value *argv, mrb_value block);
/**
 * Create a symbol from C string. But usually it's better to
 * use MRB_SYM, MRB_OPSYM, MRB_CVSYM, MRB_IVSYM, MRB_GVSYM,
 * MRB_SYM_B, MRB_SYM_Q, MRB_SYM_E macros.
 *
 * Example:
 *
 *     # Ruby style:
 *     :pizza # => :pizza
 *
 *     // C style:
 *     mrb_sym sym1 = mrb_intern_lit(mrb, "pizza"); //  => :pizza
 *     mrb_sym sym2 = MRB_SYM(pizza);               //  => :pizza
 *     mrb_sym sym3 = MRB_SYM_Q(pizza);             //  => :pizza?
 *
 * @param mrb The current mruby state.
 * @param str The string to be symbolized
 * @return [mrb_sym] mrb_sym A symbol.
 */
MRB_API mrb_sym mrb_intern_cstr(mrb_state *mrb, const char* str);
MRB_API mrb_sym mrb_intern(mrb_state*,const char*,size_t);
MRB_API mrb_sym mrb_intern_static(mrb_state*,const char*,size_t);
#define mrb_intern_lit(mrb, lit) mrb_intern_static(mrb, (lit ""), mrb_strlen_lit(lit))
MRB_API mrb_sym mrb_intern_str(mrb_state*,mrb_value);
/* mrb_intern_check series functions returns 0 if the symbol is not defined */
MRB_API mrb_sym mrb_intern_check_cstr(mrb_state*,const char*);
MRB_API mrb_sym mrb_intern_check(mrb_state*,const char*,size_t);
MRB_API mrb_sym mrb_intern_check_str(mrb_state*,mrb_value);
/* mrb_check_intern series functions returns nil if the symbol is not defined */
/* otherwise returns mrb_value */
MRB_API mrb_value mrb_check_intern_cstr(mrb_state*,const char*);
MRB_API mrb_value mrb_check_intern(mrb_state*,const char*,size_t);
MRB_API mrb_value mrb_check_intern_str(mrb_state*,mrb_value);
MRB_API const char *mrb_sym_name(mrb_state*,mrb_sym);
MRB_API const char *mrb_sym_name_len(mrb_state*,mrb_sym,mrb_int*);
MRB_API const char *mrb_sym_dump(mrb_state*,mrb_sym);
MRB_API mrb_value mrb_sym_str(mrb_state*,mrb_sym);
#define mrb_sym2name(mrb,sym) mrb_sym_name(mrb,sym)
#define mrb_sym2name_len(mrb,sym,len) mrb_sym_name_len(mrb,sym,len)
#define mrb_sym2str(mrb,sym) mrb_sym_str(mrb,sym)

MRB_API void *mrb_malloc(mrb_state*, size_t);         /* raise RuntimeError if no mem */
MRB_API void *mrb_calloc(mrb_state*, size_t, size_t); /* ditto */
MRB_API void *mrb_realloc(mrb_state*, void*, size_t); /* ditto */
MRB_API void *mrb_realloc_simple(mrb_state*, void*, size_t); /* return NULL if no memory available */
MRB_API void *mrb_malloc_simple(mrb_state*, size_t);  /* return NULL if no memory available */
MRB_API struct RBasic *mrb_obj_alloc(mrb_state*, enum mrb_vtype, struct RClass*);
MRB_API void mrb_free(mrb_state*, void*);

/**
 * Allocates a Ruby object that matches the constant literal defined in
 * `enum mrb_vtype` and returns a pointer to the corresponding C type.
 *
 * @param mrb   The current mruby state
 * @param tt    The constant literal of `enum mrb_vtype`
 * @param klass A Class object
 * @return      Reference to the newly created object
 */
#define MRB_OBJ_ALLOC(mrb, tt, klass) ((MRB_VTYPE_TYPEOF(tt)*)mrb_obj_alloc(mrb, tt, klass))

MRB_API mrb_value mrb_str_new(mrb_state *mrb, const char *p, mrb_int len);

/**
 * Turns a C string into a Ruby string value.
 */
MRB_API mrb_value mrb_str_new_cstr(mrb_state*, const char*);
MRB_API mrb_value mrb_str_new_static(mrb_state *mrb, const char *p, mrb_int len);
#define mrb_str_new_lit(mrb, lit) mrb_str_new_static(mrb, (lit), mrb_strlen_lit(lit))

MRB_API mrb_value mrb_obj_freeze(mrb_state*, mrb_value);
#define mrb_str_new_frozen(mrb,p,len) mrb_obj_freeze(mrb,mrb_str_new(mrb,p,len))
#define mrb_str_new_cstr_frozen(mrb,p) mrb_obj_freeze(mrb,mrb_str_new_cstr(mrb,p))
#define mrb_str_new_static_frozen(mrb,p,len) mrb_obj_freeze(mrb,mrb_str_new_static(mrb,p,len))
#define mrb_str_new_lit_frozen(mrb,lit) mrb_obj_freeze(mrb,mrb_str_new_lit(mrb,lit))

#ifdef _WIN32
MRB_API char* mrb_utf8_from_locale(const char *p, int len);
MRB_API char* mrb_locale_from_utf8(const char *p, int len);
#define mrb_locale_free(p) free(p)
#define mrb_utf8_free(p) free(p)
#else
#define mrb_utf8_from_locale(p, l) ((char*)(p))
#define mrb_locale_from_utf8(p, l) ((char*)(p))
#define mrb_locale_free(p)
#define mrb_utf8_free(p)
#endif

/**
 * Creates new mrb_state.
 *
 * @return
 *      Pointer to the newly created mrb_state.
 */
MRB_API mrb_state* mrb_open(void);

/**
 * Create new mrb_state with just the mruby core
 *
 * @param f
 *      Reference to the allocation function.
 *      Use mrb_basic_alloc_func for the default
 * @param ud
 *      User data will be passed to custom allocator f.
 *      If user data isn't required just pass NULL.
 * @return
 *      Pointer to the newly created mrb_state.
 */
MRB_API mrb_state* mrb_open_core(void);

/**
 * Closes and frees a mrb_state.
 *
 * @param mrb
 *      Pointer to the mrb_state to be closed.
 */
MRB_API void mrb_close(mrb_state *mrb);
#ifndef MRB_NO_METHOD_CACHE
MRB_API void mrb_method_cache_clear(mrb_state *mrb);
#else
#define mrb_method_cache_clear(mrb) ((void)0)
#endif

/**
 * Check if mrb_open() failed
 *
 * @param mrb
 *      Pointer returned from mrb_open() or mrb_open_core().
 * @return
 *      Non-zero if initialization failed, 0 if succeeded.
 * @note
 *      mrb_open() may return non-NULL even on failure (with mrb->exc set).
 *      Use this macro to check for failure:
 *      @code
 *      mrb_state *mrb = mrb_open();
 *      if (MRB_OPEN_FAILURE(mrb)) {
 *        if (mrb) {
 *          // Inspect mrb->exc for error details
 *          mrb_close(mrb);
 *        }
 *        return EXIT_FAILURE;
 *      }
 *      @endcode
 */
#define MRB_OPEN_FAILURE(mrb) (!(mrb) || (mrb)->exc)

/**
 * Check if mrb_open() succeeded
 *
 * @param mrb
 *      Pointer returned from mrb_open() or mrb_open_core().
 * @return
 *      Non-zero if initialization succeeded, 0 if failed.
 */
#define MRB_OPEN_SUCCESS(mrb) (!MRB_OPEN_FAILURE(mrb))

/**
 * The memory allocation function. You can redefine this function for your own allocator.
 *
 */
MRB_API void* mrb_basic_alloc_func(void*, size_t);

MRB_API mrb_value mrb_top_self(mrb_state *mrb);

/**
 * Enter the mruby VM and execute the proc.
 *
 * @param mrb
 *      The current mruby state.
 * @param proc
 *      An object containing `irep`.
 *      If supplied an object containing anything other than `irep`, it will probably crash.
 * @param self
 *      `self` on the execution context of `proc`.
 * @param stack_keep
 *      Specifies the number of values to hold from the stack top.
 *      Values on the stack outside this range will be initialized to `nil`.
 *
 * @note
 *      When called from a C function defined as a method, the current stack is destroyed.
 *      If you want to use arguments obtained by `mrb_get_args()` or other methods after `mrb_top_run()`,
 *      you must protect them by `mrb_gc_protect()` or other ways before this function.
 *      Or consider using `mrb_yield()` family functions.
 */
MRB_API mrb_value mrb_top_run(mrb_state *mrb, const struct RProc *proc, mrb_value self, mrb_int stack_keep);

MRB_API mrb_value mrb_vm_run(mrb_state *mrb, const struct RProc *proc, mrb_value self, mrb_int stack_keep);
MRB_API mrb_value mrb_vm_exec(mrb_state *mrb, const struct RProc *proc, const mrb_code *iseq);
/* compatibility macros */
#define mrb_toplevel_run_keep(m,p,k) mrb_top_run((m),(p),mrb_top_self(m),(k))
#define mrb_toplevel_run(m,p) mrb_toplevel_run_keep((m),(p),0)
#define mrb_context_run(m,p,s,k) mrb_vm_run((m),(p),(s),(k))

MRB_API void mrb_p(mrb_state*, mrb_value);
MRB_API mrb_int mrb_obj_id(mrb_value obj);
MRB_API mrb_sym mrb_obj_to_sym(mrb_state *mrb, mrb_value name);

MRB_API mrb_bool mrb_obj_eq(mrb_state *mrb, mrb_value a, mrb_value b);
MRB_API mrb_bool mrb_obj_equal(mrb_state *mrb, mrb_value a, mrb_value b);
MRB_API mrb_bool mrb_equal(mrb_state *mrb, mrb_value obj1, mrb_value obj2);
#ifndef MRB_NO_FLOAT
MRB_API mrb_value mrb_ensure_float_type(mrb_state *mrb, mrb_value val);
#define mrb_as_float(mrb, x) mrb_float(mrb_ensure_float_type(mrb, x))
/* obsolete: use mrb_ensure_float_type() instead */
#define mrb_to_float(mrb, val) mrb_ensure_float_type(mrb, val)
#endif
MRB_API mrb_value mrb_inspect(mrb_state *mrb, mrb_value obj);
MRB_API mrb_bool mrb_eql(mrb_state *mrb, mrb_value obj1, mrb_value obj2);
/* mrb_cmp(mrb, obj1, obj2): 1:0:-1; -2 for error */
MRB_API mrb_int mrb_cmp(mrb_state *mrb, mrb_value obj1, mrb_value obj2);

/* recursion detection */
MRB_API mrb_bool mrb_recursive_method_p(mrb_state *mrb, mrb_sym mid, mrb_value obj1, mrb_value obj2);
MRB_API mrb_bool mrb_recursive_func_p(mrb_state *mrb, mrb_sym mid, mrb_value obj1, mrb_value obj2);

#define MRB_RECURSIVE_P(mrb, mid, obj1, obj2) \
  mrb_recursive_method_p(mrb, mid, obj1, obj2)

#define MRB_RECURSIVE_UNARY_P(mrb, mid, obj) \
  mrb_recursive_method_p(mrb, mid, obj, mrb_nil_value())

#define MRB_RECURSIVE_BINARY_P(mrb, mid, obj1, obj2) \
  mrb_recursive_method_p(mrb, mid, obj1, obj2)

#define MRB_RECURSIVE_FUNC_P(mrb, mid, obj) \
  mrb_recursive_func_p(mrb, mid, obj, mrb_nil_value())

#define MRB_RECURSIVE_BINARY_FUNC_P(mrb, mid, obj1, obj2) \
  mrb_recursive_func_p(mrb, mid, obj1, obj2)

#define mrb_gc_arena_save(mrb) ((mrb)->gc.arena_idx)
#define mrb_gc_arena_restore(mrb, idx) ((mrb)->gc.arena_idx = (idx))

MRB_API void mrb_garbage_collect(mrb_state*);
MRB_API void mrb_full_gc(mrb_state*);
MRB_API void mrb_incremental_gc(mrb_state*);
MRB_API void mrb_gc_mark(mrb_state*,struct RBasic*);
#define mrb_gc_mark_value(mrb,val) do {\
  if (!mrb_immediate_p(val)) mrb_gc_mark((mrb), mrb_basic_ptr(val)); \
} while (0)
MRB_API void mrb_field_write_barrier(mrb_state*, struct RBasic*, struct RBasic*);
#define mrb_field_write_barrier_value(mrb, obj, val) do{\
  if (!mrb_immediate_p(val)) mrb_field_write_barrier((mrb), (obj), mrb_basic_ptr(val)); \
} while (0)
MRB_API void mrb_write_barrier(mrb_state *, struct RBasic*);

MRB_API mrb_value mrb_type_convert(mrb_state *mrb, mrb_value val, enum mrb_vtype type, mrb_sym method);
#define mrb_convert_type(mrb, val, type, tname, method) mrb_type_convert(mrb, val, type, mrb_intern_lit(mrb, method))
MRB_API mrb_value mrb_type_convert_check(mrb_state *mrb, mrb_value val, enum mrb_vtype type, mrb_sym method);
#define mrb_check_convert_type(mrb, val, type, tname, method) mrb_type_convert_check(mrb, val, type, mrb_intern_lit(mrb, method))

MRB_API mrb_value mrb_any_to_s(mrb_state *mrb, mrb_value obj);
MRB_API const char * mrb_obj_classname(mrb_state *mrb, mrb_value obj);
MRB_API struct RClass* mrb_obj_class(mrb_state *mrb, mrb_value obj);
MRB_API mrb_value mrb_class_path(mrb_state *mrb, struct RClass *c);
MRB_API mrb_bool mrb_obj_is_kind_of(mrb_state *mrb, mrb_value obj, struct RClass *c);
MRB_API mrb_value mrb_obj_inspect(mrb_state *mrb, mrb_value self);
MRB_API mrb_value mrb_obj_clone(mrb_state *mrb, mrb_value self);

#ifndef ISPRINT
#define ISASCII(c) ((unsigned)(c) <= 0x7f)
#define ISPRINT(c) (((unsigned)(c) - 0x20) < 0x5f)
#define ISSPACE(c) ((c) == ' ' || (unsigned)(c) - '\t' < 5)
#define ISUPPER(c) (((unsigned)(c) - 'A') < 26)
#define ISLOWER(c) (((unsigned)(c) - 'a') < 26)
#define ISALPHA(c) ((((unsigned)(c) | 0x20) - 'a') < 26)
#define ISDIGIT(c) (((unsigned)(c) - '0') < 10)
#define ISXDIGIT(c) (ISDIGIT(c) || ((unsigned)(c) | 0x20) - 'a' < 6)
#define ISALNUM(c) (ISALPHA(c) || ISDIGIT(c))
#define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#define ISCNTRL(c) ((unsigned)(c) < 0x20 || (c) == 0x7f)
#define TOUPPER(c) (ISLOWER(c) ? ((c) & 0x5f) : (c))
#define TOLOWER(c) (ISUPPER(c) ? ((c) | 0x20) : (c))
#endif

MRB_API mrb_value mrb_exc_new(mrb_state *mrb, struct RClass *c, const char *ptr, mrb_int len);
MRB_API mrb_noreturn void mrb_exc_raise(mrb_state *mrb, mrb_value exc);

MRB_API mrb_noreturn void mrb_raise(mrb_state *mrb, struct RClass *c, const char *msg);
MRB_API mrb_noreturn void mrb_raisef(mrb_state *mrb, struct RClass *c, const char *fmt, ...);
MRB_API mrb_noreturn void mrb_name_error(mrb_state *mrb, mrb_sym id, const char *fmt, ...);
MRB_API mrb_noreturn void mrb_frozen_error(mrb_state *mrb, void *frozen_obj);
MRB_API mrb_noreturn void mrb_argnum_error(mrb_state *mrb, mrb_int argc, int min, int max);
MRB_API void mrb_warn(mrb_state *mrb, const char *fmt, ...);
MRB_API mrb_noreturn void mrb_bug(mrb_state *mrb, const char *mesg);
MRB_API void mrb_print_backtrace(mrb_state *mrb);
MRB_API void mrb_print_error(mrb_state *mrb);
/* function for `raisef` formatting */
MRB_API mrb_value mrb_vformat(mrb_state *mrb, const char *format, va_list ap);

/* macros to get typical exception objects
   note:
   + those E_* macros requires mrb_state* variable named mrb.
   + exception objects obtained from those macros are local to mrb
*/
#define MRB_ERROR_SYM(sym) mrb_intern_lit(mrb, #sym)
#define E_EXCEPTION          mrb->eException_class
#define E_STANDARD_ERROR     mrb->eStandardError_class
#define E_RUNTIME_ERROR      mrb_exc_get_id(mrb, MRB_ERROR_SYM(RuntimeError))
#define E_TYPE_ERROR         mrb_exc_get_id(mrb, MRB_ERROR_SYM(TypeError))
#define E_ZERODIV_ERROR      mrb_exc_get_id(mrb, MRB_ERROR_SYM(ZeroDivisionError))
#define E_ARGUMENT_ERROR     mrb_exc_get_id(mrb, MRB_ERROR_SYM(ArgumentError))
#define E_INDEX_ERROR        mrb_exc_get_id(mrb, MRB_ERROR_SYM(IndexError))
#define E_RANGE_ERROR        mrb_exc_get_id(mrb, MRB_ERROR_SYM(RangeError))
#define E_NAME_ERROR         mrb_exc_get_id(mrb, MRB_ERROR_SYM(NameError))
#define E_NOMETHOD_ERROR     mrb_exc_get_id(mrb, MRB_ERROR_SYM(NoMethodError))
#define E_SCRIPT_ERROR       mrb_exc_get_id(mrb, MRB_ERROR_SYM(ScriptError))
#define E_SYNTAX_ERROR       mrb_exc_get_id(mrb, MRB_ERROR_SYM(SyntaxError))
#define E_LOCALJUMP_ERROR    mrb_exc_get_id(mrb, MRB_ERROR_SYM(LocalJumpError))
#define E_REGEXP_ERROR       mrb_exc_get_id(mrb, MRB_ERROR_SYM(RegexpError))
#define E_FROZEN_ERROR       mrb_exc_get_id(mrb, MRB_ERROR_SYM(FrozenError))
#define E_NOTIMP_ERROR       mrb_exc_get_id(mrb, MRB_ERROR_SYM(NotImplementedError))
#define E_KEY_ERROR          mrb_exc_get_id(mrb, MRB_ERROR_SYM(KeyError))
#ifndef MRB_NO_FLOAT
# define E_FLOATDOMAIN_ERROR mrb_exc_get_id(mrb, MRB_ERROR_SYM(FloatDomainError))
#endif

MRB_API mrb_value mrb_yield(mrb_state *mrb, mrb_value b, mrb_value arg);
MRB_API mrb_value mrb_yield_argv(mrb_state *mrb, mrb_value b, mrb_int argc, const mrb_value *argv);
MRB_API mrb_value mrb_yield_with_class(mrb_state *mrb, mrb_value b, mrb_int argc, const mrb_value *argv, mrb_value self, struct RClass *c);

/* continue execution to the proc */
/* this function should always be called as the last function of a method */
/* e.g. return mrb_yield_cont(mrb, proc, self, argc, argv); */
mrb_value mrb_yield_cont(mrb_state *mrb, mrb_value b, mrb_value self, mrb_int argc, const mrb_value *argv);

/* mrb_gc_protect() leaves the object in the arena */
MRB_API void mrb_gc_protect(mrb_state *mrb, mrb_value obj);
/* mrb_gc_register() keeps the object from GC. */
MRB_API void mrb_gc_register(mrb_state *mrb, mrb_value obj);
/* mrb_gc_unregister() removes the object from GC root. */
MRB_API void mrb_gc_unregister(mrb_state *mrb, mrb_value obj);

/* type conversion/check functions */
MRB_API mrb_value mrb_ensure_array_type(mrb_state *mrb, mrb_value self);
MRB_API mrb_value mrb_check_array_type(mrb_state *mrb, mrb_value self);
MRB_API mrb_value mrb_ensure_hash_type(mrb_state *mrb, mrb_value hash);
MRB_API mrb_value mrb_check_hash_type(mrb_state *mrb, mrb_value hash);
MRB_API mrb_value mrb_ensure_string_type(mrb_state *mrb, mrb_value str);
MRB_API mrb_value mrb_check_string_type(mrb_state *mrb, mrb_value str);
/* obsolete: use mrb_ensure_string_type() instead */
#define mrb_string_type(mrb, str) mrb_ensure_string_type(mrb,str)
#define mrb_to_str(mrb, str) mrb_ensure_string_type(mrb,str)
/* obsolete: use mrb_obj_as_string() instead */
#define mrb_str_to_str(mrb, str) mrb_obj_as_string(mrb, str)
/* check if val is an integer (including Bigint) */
MRB_API mrb_value mrb_ensure_integer_type(mrb_state *mrb, mrb_value val);
/* check if val fit in mrb_int */
MRB_API mrb_value mrb_ensure_int_type(mrb_state *mrb, mrb_value val);
#define mrb_as_int(mrb, val) mrb_integer(mrb_ensure_int_type(mrb, val))
/* obsolete: use mrb_ensure_int_type() instead */
#define mrb_to_integer(mrb, val) mrb_ensure_int_type(mrb, val)
#define mrb_to_int(mrb, val) mrb_ensure_int_type(mrb, val)

/* string type checking (contrary to the name, it doesn't convert) */
MRB_API void mrb_check_type(mrb_state *mrb, mrb_value x, enum mrb_vtype t);
MRB_API void mrb_check_frozen(mrb_state *mrb, void *);
MRB_API void mrb_check_frozen_value(mrb_state *mrb, mrb_value v);
MRB_API void mrb_define_alias(mrb_state *mrb, struct RClass *c, const char *a, const char *b);
MRB_API void mrb_define_alias_id(mrb_state *mrb, struct RClass *c, mrb_sym a, mrb_sym b);
MRB_API const char *mrb_class_name(mrb_state *mrb, struct RClass* klass);
MRB_API void mrb_define_global_const(mrb_state *mrb, const char *name, mrb_value val);

MRB_API mrb_value mrb_attr_get(mrb_state *mrb, mrb_value obj, mrb_sym id);

MRB_API mrb_bool mrb_respond_to(mrb_state *mrb, mrb_value obj, mrb_sym mid);
MRB_API mrb_bool mrb_obj_is_instance_of(mrb_state *mrb, mrb_value obj, const struct RClass* c);
MRB_API mrb_bool mrb_func_basic_p(mrb_state *mrb, mrb_value obj, mrb_sym mid, mrb_func_t func);

/* obsolete function(s); will be removed */
#define mrb_int(mrb, val) mrb_as_int(mrb, val)

/**
 * Create a new Fiber from proc object
 *
 * Implemented in mruby-fiber
 */
MRB_API mrb_value mrb_fiber_new(mrb_state *mrb, const struct RProc *proc);

/**
 * Resume a Fiber
 *
 * Implemented in mruby-fiber
 *
 * Switches to the specified fiber and executes. Like the `Fiber#resume` method.
 */
MRB_API mrb_value mrb_fiber_resume(mrb_state *mrb, mrb_value fib, mrb_int argc, const mrb_value *argv);

/**
 * Yield a Fiber
 *
 * Implemented in mruby-fiber
 *
 * Passes control to the caller fiber of the running fiber. Like the `Fiber.yield` method.
 *
 * @note This function is only available from inside a function defined as a method by,
 *       for example, `mrb_define_method()`.
 *       Also, the work following `mrb_fiber_yield()` cannot be performed,
 *       and the return value of `mrb_fiber_yield()` must be returned as is.
 *
 *           return mrb_fiber_yield(mrb, argc, argv);
 */
MRB_API mrb_value mrb_fiber_yield(mrb_state *mrb, mrb_int argc, const mrb_value *argv);

/**
 * Check if a Fiber is alive
 *
 * Implemented in mruby-fiber
 */
MRB_API mrb_value mrb_fiber_alive_p(mrb_state *mrb, mrb_value fib);

/**
 * FiberError reference
 *
 * Implemented in mruby-fiber
 */
#define E_FIBER_ERROR mrb_exc_get_id(mrb, MRB_ERROR_SYM(FiberError))
MRB_API void mrb_stack_extend(mrb_state*, mrb_int);

/* temporary memory allocation, only effective while GC arena is kept */
MRB_API void* mrb_temp_alloc(mrb_state *mrb, size_t);
#define mrb_alloca(mrb, size) mrb_temp_alloc(mrb, size) /* for compatibility */

MRB_API void mrb_state_atexit(mrb_state *mrb, mrb_atexit_func func);

MRB_API void mrb_show_version(mrb_state *mrb);
MRB_API void mrb_show_copyright(mrb_state *mrb);

MRB_API mrb_value mrb_format(mrb_state *mrb, const char *format, ...);

#ifdef MRB_PRESYM_SCANNING

/* ======== mruby/presym/scanning.h (inlined) ======== */
/**
** @file mruby/presym/scanning.h - Scanning Preallocated Symbols
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_PRESYM_SCANNING_H

#define MRB_PRESYM_SCANNING_TAGGED(arg) <@! arg !@>

#undef mrb_intern_lit
#define mrb_intern_lit(mrb, name) MRB_PRESYM_SCANNING_TAGGED(name)
#define mrb_intern_cstr(mrb, name) MRB_PRESYM_SCANNING_TAGGED(name)
#define mrb_define_method(mrb, c, name, ...) MRB_PRESYM_SCANNING_TAGGED(name) (c) (__VA_ARGS__)
#define mrb_define_class_method(mrb, c, name, ...) MRB_PRESYM_SCANNING_TAGGED(name) (c) (__VA_ARGS__)
#define mrb_define_singleton_method(mrb, c, name, ...) MRB_PRESYM_SCANNING_TAGGED(name) (c) (__VA_ARGS__)
#define mrb_define_class(mrb, name, s) MRB_PRESYM_SCANNING_TAGGED(name) (s)
#define mrb_define_class_under(mrb, o, name, s) MRB_PRESYM_SCANNING_TAGGED(name) (o) (s)
#define mrb_define_module(mrb, name) MRB_PRESYM_SCANNING_TAGGED(name)
#define mrb_define_module_under(mrb, o, name) MRB_PRESYM_SCANNING_TAGGED(name) (o)
#define mrb_define_module_function(mrb, c, name, ...) MRB_PRESYM_SCANNING_TAGGED(name) (c) (__VA_ARGS__)
#define mrb_define_const(mrb, c, name, v) MRB_PRESYM_SCANNING_TAGGED(name) (c) (v)
#define mrb_define_global_const(mrb, name, v) MRB_PRESYM_SCANNING_TAGGED(name) (v)
#define mrb_define_alias(mrb, c, a, b) MRB_PRESYM_SCANNING_TAGGED(a) MRB_PRESYM_SCANNING_TAGGED(b) (c)
#define mrb_class_get(mrb, name) MRB_PRESYM_SCANNING_TAGGED(name)
#define mrb_class_get_under(mrb, outer, name) MRB_PRESYM_SCANNING_TAGGED(name) (outer)
#define mrb_module_get(mrb, name) MRB_PRESYM_SCANNING_TAGGED(name)
#define mrb_module_get_under(mrb, outer, name) MRB_PRESYM_SCANNING_TAGGED(name) (outer)
#define mrb_funcall(mrb, v, name, ...) MRB_PRESYM_SCANNING_TAGGED(name) (v) (__VA_ARGS__)

#define MRB_OPSYM(name) MRB_OPSYM__##name(mrb)
#define MRB_GVSYM(name) MRB_PRESYM_SCANNING_TAGGED("$" #name)
#define MRB_CVSYM(name) MRB_PRESYM_SCANNING_TAGGED("@@" #name)
#define MRB_IVSYM(name) MRB_PRESYM_SCANNING_TAGGED("@" #name)
#define MRB_SYM_B(name) MRB_PRESYM_SCANNING_TAGGED(#name "!")
#define MRB_SYM_Q(name) MRB_PRESYM_SCANNING_TAGGED(#name "?")
#define MRB_SYM_E(name) MRB_PRESYM_SCANNING_TAGGED(#name "=")
#define MRB_SYM(name) MRB_PRESYM_SCANNING_TAGGED(#name)

/* backward compatibility: _2 variants accept but ignore mrb_state* */
#define MRB_OPSYM_2(mrb, name) MRB_OPSYM(name)
#define MRB_GVSYM_2(mrb, name) MRB_GVSYM(name)
#define MRB_CVSYM_2(mrb, name) MRB_CVSYM(name)
#define MRB_IVSYM_2(mrb, name) MRB_IVSYM(name)
#define MRB_SYM_B_2(mrb, name) MRB_SYM_B(name)
#define MRB_SYM_Q_2(mrb, name) MRB_SYM_Q(name)
#define MRB_SYM_E_2(mrb, name) MRB_SYM_E(name)
#define MRB_SYM_2(mrb, name) MRB_SYM(name)

#define MRB_OPSYM__not(mrb) MRB_PRESYM_SCANNING_TAGGED("!")
#define MRB_OPSYM__mod(mrb) MRB_PRESYM_SCANNING_TAGGED("%")
#define MRB_OPSYM__and(mrb) MRB_PRESYM_SCANNING_TAGGED("&")
#define MRB_OPSYM__mul(mrb) MRB_PRESYM_SCANNING_TAGGED("*")
#define MRB_OPSYM__add(mrb) MRB_PRESYM_SCANNING_TAGGED("+")
#define MRB_OPSYM__sub(mrb) MRB_PRESYM_SCANNING_TAGGED("-")
#define MRB_OPSYM__div(mrb) MRB_PRESYM_SCANNING_TAGGED("/")
#define MRB_OPSYM__lt(mrb) MRB_PRESYM_SCANNING_TAGGED("<")
#define MRB_OPSYM__gt(mrb) MRB_PRESYM_SCANNING_TAGGED(">")
#define MRB_OPSYM__xor(mrb) MRB_PRESYM_SCANNING_TAGGED("^")
#define MRB_OPSYM__tick(mrb) MRB_PRESYM_SCANNING_TAGGED("`")
#define MRB_OPSYM__or(mrb) MRB_PRESYM_SCANNING_TAGGED("|")
#define MRB_OPSYM__neg(mrb) MRB_PRESYM_SCANNING_TAGGED("~")
#define MRB_OPSYM__neq(mrb) MRB_PRESYM_SCANNING_TAGGED("!=")
#define MRB_OPSYM__nmatch(mrb) MRB_PRESYM_SCANNING_TAGGED("!~")
#define MRB_OPSYM__andand(mrb) MRB_PRESYM_SCANNING_TAGGED("&&")
#define MRB_OPSYM__pow(mrb) MRB_PRESYM_SCANNING_TAGGED("**")
#define MRB_OPSYM__plus(mrb) MRB_PRESYM_SCANNING_TAGGED("+@")
#define MRB_OPSYM__minus(mrb) MRB_PRESYM_SCANNING_TAGGED("-@")
#define MRB_OPSYM__lshift(mrb) MRB_PRESYM_SCANNING_TAGGED("<<")
#define MRB_OPSYM__le(mrb) MRB_PRESYM_SCANNING_TAGGED("<=")
#define MRB_OPSYM__eq(mrb) MRB_PRESYM_SCANNING_TAGGED("==")
#define MRB_OPSYM__match(mrb) MRB_PRESYM_SCANNING_TAGGED("=~")
#define MRB_OPSYM__ge(mrb) MRB_PRESYM_SCANNING_TAGGED(">=")
#define MRB_OPSYM__rshift(mrb) MRB_PRESYM_SCANNING_TAGGED(">>")
#define MRB_OPSYM__aref(mrb) MRB_PRESYM_SCANNING_TAGGED("[]")
#define MRB_OPSYM__oror(mrb) MRB_PRESYM_SCANNING_TAGGED("||")
#define MRB_OPSYM__cmp(mrb) MRB_PRESYM_SCANNING_TAGGED("<=>")
#define MRB_OPSYM__eqq(mrb) MRB_PRESYM_SCANNING_TAGGED("===")
#define MRB_OPSYM__aset(mrb) MRB_PRESYM_SCANNING_TAGGED("[]=")


#else

/* ======== mruby/presym.h (inlined) ======== */
/**
** @file mruby/presym.h - Preallocated Symbols
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_PRESYM_H

#if !defined(MRB_PRESYM_SCANNING)


/* ======== mruby/presym/id.h (inlined) ======== */
enum mruby_presym {
  MRB_OPSYM__not = 1,
  MRB_OPSYM__mod = 2,
  MRB_OPSYM__and = 3,
  MRB_OPSYM__mul = 4,
  MRB_OPSYM__add = 5,
  MRB_OPSYM__sub = 6,
  MRB_OPSYM__div = 7,
  MRB_OPSYM__lt = 8,
  MRB_OPSYM__gt = 9,
  MRB_OPSYM__xor = 10,
  MRB_OPSYM__tick = 11,
  MRB_SYM__a = 12,
  MRB_SYM__e = 13,
  MRB_SYM__h = 14,
  MRB_SYM__i = 15,
  MRB_SYM__k = 16,
  MRB_SYM__p = 17,
  MRB_SYM__s = 18,
  MRB_SYM__v = 19,
  MRB_SYM__x = 20,
  MRB_SYM__y = 21,
  MRB_OPSYM__or = 22,
  MRB_OPSYM__neg = 23,
  MRB_OPSYM__neq = 24,
  MRB_OPSYM__nmatch = 25,
  MRB_OPSYM__pow = 26,
  MRB_OPSYM__plus = 27,
  MRB_OPSYM__minus = 28,
  MRB_OPSYM__lshift = 29,
  MRB_OPSYM__le = 30,
  MRB_OPSYM__eq = 31,
  MRB_OPSYM__match = 32,
  MRB_OPSYM__ge = 33,
  MRB_OPSYM__rshift = 34,
  MRB_SYM__GC = 35,
  MRB_OPSYM__aref = 36,
  MRB_SYM__sv = 37,
  MRB_OPSYM__cmp = 38,
  MRB_OPSYM__eqq = 39,
  MRB_SYM__NAN = 40,
  MRB_OPSYM__aset = 41,
  MRB_SYM__abs = 42,
  MRB_SYM__ary = 43,
  MRB_SYM__cmp = 44,
  MRB_SYM__div = 45,
  MRB_SYM__dup = 46,
  MRB_SYM__end = 47,
  MRB_SYM__idx = 48,
  MRB_SYM__key = 49,
  MRB_SYM__len = 50,
  MRB_SYM__lim = 51,
  MRB_SYM__map = 52,
  MRB_SYM__max = 53,
  MRB_SYM__mid = 54,
  MRB_SYM__min = 55,
  MRB_SYM__new = 56,
  MRB_SYM__num = 57,
  MRB_SYM__obj = 58,
  MRB_SYM__pop = 59,
  MRB_SYM__pos = 60,
  MRB_SYM__quo = 61,
  MRB_SYM__str = 62,
  MRB_SYM__sub = 63,
  MRB_SYM__sym = 64,
  MRB_SYM__val = 65,
  MRB_SYM__Hash = 66,
  MRB_SYM__NONE = 67,
  MRB_SYM__Proc = 68,
  MRB_SYM_Q__all = 69,
  MRB_SYM_Q__any = 70,
  MRB_SYM__args = 71,
  MRB_SYM__attr = 72,
  MRB_SYM__call = 73,
  MRB_SYM__ceil = 74,
  MRB_SYM__chop = 75,
  MRB_SYM__each = 76,
  MRB_SYM_Q__eql = 77,
  MRB_SYM__fdiv = 78,
  MRB_SYM__find = 79,
  MRB_SYM__flag = 80,
  MRB_SYM__grep = 81,
  MRB_SYM__gsub = 82,
  MRB_SYM__hash = 83,
  MRB_SYM__join = 84,
  MRB_SYM_Q__key = 85,
  MRB_SYM__keys = 86,
  MRB_SYM__last = 87,
  MRB_SYM__loop = 88,
  MRB_SYM_B__map = 89,
  MRB_SYM__name = 90,
  MRB_SYM_Q__nan = 91,
  MRB_SYM__next = 92,
  MRB_SYM_Q__nil = 93,
  MRB_SYM__opts = 94,
  MRB_SYM__plen = 95,
  MRB_SYM__push = 96,
  MRB_SYM__size = 97,
  MRB_SYM__sort = 98,
  MRB_SYM__step = 99,
  MRB_SYM_B__sub = 100,
  MRB_SYM__succ = 101,
  MRB_SYM__to_a = 102,
  MRB_SYM__to_f = 103,
  MRB_SYM__to_i = 104,
  MRB_SYM__to_s = 105,
  MRB_SYM__upto = 106,
  MRB_SYM__vals = 107,
  MRB_IVSYM__args = 108,
  MRB_IVSYM__name = 109,
  MRB_SYM__Array = 110,
  MRB_SYM__Class = 111,
  MRB_SYM__Float = 112,
  MRB_SYM__Range = 113,
  MRB_SYM___keys = 114,
  MRB_SYM__arity = 115,
  MRB_SYM__ary_F = 116,
  MRB_SYM__ary_T = 117,
  MRB_SYM__assoc = 118,
  MRB_SYM__begin = 119,
  MRB_SYM__block = 120,
  MRB_SYM__bytes = 121,
  MRB_SYM__chomp = 122,
  MRB_SYM_B__chop = 123,
  MRB_SYM__class = 124,
  MRB_SYM__clear = 125,
  MRB_SYM__clone = 126,
  MRB_SYM__first = 127,
  MRB_SYM__floor = 128,
  MRB_SYM__found = 129,
  MRB_SYM_B__gsub = 130,
  MRB_SYM__index = 131,
  MRB_SYM_Q__is_a = 132,
  MRB_SYM__merge = 133,
  MRB_SYM__other = 134,
  MRB_SYM__print = 135,
  MRB_SYM__raise = 136,
  MRB_SYM__round = 137,
  MRB_SYM__shift = 138,
  MRB_SYM__slice = 139,
  MRB_SYM_B__sort = 140,
  MRB_SYM__split = 141,
  MRB_SYM__start = 142,
  MRB_SYM__store = 143,
  MRB_SYM__times = 144,
  MRB_SYM__Fixnum = 145,
  MRB_SYM__Kernel = 146,
  MRB_SYM__Module = 147,
  MRB_SYM__Object = 148,
  MRB_SYM__String = 149,
  MRB_SYM__Symbol = 150,
  MRB_SYM____id__ = 151,
  MRB_SYM_B__chomp = 152,
  MRB_SYM__concat = 153,
  MRB_SYM__delete = 154,
  MRB_SYM__detect = 155,
  MRB_SYM__divmod = 156,
  MRB_SYM__downto = 157,
  MRB_SYM_Q__empty = 158,
  MRB_SYM__enable = 159,
  MRB_SYM_Q__equal = 160,
  MRB_SYM__extend = 161,
  MRB_SYM__freeze = 162,
  MRB_SYM__ifnone = 163,
  MRB_SYM__inject = 164,
  MRB_SYM__intern = 165,
  MRB_SYM__lambda = 166,
  MRB_SYM__length = 167,
  MRB_SYM__offset = 168,
  MRB_SYM__others = 169,
  MRB_SYM__public = 170,
  MRB_SYM__rassoc = 171,
  MRB_SYM__reduce = 172,
  MRB_SYM__rehash = 173,
  MRB_SYM__reject = 174,
  MRB_SYM__result = 175,
  MRB_SYM__rindex = 176,
  MRB_SYM__select = 177,
  MRB_SYM__string = 178,
  MRB_SYM__to_int = 179,
  MRB_SYM__to_str = 180,
  MRB_SYM__to_sym = 181,
  MRB_SYM__upcase = 182,
  MRB_SYM_Q__value = 183,
  MRB_SYM__values = 184,
  MRB_SYM__Integer = 185,
  MRB_SYM__Numeric = 186,
  MRB_SYM____merge = 187,
  MRB_SYM__collect = 188,
  MRB_SYM__default = 189,
  MRB_SYM__disable = 190,
  MRB_SYM__entries = 191,
  MRB_SYM_Q__finite = 192,
  MRB_SYM_Q__frozen = 193,
  MRB_SYM__getbyte = 194,
  MRB_SYM__include = 195,
  MRB_SYM__inspect = 196,
  MRB_SYM_Q__member = 197,
  MRB_SYM__message = 198,
  MRB_SYM__pattern = 199,
  MRB_SYM__pointer = 200,
  MRB_SYM__prepend = 201,
  MRB_SYM__private = 202,
  MRB_SYM_B__reject = 203,
  MRB_SYM__replace = 204,
  MRB_SYM__reverse = 205,
  MRB_SYM_B__select = 206,
  MRB_SYM__sep_len = 207,
  MRB_SYM__setbyte = 208,
  MRB_SYM__to_enum = 209,
  MRB_SYM__to_hash = 210,
  MRB_SYM__to_proc = 211,
  MRB_SYM__unshift = 212,
  MRB_SYM_B__upcase = 213,
  MRB_SYM__INFINITY = 214,
  MRB_SYM__KeyError = 215,
  MRB_SYM__NilClass = 216,
  MRB_SYM____delete = 217,
  MRB_SYM____except = 218,
  MRB_SYM____send__ = 219,
  MRB_SYM____svalue = 220,
  MRB_SYM____to_int = 221,
  MRB_SYM__allocate = 222,
  MRB_SYM_Q__between = 223,
  MRB_SYM__bytesize = 224,
  MRB_SYM_B__collect = 225,
  MRB_SYM_E__default = 226,
  MRB_SYM__downcase = 227,
  MRB_SYM__each_key = 228,
  MRB_SYM__extended = 229,
  MRB_SYM__find_all = 230,
  MRB_SYM_Q__has_key = 231,
  MRB_SYM_Q__include = 232,
  MRB_SYM__included = 233,
  MRB_SYM_Q__kind_of = 234,
  MRB_SYM_B__reverse = 235,
  MRB_SYM__self_len = 236,
  MRB_SYM__str_each = 237,
  MRB_SYM__truncate = 238,
  MRB_SYM__Exception = 239,
  MRB_SYM__NameError = 240,
  MRB_SYM__TrueClass = 241,
  MRB_SYM__TypeError = 242,
  MRB_SYM____compact = 243,
  MRB_SYM____outer__ = 244,
  MRB_SYM___gc_root_ = 245,
  MRB_SYM___sys_fail = 246,
  MRB_SYM__ancestors = 247,
  MRB_SYM__backtrace = 248,
  MRB_SYM__byteindex = 249,
  MRB_SYM__byteslice = 250,
  MRB_SYM__const_get = 251,
  MRB_SYM__const_set = 252,
  MRB_SYM__delete_at = 253,
  MRB_SYM_B__downcase = 254,
  MRB_SYM__each_byte = 255,
  MRB_SYM__each_line = 256,
  MRB_SYM__exception = 257,
  MRB_SYM_Q__infinite = 258,
  MRB_SYM__inherited = 259,
  MRB_SYM_Q__iterator = 260,
  MRB_SYM__object_id = 261,
  MRB_SYM__partition = 262,
  MRB_SYM__prepended = 263,
  MRB_SYM__protected = 264,
  MRB_SYM__separator = 265,
  MRB_SYM__Comparable = 266,
  MRB_SYM__Enumerable = 267,
  MRB_SYM__FalseClass = 268,
  MRB_SYM__IndexError = 269,
  MRB_SYM__RangeError = 270,
  MRB_SYM____case_eqq = 271,
  MRB_SYM____num_to_a = 272,
  MRB_SYM__byterindex = 273,
  MRB_SYM__bytesplice = 274,
  MRB_SYM__capitalize = 275,
  MRB_SYM__class_eval = 276,
  MRB_SYM__each_index = 277,
  MRB_SYM__each_value = 278,
  MRB_SYM_Q__has_value = 279,
  MRB_SYM__initialize = 280,
  MRB_SYM__step_ratio = 281,
  MRB_SYM__superclass = 282,
  MRB_SYM__BasicObject = 283,
  MRB_SYM__FrozenError = 284,
  MRB_SYM__RUBY_ENGINE = 285,
  MRB_SYM__RegexpError = 286,
  MRB_SYM__ScriptError = 287,
  MRB_SYM__SyntaxError = 288,
  MRB_SYM__attr_reader = 289,
  MRB_SYM__attr_writer = 290,
  MRB_SYM_B__capitalize = 291,
  MRB_SYM__const_added = 292,
  MRB_SYM__deconstruct = 293,
  MRB_SYM__module_eval = 294,
  MRB_SYM_Q__respond_to = 295,
  MRB_SYM_E__step_ratio = 296,
  MRB_SYM__RUBY_VERSION = 297,
  MRB_SYM__RuntimeError = 298,
  MRB_SYM____ENCODING__ = 299,
  MRB_SYM____attached__ = 300,
  MRB_SYM____pat_values = 301,
  MRB_SYM__alias_method = 302,
  MRB_SYM_Q__block_given = 303,
  MRB_SYM__default_proc = 304,
  MRB_SYM_Q__exclude_end = 305,
  MRB_SYM_Q__instance_of = 306,
  MRB_SYM__method_added = 307,
  MRB_SYM__remove_const = 308,
  MRB_SYM__undef_method = 309,
  MRB_SYM__ArgumentError = 310,
  MRB_SYM__MRUBY_VERSION = 311,
  MRB_SYM__NoMemoryError = 312,
  MRB_SYM__NoMethodError = 313,
  MRB_SYM__StandardError = 314,
  MRB_SYM__StopIteration = 315,
  MRB_SYM____classname__ = 316,
  MRB_SYM____sub_replace = 317,
  MRB_SYM____update_hash = 318,
  MRB_SYM__attr_accessor = 319,
  MRB_SYM__const_missing = 320,
  MRB_SYM_E__default_proc = 321,
  MRB_SYM__define_method = 322,
  MRB_SYM__instance_eval = 323,
  MRB_SYM__set_backtrace = 324,
  MRB_SYM__LocalJumpError = 325,
  MRB_SYM____upto_endless = 326,
  MRB_SYM_Q__const_defined = 327,
  MRB_SYM__interval_ratio = 328,
  MRB_SYM__method_missing = 329,
  MRB_SYM__method_removed = 330,
  MRB_SYM__paragraph_mode = 331,
  MRB_SYM__MRUBY_COPYRIGHT = 332,
  MRB_SYM__SystemCallError = 333,
  MRB_SYM__each_with_index = 334,
  MRB_SYM__initialize_copy = 335,
  MRB_SYM_E__interval_ratio = 336,
  MRB_SYM_Q__method_defined = 337,
  MRB_SYM__module_function = 338,
  MRB_SYM__FloatDomainError = 339,
  MRB_SYM__MRUBY_RELEASE_NO = 340,
  MRB_SYM__SystemStackError = 341,
  MRB_SYM__deconstruct_keys = 342,
  MRB_SYM__method_undefined = 343,
  MRB_SYM__MRUBY_DESCRIPTION = 344,
  MRB_SYM__ZeroDivisionError = 345,
  MRB_SYM__generational_mode = 346,
  MRB_SYM__MRUBY_RELEASE_DATE = 347,
  MRB_SYM_E__generational_mode = 348,
  MRB_SYM__NotImplementedError = 349,
  MRB_SYM__RUBY_ENGINE_VERSION = 350,
  MRB_SYM_Q____method_recursive = 351,
  MRB_SYM_Q__respond_to_missing = 352,
  MRB_SYM____coerce_step_counter = 353,
  MRB_SYM__NoMatchingPatternError = 354,
  MRB_SYM__singleton_method_added = 355,
  MRB_SYM__remove_instance_variable = 356,
  MRB_SYM__singleton_method_removed = 357,
  MRB_SYM__singleton_method_undefined = 358,
};

#define MRB_PRESYM_MAX 358


/*
 * Where `mrb_intern_lit` is allowed for symbol interning, it is directly
 * replaced by the symbol ID using the following macros.
 *
 *   MRB_OPSYM(xor)  //=> ^      (Operator)
 *   MRB_GVSYM(xor)  //=> $xor   (Global Variable)
 *   MRB_CVSYM(xor)  //=> @@xor  (Class Variable)
 *   MRB_IVSYM(xor)  //=> @xor   (Instance Variable)
 *   MRB_SYM_B(xor)  //=> xor!   (Method with Bang)
 *   MRB_SYM_Q(xor)  //=> xor?   (Method with Question mark)
 *   MRB_SYM_E(xor)  //=> xor=   (Method with Equal)
 *   MRB_SYM(xor)    //=> xor    (Word characters)
 *
 * For `MRB_OPSYM`, specify the names corresponding to operators (see
 * `MRuby::Presym::OPERATORS` in `lib/mruby/presym.rb` for the names that
 * can be specified for it). Other than that, describe only word characters
 * excluding leading and ending punctuation.
 *
 * These macros are expanded to compile-time integer constants.
 */

#define MRB_OPSYM(name) MRB_OPSYM__##name
#define MRB_GVSYM(name) MRB_GVSYM__##name
#define MRB_CVSYM(name) MRB_CVSYM__##name
#define MRB_IVSYM(name) MRB_IVSYM__##name
#define MRB_SYM_B(name) MRB_SYM_B__##name
#define MRB_SYM_Q(name) MRB_SYM_Q__##name
#define MRB_SYM_E(name) MRB_SYM_E__##name
#define MRB_SYM(name) MRB_SYM__##name

/* backward compatibility: _2 variants accept but ignore mrb_state* */
#define MRB_OPSYM_2(mrb, name) MRB_OPSYM(name)
#define MRB_GVSYM_2(mrb, name) MRB_GVSYM(name)
#define MRB_CVSYM_2(mrb, name) MRB_CVSYM(name)
#define MRB_IVSYM_2(mrb, name) MRB_IVSYM(name)
#define MRB_SYM_B_2(mrb, name) MRB_SYM_B(name)
#define MRB_SYM_Q_2(mrb, name) MRB_SYM_Q(name)
#define MRB_SYM_E_2(mrb, name) MRB_SYM_E(name)
#define MRB_SYM_2(mrb, name) MRB_SYM(name)

#define MRB_PRESYM_DEFINE_VAR_AND_INITER(name, size, ...)                     \
  static const mrb_sym name[] = {__VA_ARGS__};

#define MRB_PRESYM_INIT_SYMBOLS(mrb, name) (void)(mrb)

/* use MRB_SYM() for E_RUNTIME_ERROR etc. */
#undef MRB_ERROR_SYM
#define MRB_ERROR_SYM(sym) MRB_SYM(sym)

#endif  /* !MRB_PRESYM_SCANNING */


#endif

#if 0
/* memcpy and memset does not work with gdb reverse-next on my box */
/* use naive memcpy and memset instead */
#undef memcpy
#undef memset
static void*
mrbmemcpy(void *dst, const void *src, size_t n)
{
  char *d = (char*)dst;
  const char *s = (const char*)src;
  while (n--)
    *d++ = *s++;
  return d;
}
#define memcpy(a,b,c) mrbmemcpy(a,b,c)

static void*
mrbmemset(void *s, int c, size_t n)
{
  char *t = (char*)s;
  while (n--)
    *t++ = c;
  return s;
}
#define memset(a,b,c) mrbmemset(a,b,c)
#endif

#define mrb_int_hash_func(mrb,key) (uint32_t)((key)^((key)<<2)^((key)>>2))

#define MRB_UNIQNAME(name)         MRB_UNIQNAME_1(name, __LINE__)
#define MRB_UNIQNAME_1(name, line) MRB_UNIQNAME_2(name, line)
#define MRB_UNIQNAME_2(name, line) name##line

MRB_END_DECL


/* ======== mruby/array.h ======== */
/**
** @file mruby/array.h - Array class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_ARRAY_H

// #include "common.h" - in amalgam

/*
 * Array class
 */
MRB_BEGIN_DECL

typedef struct mrb_shared_array {
  int refcnt;
  mrb_ssize len;
  mrb_value *ptr;
} mrb_shared_array;

#if defined(MRB_32BIT) && defined(MRB_NO_BOXING) && (!defined(MRB_USE_FLOAT32) || defined(MRB_INT64)) && !defined(MRB_ARY_NO_EMBED)
# define MRB_ARY_NO_EMBED
#endif

#ifdef MRB_ARY_NO_EMBED
# define MRB_ARY_EMBED_LEN_MAX 0
#else
# define MRB_ARY_EMBED_LEN_MAX ((mrb_int)(sizeof(void*)*3/sizeof(mrb_value)))
mrb_static_assert(MRB_ARY_EMBED_LEN_MAX > 0, "MRB_ARY_EMBED_LEN_MAX > 0");
#endif

struct RArray {
  MRB_OBJECT_HEADER;
  union {
    struct {
      mrb_ssize len;
      union {
        mrb_ssize capa;
        mrb_shared_array *shared;
      } aux;
      mrb_value *ptr;
    } heap;
#ifndef MRB_ARY_NO_EMBED
    mrb_value ary[MRB_ARY_EMBED_LEN_MAX];
#endif
  } as;
};

#define mrb_ary_ptr(v)    ((struct RArray*)(mrb_ptr(v)))
#define mrb_ary_value(p)  mrb_obj_value((void*)(p))
#define RARRAY(v)  ((struct RArray*)(mrb_ptr(v)))

#ifdef MRB_ARY_NO_EMBED
#define ARY_EMBED_P(a) 0
#define ARY_UNSET_EMBED_FLAG(a) (void)0
#define ARY_EMBED_LEN(a) 0
#define ARY_SET_EMBED_LEN(a,len) (void)0
#define ARY_EMBED_PTR(a) 0
#else
#define MRB_ARY_EMBED_MASK  7
#define ARY_EMBED_P(a) ((a)->flags & MRB_ARY_EMBED_MASK)
#define ARY_UNSET_EMBED_FLAG(a) ((a)->flags &= ~(MRB_ARY_EMBED_MASK))
#define ARY_EMBED_LEN(a) ((mrb_int)(((a)->flags & MRB_ARY_EMBED_MASK) - 1))
#define ARY_SET_EMBED_LEN(a,len) ((a)->flags = ((a)->flags&~MRB_ARY_EMBED_MASK) | ((uint32_t)(len) + 1))
#define ARY_EMBED_PTR(a) ((a)->as.ary)
#endif

#define ARY_LEN(a) (ARY_EMBED_P(a)?ARY_EMBED_LEN(a):(mrb_int)(a)->as.heap.len)
#define ARY_PTR(a) (ARY_EMBED_P(a)?ARY_EMBED_PTR(a):(a)->as.heap.ptr)
#define RARRAY_LEN(a) ARY_LEN(RARRAY(a))
#define RARRAY_PTR(a) ARY_PTR(RARRAY(a))
#define ARY_SET_LEN(a,n) do {\
  if (ARY_EMBED_P(a)) {\
    mrb_assert((n) <= MRB_ARY_EMBED_LEN_MAX); \
    ARY_SET_EMBED_LEN(a,n);\
  }\
  else\
    (a)->as.heap.len = (n);\
} while (0)
#define ARY_CAPA(a) (ARY_EMBED_P(a)?MRB_ARY_EMBED_LEN_MAX:(a)->as.heap.aux.capa)
#define MRB_ARY_SHARED      256
#define ARY_SHARED_P(a) ((a)->flags & MRB_ARY_SHARED)
#define ARY_SET_SHARED_FLAG(a) ((a)->flags |= MRB_ARY_SHARED)
#define ARY_UNSET_SHARED_FLAG(a) ((a)->flags &= ~MRB_ARY_SHARED)

MRB_API void mrb_ary_modify(mrb_state*, struct RArray*);
MRB_API mrb_value mrb_ary_dup(mrb_state*, mrb_value ary);
MRB_API mrb_value mrb_ary_make_shared_copy(mrb_state*, mrb_value ary);
MRB_API mrb_value mrb_ary_new_capa(mrb_state*, mrb_int);

/*
 * Initializes a new array.
 *
 * Equivalent to:
 *
 *      Array.new
 *
 * @param mrb The mruby state reference.
 * @return The initialized array.
 */
MRB_API mrb_value mrb_ary_new(mrb_state *mrb);

/*
 * Initializes a new array with initial values
 *
 * Equivalent to:
 *
 *      Array[value1, value2, ...]
 *
 * @param mrb The mruby state reference.
 * @param size The number of values.
 * @param vals The actual values.
 * @return The initialized array.
 */
MRB_API mrb_value mrb_ary_new_from_values(mrb_state *mrb, mrb_int size, const mrb_value *vals);

/*
 * Initializes a new array with two initial values
 *
 * Equivalent to:
 *
 *      Array[car, cdr]
 *
 * @param mrb The mruby state reference.
 * @param car The first value.
 * @param cdr The second value.
 * @return The initialized array.
 */
MRB_API mrb_value mrb_assoc_new(mrb_state *mrb, mrb_value car, mrb_value cdr);

/*
 * Concatenate two arrays. The target array will be modified
 *
 * Equivalent to:
 *      ary.concat(other)
 *
 * @param mrb The mruby state reference.
 * @param self The target array.
 * @param other The array that will be concatenated to self.
 */
MRB_API void mrb_ary_concat(mrb_state *mrb, mrb_value self, mrb_value other);

/*
 * Create an array from the input. It tries calling to_a on the
 * value. If value does not respond to that, it creates a new
 * array with just this value.
 *
 * @param mrb The mruby state reference.
 * @param value The value to change into an array.
 * @return An array representation of value.
 */
MRB_API mrb_value mrb_ary_splat(mrb_state *mrb, mrb_value value);

/*
 * Pushes value into array.
 *
 * Equivalent to:
 *
 *      ary << value
 *
 * @param mrb The mruby state reference.
 * @param ary The array in which the value will be pushed
 * @param value The value to be pushed into array
 */
MRB_API void mrb_ary_push(mrb_state *mrb, mrb_value array, mrb_value value);

/*
 * Pops the last element from the array.
 *
 * Equivalent to:
 *
 *      ary.pop
 *
 * @param mrb The mruby state reference.
 * @param ary The array from which the value will be popped.
 * @return The popped value.
 */
MRB_API mrb_value mrb_ary_pop(mrb_state *mrb, mrb_value ary);

/*
 * Sets a value on an array at the given index
 *
 * Equivalent to:
 *
 *      ary[n] = val
 *
 * @param mrb The mruby state reference.
 * @param ary The target array.
 * @param n The array index being referenced.
 * @param val The value being set.
 */
MRB_API void mrb_ary_set(mrb_state *mrb, mrb_value ary, mrb_int n, mrb_value val);

/*
 * Replace the array with another array
 *
 * Equivalent to:
 *
 *      ary.replace(other)
 *
 * @param mrb The mruby state reference
 * @param self The target array.
 * @param other The array to replace it with.
 */
MRB_API void mrb_ary_replace(mrb_state *mrb, mrb_value self, mrb_value other);

/*
 * Unshift an element into the array
 *
 * Equivalent to:
 *
 *     ary.unshift(item)
 *
 * @param mrb The mruby state reference.
 * @param self The target array.
 * @param item The item to unshift.
 */
MRB_API mrb_value mrb_ary_unshift(mrb_state *mrb, mrb_value self, mrb_value item);

/*
 * Get nth element in the array
 *
 * Equivalent to:
 *
 *     ary[offset]
 *
 * @param ary The target array.
 * @param offset The element position (negative counts from the tail).
 */
MRB_API mrb_value mrb_ary_entry(mrb_value ary, mrb_int offset);
#define mrb_ary_ref(mrb, ary, n) mrb_ary_entry(ary, n)

/*
 * Replace subsequence of an array.
 *
 * Equivalent to:
 *
 *      ary[head, len] = rpl
 *
 * @param mrb The mruby state reference.
 * @param self The array from which the value will be partiality replaced.
 * @param head Beginning position of a replacement subsequence.
 * @param len Length of a replacement subsequence.
 * @param rpl The array of replacement elements.
 *            It is possible to pass `mrb_undef_value()` instead of an empty array.
 * @return The receiver array.
 */
MRB_API mrb_value mrb_ary_splice(mrb_state *mrb, mrb_value self, mrb_int head, mrb_int len, mrb_value rpl);

/*
 * Shifts the first element from the array.
 *
 * Equivalent to:
 *
 *      ary.shift
 *
 * @param mrb The mruby state reference.
 * @param self The array from which the value will be shifted.
 * @return The shifted value.
 */
MRB_API mrb_value mrb_ary_shift(mrb_state *mrb, mrb_value self);

/*
 * Removes all elements from the array
 *
 * Equivalent to:
 *
 *      ary.clear
 *
 * @param mrb The mruby state reference.
 * @param self The target array.
 * @return self
 */
MRB_API mrb_value mrb_ary_clear(mrb_state *mrb, mrb_value self);

/*
 * Join the array elements together in a string
 *
 * Equivalent to:
 *
 *      ary.join(sep="")
 *
 * @param mrb The mruby state reference.
 * @param ary The target array
 * @param sep The separator, can be NULL
 */
MRB_API mrb_value mrb_ary_join(mrb_state *mrb, mrb_value ary, mrb_value sep);

/*
 * Update the capacity of the array
 *
 * @param mrb The mruby state reference.
 * @param ary The target array.
 * @param new_len The new capacity of the array
 */
MRB_API mrb_value mrb_ary_resize(mrb_state *mrb, mrb_value ary, mrb_int new_len);


MRB_END_DECL


/* ======== mruby/string.h ======== */
/**
** @file mruby/string.h - String class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_STRING_H

// #include "common.h" - in amalgam

/**
 * String class
 */
MRB_BEGIN_DECL

extern const char mrb_digitmap[];

#define RSTRING_EMBED_LEN_MAX \
  ((mrb_int)(sizeof(void*) * 3 + sizeof(void*) - 32 / CHAR_BIT - 1))

struct RString {
  MRB_OBJECT_HEADER;
  union {
    struct {
      mrb_ssize len;
      union {
        mrb_ssize capa;
        struct mrb_shared_string *shared;
        struct RString *fshared;
      } aux;
      char *ptr;
    } heap;
  } as;
};
struct RStringEmbed {
  MRB_OBJECT_HEADER;
  char ary[RSTRING_EMBED_LEN_MAX+1];
};

#define RSTR_SET_TYPE(s, type) ((s)->flags = ((s)->flags & ~(MRB_STR_TYPE_MASK|MRB_STR_EMBED_LEN_MASK)) | MRB_STR_##type)

#define MRB_STR_NORMAL    0
#define MRB_STR_SHARED    1
#define MRB_STR_FSHARED   2
#define MRB_STR_NOFREE    4
#define MRB_STR_EMBED     8
#define MRB_STR_TYPE_MASK 15

#define MRB_STR_EMBED_LEN_SHIFT 6
#define MRB_STR_EMBED_LEN_BITS 5
#define MRB_STR_EMBED_LEN_MASK (((1 << MRB_STR_EMBED_LEN_BITS) - 1) << MRB_STR_EMBED_LEN_SHIFT)

#define MRB_STR_BINARY    16
#define MRB_STR_SINGLE_BYTE 32
#define MRB_STR_STATE_MASK 48

#define RSTR_EMBED_P(s) ((s)->flags & MRB_STR_EMBED)
#define RSTR_SET_EMBED_FLAG(s) ((s)->flags |= MRB_STR_EMBED)
#define RSTR_SET_EMBED_LEN(s, n) do {\
  size_t tmp_n = (n);\
  (s)->flags &= ~MRB_STR_EMBED_LEN_MASK;\
  (s)->flags |= (tmp_n) << MRB_STR_EMBED_LEN_SHIFT;\
} while (0)
#define RSTR_SET_LEN(s, n) do {\
  if (RSTR_EMBED_P(s)) {\
    RSTR_SET_EMBED_LEN((s),(n));\
  }\
  else {\
    (s)->as.heap.len = (mrb_ssize)(n);\
  }\
} while (0)
#define RSTR_EMBED_PTR(s) (((struct RStringEmbed*)(s))->ary)
#define RSTR_EMBED_LEN(s)\
  (mrb_int)(((s)->flags & MRB_STR_EMBED_LEN_MASK) >> MRB_STR_EMBED_LEN_SHIFT)
#define RSTR_EMBEDDABLE_P(len) ((len) <= RSTRING_EMBED_LEN_MAX)

#define RSTR_PTR(s) ((RSTR_EMBED_P(s)) ? RSTR_EMBED_PTR(s) : (s)->as.heap.ptr)
#define RSTR_LEN(s) ((RSTR_EMBED_P(s)) ? RSTR_EMBED_LEN(s) : (s)->as.heap.len)
#define RSTR_CAPA(s) (RSTR_EMBED_P(s) ? RSTRING_EMBED_LEN_MAX : (s)->as.heap.aux.capa)

#define RSTR_SHARED_P(s) ((s)->flags & MRB_STR_SHARED)
#define RSTR_FSHARED_P(s) ((s)->flags & MRB_STR_FSHARED)
#define RSTR_NOFREE_P(s) ((s)->flags & MRB_STR_NOFREE)

#ifdef MRB_UTF8_STRING
# define RSTR_SINGLE_BYTE_P(s) ((s)->flags & MRB_STR_SINGLE_BYTE)
# define RSTR_SET_SINGLE_BYTE_FLAG(s) ((s)->flags |= MRB_STR_SINGLE_BYTE)
# define RSTR_UNSET_SINGLE_BYTE_FLAG(s) ((s)->flags &= ~MRB_STR_SINGLE_BYTE)
# define RSTR_WRITE_SINGLE_BYTE_FLAG(s, v) (RSTR_UNSET_SINGLE_BYTE_FLAG(s), (s)->flags |= v)
# define RSTR_COPY_SINGLE_BYTE_FLAG(dst, src) RSTR_WRITE_SINGLE_BYTE_FLAG(dst, RSTR_SINGLE_BYTE_P(src))
#else
# define RSTR_SINGLE_BYTE_P(s) TRUE
# define RSTR_SET_SINGLE_BYTE_FLAG(s) (void)0
# define RSTR_UNSET_SINGLE_BYTE_FLAG(s) (void)0
# define RSTR_WRITE_SINGLE_BYTE_FLAG(s, v) (void)0
# define RSTR_COPY_SINGLE_BYTE_FLAG(dst, src) (void)0
#endif
#define RSTR_SET_ASCII_FLAG(s) RSTR_SET_SINGLE_BYTE_FLAG(s)
#define RSTR_BINARY_P(s) ((s)->flags & MRB_STR_BINARY)

/**
 * Returns a pointer from a Ruby string
 */
#define mrb_str_ptr(s)       ((struct RString*)(mrb_ptr(s)))
#define RSTRING(s)           mrb_str_ptr(s)
#define RSTRING_PTR(s)       RSTR_PTR(RSTRING(s))
#define RSTRING_EMBED_LEN(s) RSTR_EMBED_LEN(RSTRING(s))
#define RSTRING_LEN(s)       RSTR_LEN(RSTRING(s))
#define RSTRING_CAPA(s)      RSTR_CAPA(RSTRING(s))
#define RSTRING_END(s)       (RSTRING_PTR(s) + RSTRING_LEN(s))
#define RSTRING_CSTR(mrb,s)  mrb_string_cstr(mrb, s)

MRB_API void mrb_str_modify(mrb_state *mrb, struct RString *s);
/* mrb_str_modify() with keeping ASCII flag if set */
MRB_API void mrb_str_modify_keep_ascii(mrb_state *mrb, struct RString *s);

/**
 * Finds the index of a substring in a string
 */
MRB_API mrb_int mrb_str_index(mrb_state *mrb, mrb_value str, const char *p, mrb_int len, mrb_int offset);
#define mrb_str_index_lit(mrb, str, lit, off) mrb_str_index(mrb, str, lit, mrb_strlen_lit(lit), off);

/**
 * Appends self to other. Returns self as a concatenated string.
 *
 *
 * Example:
 *
 *     int
 *     main(int argc,
 *          char **argv)
 *     {
 *       // Variable declarations.
 *       mrb_value str1;
 *       mrb_value str2;
 *
 *       mrb_state *mrb = mrb_open();
 *       if (!mrb)
 *       {
 *          // handle error
 *       }
 *
 *       // Creates new Ruby strings.
 *       str1 = mrb_str_new_lit(mrb, "abc");
 *       str2 = mrb_str_new_lit(mrb, "def");
 *
 *       // Concatenates str2 to str1.
 *       mrb_str_concat(mrb, str1, str2);
 *
 *       // Prints new Concatenated Ruby string.
 *       mrb_p(mrb, str1);
 *
 *       mrb_close(mrb);
 *       return 0;
 *     }
 *
 * Result:
 *
 *     => "abcdef"
 *
 * @param mrb The current mruby state.
 * @param self String to concatenate.
 * @param other String to append to self.
 * @return [mrb_value] Returns a new String appending other to self.
 */
MRB_API void mrb_str_concat(mrb_state *mrb, mrb_value self, mrb_value other);

/**
 * Adds two strings together.
 *
 *
 * Example:
 *
 *     int
 *     main(int argc,
 *          char **argv)
 *     {
 *       // Variable declarations.
 *       mrb_value a;
 *       mrb_value b;
 *       mrb_value c;
 *
 *       mrb_state *mrb = mrb_open();
 *       if (!mrb)
 *       {
 *          // handle error
 *       }
 *
 *       // Creates two Ruby strings from the passed in C strings.
 *       a = mrb_str_new_lit(mrb, "abc");
 *       b = mrb_str_new_lit(mrb, "def");
 *
 *       // Prints both C strings.
 *       mrb_p(mrb, a);
 *       mrb_p(mrb, b);
 *
 *       // Concatenates both Ruby strings.
 *       c = mrb_str_plus(mrb, a, b);
 *
 *       // Prints new Concatenated Ruby string.
 *       mrb_p(mrb, c);
 *
 *       mrb_close(mrb);
 *       return 0;
 *     }
 *
 *
 * Result:
 *
 *     => "abc"  # First string
 *     => "def"  # Second string
 *     => "abcdef" # First & Second concatenated.
 *
 * @param mrb The current mruby state.
 * @param a First string to concatenate.
 * @param b Second string to concatenate.
 * @return [mrb_value] Returns a new String containing a concatenated to b.
 */
MRB_API mrb_value mrb_str_plus(mrb_state *mrb, mrb_value a, mrb_value b);

/**
 * Converts pointer into a Ruby string.
 *
 * @param mrb The current mruby state.
 * @param p The pointer to convert to Ruby string.
 * @return [mrb_value] Returns a new Ruby String.
 */
MRB_API mrb_value mrb_ptr_to_str(mrb_state *mrb, void *p);

/**
 * Returns an object as a Ruby string.
 *
 * @param mrb The current mruby state.
 * @param obj An object to return as a Ruby string.
 * @return [mrb_value] An object as a Ruby string.
 */
MRB_API mrb_value mrb_obj_as_string(mrb_state *mrb, mrb_value obj);

/**
 * Resizes the string's length. Returns the amount of characters
 * in the specified by len.
 *
 * Example:
 *
 *     int
 *     main(int argc,
 *          char **argv)
 *     {
 *         // Variable declaration.
 *         mrb_value str;
 *
 *         mrb_state *mrb = mrb_open();
 *         if (!mrb)
 *         {
 *            // handle error
 *         }
 *         // Creates a new string.
 *         str = mrb_str_new_lit(mrb, "Hello, world!");
 *         // Returns 5 characters of
 *         mrb_str_resize(mrb, str, 5);
 *         mrb_p(mrb, str);
 *
 *         mrb_close(mrb);
 *         return 0;
 *      }
 *
 * Result:
 *
 *      => "Hello"
 *
 * @param mrb The current mruby state.
 * @param str The Ruby string to resize.
 * @param len The length.
 * @return [mrb_value] An object as a Ruby string.
 */
MRB_API mrb_value mrb_str_resize(mrb_state *mrb, mrb_value str, mrb_int len);

/**
 * Returns a sub string.
 *
 * Example:
 *
 *     int
 *     main(int argc,
 *     char const **argv)
 *     {
 *       // Variable declarations.
 *       mrb_value str1;
 *       mrb_value str2;
 *
 *       mrb_state *mrb = mrb_open();
 *       if (!mrb)
 *       {
 *         // handle error
 *       }
 *       // Creates new string.
 *       str1 = mrb_str_new_lit(mrb, "Hello, world!");
 *       // Returns a sub-string within the range of 0..2
 *       str2 = mrb_str_substr(mrb, str1, 0, 2);
 *
 *       // Prints sub-string.
 *       mrb_p(mrb, str2);
 *
 *       mrb_close(mrb);
 *       return 0;
 *     }
 *
 * Result:
 *
 *     => "He"
 *
 * @param mrb The current mruby state.
 * @param str Ruby string.
 * @param beg The beginning point of the sub-string.
 * @param len The end point of the sub-string.
 * @return [mrb_value] An object as a Ruby sub-string.
 */
MRB_API mrb_value mrb_str_substr(mrb_state *mrb, mrb_value str, mrb_int beg, mrb_int len);

MRB_API mrb_value mrb_str_new_capa(mrb_state *mrb, mrb_int capa);
#define mrb_str_buf_new(mrb, capa) mrb_str_new_capa(mrb, (capa))

/* NULL terminated C string from mrb_value */
MRB_API const char *mrb_string_cstr(mrb_state *mrb, mrb_value str);
/* NULL terminated C string from mrb_value; `str` will be updated */
MRB_API const char *mrb_string_value_cstr(mrb_state *mrb, mrb_value *str);
/* obsolete: use RSTRING_PTR() */
#define mrb_string_value_ptr(mrb, str) RSTRING_PTR(str)
/* obsolete: use RSTRING_LEN() */
#define mrb_string_value_len(mrb, str) RSTRING_LEN(str)
/* obsolete: substituted by a macro; shall be removed */
#define mrb_str_strlen(mrb, s) strlen(RSTR_PTR(s))

/**
 * Duplicates a string object.
 *
 *
 * @param mrb The current mruby state.
 * @param str Ruby string.
 * @return [mrb_value] Duplicated Ruby string.
 */
MRB_API mrb_value mrb_str_dup(mrb_state *mrb, mrb_value str);

/**
 * Returns a symbol from a passed in Ruby string.
 *
 * @param mrb The current mruby state.
 * @param self Ruby string.
 * @return [mrb_value] A symbol.
 */
MRB_API mrb_value mrb_str_intern(mrb_state *mrb, mrb_value self);

MRB_API mrb_value mrb_str_to_integer(mrb_state *mrb, mrb_value str, mrb_int base, mrb_bool badcheck);
/* obsolete: use mrb_str_to_integer() */
#define mrb_str_to_inum(mrb, str, base, badcheck) mrb_str_to_integer(mrb, str, base, badcheck)
MRB_API double mrb_str_to_dbl(mrb_state *mrb, mrb_value str, mrb_bool badcheck);

/**
 * Returns true if the strings match and false if the strings don't match.
 *
 * @param mrb The current mruby state.
 * @param str1 Ruby string to compare.
 * @param str2 Ruby string to compare.
 * @return [mrb_value] boolean value.
 */
MRB_API mrb_bool mrb_str_equal(mrb_state *mrb, mrb_value str1, mrb_value str2);

/**
 * Returns a concatenated string comprised of a Ruby string and a C string.
 *
 * @param mrb The current mruby state.
 * @param str Ruby string.
 * @param ptr A C string.
 * @param len length of C string.
 * @return [mrb_value] A Ruby string.
 * @see mrb_str_cat_cstr
 */
MRB_API mrb_value mrb_str_cat(mrb_state *mrb, mrb_value str, const char *ptr, size_t len);

/**
 * Returns a concatenated string comprised of a Ruby string and a C string.
 *
 * @param mrb The current mruby state.
 * @param str Ruby string.
 * @param ptr A C string.
 * @return [mrb_value] A Ruby string.
 * @see mrb_str_cat
 */
MRB_API mrb_value mrb_str_cat_cstr(mrb_state *mrb, mrb_value str, const char *ptr);
MRB_API mrb_value mrb_str_cat_str(mrb_state *mrb, mrb_value str, mrb_value str2);
#define mrb_str_cat_lit(mrb, str, lit) mrb_str_cat(mrb, str, lit, mrb_strlen_lit(lit))

/**
 * Adds str2 to the end of str1.
 */
MRB_API mrb_value mrb_str_append(mrb_state *mrb, mrb_value str, mrb_value str2);

/**
 * Returns 0 if both Ruby strings are equal. Returns a value < 0 if Ruby str1 is less than Ruby str2. Returns a value > 0 if Ruby str2 is greater than Ruby str1.
 */
MRB_API int mrb_str_cmp(mrb_state *mrb, mrb_value str1, mrb_value str2);

/**
 * Returns a newly allocated C string from a Ruby string.
 * This is an utility function to pass a Ruby string to C library functions.
 *
 * - Returned string does not contain any NUL characters (but terminator).
 * - It raises an ArgumentError exception if Ruby string contains
 *   NUL characters.
 * - Returned string will be freed automatically on next GC.
 * - Caller can modify returned string without affecting Ruby string
 *   (e.g. it can be used for mkstemp(3)).
 *
 * @param mrb The current mruby state.
 * @param str Ruby string. Must be an instance of String.
 * @return [char *] A newly allocated C string.
 */
MRB_API char *mrb_str_to_cstr(mrb_state *mrb, mrb_value str);

/* For backward compatibility */
#define mrb_str_cat2(mrb, str, ptr) mrb_str_cat_cstr(mrb, str, ptr)
#define mrb_str_buf_cat(mrb, str, ptr, len) mrb_str_cat(mrb, str, ptr, len)
#define mrb_str_buf_append(mrb, str, str2) mrb_str_cat_str(mrb, str, str2)

MRB_END_DECL


/* ======== mruby/hash.h ======== */
/**
** @file mruby/hash.h - Hash class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_HASH_H

// #include "common.h" - in amalgam

/**
 * Hash class
 */
MRB_BEGIN_DECL

/* offset of `iv` must match struct RObject */
struct RHash {
  MRB_OBJECT_HEADER;
#ifdef MRB_64BIT
  uint32_t size;
  struct iv_tbl *iv;
  uint32_t ea_capa;
  uint32_t ea_n_used;
#else
  struct iv_tbl *iv;
  uint32_t size;
#endif
  union {
    struct hash_entry *ea;
    struct hash_table *ht;
  } hsh;
};

#define mrb_hash_ptr(v)    ((struct RHash*)(mrb_ptr(v)))
#define mrb_hash_value(p)  mrb_obj_value((void*)(p))

MRB_API mrb_value mrb_hash_new_capa(mrb_state *mrb, mrb_int capa);

/*
 * Initializes a new hash.
 *
 * Equivalent to:
 *
 *      Hash.new
 *
 * @param mrb The mruby state reference.
 * @return The initialized hash.
 */
MRB_API mrb_value mrb_hash_new(mrb_state *mrb);

/*
 * Sets a keys and values to hashes.
 *
 * Equivalent to:
 *
 *      hash[key] = val
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to set.
 * @param val The value to set.
 * @return The value.
 */
MRB_API void mrb_hash_set(mrb_state *mrb, mrb_value hash, mrb_value key, mrb_value val);

/*
 * Gets a value from a key. If the key is not found, the default of the
 * hash is used.
 *
 * Equivalent to:
 *
 *     hash[key]
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to get.
 * @return The found value.
 */
MRB_API mrb_value mrb_hash_get(mrb_state *mrb, mrb_value hash, mrb_value key);

/*
 * Gets a value from a key. If the key is not found, the default parameter is
 * used.
 *
 * Equivalent to:
 *
 *     hash.key?(key) ? hash[key] : def
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to get.
 * @param def The default value.
 * @return The found value.
 */
MRB_API mrb_value mrb_hash_fetch(mrb_state *mrb, mrb_value hash, mrb_value key, mrb_value def);

/*
 * Deletes hash key and value pair.
 *
 * Equivalent to:
 *
 *     hash.delete(key)
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to delete.
 * @return The deleted value. This value is not protected from GC. Use `mrb_gc_protect()` if necessary.
 */
MRB_API mrb_value mrb_hash_delete_key(mrb_state *mrb, mrb_value hash, mrb_value key);

/*
 * Gets an array of keys.
 *
 * Equivalent to:
 *
 *     hash.keys
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return An array with the keys of the hash.
 */
MRB_API mrb_value mrb_hash_keys(mrb_state *mrb, mrb_value hash);
/*
 * Check if the hash has the key.
 *
 * Equivalent to:
 *
 *     hash.key?(key)
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to check existence.
 * @return True if the hash has the key
 */
MRB_API mrb_bool mrb_hash_key_p(mrb_state *mrb, mrb_value hash, mrb_value key);

/*
 * Check if the hash is empty
 *
 * Equivalent to:
 *
 *     hash.empty?
 *
 * @param mrb The mruby state reference.
 * @param self The target hash.
 * @return True if the hash is empty, false otherwise.
 */
MRB_API mrb_bool mrb_hash_empty_p(mrb_state *mrb, mrb_value self);

/*
 * Gets an array of values.
 *
 * Equivalent to:
 *
 *     hash.values
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return An array with the values of the hash.
 */
MRB_API mrb_value mrb_hash_values(mrb_state *mrb, mrb_value hash);

/*
 * Clears the hash.
 *
 * Equivalent to:
 *
 *     hash.clear
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return The hash
 */
MRB_API mrb_value mrb_hash_clear(mrb_state *mrb, mrb_value hash);

/*
 * Get hash size.
 *
 * Equivalent to:
 *
 *      hash.size
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return The hash size.
 */
MRB_API mrb_int mrb_hash_size(mrb_state *mrb, mrb_value hash);

/*
 * Copies the hash. This function does NOT copy the instance variables
 * (except for the default value). Use mrb_obj_dup() to copy the instance
 * variables as well.
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return The copy of the hash
 */
MRB_API mrb_value mrb_hash_dup(mrb_state *mrb, mrb_value hash);

/*
 * Merges two hashes. The first hash will be modified by the
 * second hash.
 *
 * @param mrb The mruby state reference.
 * @param hash1 The target hash.
 * @param hash2 Updating hash
 */
MRB_API void mrb_hash_merge(mrb_state *mrb, mrb_value hash1, mrb_value hash2);

#define RHASH(hash) ((struct RHash*)(mrb_ptr(hash)))

#define MRB_HASH_IB_BIT_BIT         5
#define MRB_HASH_AR_EA_CAPA_BIT     5
#define MRB_HASH_IB_BIT_SHIFT       0
#define MRB_HASH_AR_EA_CAPA_SHIFT   0
#define MRB_HASH_AR_EA_N_USED_SHIFT MRB_HASH_AR_EA_CAPA_BIT
#define MRB_HASH_SIZE_FLAGS_SHIFT   (MRB_HASH_AR_EA_CAPA_BIT * 2)
#define MRB_HASH_IB_BIT_MASK        ((1 << MRB_HASH_IB_BIT_BIT) - 1)
#define MRB_HASH_AR_EA_CAPA_MASK    ((1 << MRB_HASH_AR_EA_CAPA_BIT) - 1)
#define MRB_HASH_AR_EA_N_USED_MASK  (MRB_HASH_AR_EA_CAPA_MASK << MRB_HASH_AR_EA_N_USED_SHIFT)
#define MRB_HASH_DEFAULT            (1 << (MRB_HASH_SIZE_FLAGS_SHIFT + 0))
#define MRB_HASH_PROC_DEFAULT       (1 << (MRB_HASH_SIZE_FLAGS_SHIFT + 1))
#define MRB_HASH_HT                 (1 << (MRB_HASH_SIZE_FLAGS_SHIFT + 2))
#define MRB_RHASH_DEFAULT_P(hash) (RHASH(hash)->flags & MRB_HASH_DEFAULT)
#define MRB_RHASH_PROCDEFAULT_P(hash) (RHASH(hash)->flags & MRB_HASH_PROC_DEFAULT)

/* return non zero to break the loop */
typedef int (mrb_hash_foreach_func)(mrb_state *mrb, mrb_value key, mrb_value val, void *data);
MRB_API void mrb_hash_foreach(mrb_state *mrb, struct RHash *hash, mrb_hash_foreach_func *func, void *p);

MRB_END_DECL


/* ======== mruby/class.h ======== */
/**
** @file mruby/class.h - Class class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_CLASS_H

// #include "common.h" - in amalgam

/**
 * Class class
 */
MRB_BEGIN_DECL

struct RClass {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
  struct mrb_mt_tbl *mt;
  struct RClass *super;
};

#define mrb_class_ptr(v)    ((struct RClass*)(mrb_ptr(v)))

MRB_INLINE struct RClass*
mrb_class(mrb_state *mrb, mrb_value v)
{
  if (!mrb_immediate_p(v)) {
    return mrb_obj_ptr(v)->c;
  }

  switch (mrb_type(v)) {
  case MRB_TT_FALSE:
    if (mrb_fixnum(v))
      return mrb->false_class;
    return mrb->nil_class;
  case MRB_TT_TRUE:
    return mrb->true_class;
  case MRB_TT_SYMBOL:
    return mrb->symbol_class;
  case MRB_TT_INTEGER:
    return mrb->integer_class;
#ifndef MRB_NO_FLOAT
  case MRB_TT_FLOAT:
    return mrb->float_class;
#endif
  case MRB_TT_CPTR:
    return mrb->object_class;
  default:
    return NULL;
  }
}

/* flags:
   20:   frozen
   19:   is_prepended
   18:   is_origin
   17:   is_inherited (used by method cache)
   7-16: unused
   6:    prohibit Class#allocate
   0-5:  instance type
*/
#define MRB_FL_CLASS_IS_PREPENDED (1 << 19)
#define MRB_FL_CLASS_IS_ORIGIN (1 << 18)
#define MRB_CLASS_ORIGIN(c) do {\
  if ((c)->flags & MRB_FL_CLASS_IS_PREPENDED) {\
    (c) = (c)->super;\
    while (!((c)->flags & MRB_FL_CLASS_IS_ORIGIN)) {\
      (c) = (c)->super;\
    }\
  }\
} while (0)
#define MRB_FL_CLASS_IS_INHERITED (1 << 17)
#define MRB_INSTANCE_TT_MASK (0x1F)
#define MRB_SET_INSTANCE_TT(c, tt) ((c)->flags = (((c)->flags & ~MRB_INSTANCE_TT_MASK) | (char)(tt)))
#define MRB_INSTANCE_TT(c) (enum mrb_vtype)((c)->flags & MRB_INSTANCE_TT_MASK)
#define MRB_FL_UNDEF_ALLOCATE (1 << 6)
#define MRB_UNDEF_ALLOCATOR(c) (mrb_assert((c)->tt == MRB_TT_CLASS), (c)->flags |= MRB_FL_UNDEF_ALLOCATE)
#define MRB_UNDEF_ALLOCATOR_P(c) ((c)->flags & MRB_FL_UNDEF_ALLOCATE)
#define MRB_DEFINE_ALLOCATOR(c) ((c)->flags &= ~MRB_FL_UNDEF_ALLOCATE)

MRB_API void mrb_define_method_raw(mrb_state*, struct RClass*, mrb_sym, mrb_method_t);
MRB_API void mrb_alias_method(mrb_state*, struct RClass *c, mrb_sym a, mrb_sym b);
MRB_API void mrb_remove_method(mrb_state *mrb, struct RClass *c, mrb_sym sym);

MRB_API mrb_method_t mrb_method_search_vm(mrb_state*, struct RClass**, mrb_sym);
MRB_API mrb_method_t mrb_method_search(mrb_state*, struct RClass*, mrb_sym);

MRB_API struct RClass* mrb_class_real(struct RClass* cl);
MRB_API struct RClass* mrb_class_outer(mrb_state *mrb, struct RClass *c);

#ifndef MRB_NO_METHOD_CACHE
void mrb_mc_clear_by_class(mrb_state *mrb, struct RClass* c);
#else
#define mrb_mc_clear_by_class(mrb,c)
#endif

/* return non zero to break the loop */
typedef int (mrb_mt_foreach_func)(mrb_state*,mrb_sym,mrb_method_t,void*);
MRB_API void mrb_mt_foreach(mrb_state*, struct RClass*, mrb_mt_foreach_func*, void*);

/* ROM method table types for static method registration */
union mrb_mt_ptr {
  const struct RProc *proc;
  mrb_func_t func;
};

/* entry combining function pointer, symbol key, and flags */
typedef struct mrb_mt_entry {
  union mrb_mt_ptr val;
  mrb_sym key;              /* pure symbol ID (no flags packed) */
  uint32_t flags;           /* method flags + aspec */
} mrb_mt_entry;

typedef struct mrb_mt_tbl {
  int               size;
  int               alloc;  /* bit 30: MRB_MT_READONLY_BIT, bit 29: MRB_MT_FROZEN_BIT */
  mrb_mt_entry     *ptr;
  struct mrb_mt_tbl *next;
} mrb_mt_tbl;

#define MRB_MT_READONLY_BIT  (1 << 30)
#define MRB_MT_FROZEN_BIT    (1 << 29)
#define MRB_MT_FUNC    (1 << 24)  /* MRB_METHOD_FUNC_FL */
#define MRB_MT_PUBLIC  0
#define MRB_MT_PRIVATE (1 << 25)  /* MRB_METHOD_PRIVATE_FL */

/* ROM table entry: 3rd param is MRB_ARGS_*() optionally OR'd with MRB_MT_PRIVATE. */
#define MRB_MT_ENTRY(fn, sym, flags) \
  { { .func = (fn) }, (sym), (flags) | MRB_MT_FUNC }
#define MRB_MT_ASPEC(flags) ((mrb_aspec)((flags) & 0xffffff))

/* "removed" tombstone: MRB_MT_FUNC flag set with NULL function pointer.
   This combination never occurs naturally (C functions are never NULL).
   Unlike undef (proc=NULL without MRB_MT_FUNC), a removed marker makes
   mt_get() return 0 ("not found"), blocking ROM chain walk while
   allowing superclass lookup. */
#define MRB_MT_REMOVED_P(e) (((e).flags&MRB_MT_FUNC) && (e).val.func==NULL)

/* Singly-linked list node for tracking heap-allocated ROM wrappers. */
struct mrb_mt_rom_list {
  mrb_mt_tbl *tbl;
  struct mrb_mt_rom_list *next;
};

/* Allocate a per-state ROM layer wrapping the const entries array,
   and push it onto the class's method table chain. */
void mrb_mt_init_rom(mrb_state *mrb, struct RClass *c,
                     const mrb_mt_entry *entries, int size);
#define MRB_MT_INIT_ROM(mrb, cls, entries) \
  mrb_mt_init_rom(mrb, cls, entries, \
                  (int)(sizeof(entries)/sizeof(entries[0])))

MRB_END_DECL


/* ======== mruby/proc.h ======== */
/**
** @file mruby/proc.h - Proc class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_PROC_H

// #include "common.h" - in amalgam

/* ======== mruby/irep.h (inlined) ======== */
/**
** @file mruby/irep.h - mrb_irep structure
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_IREP_H

// #include "common.h" - in amalgam

/* ======== mruby/compile.h (inlined) ======== */
/**
** @file mruby/compile.h - mruby parser
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_COMPILE_H

// #include "common.h" - in amalgam

/* ======== mruby/mempool.h (inlined) ======== */
/*
** mempool.h - memory pool
**
** See Copyright Notice in mruby.h
*/

/* memory pool implementation */
typedef struct mempool mempool;
MRB_API struct mempool* mempool_open(void);
MRB_API void mempool_close(struct mempool*);
MRB_API void* mempool_alloc(struct mempool*, size_t);
MRB_API void* mempool_realloc(struct mempool*, void*, size_t oldlen, size_t newlen);

/* compatibility layer */
typedef struct mempool mrb_mempool;
#define mrb_mempool_open(m) mempool_open()
#define mrb_mempool_close(m) mempool_close(m)
#define mrb_mempool_alloc(m, size) mempool_alloc((m),(size))
#define mrb_mempool_realloc(m, ptr, oldlen, newlen) mempool_realloc((m),(ptr),(oldlen),(newlen))


/**
 * mruby Compiler
 */
MRB_BEGIN_DECL

// #include <mruby.h> - in amalgam

struct mrb_parser_state;
/* load context */
typedef struct mrb_ccontext {
  mrb_sym *syms;
  int slen;
  char *filename;
  uint16_t lineno;
  int (*partial_hook)(struct mrb_parser_state*);
  void *partial_data;
  struct RClass *target_class;
  mrb_bool capture_errors:1;
  mrb_bool dump_result:1;
  mrb_bool no_exec:1;
  mrb_bool keep_lv:1;
  mrb_bool no_optimize:1;
  mrb_bool no_ext_ops:1;
  mrb_bool no_return_value:1;
  const struct RProc *upper;

  size_t parser_nerr;
} mrb_ccontext;                 /* compiler context */

MRB_API mrb_ccontext* mrb_ccontext_new(mrb_state *mrb);
MRB_API void mrb_ccontext_free(mrb_state *mrb, mrb_ccontext *cxt);
MRB_API const char *mrb_ccontext_filename(mrb_state *mrb, mrb_ccontext *c, const char *s);
MRB_API void mrb_ccontext_partial_hook(mrb_ccontext *c, int (*partial_hook)(struct mrb_parser_state*), void*data);
MRB_API void mrb_ccontext_cleanup_local_variables(mrb_ccontext *c);

/* compatibility macros */
#define mrbc_context mrb_ccontext
#define mrbc_context_new mrb_ccontext_new
#define mrbc_context_free mrb_ccontext_free
#define mrbc_filename mrb_ccontext_filename
#define mrbc_partial_hook mrb_ccontext_partial_hook
#define mrbc_cleanup_local_variables mrb_ccontext_cleanup_local_variables

/* AST node structure */
typedef struct mrb_ast_node mrb_ast_node;

/* lexer states */
enum mrb_lex_state_enum {
  EXPR_BEG,                   /* ignore newline, +/- is a sign. */
  EXPR_END,                   /* newline significant, +/- is an operator. */
  EXPR_ENDARG,                /* ditto, and unbound braces. */
  EXPR_ENDFN,                 /* ditto, and unbound braces. */
  EXPR_ARG,                   /* newline significant, +/- is an operator. */
  EXPR_CMDARG,                /* newline significant, +/- is an operator. */
  EXPR_MID,                   /* newline significant, +/- is a sign. */
  EXPR_FNAME,                 /* ignore newline, no reserved words. */
  EXPR_DOT,                   /* right after '.' or '::', no reserved words. */
  EXPR_CLASS,                 /* immediate after 'class', no here document. */
  EXPR_VALUE,                 /* alike EXPR_BEG but label is disallowed. */
  EXPR_MAX_STATE
};

/* saved error message */
struct mrb_parser_message {
  uint16_t lineno;
  int column;
  char* message;
};

#define MRB_PARSER_TOKBUF_MAX (UINT16_MAX-1)
#define MRB_PARSER_TOKBUF_SIZE 256

/* parser structure */
struct mrb_parser_state {
  mrb_state *mrb;
  mempool *pool;
  mrb_ast_node *cells;
  const char *s, *send;
#ifndef MRB_NO_STDIO
  /* If both f and s are non-null, it will be taken preferentially from s until s < send. */
  FILE *f;
#endif
  mrb_ccontext *cxt;
  mrb_sym filename_sym;
  uint16_t lineno;
  int column;

  enum mrb_lex_state_enum lstate;
  struct parser_lex_strterm *lex_strterm;

  unsigned int cond_stack;
  unsigned int cmdarg_stack;
  int paren_nest;
  int lpar_beg;
  int in_def, in_single, in_kwarg;
  mrb_bool cmd_start:1;
  mrb_ast_node *locals;

  mrb_ast_node *pb;
  char *tokbuf;
  char buf[MRB_PARSER_TOKBUF_SIZE];
  int tidx;
  int tsiz;

  mrb_ast_node *heredocs_from_nextline;
  mrb_ast_node *parsing_heredoc;

  void *ylval;

  size_t nerr;
  size_t nwarn;
  mrb_ast_node *tree;

  mrb_bool no_optimize:1;
  mrb_bool capture_errors:1;
  mrb_bool no_ext_ops:1;
  mrb_bool no_return_value:1;
  const struct RProc *upper;
  struct mrb_parser_message error_buffer[10];
  struct mrb_parser_message warn_buffer[10];

  mrb_sym* filename_table;
  uint16_t filename_table_length;
  uint16_t current_filename_index;

  /* Variable-sized node management */
  mrb_ast_node *nvars;
};

MRB_API struct mrb_parser_state* mrb_parser_new(mrb_state*);
MRB_API void mrb_parser_free(struct mrb_parser_state*);
MRB_API void mrb_parser_parse(struct mrb_parser_state*,mrb_ccontext*);

MRB_API void mrb_parser_set_filename(struct mrb_parser_state*, char const*);
MRB_API mrb_sym mrb_parser_get_filename(struct mrb_parser_state*, uint16_t idx);

/* utility functions */
#ifndef MRB_NO_STDIO
MRB_API struct mrb_parser_state* mrb_parse_file(mrb_state*,FILE*,mrb_ccontext*);
#endif
MRB_API struct mrb_parser_state* mrb_parse_string(mrb_state*,const char*,mrb_ccontext*);
MRB_API struct mrb_parser_state* mrb_parse_nstring(mrb_state*,const char*,size_t,mrb_ccontext*);
MRB_API struct RProc* mrb_generate_code(mrb_state*, struct mrb_parser_state*);
MRB_API mrb_value mrb_load_exec(mrb_state *mrb, struct mrb_parser_state *p, mrb_ccontext *c);

/**
 * program load functions
 *
 * Please note! Currently due to interactions with the GC calling these functions will
 * leak one RProc object per function call.
 * To prevent this save the current memory arena before calling and restore the arena
 * right after, like so
 *
 *      int ai = mrb_gc_arena_save(mrb);
 *      mrb_value status = mrb_load_string(mrb, buffer);
 *      mrb_gc_arena_restore(mrb, ai);
 *
 * Also, when called from a C function defined as a method, the current stack is destroyed.
 * If processing continues after this function, the objects obtained from the arguments
 * must be protected as needed before this function.
 */
#ifndef MRB_NO_STDIO
MRB_API mrb_value mrb_load_file(mrb_state*,FILE*);
MRB_API mrb_value mrb_load_file_cxt(mrb_state*,FILE*, mrb_ccontext *cxt);
MRB_API mrb_value mrb_load_detect_file_cxt(mrb_state *mrb, FILE *fp, mrb_ccontext *c);
#endif
MRB_API mrb_value mrb_load_string(mrb_state *mrb, const char *s);
MRB_API mrb_value mrb_load_nstring(mrb_state *mrb, const char *s, size_t len);
MRB_API mrb_value mrb_load_string_cxt(mrb_state *mrb, const char *s, mrb_ccontext *cxt);
MRB_API mrb_value mrb_load_nstring_cxt(mrb_state *mrb, const char *s, size_t len, mrb_ccontext *cxt);

/** @} */
MRB_END_DECL



/**
 * Compiled mruby scripts.
 */
MRB_BEGIN_DECL

enum irep_pool_type {
  IREP_TT_STR = 0,    /* string (need free) */
  IREP_TT_SSTR = 2,   /* string (static) */
  IREP_TT_INT32 = 1,  /* 32-bit integer */
  IREP_TT_INT64 = 3,  /* 64-bit integer */
  IREP_TT_BIGINT = 7, /* big integer (not yet supported) */
  IREP_TT_FLOAT = 5,  /* float (double/float) */
};

#define IREP_TT_NFLAG 1 /* number (non string) flag */
#define IREP_TT_SFLAG 2 /* static string flag */

typedef struct mrb_irep_pool {
  uint32_t tt; /* packed type and length (for string) */
  union {
    const char *str;
    int32_t i32;
    int64_t i64;
#ifndef MRB_NO_FLOAT
    mrb_float f;
#endif
  } u;
} mrb_irep_pool;

enum mrb_catch_type {
  MRB_CATCH_RESCUE = 0,
  MRB_CATCH_ENSURE = 1,
};

struct mrb_irep_catch_handler {
  uint8_t type; /* enum mrb_catch_type */
  uint8_t begin[4];  /* The starting address to match the handler. Includes this. */
  uint8_t end[4];    /* The endpoint address that matches the handler. Not Includes this. */
  uint8_t target[4]; /* The address to jump to if a match is made. */
};

/* Program data array struct */
struct mrb_irep {
  uint16_t nlocals; /* Number of local variables */
  uint16_t nregs;   /* Number of register variables */
  uint16_t clen;    /* Number of catch handlers */
  uint8_t flags;

  const mrb_code *iseq;
  /*
   * A catch handler table is placed after the iseq entity.
   * The reason it doesn't add fields to the structure is to keep the mrb_irep
   * structure from bloating. The catch handler table can be obtained with
   * `mrb_irep_catch_handler_table(irep)`.
   */
  const mrb_irep_pool *pool;
  const mrb_sym *syms;
  const struct mrb_irep *const *reps;

  const mrb_sym *lv;
  /* debug info */
  struct mrb_irep_debug_info *debug_info;

  uint32_t ilen;
  uint16_t plen, slen;
  uint16_t rlen;
  uint16_t refcnt;
};

#define MRB_ISEQ_NO_FREE 1
#define MRB_IREP_NO_FREE 2
#define MRB_IREP_STATIC (MRB_ISEQ_NO_FREE | MRB_IREP_NO_FREE)
#define MRB_IREP_CONSOLIDATED 4  /* pool/syms/reps packed with irep struct */

MRB_API mrb_irep *mrb_add_irep(mrb_state *mrb);

/**
 * load mruby bytecode functions
 *
 * Please note! Currently due to interactions with the GC calling these
 * functions will leak one RProc object per function call. To prevent this save
 * the current memory arena before calling and restore the arena right after,
 * like so
 *
 *      int ai = mrb_gc_arena_save(mrb);
 *      mrb_value status = mrb_load_irep(mrb, buffer);
 *      mrb_gc_arena_restore(mrb, ai);
 *
 * Also, when called from a C function defined as a method, the current stack is
 * destroyed. If processing continues after this function, the objects obtained
 * from the arguments must be protected as needed before this function.
 */

/* @param [const uint8_t*] irep code, expected as a literal */
MRB_API mrb_value mrb_load_irep(mrb_state *, const uint8_t *);

/*
 * @param [const void*] irep code
 * @param [size_t] size of irep buffer.
 */
MRB_API mrb_value mrb_load_irep_buf(mrb_state *, const void *, size_t);

/* @param [const uint8_t*] irep code, expected as a literal */
MRB_API mrb_value mrb_load_irep_cxt(mrb_state *, const uint8_t *,
                                    mrbc_context *);

/*
 * @param [const void*] irep code
 * @param [size_t] size of irep buffer.
 */
MRB_API mrb_value mrb_load_irep_buf_cxt(mrb_state *, const void *, size_t,
                                        mrbc_context *);

struct mrb_insn_data {
  uint8_t insn;
  uint32_t a;
  uint16_t b;
  uint16_t c;
  const mrb_code *addr;
};

#define mrb_irep_catch_handler_pack(n, v) uint32_to_bin(n, v)
#define mrb_irep_catch_handler_unpack(v) bin_to_uint32(v)

void mrb_irep_incref(mrb_state *, struct mrb_irep *);
void mrb_irep_decref(mrb_state *, struct mrb_irep *);
void mrb_irep_cutref(mrb_state *, struct mrb_irep *);

MRB_END_DECL


#include <string.h>

/**
 * Proc class
 */
MRB_BEGIN_DECL

/*
 * env object (for internal used)
 *
 * - don't create multiple envs on one ci.
 * - don't share a env to different ci.
 * - don't attach a closed env to any ci.
 */
struct REnv {
  MRB_OBJECT_HEADER;
  mrb_value *stack;
  struct mrb_context *cxt; /* if not null, it means that the stack is shared with the call frame */
  mrb_sym mid;
};

/* flags (20bits): 1(ZERO):1(separate module):2(visibility):8(cioff/bidx):8(stack_len) */
#define MRB_ENV_SET_LEN(e,len) ((e)->flags = (((e)->flags & ~0xff)|((unsigned int)(len) & 0xff)))
#define MRB_ENV_LEN(e) ((mrb_int)((e)->flags & 0xff))
#define MRB_ENV_CLOSE(e) ((e)->cxt = NULL)
#define MRB_ENV_ONSTACK_P(e) ((e)->cxt != NULL)
#define MRB_ENV_BIDX(e) (((e)->flags >> 8) & 0xff)
#define MRB_ENV_SET_BIDX(e,idx) ((e)->flags = (((e)->flags & ~(0xff<<8))|((unsigned int)(idx) & 0xff)<<8))
#define MRB_ENV_SET_VISIBILITY(e, vis) MRB_FLAGS_SET((e)->flags, 16, 2, vis)
#define MRB_ENV_VISIBILITY(e) MRB_FLAGS_GET((e)->flags, 16, 2)
#define MRB_ENV_VISIBILITY_BREAK_P(e) MRB_FLAG_CHECK((e)->flags, 18)
#define MRB_ENV_COPY_FLAGS_FROM_CI(e, ci) MRB_FLAGS_SET((e)->flags, 16, 3, (ci)->vis)

/*
 * Returns TRUE on success.
 * If the function fails:
 * * Returns FALSE if noraise is TRUE.
 * * Raises a NoMemoryError exception if noraise is FALSE.
 */
mrb_bool mrb_env_unshare(mrb_state*, struct REnv*, mrb_bool noraise);

struct RProc {
  MRB_OBJECT_HEADER;
  union {
    const mrb_irep *irep;
    mrb_func_t func;
    mrb_sym mid;
  } body;
  const struct RProc *upper;
  union {
    struct RClass *target_class;
    struct REnv *env;
  } e;
};

/* aspec access */
#define MRB_ASPEC_REQ(a)          (((a) >> 18) & 0x1f)
#define MRB_ASPEC_OPT(a)          (((a) >> 13) & 0x1f)
#define MRB_ASPEC_REST(a)         (((a) >> 12) & 0x1)
#define MRB_ASPEC_POST(a)         (((a) >> 7) & 0x1f)
#define MRB_ASPEC_KEY(a)          (((a) >> 2) & 0x1f)
#define MRB_ASPEC_KDICT(a)        (((a) >> 1) & 0x1)
#define MRB_ASPEC_BLOCK(a)        ((a) & 1)
#define MRB_ASPEC_NOBLOCK(a)      (((a) >> 23) & 0x1)

#define MRB_PROC_CFUNC_FL 128
#define MRB_PROC_CFUNC_P(p) (((p)->flags & MRB_PROC_CFUNC_FL) != 0)
#define MRB_PROC_CFUNC(p) (p)->body.func
#define MRB_PROC_STRICT 256
#define MRB_PROC_STRICT_P(p) (((p)->flags & MRB_PROC_STRICT) != 0)
#define MRB_PROC_ORPHAN 512
#define MRB_PROC_ORPHAN_P(p) (((p)->flags & MRB_PROC_ORPHAN) != 0)
#define MRB_PROC_ENVSET 1024
#define MRB_PROC_ENV_P(p) (((p)->flags & MRB_PROC_ENVSET) != 0)
#define MRB_PROC_ENV(p) (MRB_PROC_ENV_P(p) ? (p)->e.env : NULL)
#define MRB_PROC_TARGET_CLASS(p) (MRB_PROC_ENV_P(p) ? (p)->e.env->c : (p)->e.target_class)
#define MRB_PROC_SET_TARGET_CLASS(p,tc) do {\
  if (MRB_PROC_ENV_P(p)) {\
    (p)->e.env->c = (tc);\
    mrb_field_write_barrier(mrb, (struct RBasic*)(p)->e.env, (struct RBasic*)(tc));\
  }\
  else {\
    (p)->e.target_class = (tc);\
    mrb_field_write_barrier(mrb, (struct RBasic*)p, (struct RBasic*)(tc));\
  }\
} while (0)
#define MRB_PROC_SCOPE 2048
#define MRB_PROC_SCOPE_P(p) (((p)->flags & MRB_PROC_SCOPE) != 0)
#define MRB_PROC_NOARG 4096 /* for MRB_PROC_CFUNC_FL, aspec == MRB_ARGS_NONE() */
#define MRB_PROC_NOARG_P(p) (((p)->flags & MRB_PROC_NOARG) != 0)
#define MRB_PROC_ALIAS 8192
#define MRB_PROC_ALIAS_P(p) (((p)->flags & MRB_PROC_ALIAS) != 0)

#define mrb_proc_ptr(v)    ((struct RProc*)(mrb_ptr(v)))

struct RProc *mrb_proc_new(mrb_state*, const mrb_irep*);
MRB_API struct RProc *mrb_proc_new_cfunc(mrb_state*, mrb_func_t);
MRB_API struct RProc *mrb_closure_new_cfunc(mrb_state *mrb, mrb_func_t func, int nlocals);

/* following functions are defined in mruby-proc-ext so please include it when using */
MRB_API struct RProc *mrb_proc_new_cfunc_with_env(mrb_state *mrb, mrb_func_t func, mrb_int argc, const mrb_value *argv);
MRB_API mrb_value mrb_proc_cfunc_env_get(mrb_state *mrb, mrb_int idx);
/* old name */
#define mrb_cfunc_env_get(mrb, idx) mrb_proc_cfunc_env_get(mrb, idx)

#define MRB_METHOD_FUNC_FL    (1 << 24)
#define MRB_METHOD_PUBLIC_FL  0
#define MRB_METHOD_PRIVATE_FL (1 << 25)
#define MRB_METHOD_PROTECTED_FL (1 << 26)
#define MRB_METHOD_VDEFAULT_FL ((1 << 25) | (1 << 26))
#define MRB_METHOD_VISIBILITY_MASK ((1 << 25) | (1 << 26))

#define MRB_METHOD_FUNC_P(m) ((m).flags&MRB_METHOD_FUNC_FL)
#define MRB_METHOD_FUNC(m) ((m).as.func)
#define MRB_METHOD_FROM_FUNC(m,fn) do{(m).flags=MRB_METHOD_FUNC_FL;(m).as.func=(fn);}while(0)
#define MRB_METHOD_FROM_PROC(m,pr) do{(m).flags=0;(m).as.proc=(pr);}while(0)
#define MRB_METHOD_PROC_P(m) (!MRB_METHOD_FUNC_P(m))
#define MRB_METHOD_PROC(m) ((m).as.proc)
#define MRB_METHOD_UNDEF_P(m) ((m).as.proc==NULL)
#define MRB_METHOD_VISIBILITY(m) ((m).flags & MRB_METHOD_VISIBILITY_MASK)
#define MRB_SET_VISIBILITY_FLAGS(f,v) ((f)=(((f)&~MRB_METHOD_VISIBILITY_MASK)|(v)))
#define MRB_METHOD_SET_VISIBILITY(m,v) MRB_SET_VISIBILITY_FLAGS((m).flags,(v))

#define MRB_METHOD_CFUNC_P(m) (MRB_METHOD_FUNC_P(m) || (MRB_METHOD_PROC(m)?(MRB_PROC_CFUNC_P(MRB_METHOD_PROC(m))):FALSE))
/* use MRB_METHOD_CFUNC(m) only when MRB_METHOD_CFUNC_P(m) is true */
#define MRB_METHOD_CFUNC(m) (MRB_METHOD_FUNC_P(m)?MRB_METHOD_FUNC(m):MRB_PROC_CFUNC(MRB_METHOD_PROC(m)))

MRB_API mrb_value mrb_load_proc(mrb_state *mrb, const struct RProc *proc);

/**
 *  It can be used to isolate top-level scopes referenced by blocks generated by
 *  `mrb_load_string_cxt()` or similar called before entering the mruby VM (e.g. from `main()`).
 *  In that case, the `ci` parameter should be `mrb->c->cibase`.
 *
 *      #include <mruby.h>
 *      #include <mruby/compile.h>
 *      #include <mruby/proc.h>
 *
 *      int
 *      main(int argc, char **argv)
 *      {
 *        mrb_state *mrb;
 *        mrb_ccontext *cxt;
 *        mrb_value blk, ret;
 *
 *        mrb = mrb_open();
 *        cxt = mrb_ccontext_new(mrb);
 *        blk = mrb_load_string_cxt(mrb, "x, y, z = 1, 2, 3; proc { [x, y, z] }", cxt);
 *        mrb_vm_ci_env_clear(mrb, mrb->c->cibase);
 *        mrb_load_string_cxt(mrb, "x, y, z = 4, 5, 6", cxt);
 *        ret = mrb_funcall(mrb, blk, "call", 0);
 *        mrb_p(mrb, ret);  // => [1, 2, 3]
 *                          // => [4, 5, 6] if `mrb_vm_ci_env_clear()` is commented out
 *        mrb_ccontext_free(mrb, cxt);
 *        mrb_close(mrb);
 *
 *        return 0;
 *      }
 *
 *  The top-level local variable names stored in `mrb_ccontext` are retained.
 *  Use also `mrb_ccontext_cleanup_local_variables()` at the same time, if necessary.
 */
MRB_API void mrb_vm_ci_env_clear(mrb_state *mrb, mrb_callinfo *ci);

void mrb_vm_ci_proc_set(mrb_callinfo *ci, const struct RProc *p);
struct RClass * mrb_vm_ci_target_class(const mrb_callinfo *ci);
void mrb_vm_ci_target_class_set(mrb_callinfo *ci, struct RClass *tc);
struct REnv * mrb_vm_ci_env(const mrb_callinfo *ci);
void mrb_vm_ci_env_set(mrb_callinfo *ci, struct REnv *e);

MRB_END_DECL


/* ======== mruby/range.h ======== */
/**
** @file mruby/range.h - Range class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_RANGE_H

// #include "common.h" - in amalgam

/**
 * Range class
 */
MRB_BEGIN_DECL

#if defined(MRB_NAN_BOXING) && defined(MRB_64BIT) || defined(MRB_WORD_BOXING)
# define MRB_RANGE_EMBED
#endif

#ifdef MRB_RANGE_EMBED
struct RRange {
  MRB_OBJECT_HEADER;
  mrb_value beg;
  mrb_value end;
  mrb_bool excl;
};
# define mrb_gc_free_range(mrb, p) ((void)0)
# define RANGE_BEG(p) ((p)->beg)
# define RANGE_END(p) ((p)->end)
#else
typedef struct mrb_range_edges {
  mrb_value beg;
  mrb_value end;
} mrb_range_edges;
struct RRange {
  MRB_OBJECT_HEADER;
  mrb_range_edges *edges;
  mrb_bool excl;
};
# define mrb_gc_free_range(mrb, p) mrb_free(mrb, (p)->edges)
# define RANGE_BEG(p) ((p)->edges->beg)
# define RANGE_END(p) ((p)->edges->end)
#endif

#define mrb_range_beg(mrb, r) RANGE_BEG(mrb_range_ptr(mrb, r))
#define mrb_range_end(mrb, r) RANGE_END(mrb_range_ptr(mrb, r))
#define mrb_range_excl_p(mrb, r) RANGE_EXCL(mrb_range_ptr(mrb, r))
#define mrb_range_raw_ptr(r) ((struct RRange*)mrb_ptr(r))
#define mrb_range_value(p) mrb_obj_value((void*)(p))
#define RANGE_EXCL(p) ((p)->excl)

MRB_API struct RRange* mrb_range_ptr(mrb_state *mrb, mrb_value range);

/*
 * Initializes a Range.
 *
 * If the third parameter is FALSE then it includes the last value in the range.
 * If the third parameter is TRUE then it excludes the last value in the range.
 *
 * @param start the beginning value.
 * @param end the ending value.
 * @param exclude represents the inclusion or exclusion of the last value.
 */
MRB_API mrb_value mrb_range_new(mrb_state *mrb, mrb_value start, mrb_value end, mrb_bool exclude);

enum mrb_range_beg_len {
  MRB_RANGE_TYPE_MISMATCH = 0,  /* (failure) not range */
  MRB_RANGE_OK = 1,             /* (success) range */
  MRB_RANGE_OUT = 2             /* (failure) out of range */
};

MRB_API enum mrb_range_beg_len mrb_range_beg_len(mrb_state *mrb, mrb_value range, mrb_int *begp, mrb_int *lenp, mrb_int len, mrb_bool trunc);

MRB_END_DECL


/* ======== mruby/variable.h ======== */
/**
** @file mruby/variable.h - mruby variables
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_VARIABLE_H

#if defined(__GNUC__) || defined(__clang__)
#define MRB_MEM_PREFETCH(addr) __builtin_prefetch(addr, 0, 1)
#else
#define MRB_MEM_PREFETCH(addr)
#endif

// #include "common.h" - in amalgam

/**
 * Functions to access mruby variables.
 */
MRB_BEGIN_DECL

MRB_API mrb_value mrb_const_get(mrb_state*, mrb_value, mrb_sym);
MRB_API void mrb_const_set(mrb_state*, mrb_value, mrb_sym, mrb_value);
MRB_API mrb_bool mrb_const_defined(mrb_state*, mrb_value, mrb_sym);
MRB_API void mrb_const_remove(mrb_state*, mrb_value, mrb_sym);

MRB_API mrb_bool mrb_iv_name_sym_p(mrb_state *mrb, mrb_sym sym);
MRB_API void mrb_iv_name_sym_check(mrb_state *mrb, mrb_sym sym);
MRB_API mrb_value mrb_obj_iv_get(mrb_state *mrb, struct RObject *obj, mrb_sym sym);
MRB_API void mrb_obj_iv_set(mrb_state *mrb, struct RObject *obj, mrb_sym sym, mrb_value v);
MRB_API mrb_bool mrb_obj_iv_defined(mrb_state *mrb, struct RObject *obj, mrb_sym sym);
MRB_API mrb_value mrb_iv_get(mrb_state *mrb, mrb_value obj, mrb_sym sym);
MRB_API void mrb_iv_set(mrb_state *mrb, mrb_value obj, mrb_sym sym, mrb_value v);
MRB_API mrb_bool mrb_iv_defined(mrb_state*, mrb_value, mrb_sym);
MRB_API mrb_value mrb_iv_remove(mrb_state *mrb, mrb_value obj, mrb_sym sym);
MRB_API void mrb_iv_copy(mrb_state *mrb, mrb_value dst, mrb_value src);
MRB_API mrb_bool mrb_const_defined_at(mrb_state *mrb, mrb_value mod, mrb_sym id);

/**
 * Get a global variable. Will return nil if the var does not exist
 *
 * Example:
 *
 *     !!!ruby
 *     # Ruby style
 *     var = $value
 *
 *     !!!c
 *     // C style
 *     mrb_sym sym = mrb_intern_lit(mrb, "$value");
 *     mrb_value var = mrb_gv_get(mrb, sym);
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 * @return The value of that global variable. May be nil
 */
MRB_API mrb_value mrb_gv_get(mrb_state *mrb, mrb_sym sym);

/**
 * Set a global variable
 *
 * Example:
 *
 *     !!!ruby
 *     # Ruby style
 *     $value = "foo"
 *
 *     !!!c
 *     // C style
 *     mrb_sym sym = mrb_intern_lit(mrb, "$value");
 *     mrb_gv_set(mrb, sym, mrb_str_new_lit("foo"));
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 * @param val The value of the global variable
 */
MRB_API void mrb_gv_set(mrb_state *mrb, mrb_sym sym, mrb_value val);

/**
 * Remove a global variable.
 *
 * Example:
 *
 *     # Ruby style
 *     $value = nil
 *
 *     // C style
 *     mrb_sym sym = mrb_intern_lit(mrb, "$value");
 *     mrb_gv_remove(mrb, sym);
 *
 * @param mrb The mruby state reference
 * @param sym The name of the global variable
 */
MRB_API void mrb_gv_remove(mrb_state *mrb, mrb_sym sym);

MRB_API mrb_value mrb_cv_get(mrb_state *mrb, mrb_value mod, mrb_sym sym);
MRB_API void mrb_mod_cv_set(mrb_state *mrb, struct RClass * c, mrb_sym sym, mrb_value v);
MRB_API void mrb_cv_set(mrb_state *mrb, mrb_value mod, mrb_sym sym, mrb_value v);
MRB_API mrb_bool mrb_cv_defined(mrb_state *mrb, mrb_value mod, mrb_sym sym);

/* return non zero to break the loop */
typedef int (mrb_iv_foreach_func)(mrb_state*,mrb_sym,mrb_value,void*);
MRB_API void mrb_iv_foreach(mrb_state *mrb, mrb_value obj, mrb_iv_foreach_func *func, void *p);

MRB_END_DECL


/* ======== mruby/numeric.h ======== */
/**
** @file mruby/numeric.h - Numeric, Integer, Float class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_NUMERIC_H

// #include "common.h" - in amalgam

/**
 * Numeric class and it's sub-classes.
 *
 * Integer and Float
 */
MRB_BEGIN_DECL

#define TYPED_POSFIXABLE(f,t) ((f) <= (t)MRB_FIXNUM_MAX)
#define TYPED_NEGFIXABLE(f,t) ((f) >= (t)MRB_FIXNUM_MIN)
#define TYPED_FIXABLE(f,t) (TYPED_POSFIXABLE(f,t) && TYPED_NEGFIXABLE(f,t))
#define POSFIXABLE(f) TYPED_POSFIXABLE(f,mrb_int)
#define NEGFIXABLE(f) TYPED_NEGFIXABLE(f,mrb_int)
#define FIXABLE(f) TYPED_FIXABLE(f,mrb_int)
#ifndef MRB_NO_FLOAT
#ifdef MRB_INT64
#define FIXABLE_FLOAT(f) ((f)>=-9223372036854775808.0 && (f)<9223372036854775808.0)
#else
#define FIXABLE_FLOAT(f) TYPED_FIXABLE(f,mrb_float)
#endif
#endif

/* utility functions */
MRB_API mrb_value mrb_num_add(mrb_state *mrb, mrb_value x, mrb_value y);
MRB_API mrb_value mrb_num_sub(mrb_state *mrb, mrb_value x, mrb_value y);
MRB_API mrb_value mrb_num_mul(mrb_state *mrb, mrb_value x, mrb_value y);
/* obsolete old names */
#define mrb_num_plus(mrb, x, y) mrb_num_add(mrb, x, y)
#define mrb_num_minus(mrb, x, y) mrb_num_sub(mrb, x, y)

MRB_API mrb_value mrb_integer_to_str(mrb_state *mrb, mrb_value x, mrb_int base);
MRB_API char *mrb_int_to_cstr(char *buf, size_t len, mrb_int n, mrb_int base);

/* obsolete function(s); will be removed */
#define mrb_fixnum_to_str(mrb, x, base) mrb_integer_to_str(mrb, x, base)

#ifndef __has_builtin
  #define __has_builtin(x) 0
#endif

#if (defined(__GNUC__) && __GNUC__ >= 5) ||   \
    (__has_builtin(__builtin_add_overflow) && \
     __has_builtin(__builtin_sub_overflow) && \
     __has_builtin(__builtin_mul_overflow))
# define MRB_HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS
#endif

/*
// Clang 3.8 and 3.9 have problem compiling mruby in 32-bit mode, when MRB_INT64 is set
// because of missing __mulodi4 and similar functions in its runtime. We need to use custom
// implementation for them.
*/
#ifdef MRB_HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS
#if defined(__clang__) && (__clang_major__ == 3) && (__clang_minor__ >= 8) && \
    defined(MRB_32BIT) && defined(MRB_INT64)
#undef MRB_HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS
#endif
#endif

#ifdef MRB_HAVE_TYPE_GENERIC_CHECKED_ARITHMETIC_BUILTINS

static inline mrb_bool
mrb_int_add_overflow(mrb_int augend, mrb_int addend, mrb_int *sum)
{
  return __builtin_add_overflow(augend, addend, sum);
}

static inline mrb_bool
mrb_int_sub_overflow(mrb_int minuend, mrb_int subtrahend, mrb_int *difference)
{
  return __builtin_sub_overflow(minuend, subtrahend, difference);
}

static inline mrb_bool
mrb_int_mul_overflow(mrb_int multiplier, mrb_int multiplicand, mrb_int *product)
{
  return __builtin_mul_overflow(multiplier, multiplicand, product);
}

#else

#define MRB_INT_OVERFLOW_MASK ((mrb_uint)1 << (MRB_INT_BIT - 1))

static inline mrb_bool
mrb_int_add_overflow(mrb_int a, mrb_int b, mrb_int *c)
{
  mrb_uint x = (mrb_uint)a;
  mrb_uint y = (mrb_uint)b;
  mrb_uint z = (mrb_uint)(x + y);
  *c = (mrb_int)z;
  return !!(((x ^ z) & (y ^ z)) & MRB_INT_OVERFLOW_MASK);
}

static inline mrb_bool
mrb_int_sub_overflow(mrb_int a, mrb_int b, mrb_int *c)
{
  mrb_uint x = (mrb_uint)a;
  mrb_uint y = (mrb_uint)b;
  mrb_uint z = (mrb_uint)(x - y);
  *c = (mrb_int)z;
  return !!(((x ^ z) & (~y ^ z)) & MRB_INT_OVERFLOW_MASK);
}

static inline mrb_bool
mrb_int_mul_overflow(mrb_int a, mrb_int b, mrb_int *c)
{
#ifdef MRB_INT32
  int64_t n = (int64_t)a * b;
  *c = (mrb_int)n;
  return n > MRB_INT_MAX || n < MRB_INT_MIN;
#else /* MRB_INT64 */
  *c = a * b;
  if (a > 0 && b > 0 && a > MRB_INT_MAX / b) return TRUE;
  if (a < 0 && b > 0 && a < MRB_INT_MIN / b) return TRUE;
  if (a > 0 && b < 0 && b < MRB_INT_MIN / a) return TRUE;
  if (a < 0 && b < 0 && (a <= MRB_INT_MIN || b <= MRB_INT_MIN || -a > MRB_INT_MAX / -b))
    return TRUE;
  return FALSE;
#endif
}

#undef MRB_INT_OVERFLOW_MASK

#endif

#ifndef MRB_NO_FLOAT

# define MRB_FLT_RADIX          FLT_RADIX

# ifdef MRB_USE_FLOAT32
#  define MRB_FLT_MANT_DIG      FLT_MANT_DIG
#  define MRB_FLT_EPSILON       FLT_EPSILON
#  define MRB_FLT_DIG           FLT_DIG
#  define MRB_FLT_MIN_EXP       FLT_MIN_EXP
#  define MRB_FLT_MIN           FLT_MIN
#  define MRB_FLT_MIN_10_EXP    FLT_MIN_10_EXP
#  define MRB_FLT_MAX_EXP       FLT_MAX_EXP
#  define MRB_FLT_MAX           FLT_MAX
#  define MRB_FLT_MAX_10_EXP    FLT_MAX_10_EXP

# else /* not MRB_USE_FLOAT32 */
#  define MRB_FLT_MANT_DIG      DBL_MANT_DIG
#  define MRB_FLT_EPSILON       DBL_EPSILON
#  define MRB_FLT_DIG           DBL_DIG
#  define MRB_FLT_MIN_EXP       DBL_MIN_EXP
#  define MRB_FLT_MIN           DBL_MIN
#  define MRB_FLT_MIN_10_EXP    DBL_MIN_10_EXP
#  define MRB_FLT_MAX_EXP       DBL_MAX_EXP
#  define MRB_FLT_MAX           DBL_MAX
#  define MRB_FLT_MAX_10_EXP    DBL_MAX_10_EXP
# endif /* MRB_USE_FLOAT32 */

MRB_API mrb_value mrb_float_to_integer(mrb_state *mrb, mrb_value val);

/* internal functions */
mrb_float mrb_div_float(mrb_float x, mrb_float y);
mrb_value mrb_float_to_str(mrb_state *mrb, mrb_value x, const char *fmt);
int mrb_format_float(mrb_float f, char *buf, size_t buf_size, char fmt, int prec, char sign);

#endif /* MRB_NO_FLOAT */

MRB_END_DECL


/* ======== mruby/error.h ======== */
/**
** @file mruby/error.h - Exception class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_ERROR_H

// #include "common.h" - in amalgam
#include <string.h>

/**
 * mruby error handling.
 */
MRB_BEGIN_DECL

struct RException {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
  struct RBasic *mesg;         // NULL or probably RString
  struct RBasic *backtrace;    // NULL, RArray or RData
};

/* error that should terminate execution */
#define MRB_EXC_EXIT 65536
#define MRB_EXC_EXIT_P(e) ((e)->flags & MRB_EXC_EXIT)
/* retrieve status value from exc; need <mruby/variable.h> and <mruby/presym.h> */
#define MRB_EXC_EXIT_STATUS(mrb,e) ((int)mrb_as_int((mrb),mrb_obj_iv_get((mrb),(e),MRB_SYM(status))))
/* exit with SystemExit status */
#define MRB_EXC_CHECK_EXIT(mrb,e) do {if (MRB_EXC_EXIT_P(e)) exit(MRB_EXC_EXIT_STATUS((mrb),(e)));} while (0)

#define mrb_exc_ptr(v) ((struct RException*)mrb_ptr(v))

MRB_API mrb_noreturn void mrb_sys_fail(mrb_state *mrb, const char *mesg);
MRB_API mrb_value mrb_exc_new_str(mrb_state *mrb, struct RClass* c, mrb_value str);
#define mrb_exc_new_lit(mrb, c, lit) mrb_exc_new_str(mrb, c, mrb_str_new_lit(mrb, lit))
MRB_API mrb_noreturn void mrb_no_method_error(mrb_state *mrb, mrb_sym id, mrb_value args, const char *fmt, ...);

#if defined(MRB_NAN_BOXING) || defined(MRB_WORD_BOXING) || defined(MRB_64BIT)
#undef MRB_USE_RBREAK_VALUE_UNION
#else
#define MRB_USE_RBREAK_VALUE_UNION 1
#endif

/*
 *  flags:
 *      0..7:   enum mrb_vtype (only when defined MRB_USE_RBREAK_VALUE_UNION)
 *      8..10:  RBREAK_TAGs in src/vm.c (otherwise, set to 0)
 */
struct RBreak {
  MRB_OBJECT_HEADER;
  uintptr_t ci_break_index; // The top-level ci index to break. One before the return destination.
#ifndef MRB_USE_RBREAK_VALUE_UNION
  mrb_value val;
#else
  /* Store value as uint32_t words instead of union mrb_value_union
     to avoid 8-byte alignment of int64_t/double on 32-bit platforms
     (e.g., ARM, MIPS, PowerPC) which would inflate struct size beyond
     the 5-word RVALUE limit due to padding. */
  uint32_t value[sizeof(union mrb_value_union) / sizeof(uint32_t)];
#endif
};

#ifndef MRB_USE_RBREAK_VALUE_UNION
#define mrb_break_value_get(brk) ((brk)->val)
#define mrb_break_value_set(brk, v) ((brk)->val = v)
#else
#define RBREAK_VALUE_TT_MASK ((1 << 8) - 1)
static inline mrb_value
mrb_break_value_get(struct RBreak *brk)
{
  mrb_value val;
  memcpy(&val.value, brk->value, sizeof(val.value));
  val.tt = (enum mrb_vtype)(brk->flags & RBREAK_VALUE_TT_MASK);
  return val;
}
static inline void
mrb_break_value_set(struct RBreak *brk, mrb_value val)
{
  memcpy(brk->value, &val.value, sizeof(val.value));
  brk->flags &= ~RBREAK_VALUE_TT_MASK;
  brk->flags |= val.tt;
}
#endif  /* MRB_USE_RBREAK_VALUE_UNION */

/**
 * Error check
 *
 */
/* clear error status in the mrb_state structure */
MRB_API void mrb_clear_error(mrb_state *mrb);
/* returns TRUE if error in the previous call; internally calls mrb_clear_error() */
MRB_API mrb_bool mrb_check_error(mrb_state *mrb);

/**
 * Protect
 *
 */
typedef mrb_value mrb_protect_error_func(mrb_state *mrb, void *userdata);
MRB_API mrb_value mrb_protect_error(mrb_state *mrb, mrb_protect_error_func *body, void *userdata, mrb_bool *error);

/**
 * Protect (takes mrb_value for body argument)
 *
 * Implemented in the mruby-error mrbgem
 */
MRB_API mrb_value mrb_protect(mrb_state *mrb, mrb_func_t body, mrb_value data, mrb_bool *state);

/**
 * Ensure
 *
 * Implemented in the mruby-error mrbgem
 */
MRB_API mrb_value mrb_ensure(mrb_state *mrb, mrb_func_t body, mrb_value b_data,
                             mrb_func_t ensure, mrb_value e_data);

/**
 * Rescue
 *
 * Implemented in the mruby-error mrbgem
 */
MRB_API mrb_value mrb_rescue(mrb_state *mrb, mrb_func_t body, mrb_value b_data,
                             mrb_func_t rescue, mrb_value r_data);

/**
 * Rescue exception
 *
 * Implemented in the mruby-error mrbgem
 */
MRB_API mrb_value mrb_rescue_exceptions(mrb_state *mrb, mrb_func_t body, mrb_value b_data,
                                        mrb_func_t rescue, mrb_value r_data,
                                        mrb_int len, struct RClass **classes);

/**
 *  Calls `func` via `mrb_protect_error()` and then always executes the user block exactly once.
 *  Even if a global jump (similar to a Ruby exception) occurs within `func`, the block will be executed,
 *  and after the block's completion, the global jump will be re-thrown.
 *
 *  By checking `mrb->exc != NULL` within the block, you can determine if a global jump occurred in `func`.
 *
 *  If you want to suppress the global jump and continue processing, use `mrb_clear_error(mrb); break;`.
 *
 *  - `mrb`: The mruby state reference
 *  - `result_var`: Pre-defined mrb_value type variable (to receive `func`'s return value)
 *  - `func`: Function to call (compatible with `mrb_protect_error_func`)
 *  - `data`: User data to pass to `func`
 *
 *  Example:
 *
 *      mrb_value result;
 *      MRB_ENSURE(mrb, result, body_func, userdata) {
 *        // This block is always executed (equivalent to Ruby's ensure)
 *
 *        if (mrb->exc) {
 *          // Post-processing when an exception occurs
 *        }
 *
 *        // To ignore the global jump, use `mrb_clear_error(mrb); break;` here
 *      }
 */
#define MRB_ENSURE(mrb, result_var, func, data) \
        for (mrb_bool MRB_UNIQNAME(_break_) = FALSE; \
             !MRB_UNIQNAME(_break_) && \
                (((result_var) = mrb_protect_error(mrb, func, data, &MRB_UNIQNAME(_break_))), \
                 ((mrb)->exc = (MRB_UNIQNAME(_break_) ? mrb_obj_ptr((result_var)) : NULL)), \
                 TRUE); \
             (void)(MRB_UNIQNAME(_break_) && (mrb)->jmp && (mrb_exc_raise(mrb, result_var), TRUE)), \
                MRB_UNIQNAME(_break_) = TRUE)

MRB_END_DECL


/* ======== mruby/data.h ======== */
/**
** @file mruby/data.h - Data class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_DATA_H

// #include "common.h" - in amalgam

/**
 * Custom C wrapped data.
 *
 * Defining Ruby wrappers around native objects.
 */
MRB_BEGIN_DECL

/**
 * Custom data type description.
 */
typedef struct mrb_data_type {
  /** data type name */
  const char *struct_name;

  /** data type release function pointer */
  void (*dfree)(mrb_state *mrb, void*);
} mrb_data_type;

struct RData {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
  const mrb_data_type *type;
  void *data;
};

MRB_API struct RData *mrb_data_object_alloc(mrb_state *mrb, struct RClass* klass, void *datap, const mrb_data_type *type);

#define Data_Wrap_Struct(mrb,klass,type,ptr)\
  mrb_data_object_alloc(mrb,klass,ptr,type)

#define Data_Make_Struct(mrb,klass,strct,type,sval,data_obj) do { \
  (data_obj) = Data_Wrap_Struct(mrb,klass,type,NULL);\
  (sval) = (strct*)mrb_malloc(mrb, sizeof(strct));                     \
  { static const strct zero = { 0 }; *(sval) = zero; };\
  (data_obj)->data = (sval);\
} while (0)

#define RDATA(obj)         ((struct RData*)(mrb_ptr(obj)))
#define DATA_PTR(d)        (RDATA(d)->data)
#define DATA_TYPE(d)       (RDATA(d)->type)
MRB_API void mrb_data_check_type(mrb_state *mrb, mrb_value, const mrb_data_type*);
MRB_API void *mrb_data_get_ptr(mrb_state *mrb, mrb_value, const mrb_data_type*);
#define DATA_GET_PTR(mrb,obj,dtype,type) (type*)mrb_data_get_ptr(mrb,obj,dtype)
MRB_API void *mrb_data_check_get_ptr(mrb_state *mrb, mrb_value, const mrb_data_type*);
#define DATA_CHECK_GET_PTR(mrb,obj,dtype,type) (type*)mrb_data_check_get_ptr(mrb,obj,dtype)

/* obsolete functions and macros */
#define mrb_data_check_and_get(mrb,obj,dtype) mrb_data_get_ptr(mrb,obj,dtype)
#define mrb_get_datatype(mrb,val,type) mrb_data_get_ptr(mrb, val, type)
#define mrb_check_datatype(mrb,val,type) mrb_data_get_ptr(mrb, val, type)
#define Data_Get_Struct(mrb,obj,type,sval) do {\
  *(void**)&sval = mrb_data_get_ptr(mrb, obj, type); \
} while (0)

MRB_INLINE void
mrb_data_init(mrb_value v, void *ptr, const mrb_data_type *type)
{
  mrb_assert(mrb_data_p(v));
  DATA_PTR(v) = ptr;
  DATA_TYPE(v) = type;
}

MRB_END_DECL


/* ======== mruby/istruct.h ======== */
/**
** @file mruby/istruct.h - Inline structures
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_ISTRUCT_H

// #include "common.h" - in amalgam
#include <string.h>

/**
 * Inline structures that fit in RVALUE
 *
 * They cannot have finalizer, and cannot have instance variables.
 */
MRB_BEGIN_DECL

#define ISTRUCT_DATA_SIZE (sizeof(void*) * 3)

struct RIStruct {
  MRB_OBJECT_HEADER;
  union {
    intptr_t inline_alignment[3];
    char inline_data[ISTRUCT_DATA_SIZE];
  };
};

#define RISTRUCT(obj)         ((struct RIStruct*)(mrb_ptr(obj)))
#define ISTRUCT_PTR(obj)      (RISTRUCT(obj)->inline_data)

MRB_INLINE mrb_int mrb_istruct_size(void)
{
  return ISTRUCT_DATA_SIZE;
}

MRB_INLINE void* mrb_istruct_ptr(mrb_value object)
{
  return ISTRUCT_PTR(object);
}

MRB_INLINE void mrb_istruct_copy(mrb_value dest, mrb_value src)
{
  memcpy(ISTRUCT_PTR(dest), ISTRUCT_PTR(src), ISTRUCT_DATA_SIZE);
}

MRB_END_DECL


/* ======== mruby/mempool.h ======== */
/*
** mempool.h - memory pool
**
** See Copyright Notice in mruby.h
*/

/* memory pool implementation */
typedef struct mempool mempool;
MRB_API struct mempool* mempool_open(void);
MRB_API void mempool_close(struct mempool*);
MRB_API void* mempool_alloc(struct mempool*, size_t);
MRB_API void* mempool_realloc(struct mempool*, void*, size_t oldlen, size_t newlen);

/* compatibility layer */
typedef struct mempool mrb_mempool;
#define mrb_mempool_open(m) mempool_open()
#define mrb_mempool_close(m) mempool_close(m)
#define mrb_mempool_alloc(m, size) mempool_alloc((m),(size))
#define mrb_mempool_realloc(m, ptr, oldlen, newlen) mempool_realloc((m),(ptr),(oldlen),(newlen))

/* ======== mruby/debug.h ======== */
/**
** @file mruby/debug.h - mruby debug info
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_DEBUG_H

// #include "common.h" - in amalgam

/**
 * mruby Debugging.
 */
MRB_BEGIN_DECL

typedef enum mrb_debug_line_type {
  mrb_debug_line_ary = 0,
  mrb_debug_line_flat_map,
  mrb_debug_line_packed_map
} mrb_debug_line_type;

typedef struct mrb_irep_debug_info_line {
  uint32_t start_pos;
  uint16_t line;
} mrb_irep_debug_info_line;

typedef struct mrb_irep_debug_info_file {
  uint32_t start_pos;
  mrb_sym filename_sym;
  uint32_t line_entry_count;
  mrb_debug_line_type line_type;
  union {
    const char *s;
    void *ptr;
    const uint16_t *ary;
    const mrb_irep_debug_info_line *flat_map;
    const uint8_t *packed_map;
  } lines;
} mrb_irep_debug_info_file;

typedef struct mrb_irep_debug_info {
  uint32_t pc_count;
  uint16_t flen;
  mrb_irep_debug_info_file **files;
} mrb_irep_debug_info;

/*
 * get filename from irep's debug info and program counter
 * @return returns NULL if not found
 */
MRB_API const char *mrb_debug_get_filename(mrb_state *mrb, const mrb_irep *irep, uint32_t pc);

/*
 * get line from irep's debug info and program counter
 * @return returns -1 if not found
 */
MRB_API int32_t mrb_debug_get_line(mrb_state *mrb, const mrb_irep *irep, uint32_t pc);

/*
 * get line and filename from irep's debug info and program counter
 * @return returns FALSE if not found
 */
MRB_API mrb_bool mrb_debug_get_position(mrb_state *mrb, const mrb_irep *irep, uint32_t pc, int32_t *lp, const char **fp);

MRB_API mrb_irep_debug_info *mrb_debug_info_alloc(mrb_state *mrb, mrb_irep *irep);
MRB_API mrb_irep_debug_info_file *mrb_debug_info_append_file(
    mrb_state *mrb, mrb_irep_debug_info *info,
    const char *filename, uint16_t *lines,
    uint32_t start_pos, uint32_t end_pos);
MRB_API void mrb_debug_info_free(mrb_state *mrb, mrb_irep_debug_info *d);

MRB_END_DECL


/* ======== mruby/dump.h ======== */
/**
** @file mruby/dump.h - mruby binary dumper (mrbc binary format)
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_DUMP_H

// #include <mruby.h> - in amalgam
// #include <mruby/irep.h> - in amalgam
// #include "common.h" - in amalgam

/**
 * Dumping compiled mruby script.
 */
MRB_BEGIN_DECL

/* flags for mrb_dump_irep{,_binary,_cfunc,_cstruct} */
#define MRB_DUMP_DEBUG_INFO 1
#define MRB_DUMP_STATIC 2
#define MRB_DUMP_NO_LVAR 4

#ifndef MRB_NO_STDIO
MRB_API mrb_value mrb_load_irep_file(mrb_state*,FILE*);
MRB_API mrb_value mrb_load_irep_file_cxt(mrb_state*, FILE*, mrb_ccontext*);
mrb_irep *mrb_read_irep_file(mrb_state*, FILE*);
int mrb_dump_irep_binary(mrb_state*, const mrb_irep*, uint8_t, FILE*);
#endif
/* avoid mrb_read_irep(); use mrb_read_irep_buf() instead (may cause buffer overflow) */
MRB_API mrb_irep *mrb_read_irep(mrb_state*, const uint8_t*);
MRB_API mrb_irep *mrb_read_irep_buf(mrb_state*, const void*, size_t);

/* dump/load error code
 *
 * NOTE: MRB_DUMP_GENERAL_FAILURE is caused by
 * unspecified issues like malloc failed.
 */
#define MRB_DUMP_OK                     0
#define MRB_DUMP_GENERAL_FAILURE      (-1)
#define MRB_DUMP_WRITE_FAULT          (-2)
#define MRB_DUMP_READ_FAULT           (-3)
#define MRB_DUMP_INVALID_FILE_HEADER  (-4)
#define MRB_DUMP_INVALID_IREP         (-5)
#define MRB_DUMP_INVALID_ARGUMENT     (-6)

/* null symbol length */
#define MRB_DUMP_NULL_SYM_LEN         0xFFFF

/* Rite Binary File header */
#define RITE_BINARY_IDENT              "RITE"
/* Binary Format Version Major:Minor */
/*   Major: Incompatible to prior versions */
/*   Minor: Upper-compatible to prior versions */
#define RITE_BINARY_MAJOR_VER          "04"
#define RITE_BINARY_MINOR_VER          "00"
#define RITE_BINARY_FORMAT_VER         RITE_BINARY_MAJOR_VER RITE_BINARY_MINOR_VER
#define RITE_COMPILER_NAME             "MATZ"
#define RITE_COMPILER_VERSION          "0000"

#define RITE_VM_VER                    "0400"

#define RITE_BINARY_EOF                "END\0"
#define RITE_SECTION_IREP_IDENT        "IREP"
#define RITE_SECTION_DEBUG_IDENT       "DBG\0"
#define RITE_SECTION_LV_IDENT          "LVAR"

#define MRB_DUMP_DEFAULT_STR_LEN      128
#define MRB_DUMP_ALIGNMENT            sizeof(uint32_t)

/* binary header */
struct rite_binary_header {
  uint8_t binary_ident[4];    /* Binary Identifier */
  uint8_t major_version[2];   /* Binary Format Major Version */
  uint8_t minor_version[2];   /* Binary Format Minor Version */
  uint8_t binary_size[4];     /* Binary Size */
  uint8_t compiler_name[4];   /* Compiler name */
  uint8_t compiler_version[4];
};

/* section header */
#define RITE_SECTION_HEADER \
  uint8_t section_ident[4]; \
  uint8_t section_size[4]

struct rite_section_header {
  RITE_SECTION_HEADER;
};

struct rite_section_irep_header {
  RITE_SECTION_HEADER;

  uint8_t rite_version[4];    /* Rite Instruction Specification Version */
};

struct rite_section_debug_header {
  RITE_SECTION_HEADER;
};

struct rite_section_lv_header {
  RITE_SECTION_HEADER;
};

#define RITE_LV_NULL_MARK              UINT16_MAX

struct rite_binary_footer {
  RITE_SECTION_HEADER;
};

static inline size_t
uint8_to_bin(uint8_t s, uint8_t *bin)
{
  *bin = s;
  return sizeof(uint8_t);
}

static inline size_t
uint16_to_bin(uint16_t s, uint8_t *bin)
{
  *bin++ = (s >> 8) & 0xff;
  *bin   = s & 0xff;
  return sizeof(uint16_t);
}

static inline size_t
uint32_to_bin(uint32_t l, uint8_t *bin)
{
  *bin++ = (l >> 24) & 0xff;
  *bin++ = (l >> 16) & 0xff;
  *bin++ = (l >> 8) & 0xff;
  *bin   = l & 0xff;
  return sizeof(uint32_t);
}

static inline uint32_t
bin_to_uint32(const uint8_t *bin)
{
  return (uint32_t)bin[0] << 24 |
         (uint32_t)bin[1] << 16 |
         (uint32_t)bin[2] << 8  |
         (uint32_t)bin[3];
}

static inline uint16_t
bin_to_uint16(const uint8_t *bin)
{
  return (uint16_t)bin[0] << 8 |
         (uint16_t)bin[1];
}

static inline uint8_t
bin_to_uint8(const uint8_t *bin)
{
  return (uint8_t)bin[0];
}

MRB_END_DECL

/* mruby/irep.h - already included */

/* ======== mruby/opcode.h ======== */
/**
** @file mruby/opcode.h - RiteVM operation codes
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_OPCODE_H

enum mrb_insn {
#define OPCODE(x,_) OP_ ## x,

/* ======== mruby/ops.h (inlined) ======== */
/* operand types:
   + Z: no operand
   + B: 8bit
   + BB: 8+8bit
   + BBB: 8+8+8bit
   + BS: 8+16bit
   + BSS: 8+16+16bit
   + S: 16bit
   + W: 24bit
*/

/*-----------------------------------------------------------------------
operation code    operands      semantics
------------------------------------------------------------------------*/
OPCODE(NOP,        Z)        /* no operation */
OPCODE(MOVE,       BB)       /* R[a] = R[b] */
OPCODE(LOADL,      BB)       /* R[a] = Pool[b] */
OPCODE(LOADI8,     BB)       /* R[a] = mrb_int(b) */
OPCODE(LOADINEG,   BB)       /* R[a] = mrb_int(-b) */
OPCODE(LOADI__1,   B)        /* R[a] = mrb_int(-1) */
OPCODE(LOADI_0,    B)        /* R[a] = mrb_int(0) */
OPCODE(LOADI_1,    B)        /* R[a] = mrb_int(1) */
OPCODE(LOADI_2,    B)        /* R[a] = mrb_int(2) */
OPCODE(LOADI_3,    B)        /* R[a] = mrb_int(3) */
OPCODE(LOADI_4,    B)        /* R[a] = mrb_int(4) */
OPCODE(LOADI_5,    B)        /* R[a] = mrb_int(5) */
OPCODE(LOADI_6,    B)        /* R[a] = mrb_int(6) */
OPCODE(LOADI_7,    B)        /* R[a] = mrb_int(7) */
OPCODE(LOADI16,    BS)       /* R[a] = mrb_int(b) */
OPCODE(LOADI32,    BSS)      /* R[a] = mrb_int((b<<16)+c) */
OPCODE(LOADSYM,    BB)       /* R[a] = Syms[b] */
OPCODE(LOADNIL,    B)        /* R[a] = nil */
OPCODE(LOADSELF,   B)        /* R[a] = self */
OPCODE(LOADTRUE,   B)        /* R[a] = true */
OPCODE(LOADFALSE,  B)        /* R[a] = false */
OPCODE(GETGV,      BB)       /* R[a] = getglobal(Syms[b]) */
OPCODE(SETGV,      BB)       /* setglobal(Syms[b], R[a]) */
OPCODE(GETSV,      BB)       /* R[a] = Special[Syms[b]] */
OPCODE(SETSV,      BB)       /* Special[Syms[b]] = R[a] */
OPCODE(GETIV,      BB)       /* R[a] = ivget(Syms[b]) */
OPCODE(SETIV,      BB)       /* ivset(Syms[b],R[a]) */
OPCODE(GETCV,      BB)       /* R[a] = cvget(Syms[b]) */
OPCODE(SETCV,      BB)       /* cvset(Syms[b],R[a]) */
OPCODE(GETCONST,   BB)       /* R[a] = constget(Syms[b]) */
OPCODE(SETCONST,   BB)       /* constset(Syms[b],R[a]) */
OPCODE(GETMCNST,   BB)       /* R[a] = R[a]::Syms[b] */
OPCODE(SETMCNST,   BB)       /* R[a+1]::Syms[b] = R[a] */
OPCODE(GETUPVAR,   BBB)      /* R[a] = uvget(b,c) */
OPCODE(SETUPVAR,   BBB)      /* uvset(b,c,R[a]) */
OPCODE(GETIDX,     B)        /* R[a] = R[a][R[a+1]] */
OPCODE(GETIDX0,    BB)       /* R[a] = R[b][0]; a+1 for method call */
OPCODE(SETIDX,     B)        /* R[a][R[a+1]] = R[a+2] */
OPCODE(JMP,        S)        /* pc+=a */
OPCODE(JMPIF,      BS)       /* if R[a] pc+=b */
OPCODE(JMPNOT,     BS)       /* if !R[a] pc+=b */
OPCODE(JMPNIL,     BS)       /* if R[a]==nil pc+=b */
OPCODE(JMPUW,      S)        /* unwind_and_jump_to(a) */
OPCODE(EXCEPT,     B)        /* R[a] = exc */
OPCODE(RESCUE,     BB)       /* R[b] = R[a].isa?(R[b]) */
OPCODE(RAISEIF,    B)        /* raise(R[a]) if R[a] */
OPCODE(MATCHERR,   B)        /* raise NoMatchingPatternError unless R[a] */
OPCODE(SSEND,      BBB)      /* R[a] = self.send(Syms[b],R[a+1]..,R[a+n+1]:R[a+n+2]..) (c=n|k<<4) */
OPCODE(SSEND0,     BB)       /* R[a] = self.send(Syms[b]) (no args) */
OPCODE(SSENDB,     BBB)      /* R[a] = self.send(Syms[b],R[a+1]..,R[a+n+1]:R[a+n+2]..,&R[a+n+2k+1]) */
OPCODE(SEND,       BBB)      /* R[a] = R[a].send(Syms[b],R[a+1]..,R[a+n+1]:R[a+n+2]..) (c=n|k<<4) */
OPCODE(SEND0,      BB)       /* R[a] = R[a].send(Syms[b]) (no args) */
OPCODE(SENDB,      BBB)      /* R[a] = R[a].send(Syms[b],R[a+1]..,R[a+n+1]:R[a+n+2]..,&R[a+n+2k+1]) */
OPCODE(CALL,       Z)        /* self.call(*, **, &) (But overlay the current call frame; tailcall) */
OPCODE(BLKCALL,    BB)       /* R[a] = R[a].call(R[a+1],... ,R[a+b]); direct block call */
OPCODE(SUPER,      BB)       /* R[a] = super(R[a+1],... ,R[a+b+1]) */
OPCODE(ARGARY,     BS)       /* R[a] = argument array (16=m5:r1:m5:d1:lv4) */
OPCODE(ENTER,      W)        /* arg setup according to flags (24=n1:m5:o5:r1:m5:k5:d1:b1) */
OPCODE(KEY_P,      BB)       /* R[a] = kdict.key?(Syms[b]) */
OPCODE(KEYEND,     Z)        /* raise unless kdict.empty? */
OPCODE(KARG,       BB)       /* R[a] = kdict[Syms[b]]; kdict.delete(Syms[b]) */
OPCODE(RETURN,     B)        /* return R[a] (normal) */
OPCODE(RETURN_BLK, B)        /* return R[a] (in-block return) */
OPCODE(RETSELF,    Z)        /* return self */
OPCODE(RETNIL,     Z)        /* return nil */
OPCODE(RETTRUE,    Z)        /* return true */
OPCODE(RETFALSE,   Z)        /* return false */
OPCODE(BREAK,      B)        /* break R[a] */
OPCODE(BLKPUSH,    BS)       /* R[a] = block (16=m5:r1:m5:d1:lv4) */
OPCODE(ADD,        B)        /* R[a] = R[a]+R[a+1] */
OPCODE(ADDI,       BB)       /* R[a] = R[a]+mrb_int(b) */
OPCODE(SUB,        B)        /* R[a] = R[a]-R[a+1] */
OPCODE(SUBI,       BB)       /* R[a] = R[a]-mrb_int(b) */
OPCODE(ADDILV,     BBB)      /* R[a] = R[a]+mrb_int(c); R[b],R[b+1] for method call */
OPCODE(SUBILV,     BBB)      /* R[a] = R[a]-mrb_int(c); R[b],R[b+1] for method call */
OPCODE(MUL,        B)        /* R[a] = R[a]*R[a+1] */
OPCODE(DIV,        B)        /* R[a] = R[a]/R[a+1] */
OPCODE(EQ,         B)        /* R[a] = R[a]==R[a+1] */
OPCODE(LT,         B)        /* R[a] = R[a]<R[a+1] */
OPCODE(LE,         B)        /* R[a] = R[a]<=R[a+1] */
OPCODE(GT,         B)        /* R[a] = R[a]>R[a+1] */
OPCODE(GE,         B)        /* R[a] = R[a]>=R[a+1] */
OPCODE(ARRAY,      BB)       /* R[a] = ary_new(R[a],R[a+1]..R[a+b]) */
OPCODE(ARRAY2,     BBB)      /* R[a] = ary_new(R[b],R[b+1]..R[b+c]) */
OPCODE(ARYCAT,     B)        /* ary_cat(R[a],R[a+1]) */
OPCODE(ARYPUSH,    BB)       /* ary_push(R[a],R[a+1]..R[a+b]) */
OPCODE(ARYSPLAT,   B)        /* R[a] = ary_splat(R[a]) */
OPCODE(AREF,       BBB)      /* R[a] = R[b][c] */
OPCODE(ASET,       BBB)      /* R[b][c] = R[a] */
OPCODE(APOST,      BBB)      /* *R[a],R[a+1]..R[a+c] = R[a][b..] */
OPCODE(INTERN,     B)        /* R[a] = intern(R[a]) */
OPCODE(SYMBOL,     BB)       /* R[a] = intern(Pool[b]) */
OPCODE(STRING,     BB)       /* R[a] = str_dup(Pool[b]) */
OPCODE(STRCAT,     B)        /* str_cat(R[a],R[a+1]) */
OPCODE(HASH,       BB)       /* R[a] = hash_new(R[a],R[a+1]..R[a+b*2-1]) */
OPCODE(HASHADD,    BB)       /* hash_push(R[a],R[a+1]..R[a+b*2]) */
OPCODE(HASHCAT,    B)        /* R[a] = hash_cat(R[a],R[a+1]) */
OPCODE(LAMBDA,     BB)       /* R[a] = lambda(Irep[b],L_LAMBDA) */
OPCODE(BLOCK,      BB)       /* R[a] = lambda(Irep[b],L_BLOCK) */
OPCODE(METHOD,     BB)       /* R[a] = lambda(Irep[b],L_METHOD) */
OPCODE(RANGE_INC,  B)        /* R[a] = range_new(R[a],R[a+1],FALSE) */
OPCODE(RANGE_EXC,  B)        /* R[a] = range_new(R[a],R[a+1],TRUE) */
OPCODE(OCLASS,     B)        /* R[a] = ::Object */
OPCODE(CLASS,      BB)       /* R[a] = newclass(R[a],Syms[b],R[a+1]) */
OPCODE(MODULE,     BB)       /* R[a] = newmodule(R[a],Syms[b]) */
OPCODE(EXEC,       BB)       /* R[a] = blockexec(R[a],Irep[b]) */
OPCODE(DEF,        BB)       /* R[a].newmethod(Syms[b],R[a+1]); R[a] = Syms[b] */
OPCODE(TDEF,       BBB)      /* target_class.newmethod(Syms[b],Irep[c]); R[a] = Syms[b] */
OPCODE(SDEF,       BBB)      /* R[a].singleton_class.newmethod(Syms[b],Irep[c]); R[a] = Syms[b] */
OPCODE(ALIAS,      BB)       /* alias_method(target_class,Syms[a],Syms[b]) */
OPCODE(UNDEF,      B)        /* undef_method(target_class,Syms[a]) */
OPCODE(SCLASS,     B)        /* R[a] = R[a].singleton_class */
OPCODE(TCLASS,     B)        /* R[a] = target_class */
OPCODE(DEBUG,      BBB)      /* print a,b,c */
OPCODE(ERR,        B)        /* raise(LocalJumpError, Pool[a]) */
OPCODE(EXT1,       Z)        /* make 1st operand (a) 16bit */
OPCODE(EXT2,       Z)        /* make 2nd operand (b) 16bit */
OPCODE(EXT3,       Z)        /* make 1st and 2nd operands 16bit */
OPCODE(STOP,       Z)        /* stop VM */

#undef OPCODE
};

/* backward compatibility aliases */
#define OP_LOADT OP_LOADTRUE
#define OP_LOADF OP_LOADFALSE

#define OP_L_STRICT  1
#define OP_L_CAPTURE 2
#define OP_L_METHOD  OP_L_STRICT
#define OP_L_LAMBDA  (OP_L_STRICT|OP_L_CAPTURE)
#define OP_L_BLOCK   OP_L_CAPTURE

#define PEEK_B(pc) (*(pc))
#define PEEK_S(pc) ((pc)[0]<<8|(pc)[1])
#define PEEK_W(pc) ((pc)[0]<<16|(pc)[1]<<8|(pc)[2])

#define READ_B() PEEK_B(pc++)
#define READ_S() (pc+=2, PEEK_S(pc-2))
#define READ_W() (pc+=3, PEEK_W(pc-3))

#define FETCH_Z() /* nothing */
#define FETCH_B() do {a=READ_B();} while (0)
#define FETCH_BB() do {a=READ_B(); b=READ_B();} while (0)
#define FETCH_BBB() do {a=READ_B(); b=READ_B(); c=READ_B();} while (0)
#define FETCH_BS() do {a=READ_B(); b=READ_S();} while (0)
#define FETCH_BSS() do {a=READ_B(); b=READ_S(); c=READ_S();} while (0)
#define FETCH_S() do {a=READ_S();} while (0)
#define FETCH_W() do {a=READ_W();} while (0)

/* with OP_EXT1 (1st 16bit) */
#define FETCH_Z_1() FETCH_Z()
#define FETCH_B_1() FETCH_S()
#define FETCH_BB_1() do {a=READ_S(); b=READ_B();} while (0)
#define FETCH_BBB_1() do {a=READ_S(); b=READ_B(); c=READ_B();} while (0)
#define FETCH_BS_1() do {a=READ_S(); b=READ_S();} while (0)
#define FETCH_BSS_1() do {a=READ_S(); b=READ_S();c=READ_S();} while (0)
#define FETCH_S_1() FETCH_S()
#define FETCH_W_1() FETCH_W()

/* with OP_EXT2 (2nd 16bit) */
#define FETCH_Z_2() FETCH_Z()
#define FETCH_B_2() FETCH_B()
#define FETCH_BB_2() do {a=READ_B(); b=READ_S();} while (0)
#define FETCH_BBB_2() do {a=READ_B(); b=READ_S(); c=READ_B();} while (0)
#define FETCH_BS_2() FETCH_BS()
#define FETCH_BSS_2() FETCH_BSS()
#define FETCH_S_2() FETCH_S()
#define FETCH_W_2() FETCH_W()

/* with OP_EXT3 (1st & 2nd 16bit) */
#define FETCH_Z_3() FETCH_Z()
#define FETCH_B_3() FETCH_B()
#define FETCH_BB_3() do {a=READ_S(); b=READ_S();} while (0)
#define FETCH_BBB_3() do {a=READ_S(); b=READ_S(); c=READ_B();} while (0)
#define FETCH_BS_3() do {a=READ_S(); b=READ_S();} while (0)
#define FETCH_BSS_3() FETCH_BSS_1()
#define FETCH_S_3() FETCH_S()
#define FETCH_W_3() FETCH_W()


/* ======== mruby/re.h ======== */
/**
** @file mruby/re.h - Regexp class
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_RE_H

MRB_BEGIN_DECL

#define REGEXP_CLASS          "Regexp"

MRB_END_DECL


/* ======== mruby/throw.h ======== */
/**
** @file mruby/throw.h - mruby exception throwing handler
**
** See Copyright Notice in mruby.h
**
** WARNING: This header is for mruby core internal use only.
** Do not include this header in user code or mrbgems.
**
** When MRB_USE_CXX_EXCEPTION is defined, this header requires C++
** compilation. C source files that include this header will fail
** to compile when linked into C++ projects using MRB_USE_CXX_EXCEPTION.
**
** For exception-safe code in mrbgems and user code, use the
** mrb_protect_error() API from <mruby/error.h> instead:
**
**   #include <mruby/error.h>
**
**   mrb_value my_func_body(mrb_state *mrb, void *data) {
**     // code that may raise exceptions
**     return result;
**   }
**
**   void my_func(mrb_state *mrb) {
**     mrb_bool error;
**     mrb_value result = mrb_protect_error(mrb, my_func_body, data, &error);
**     // cleanup code runs here regardless of exception
**     if (error) mrb_exc_raise(mrb, result);
**   }
*/

#define MRB_THROW_H

#if defined(MRB_USE_CXX_ABI) && !defined(__cplusplus)
#  error Trying to use C++ exception handling in C code
#endif

#if defined(MRB_USE_CXX_EXCEPTION)

# if defined(__cplusplus)

#define MRB_TRY(buf) try {
#define MRB_CATCH(buf) } catch(mrb_jmpbuf *e) { if (e != (buf)) { throw e; }
#define MRB_END_EXC(buf)  }

#define MRB_THROW(buf) throw(buf)
typedef void *mrb_jmpbuf_impl;

# else
# error "need to be compiled with C++ compiler"
# endif  /* __cplusplus */

#else

#include <setjmp.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define MRB_SETJMP _setjmp
#define MRB_LONGJMP _longjmp
#elif defined(__MINGW64__) && !defined(_M_ARM64) && defined(__GNUC__) && __GNUC__ >= 4
#define MRB_SETJMP __builtin_setjmp
#define MRB_LONGJMP __builtin_longjmp
#else
#define MRB_SETJMP setjmp
#define MRB_LONGJMP longjmp
#endif

#define MRB_TRY(buf) if (MRB_SETJMP((buf)->impl) == 0) {
#define MRB_CATCH(buf) } else {
#define MRB_END_EXC(buf) }

#define MRB_THROW(buf) MRB_LONGJMP((buf)->impl, 1);
#define mrb_jmpbuf_impl jmp_buf

#endif

struct mrb_jmpbuf {
  mrb_jmpbuf_impl impl;
};


/* ======== mruby/khash.h ======== */
/**
** @file mruby/khash.h - Hash for mruby
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_KHASH_H

#include <string.h>

// #include <mruby.h> - in amalgam
// #include "common.h" - in amalgam

/**
 * khash definitions used in mruby's hash table.
 */
MRB_BEGIN_DECL

typedef uint32_t khint_t;
typedef khint_t khiter_t;

#ifndef KHASH_INITIAL_SIZE
# define KHASH_INITIAL_SIZE 32
#endif
#define KHASH_MIN_SIZE 8
#define KHASH_SMALL_LIMIT 4

#define KH_UPPER_BOUND(x) ((x) - ((x)>>3))   /* 87.5% load factor */

/* extern uint8_t __m[]; */

/* mask for flags */
static const uint8_t __m_empty[]  = {0x02, 0x08, 0x20, 0x80};
static const uint8_t __m_del[]    = {0x01, 0x04, 0x10, 0x40};
static const uint8_t __m_either[] = {0x03, 0x0c, 0x30, 0xc0};


#define __ac_isempty(ed_flag, i) (ed_flag[(i)/4]&__m_empty[(i)%4])
#define __ac_isdel(ed_flag, i) (ed_flag[(i)/4]&__m_del[(i)%4])
#define __ac_iseither(ed_flag, i) (ed_flag[(i)/4]&__m_either[(i)%4])
#define khash_power2(v) do { \
  v--;\
  v |= v >> 1;\
  v |= v >> 2;\
  v |= v >> 4;\
  v |= v >> 8;\
  v |= v >> 16;\
  v++;\
} while (0)
#define khash_mask(h) ((h)->n_buckets-1)
#define khash_upper_bound(h) (KH_UPPER_BOUND((h)->n_buckets))

/* BREAKING CHANGE: khash structure optimized for 50% memory reduction
 *
 * The structure now uses a single data pointer instead of separate keys,
 * vals, and ed_flags pointers, reducing size from 32 to 16 bytes.
 *
 * MIGRATION REQUIRED for field access macros:
 * - OLD: kh_key(h, x)      NEW: kh_key(typename, h, x)
 * - OLD: kh_val(h, x)      NEW: kh_val(typename, h, x)
 * - OLD: kh_exist(h, x)    NEW: kh_exist(typename, h, x)
 * - OLD: KHASH_FOREACH()   NEW: KHASH_FOREACH(typename, ...)
 *
 * Function-style macros (kh_get, kh_put, etc.) remain unchanged.
 */

/* declare struct kh_xxx and kh_xxx_funcs

   name: hash name
   khkey_t: key data type
   khval_t: value data type
   kh_is_map: (0: hash set / 1: hash map)
*/
#define KHASH_DECLARE(name, khkey_t, khval_t, kh_is_map)                \
  typedef struct kh_##name {                                            \
    void *data;          /* Single allocation: [keys][vals][flags] */   \
    khint_t n_buckets;   /* Number of buckets (power of 2) */           \
    khint_t size;        /* Number of elements */                       \
  } kh_##name##_t;                                                      \
  /* Address calculation functions for optimized memory layout */       \
  static inline khkey_t* kh_keys_##name(const kh_##name##_t *h) {       \
    return (khkey_t*)(h)->data;                                         \
  }                                                                     \
  static inline khval_t* kh_vals_##name(const kh_##name##_t *h) {       \
    return kh_is_map ?                                                  \
      (khval_t*)((uint8_t*)(h)->data + sizeof(khkey_t) * (h)->n_buckets) : NULL; \
  }                                                                     \
  static inline uint8_t* kh_flags_##name(const kh_##name##_t *h) {      \
    return (uint8_t*)(h)->data + sizeof(khkey_t) * (h)->n_buckets +     \
           (kh_is_map ? sizeof(khval_t) * (h)->n_buckets : 0);          \
  }                                                                     \
  kh_##name##_t *kh_init_##name##_size(mrb_state *mrb, khint_t size);   \
  kh_##name##_t *kh_init_##name(mrb_state *mrb);                        \
  void kh_destroy_##name(mrb_state *mrb, kh_##name##_t *h);             \
  void kh_clear_##name(mrb_state *mrb, kh_##name##_t *h);               \
  khint_t kh_get_##name(mrb_state *mrb, kh_##name##_t *h, khkey_t key);           \
  khint_t kh_put_##name(mrb_state *mrb, kh_##name##_t *h, khkey_t key, int *ret); \
  void kh_resize_##name(mrb_state *mrb, kh_##name##_t *h, khint_t new_n_buckets); \
  void kh_del_##name(mrb_state *mrb, kh_##name##_t *h, khint_t x);                \
  kh_##name##_t *kh_copy_##name(mrb_state *mrb, kh_##name##_t *h);                \
  void kh_init_data_##name(mrb_state *mrb, kh_##name##_t *h, khint_t size);       \
  void kh_destroy_data_##name(mrb_state *mrb, kh_##name##_t *h);                  \
  void kh_replace_##name(mrb_state *mrb, kh_##name##_t *dst, const kh_##name##_t *src);

/* define kh_xxx_funcs

   name: hash name
   khkey_t: key data type
   khval_t: value data type
   kh_is_map: (0: hash set / 1: hash map)
   __hash_func: hash function
   __hash_equal: hash comparison function
*/
#define KHASH_DEFINE(name, khkey_t, khval_t, kh_is_map, __hash_func, __hash_equal) \
  mrb_noreturn void mrb_raise_nomemory(mrb_state *mrb);                 \
  /* Internal helper functions */                                       \
  static inline size_t kh__kv_size_##name(khint_t count) {              \
    return sizeof(khkey_t) * count +                                    \
           (kh_is_map ? sizeof(khval_t) * count : 0);                   \
  }                                                                     \
  static inline size_t kh__htable_size_##name(khint_t n_buckets) {      \
    return kh__kv_size_##name(n_buckets) + n_buckets / 4;               \
  }                                                                     \
  static inline void kh__mark_occupied_##name(kh_##name##_t *h, khint_t i) { \
    uint8_t *flags = kh_flags_##name(h);                                \
    flags[i/4] &= ~__m_either[i%4];  /* Clear both empty and deleted bits */ \
  }                                                                     \
  static inline void kh__mark_deleted_##name(kh_##name##_t *h, khint_t i) {  \
    uint8_t *flags = kh_flags_##name(h);                                \
    flags[i/4] |= __m_del[i%4];      /* Set deleted bit */              \
  }                                                                     \
  static inline khint_t kh__key_idx_##name(mrb_state *mrb, khkey_t key, kh_##name##_t *h) { \
    return __hash_func(mrb, key) & khash_mask(h);                       \
  }                                                                     \
  static inline khint_t kh__next_probe_##name(khint_t k, khint_t *step, kh_##name##_t *h) { \
    return (k+(++(*step))) & khash_mask(h);                             \
  }                                                                     \
  static inline khint_t kh__insert_key_##name(kh_##name##_t *h, khint_t index, khkey_t key) { \
    khkey_t *keys = kh_keys_##name(h);                                  \
    keys[index] = key;                                                  \
    kh__mark_occupied_##name(h, index);                                 \
    h->size++;                                                          \
    return index;                                                       \
  }                                                                     \
  static inline void kh__clear_flags_##name(kh_##name##_t *h, khint_t n_buckets) { \
    memset(kh_flags_##name(h), 0xaa, n_buckets/4);                      \
  }                                                                     \
  static inline void kh__alloc_##name(mrb_state *mrb, kh_##name##_t *h) { \
    khint_t sz = h->n_buckets;                                          \
    uint8_t *p = (uint8_t*)mrb_malloc(mrb, kh__htable_size_##name(sz)); \
    h->size = 0;                                                        \
    h->data = p;  /* Single data pointer for optimized layout */        \
    kh__clear_flags_##name(h, sz);                                      \
  }                                                                     \
  /* Small table optimization functions */                              \
  static inline int kh__is_small_##name(const kh_##name##_t *h) {       \
    return h->n_buckets == 0;  /* Small table marker */                 \
  }                                                                     \
  static inline khint_t kh__get_small_##name(mrb_state *mrb, kh_##name##_t *h, khkey_t key) { \
    khkey_t *keys = kh_keys_##name(h);                                  \
    for (khint_t i = 0; i < h->size; i++) {                             \
      if (__hash_equal(mrb, keys[i], key)) return i;                    \
    }                                                                   \
    return h->size;  /* Not found - return end position */              \
  }                                                                     \
  static inline void kh__rebuild_##name(mrb_state *mrb, kh_##name##_t *h, khint_t new_n_buckets) { \
    kh_##name##_t hh;                                                   \
    hh.data = NULL;                                                     \
    hh.size = 0;                                                        \
    kh_init_data_##name(mrb, &hh, new_n_buckets);                       \
    /* Rehash from old 'h' to 'hh' */                                   \
    khkey_t *old_keys = kh_keys_##name(h);                              \
    khval_t *old_vals = kh_vals_##name(h);                              \
    uint8_t *old_flags = kh__is_small_##name(h) ? NULL : kh_flags_##name(h); \
    khint_t limit = old_flags ? h->n_buckets : h->size;                 \
    for (khint_t i = 0; i < limit; i++) {                               \
      if (old_flags && __ac_iseither(old_flags, i)) continue;           \
      khint_t k = kh_put_##name(mrb, &hh, old_keys[i], NULL);           \
      if (kh_is_map) {                                                  \
        kh_val(name, &hh, k) = old_vals[i];                             \
      }                                                                 \
    }                                                                   \
    /* Final Swap */                                                    \
    mrb_free(mrb, h->data);                                             \
    h->data = hh.data;                                                  \
    h->n_buckets = hh.n_buckets;                                        \
    h->size = hh.size;                                                  \
  }                                                                     \
  static inline khint_t kh__put_small_##name(mrb_state *mrb, kh_##name##_t *h, khkey_t key, int *ret) { \
    /* First check if key exists */                                     \
    khint_t pos = kh__get_small_##name(mrb, h, key);                    \
    if (pos < h->size) {                                                \
      if (ret) *ret = 0;  /* Key exists */                              \
      return pos;                                                       \
    }                                                                   \
    /* Check if we need to convert to hash table */                     \
    if (h->size >= KHASH_SMALL_LIMIT) {                                 \
      /* Convert from small table to hash table */                      \
      kh__rebuild_##name(mrb, h, KHASH_MIN_SIZE);                       \
      /* Now add the new key using regular hash table */                \
      return kh_put_##name(mrb, h, key, ret);                           \
    }                                                                   \
    /* Add new element to small table */                                \
    khkey_t *keys = kh_keys_##name(h);                                  \
    keys[h->size] = key;                                                \
    h->size++;                                                          \
    if (ret) *ret = 1;  /* New key */                                   \
    return h->size - 1;                                                 \
  }                                                                     \
  kh_##name##_t *kh_init_##name##_size(mrb_state *mrb, khint_t size) {  \
    kh_##name##_t *h = (kh_##name##_t*)mrb_calloc(mrb, 1, sizeof(kh_##name##_t)); \
    kh_init_data_##name(mrb, h, size);                                  \
    return h;                                                           \
  }                                                                     \
  kh_##name##_t *kh_init_##name(mrb_state *mrb) {                       \
    return kh_init_##name##_size(mrb, KHASH_INITIAL_SIZE);              \
  }                                                                     \
  void kh_destroy_##name(mrb_state *mrb, kh_##name##_t *h)              \
  {                                                                     \
    kh_destroy_data_##name(mrb, h);                                     \
    mrb_free(mrb, h);                                                   \
  }                                                                     \
  void kh_clear_##name(mrb_state *mrb, kh_##name##_t *h)                \
  {                                                                     \
    (void)mrb;                                                          \
    if (h && h->data) {                                                 \
      kh__clear_flags_##name(h, h->n_buckets);                          \
      h->size = 0;                                                      \
    }                                                                   \
  }                                                                     \
  khint_t kh_get_##name(mrb_state *mrb, kh_##name##_t *h, khkey_t key)  \
  {                                                                     \
    if (kh__is_small_##name(h)) {                                       \
      return kh__get_small_##name(mrb, h, key);                         \
    }                                                                   \
    /* Cache calculated pointers for performance */                     \
    khkey_t *keys = kh_keys_##name(h);                                  \
    uint8_t *ed_flags = kh_flags_##name(h);                             \
    khint_t k = kh__key_idx_##name(mrb, key, h), step = 0;              \
    (void)mrb;                                                          \
    while (!__ac_isempty(ed_flags, k)) {                                \
      if (!__ac_isdel(ed_flags, k)) {                                   \
        if (__hash_equal(mrb, keys[k], key)) return k;                  \
      }                                                                 \
      k = kh__next_probe_##name(k, &step, h);                           \
    }                                                                   \
    return kh_end(h);                                                   \
  }                                                                     \
  void kh_resize_##name(mrb_state *mrb, kh_##name##_t *h, khint_t new_n_buckets) \
  {                                                                     \
    if (new_n_buckets < KHASH_MIN_SIZE)                                 \
      new_n_buckets = KHASH_MIN_SIZE;                                   \
    khash_power2(new_n_buckets);                                        \
    kh__rebuild_##name(mrb, h, new_n_buckets);                          \
  }                                                                     \
  khint_t kh_put_##name(mrb_state *mrb, kh_##name##_t *h, khkey_t key, int *ret) \
  {                                                                     \
    if (kh__is_small_##name(h)) {                                       \
      return kh__put_small_##name(mrb, h, key, ret);                    \
    }                                                                   \
    khint_t k, del_k, step = 0;                                         \
    if (h->size >= khash_upper_bound(h)) {                              \
      kh_resize_##name(mrb, h, h->n_buckets*2);                         \
    }                                                                   \
    /* Cache calculated pointers for performance */                     \
    khkey_t *keys = kh_keys_##name(h);                                  \
    uint8_t *ed_flags = kh_flags_##name(h);                             \
    k = kh__key_idx_##name(mrb, key, h);                                \
    del_k = kh_end(h);                                                  \
    while (!__ac_isempty(ed_flags, k)) {                                \
      if (!__ac_isdel(ed_flags, k)) {                                   \
        if (__hash_equal(mrb, keys[k], key)) {                          \
          if (ret) *ret = 0;                                            \
          return k;                                                     \
        }                                                               \
      }                                                                 \
      else if (del_k == kh_end(h)) {                                    \
        del_k = k;                                                      \
      }                                                                 \
      k = kh__next_probe_##name(k, &step, h);                           \
    }                                                                   \
    if (del_k != kh_end(h)) {                                           \
      /* put at del */                                                  \
      kh__insert_key_##name(h, del_k, key);                             \
      if (ret) *ret = 2;                                                \
      return del_k;                                                     \
    }                                                                   \
    else {                                                              \
      /* put at empty */                                                \
      kh__insert_key_##name(h, k, key);                                 \
      if (ret) *ret = 1;                                                \
      return k;                                                         \
    }                                                                   \
  }                                                                     \
  void kh_del_##name(mrb_state *mrb, kh_##name##_t *h, khint_t x)       \
  {                                                                     \
    (void)mrb;                                                          \
    if (kh__is_small_##name(h)) {                                       \
      /* Small table deletion: shift elements down */                   \
      mrb_assert(x < h->size);                                          \
      khkey_t *keys = kh_keys_##name(h);                                \
      khval_t *vals = kh_vals_##name(h);                                \
      for (khint_t i = x; i < h->size - 1; i++) {                       \
        keys[i] = keys[i + 1];                                          \
        if (kh_is_map) vals[i] = vals[i + 1];                           \
      }                                                                 \
      h->size--;                                                        \
    }                                                                   \
    else {                                                              \
      /* Regular hash table deletion */                                 \
      mrb_assert(x != h->n_buckets && !__ac_iseither(kh_flags_##name(h), x)); \
      kh__mark_deleted_##name(h, x);                                    \
      h->size--;                                                        \
    }                                                                   \
  }                                                                     \
  kh_##name##_t *kh_copy_##name(mrb_state *mrb, kh_##name##_t *h)       \
  {                                                                     \
    kh_##name##_t *h2 = (kh_##name##_t*)mrb_calloc(mrb, 1, sizeof(kh_##name##_t)); \
    kh_replace_##name(mrb, h2, h);                                      \
    return h2;                                                          \
  }                                                                     \
  void kh_init_data_##name(mrb_state *mrb, kh_##name##_t *h, khint_t size) { \
    if (size <= KHASH_SMALL_LIMIT) {                                    \
      /* Start as small table */                                        \
      h->n_buckets = 0;  /* Small table marker */                       \
      h->data = mrb_malloc(mrb, kh__kv_size_##name(KHASH_SMALL_LIMIT)); \
      h->size = 0;                                                      \
    }                                                                   \
    else {                                                              \
      /* Start as regular hash table */                                 \
      if (size < KHASH_MIN_SIZE)                                        \
        size = KHASH_MIN_SIZE;                                          \
      khash_power2(size);                                               \
      h->n_buckets = size;                                              \
      kh__alloc_##name(mrb, h);                                         \
    }                                                                   \
  }                                                                     \
  void kh_destroy_data_##name(mrb_state *mrb, kh_##name##_t *h)         \
  {                                                                     \
    if (h && h->data) {                                                 \
      mrb_free(mrb, h->data);  /* Free only the data allocation */      \
      h->data = NULL;                                                   \
    }                                                                   \
  }                                                                     \
  void kh_replace_##name(mrb_state *mrb, kh_##name##_t *dst, const kh_##name##_t *src) \
  {                                                                     \
    if (!src || (src->n_buckets == 0 && src->size == 0)) {              \
      /* Empty source */                                                \
      kh_destroy_data_##name(mrb, dst);                                 \
      dst->data = NULL;                                                 \
      dst->n_buckets = 0;                                               \
      dst->size = 0;                                                    \
    }                                                                   \
    else if (src->n_buckets == 0) {                                     \
      /* Small table case */                                            \
      size_t data_size = kh__kv_size_##name(KHASH_SMALL_LIMIT);         \
      dst->data = mrb_realloc(mrb, dst->data, data_size);               \
      dst->size = src->size;                                            \
      dst->n_buckets = 0;                                               \
      /* Copy only the used portion of keys and values */               \
      size_t copy_size = kh__kv_size_##name(src->size);                 \
      memcpy(dst->data, src->data, copy_size);                          \
    }                                                                   \
    else {                                                              \
      /* Regular hash table case */                                     \
      size_t data_size = kh__htable_size_##name(src->n_buckets);        \
      dst->data = mrb_realloc(mrb, dst->data, data_size);               \
      dst->size = src->size;                                            \
      dst->n_buckets = src->n_buckets;                                  \
      /* Copy the entire data block: [keys][vals][flags] */             \
      memcpy(dst->data, src->data, data_size);                          \
    }                                                                   \
  }


#define khash_t(name) kh_##name##_t

#define kh_init_size(name,mrb,size) kh_init_##name##_size(mrb,size)
#define kh_init(name,mrb) kh_init_##name(mrb)
#define kh_destroy(name, mrb, h) kh_destroy_##name(mrb, h)
#define kh_clear(name, mrb, h) kh_clear_##name(mrb, h)
#define kh_resize(name, mrb, h, s) kh_resize_##name(mrb, h, s)
#define kh_put(name, mrb, h, k) kh_put_##name(mrb, h, k, NULL)
#define kh_put2(name, mrb, h, k, r) kh_put_##name(mrb, h, k, r)
#define kh_get(name, mrb, h, k) kh_get_##name(mrb, h, k)
#define kh_del(name, mrb, h, k) kh_del_##name(mrb, h, k)
#define kh_copy(name, mrb, h) kh_copy_##name(mrb, h)
#define kh_init_data(name, mrb, h, size) kh_init_data_##name(mrb, h, size)
#define kh_destroy_data(name, mrb, h) kh_destroy_data_##name(mrb, h)
#define kh_replace(name, mrb, dst, src) kh_replace_##name(mrb, dst, src)

/* BREAKING CHANGE: Field access macros now require type name as first parameter
 * The macros keep their familiar names but now need the hash type name.
 *
 * MIGRATION: Add type name as first parameter:
 *   kh_key(h, x)      -> kh_key(typename, h, x)
 *   kh_val(h, x)      -> kh_val(typename, h, x)
 *   kh_exist(h, x)    -> kh_exist(typename, h, x)
 *   kh_value(h, x)    -> kh_value(typename, h, x)
 */

/* Type-aware access macros - same names, now with type parameter */
#define kh_exist(name, h, x) ((h)->n_buckets == 0 ? ((x) < (h)->size) : (!__ac_iseither(kh_flags_##name(h), (x))))
#define kh_key(name, h, x) (kh_keys_##name(h)[x])
#define kh_val(name, h, x) (kh_vals_##name(h)[x])
#define kh_value(name, h, x) (kh_vals_##name(h)[x])
#define kh_begin(h) (khint_t)(0)
#define kh_end(h) ((h)->n_buckets == 0 ? (h)->size : (h)->n_buckets)
#define kh_is_end(h, i) ((i) >= kh_end(h))
#define kh_size(h) ((h)->size)
#define kh_n_buckets(h) ((h)->n_buckets)

#define kh_int_hash_func(mrb,key) mrb_int_hash_func(mrb,key)
#define kh_int_hash_equal(mrb,a, b) (a == b)
#define kh_int64_hash_func(mrb,key) (khint_t)((key)>>33^(key)^(key)<<11)
#define kh_int64_hash_equal(mrb,a, b) (a == b)
static inline khint_t __ac_X31_hash_string(const char *s)
{
    khint_t h = *s;
    if (h) for (++s; *s; ++s) h = (h << 5) - h + *s;
    return h;
}
#define kh_str_hash_func(mrb,key) __ac_X31_hash_string(key)
#define kh_str_hash_equal(mrb,a, b) (strcmp(a, b) == 0)

typedef const char *kh_cstr_t;

MRB_END_DECL

/**
 * Macro for iterating over all elements in a khash.
 *
 * Usage:
 *   KHASH_FOREACH(typename, kh, k) {
 *     // k is the khiter_t iterator
 *     // Access the key with kh_key(typename, kh, k)
 *     // Access the value with kh_val(typename, kh, k) if applicable
 *     // Your code here
 *   }
 *
 * @param name The hash type name
 * @param kh   The khash to iterate over
 * @param k    The name to use for the khiter_t iterator variable
 */
/* BREAKING CHANGE: KHASH_FOREACH now requires type name as first parameter
 * OLD: KHASH_FOREACH(mrb, kh, k)
 * NEW: KHASH_FOREACH(name, kh, k)
 */
#define KHASH_FOREACH(name, kh, k) \
  if (kh) \
    for (khiter_t k = kh_begin(kh); !kh_is_end(kh, k); k++) \
      if (kh_exist(name, kh, k))


/* ======== mruby/endian.h ======== */
/**
** @file mruby/endian.h - detect endian-ness
**
** See Copyright Notice in mruby.h
*/

#define MRUBY_ENDIAN_H

#include <limits.h>

MRB_BEGIN_DECL

#if !defined(BYTE_ORDER) && defined(__BYTE_ORDER__)
# define BYTE_ORDER __BYTE_ORDER__
#endif
#if !defined(BIG_ENDIAN) && defined(__ORDER_BIG_ENDIAN__)
# define BIG_ENDIAN __ORDER_BIG_ENDIAN__
#endif
#if !defined(LITTLE_ENDIAN) && defined(__ORDER_LITTLE_ENDIAN__)
# define LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#endif

#ifdef BYTE_ORDER
# if BYTE_ORDER == BIG_ENDIAN
#  define littleendian 0
# elif BYTE_ORDER == LITTLE_ENDIAN
#  define littleendian 1
# endif
#endif
#ifndef littleendian
/* can't distinguish endian in compile time */
static inline int
check_little_endian(void)
{
  unsigned int n = 1;
  return (*(unsigned char*)&n == 1);
}
#  define littleendian check_little_endian()
#endif

MRB_END_DECL

/* mruby/presym.h - already included */
/* mruby/compile.h - already included */

#endif /* MRUBY_AMALGAM_H */
