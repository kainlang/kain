/* Generated config.h for Kain-stripped Clang. */
#ifndef CLANG_CONFIG_H
#define CLANG_CONFIG_H

/* Bug report URL. */
#define BUG_REPORT_URL "https://github.com/llvm/llvm-project/issues/new"

/* Default to -fPIE and -fpie on Linux. */
#define CLANG_DEFAULT_PIE_ON_LINUX 0

/* Default linker to use. */
#define CLANG_DEFAULT_LINKER ""

/* Default C++ stdlib to use. */
#define CLANG_DEFAULT_CXX_STDLIB ""

/* Default runtime library to use. */
#define CLANG_DEFAULT_RTLIB ""

/* Default unwind library to use. */
#define CLANG_DEFAULT_UNWINDLIB ""

/* Default objcopy to use */
#define CLANG_DEFAULT_OBJCOPY ""

/* Default OpenMP runtime used by -fopenmp. */
#define CLANG_DEFAULT_OPENMP_RUNTIME ""

/* Default architecture for SystemZ. */
#define CLANG_SYSTEMZ_DEFAULT_ARCH ""

/* Multilib basename for libdir. */
#define CLANG_INSTALL_LIBDIR_BASENAME "lib"

/* Relative directory for resource files */
#define CLANG_RESOURCE_DIR "../lib/clang/19"

/* Directories clang will search for headers */
#define C_INCLUDE_DIRS "/usr/include"

/* Default <path> to all compiler invocations for --sysroot=<path>. */
#define DEFAULT_SYSROOT ""

/* Directory where gcc is installed. */
#define GCC_INSTALL_PREFIX ""

/* Define if we have libxml2 */
/* #undef CLANG_HAVE_LIBXML */

/* Define if we have sys/resource.h (rlimits) */
/* #undef CLANG_HAVE_RLIMITS */

/* Define if we have dlfcn.h */
/* #undef CLANG_HAVE_DLFCN_H */

/* Define if dladdr() is available on this platform. */
/* #undef CLANG_HAVE_DLADDR */

/* Spawn a new process clang.exe for the CC1 tool invocation */
#define CLANG_SPAWN_CC1 1

/* Whether CIR is built into Clang */
#define CLANG_ENABLE_CIR 0

/* Enable each functionality of modules */
#define CLANG_ENABLE_STATIC_ANALYZER 0
#define CLANG_ENABLE_OBJC_REWRITER 0

/* x86 relax relocations */
#define ENABLE_X86_RELAX_RELOCATIONS 0

/* Enable IEEE binary128 as default long double format on PowerPC Linux. */
#define PPC_LINUX_DEFAULT_IEEELONGDOUBLE 0

/* Enable the experimental new constant interpreter by default */
#define CLANG_USE_EXPERIMENTAL_CONST_INTERP 0

/* Linker version detected at compile time. */
#define HOST_LINK_VERSION "lld"
#endif
