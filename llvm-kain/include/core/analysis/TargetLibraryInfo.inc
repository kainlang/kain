//===-- TargetLibraryInfo.inc - Generated from TargetLibraryInfo.td --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Hand-adapted stub for the llvm-kain stripped build.
// Minimal LibFunc enum + empty tables to unblock compilation.
//
//===----------------------------------------------------------------------===//

#ifdef GET_TARGET_LIBRARY_INFO_ENUM
// 'extern const unsigned' for values, forward-compatible with enum LibFunc : unsigned
enum LibFunc : unsigned {
  NotLibFunc = 0,
  LibFunc_acos, LibFunc_acosf, LibFunc_acosl,
  LibFunc_asin, LibFunc_asinf, LibFunc_asinl,
  LibFunc_atan, LibFunc_atan2, LibFunc_atan2f, LibFunc_atan2l, LibFunc_atanf, LibFunc_atanl,
  LibFunc_bcmp,
  LibFunc_copysign, LibFunc_copysignf, LibFunc_copysignl,
  LibFunc_cos, LibFunc_cosf, LibFunc_cosh, LibFunc_coshf, LibFunc_coshl, LibFunc_cosl,
  LibFunc_exp10, LibFunc_exp10f, LibFunc_exp10l,
  LibFunc_exp2, LibFunc_exp2f, LibFunc_exp2l,
  LibFunc_expf, LibFunc_expl,
  LibFunc_fabs, LibFunc_fabsf, LibFunc_fabsl,
  LibFunc_ldexp, LibFunc_ldexpf, LibFunc_ldexpl,
  LibFunc_log10, LibFunc_log10f, LibFunc_log10l,
  LibFunc_log1p, LibFunc_log1pf, LibFunc_log1pl,
  LibFunc_log2, LibFunc_log2f, LibFunc_log2l,
  LibFunc_logb, LibFunc_logbf, LibFunc_logbl,
  LibFunc_logf, LibFunc_logl,
  LibFunc_memchr, LibFunc_memccpy, LibFunc_memcmp, LibFunc_memcpy, LibFunc_memcpy_chk, LibFunc_memmove, LibFunc_memmove_chk, LibFunc_mempcpy, LibFunc_mempcpy_chk,
  LibFunc_memset, LibFunc_memset_chk,
  LibFunc_pow, LibFunc_powf, LibFunc_powl,
  LibFunc_memset_pattern4, LibFunc_memset_pattern8, LibFunc_memset_pattern16,
  LibFunc_sin, LibFunc_sinf, LibFunc_sinh, LibFunc_sinhf, LibFunc_sinhl, LibFunc_sinl,
  LibFunc_sqrt, LibFunc_sqrt_finite, LibFunc_sqrtf, LibFunc_sqrtf_finite, LibFunc_sqrtl, LibFunc_sqrtl_finite,
  LibFunc_stpcpy, LibFunc_strcat, LibFunc_strcmp, LibFunc_strcpy, LibFunc_strlen, LibFunc_strncat, LibFunc_strncpy, LibFunc_strnlen, LibFunc_strstr,
  LibFunc_tan, LibFunc_tanf, LibFunc_tanh, LibFunc_tanhf, LibFunc_tanhl, LibFunc_tanl,
  // C++ operator new/delete
  LibFunc_ZdaPv, LibFunc_ZdaPvRKSt9nothrow_t, LibFunc_ZdaPvSt11align_val_t,
  LibFunc_ZdaPvSt11align_val_tRKSt9nothrow_t, LibFunc_ZdaPvj, LibFunc_ZdaPvjSt11align_val_t,
  LibFunc_ZdaPvm, LibFunc_ZdaPvmSt11align_val_t, LibFunc_ZdlPv,
  LibFunc_ZdlPvRKSt9nothrow_t, LibFunc_ZdlPvSt11align_val_t,
  LibFunc_ZdlPvSt11align_val_tRKSt9nothrow_t, LibFunc_ZdlPvj, LibFunc_ZdlPvjSt11align_val_t,
  LibFunc_ZdlPvm, LibFunc_ZdlPvmSt11align_val_t, LibFunc_Znaj,
  LibFunc_ZnajRKSt9nothrow_t, LibFunc_ZnajSt11align_val_t,
  LibFunc_ZnajSt11align_val_tRKSt9nothrow_t, LibFunc_Znam,
  LibFunc_Znam12__hot_cold_t, LibFunc_ZnamRKSt9nothrow_t,
  LibFunc_ZnamRKSt9nothrow_t12__hot_cold_t, LibFunc_ZnamSt11align_val_t,
  LibFunc_ZnamSt11align_val_t12__hot_cold_t, LibFunc_ZnamSt11align_val_tRKSt9nothrow_t,
  LibFunc_ZnamSt11align_val_tRKSt9nothrow_t12__hot_cold_t, LibFunc_Znwj,
  LibFunc_ZnwjRKSt9nothrow_t, LibFunc_ZnwjSt11align_val_t,
  LibFunc_ZnwjSt11align_val_tRKSt9nothrow_t, LibFunc_Znwm,
  LibFunc_Znwm12__hot_cold_t, LibFunc_ZnwmRKSt9nothrow_t,
  LibFunc_ZnwmRKSt9nothrow_t12__hot_cold_t, LibFunc_ZnwmSt11align_val_t,
  LibFunc_ZnwmSt11align_val_t12__hot_cold_t, LibFunc_ZnwmSt11align_val_tRKSt9nothrow_t,
  LibFunc_ZnwmSt11align_val_tRKSt9nothrow_t12__hot_cold_t,
  // MSVC operator new/delete
  LibFunc_msvc_delete_array_ptr32, LibFunc_msvc_delete_array_ptr32_int,
  LibFunc_msvc_delete_array_ptr32_nothrow, LibFunc_msvc_delete_array_ptr64,
  LibFunc_msvc_delete_array_ptr64_longlong, LibFunc_msvc_delete_array_ptr64_nothrow,
  LibFunc_msvc_delete_ptr32, LibFunc_msvc_delete_ptr32_int,
  LibFunc_msvc_delete_ptr32_nothrow, LibFunc_msvc_delete_ptr64,
  LibFunc_msvc_delete_ptr64_longlong, LibFunc_msvc_delete_ptr64_nothrow,
  LibFunc_msvc_new_array_int, LibFunc_msvc_new_array_int_nothrow,
  LibFunc_msvc_new_array_longlong, LibFunc_msvc_new_array_longlong_nothrow,
  LibFunc_msvc_new_int, LibFunc_msvc_new_int_nothrow,
  LibFunc_msvc_new_longlong, LibFunc_msvc_new_longlong_nothrow,
  LibFunc_Exit, LibFunc_abort, LibFunc_abs, LibFunc_access, LibFunc_acos_finite, LibFunc_acosf_finite, LibFunc_acosh, LibFunc_acosh_finite, LibFunc_acoshf, LibFunc_acoshf_finite, LibFunc_acoshl, LibFunc_acoshl_finite, LibFunc_acosl_finite, LibFunc_aligned_alloc, LibFunc_asin_finite, LibFunc_asinf_finite, LibFunc_asinh, LibFunc_asinhf, LibFunc_asinhl, LibFunc_asinl_finite, LibFunc_atan2_finite, LibFunc_atan2f_finite, LibFunc_atan2l_finite, LibFunc_atanh, LibFunc_atanh_finite, LibFunc_atanhf, LibFunc_atanhf_finite, LibFunc_atanhl, LibFunc_atanhl_finite, LibFunc_atexit, LibFunc_atof, LibFunc_atoi, LibFunc_atol, LibFunc_atoll, LibFunc_atomic_load, LibFunc_atomic_store, LibFunc_bcopy, LibFunc_bzero, LibFunc_cabs, LibFunc_cabsf, LibFunc_cabsl, LibFunc_calloc, LibFunc_cbrt, LibFunc_cbrtf, LibFunc_cbrtl, LibFunc_ceil, LibFunc_ceilf, LibFunc_ceill, LibFunc_chmod, LibFunc_chown, LibFunc_clearerr, LibFunc_closedir, LibFunc_cosh_finite, LibFunc_coshf_finite, LibFunc_coshl_finite, LibFunc_cospi, LibFunc_cospif, LibFunc_ctermid, LibFunc_cxa_atexit, LibFunc_cxa_throw, LibFunc_dunder_isoc99_scanf, LibFunc_dunder_isoc99_sscanf, LibFunc_dunder_strdup, LibFunc_dunder_strndup, LibFunc_dunder_strtok_r, LibFunc_erf, LibFunc_erff, LibFunc_erfl, LibFunc_execl, LibFunc_execle, LibFunc_execlp, LibFunc_execv, LibFunc_execvP, LibFunc_execve, LibFunc_execvp, LibFunc_execvpe, LibFunc_exit, LibFunc_exp, LibFunc_exp10_finite, LibFunc_exp10f_finite, LibFunc_exp10l_finite, LibFunc_exp2_finite, LibFunc_exp2f_finite, LibFunc_exp2l_finite, LibFunc_exp_finite, LibFunc_expf_finite, LibFunc_expl_finite, LibFunc_expm1, LibFunc_expm1f, LibFunc_expm1l, LibFunc_fclose, LibFunc_fdim, LibFunc_fdimf, LibFunc_fdiml, LibFunc_fdopen, LibFunc_feof, LibFunc_ferror, LibFunc_fflush, LibFunc_ffs, LibFunc_ffsl, LibFunc_ffsll, LibFunc_fgetc, LibFunc_fgetc_unlocked, LibFunc_fgetpos, LibFunc_fgets, LibFunc_fgets_unlocked, LibFunc_fileno, LibFunc_fiprintf, LibFunc_flockfile, LibFunc_floor, LibFunc_floorf, LibFunc_floorl, LibFunc_fls, LibFunc_flsl, LibFunc_flsll, LibFunc_fmax, LibFunc_fmaxf, LibFunc_fmaximum_num, LibFunc_fmaximum_numf, LibFunc_fmaximum_numl, LibFunc_fmaxl, LibFunc_fmin, LibFunc_fminf, LibFunc_fminimum_num, LibFunc_fminimum_numf, LibFunc_fminimum_numl, LibFunc_fminl, LibFunc_fmod, LibFunc_fmodf, LibFunc_fmodl, LibFunc_fopen, LibFunc_fopen64, LibFunc_fork, LibFunc_fprintf, LibFunc_fputc, LibFunc_fputc_unlocked, LibFunc_fputs, LibFunc_fputs_unlocked, LibFunc_fread, LibFunc_fread_unlocked, LibFunc_free, LibFunc_frexp, LibFunc_frexpf, LibFunc_frexpl, LibFunc_fscanf, LibFunc_fseek, LibFunc_fseeko, LibFunc_fseeko64, LibFunc_fsetpos, LibFunc_fstat, LibFunc_fstat64, LibFunc_fstatvfs, LibFunc_fstatvfs64, LibFunc_ftell, LibFunc_ftello, LibFunc_ftello64, LibFunc_ftrylockfile, LibFunc_funlockfile, LibFunc_fwrite, LibFunc_fwrite_unlocked, LibFunc_getc, LibFunc_getc_unlocked, LibFunc_getchar, LibFunc_getchar_unlocked, LibFunc_getenv, LibFunc_getitimer, LibFunc_getlogin_r, LibFunc_getpwnam, LibFunc_gets, LibFunc_gettimeofday, LibFunc_htonl, LibFunc_htons, LibFunc_hypot, LibFunc_hypotf, LibFunc_hypotl, LibFunc_ilogb, LibFunc_ilogbf, LibFunc_ilogbl, LibFunc_iprintf, LibFunc_isascii, LibFunc_isdigit, LibFunc_labs, LibFunc_lchown, LibFunc_llabs, LibFunc_log, LibFunc_log10_finite, LibFunc_log10f_finite, LibFunc_log10l_finite, LibFunc_log2_finite, LibFunc_log2f_finite, LibFunc_log2l_finite, LibFunc_log_finite, LibFunc_logf_finite, LibFunc_logl_finite, LibFunc_lstat, LibFunc_lstat64, LibFunc_malloc, LibFunc_memalign, LibFunc_memccpy_chk, LibFunc_memrchr, LibFunc_mkdir, LibFunc_mktime, LibFunc_modf, LibFunc_modff, LibFunc_modfl, LibFunc_nan, LibFunc_nanf, LibFunc_nanl, LibFunc_nearbyint, LibFunc_nearbyintf, LibFunc_nearbyintl, LibFunc_nextafter, LibFunc_nextafterf, LibFunc_nextafterl, LibFunc_nexttoward, LibFunc_nexttowardf, LibFunc_nexttowardl, LibFunc_ntohl, LibFunc_ntohs, LibFunc_nvvm_reflect, LibFunc_open, LibFunc_open64, LibFunc_opendir, LibFunc_pclose, LibFunc_perror, LibFunc_popen, LibFunc_posix_memalign, LibFunc_pow_finite, LibFunc_powf_finite, LibFunc_powl_finite, LibFunc_pread, LibFunc_printf, LibFunc_putc, LibFunc_putc_unlocked, LibFunc_putchar, LibFunc_putchar_unlocked, LibFunc_puts, LibFunc_pvalloc, LibFunc_pwrite, LibFunc_qsort, LibFunc_read, LibFunc_readlink, LibFunc_realloc, LibFunc_reallocarray, LibFunc_reallocf, LibFunc_realpath, LibFunc_remainder, LibFunc_remainderf, LibFunc_remainderl, LibFunc_remove, LibFunc_remquo, LibFunc_remquof, LibFunc_remquol, LibFunc_rename, LibFunc_rewind, LibFunc_rint, LibFunc_rintf, LibFunc_rintl, LibFunc_rmdir, LibFunc_round, LibFunc_roundeven, LibFunc_roundevenf, LibFunc_roundevenl, LibFunc_roundf, LibFunc_roundl, LibFunc_scalbln, LibFunc_scalblnf, LibFunc_scalblnl, LibFunc_scalbn, LibFunc_scalbnf, LibFunc_scalbnl, LibFunc_scanf, LibFunc_setbuf, LibFunc_setitimer, LibFunc_setvbuf, LibFunc_sincos, LibFunc_sincosf, LibFunc_sincosl, LibFunc_sincospi_stret, LibFunc_sincospif_stret, LibFunc_sinh_finite, LibFunc_sinhf_finite, LibFunc_sinhl_finite, LibFunc_sinpi, LibFunc_sinpif, LibFunc_siprintf, LibFunc_size_returning_new, LibFunc_size_returning_new_aligned, LibFunc_size_returning_new_aligned_hot_cold, LibFunc_size_returning_new_hot_cold, LibFunc_small_fprintf, LibFunc_small_printf, LibFunc_small_sprintf, LibFunc_snprintf, LibFunc_snprintf_chk, LibFunc_sprintf, LibFunc_sprintf_chk, LibFunc_sscanf, LibFunc_stat, LibFunc_stat64, LibFunc_statvfs, LibFunc_statvfs64, LibFunc_stpcpy_chk, LibFunc_stpncpy, LibFunc_stpncpy_chk, LibFunc_strcasecmp, LibFunc_strcat_chk, LibFunc_strchr, LibFunc_strcoll, LibFunc_strcpy_chk, LibFunc_strcspn, LibFunc_strdup, LibFunc_strlcat, LibFunc_strlcat_chk, LibFunc_strlcpy, LibFunc_strlcpy_chk, LibFunc_strlen_chk, LibFunc_strncasecmp, LibFunc_strncat_chk, LibFunc_strncmp, LibFunc_strncpy_chk, LibFunc_strndup, LibFunc_strpbrk, LibFunc_strrchr, LibFunc_strspn, LibFunc_strtod, LibFunc_strtof, LibFunc_strtok, LibFunc_strtok_r, LibFunc_strtol, LibFunc_strtold, LibFunc_strtoll, LibFunc_strtoul, LibFunc_strtoull, LibFunc_strxfrm, LibFunc_system, LibFunc_terminate, LibFunc_tgamma, LibFunc_tgammaf, LibFunc_tgammal, LibFunc_times, LibFunc_tmpfile, LibFunc_tmpfile64, LibFunc_toascii, LibFunc_trunc, LibFunc_truncf, LibFunc_truncl, LibFunc_uname, LibFunc_under_IO_getc, LibFunc_under_IO_putc, LibFunc_ungetc, LibFunc_unlink, LibFunc_unsetenv, LibFunc_utime, LibFunc_utimes, LibFunc_valloc, LibFunc_vec_calloc, LibFunc_vec_free, LibFunc_vec_malloc, LibFunc_vec_realloc, LibFunc_vfprintf, LibFunc_vfscanf, LibFunc_vprintf, LibFunc_vscanf, LibFunc_vsnprintf, LibFunc_vsnprintf_chk, LibFunc_vsprintf, LibFunc_vsprintf_chk, LibFunc_vsscanf, LibFunc_wcslen, LibFunc_write,
  NumLibFuncs
};
#endif

#ifdef GET_TARGET_LIBRARY_INFO_IMPL_DECL
  // Inside TargetLibraryInfoImpl, LibFunc resolves to the namespace-scope enum.
  // The 'using' directive ensures the compiler treats injected class name
  // 'TargetLibraryInfoImpl::LibFunc' as an alias for 'llvm::LibFunc'.
  typedef ::llvm::LibFunc LibFunc;
  static constexpr unsigned StandardNamesOffsets[NumLibFuncs] = {};
  static constexpr unsigned StandardNamesSizeTable[NumLibFuncs] = {};
  static constexpr llvm::StringTable StandardNamesStrTable = "";
#endif

#ifdef GET_TARGET_LIBRARY_INFO_STRING_TABLE
#endif

#ifdef GET_TARGET_LIBRARY_INFO_SIGNATURE_TABLE
enum FuncArgTypeID {
  Void = 0, Bool = 1, Int8 = 2, Int16 = 3, Int32 = 4, Int64 = 5,
  Int = 6, IntX = 7, IntPlus = 8, SizeT = 9, SSizeT = 10,
  Ptr = 11, Flt = 12, Dbl = 13, LDbl = 14, Struct = 15,
  NoFuncArgType = 16, Ellip = 17, Same = 18,
  Long = 19, LLong = 20, Floating = 21
};

static constexpr FuncArgTypeID SignatureTable[NumLibFuncs][3] = {};
static constexpr unsigned SignatureOffset[NumLibFuncs + 1] = {};
#endif
