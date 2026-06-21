/* ============================================================================
 *  c_ffi.c — C support function for Fortran c_ffi_call_hotloop benchmark
 *
 *  Provides the extern "C" c_add function that Fortran calls via ISO_C_BINDING.
 *  Compile with gfortran:
 *    gfortran -O3 -march=native bench.f95 c_ffi.c -o bench
 * ============================================================================
 */

/* Use long long to match Fortran's c_int64_t on all platforms */
long long c_add(long long a, long long b) {
    return a + b;
}
