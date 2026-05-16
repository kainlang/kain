#ifndef CPU_H
#define CPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_CPU_FEATURE_X86_SSE2 (1ull << 0)
#define KAIN_CPU_FEATURE_X86_AVX (1ull << 1)
#define KAIN_CPU_FEATURE_X86_AVX2 (1ull << 2)
#define KAIN_CPU_FEATURE_X86_AVX512F (1ull << 3)
#define KAIN_CPU_FEATURE_X86_AVX512DQ (1ull << 4)
#define KAIN_CPU_FEATURE_X86_AVX512BW (1ull << 5)
#define KAIN_CPU_FEATURE_X86_AVX512VL (1ull << 6)
#define KAIN_CPU_FEATURE_X86_FMA (1ull << 7)
#define KAIN_CPU_FEATURE_X86_BMI2 (1ull << 8)

uint64_t abi_cpu_feature_mask(void);
uint64_t abi_cpu_feature_fingerprint(void);
uint64_t abi_cpu_capability_mask_for_key(const char* capability_key);
int64_t abi_cpu_has_capability(const char* capability_key);

#ifdef __cplusplus
}
#endif

#endif /* CPU_H */
