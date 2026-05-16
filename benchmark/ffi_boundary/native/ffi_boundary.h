#ifndef FFI_BOUNDARY_H
#define FFI_BOUNDARY_H

#include <stdint.h>

#ifdef _WIN32
#define FFI_BOUNDARY_EXPORT __declspec(dllexport)
#define FFI_BOUNDARY_NOINLINE __declspec(noinline)
#else
#define FFI_BOUNDARY_EXPORT __attribute__((visibility("default")))
#define FFI_BOUNDARY_NOINLINE __attribute__((noinline))
#endif

#ifdef __cplusplus
extern "C" {
#endif

FFI_BOUNDARY_EXPORT FFI_BOUNDARY_NOINLINE int64_t ffi_boundary_mix(int64_t value, int64_t salt);

#ifdef __cplusplus
}
#endif

#endif
