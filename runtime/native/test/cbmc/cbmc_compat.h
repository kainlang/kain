/* CBMC compatibility shim for MinGW GCC preprocessed headers */
/* These types are GNU extensions that CBMC's MSVC-based parser can't handle */
#ifndef CBMC_COMPAT_H
#define CBMC_COMPAT_H

typedef char* __builtin_va_list;
typedef __builtin_va_list __gnuc_va_list;

/* MinGW-specific int128 types */
typedef long long __int128;
typedef unsigned long long __uint128;

#endif
