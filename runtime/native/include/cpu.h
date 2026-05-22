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
int64_t abi_cpu_pause(void);
uint64_t abi_cpu_rdtsc(void);
uint64_t abi_cpu_cpuid_lane(int64_t leaf, int64_t subleaf, int64_t lane);
void abi_cpu_prefetch_read(const void* ptr, int64_t locality);
void abi_cpu_prefetch_write(const void* ptr, int64_t locality);
int64_t abi_cpu_logical_count(void);
int64_t abi_cpu_current_thread_id(void);
int64_t abi_cpu_set_current_thread_affinity(int64_t core_index);
int64_t abi_vm_page_size(void);
void* abi_vm_map(int64_t byte_count);
int64_t abi_vm_unmap(void* ptr, int64_t byte_count);
int64_t abi_vm_protect(void* ptr, int64_t byte_count, int64_t mode);

#ifdef __cplusplus
}
#endif

#endif /* CPU_H */
