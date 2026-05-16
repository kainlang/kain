#include "../../include/cpu.h"

#include <stddef.h>
#include <string.h>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#define CPU_X86 1
#else
#define CPU_X86 0
#endif

#if CPU_X86 && defined(_MSC_VER)
#include <intrin.h>
#elif CPU_X86 && (defined(__GNUC__) || defined(__clang__))
#include <cpuid.h>
#endif

static uint64_t g_kain_native_cpu_feature_mask = 0;
static int g_kain_native_cpu_feature_mask_initialized = 0;

static int abi_text_equals(const char* left, const char* right) {
    if (left == 0 || right == 0) {
        return 0;
    }
    return strcmp(left, right) == 0;
}

#if CPU_X86
static int abi_cpuid(int leaf, int subleaf, int out[4]) {
#if defined(_MSC_VER)
    __cpuidex(out, leaf, subleaf);
    return 1;
#elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;
    if (!__get_cpuid_count((unsigned int)leaf, (unsigned int)subleaf, &eax, &ebx, &ecx, &edx)) {
        out[0] = 0;
        out[1] = 0;
        out[2] = 0;
        out[3] = 0;
        return 0;
    }
    out[0] = (int)eax;
    out[1] = (int)ebx;
    out[2] = (int)ecx;
    out[3] = (int)edx;
    return 1;
#else
    (void)leaf;
    (void)subleaf;
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    return 0;
#endif
}

static uint64_t abi_xgetbv0(void) {
#if defined(_MSC_VER)
    return (uint64_t)_xgetbv(0);
#elif defined(__GNUC__) || defined(__clang__)
    unsigned int eax = 0;
    unsigned int edx = 0;
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
    return ((uint64_t)edx << 32) | (uint64_t)eax;
#else
    return 0;
#endif
}
#endif

static uint64_t abi_cpu_detect_feature_mask(void) {
#if CPU_X86
    int leaf1[4] = {0, 0, 0, 0};
    int leaf7[4] = {0, 0, 0, 0};
    uint64_t mask = 0;
    uint64_t xcr0 = 0;
    int osxsave = 0;
    int avx = 0;
    int ymm_state = 0;
    int zmm_state = 0;

    if (!abi_cpuid(1, 0, leaf1)) {
        return 0;
    }

    if (((uint32_t)leaf1[3] & (1u << 26)) != 0) {
        mask |= KAIN_CPU_FEATURE_X86_SSE2;
    }

    osxsave = (((uint32_t)leaf1[2] & (1u << 27)) != 0);
    avx = (((uint32_t)leaf1[2] & (1u << 28)) != 0);
    if (osxsave) {
        xcr0 = abi_xgetbv0();
    }
    ymm_state = ((xcr0 & 0x6u) == 0x6u);
    zmm_state = ((xcr0 & 0xE6u) == 0xE6u);

    if (avx && ymm_state) {
        mask |= KAIN_CPU_FEATURE_X86_AVX;
        if (((uint32_t)leaf1[2] & (1u << 12)) != 0) {
            mask |= KAIN_CPU_FEATURE_X86_FMA;
        }
    }

    if (abi_cpuid(7, 0, leaf7)) {
        uint32_t ebx = (uint32_t)leaf7[1];
        if ((mask & KAIN_CPU_FEATURE_X86_AVX) != 0 && ((ebx & (1u << 5)) != 0)) {
            mask |= KAIN_CPU_FEATURE_X86_AVX2;
        }
        if ((ebx & (1u << 8)) != 0) {
            mask |= KAIN_CPU_FEATURE_X86_BMI2;
        }
        if (zmm_state && ((ebx & (1u << 16)) != 0)) {
            mask |= KAIN_CPU_FEATURE_X86_AVX512F;
        }
        if (zmm_state && ((ebx & (1u << 17)) != 0)) {
            mask |= KAIN_CPU_FEATURE_X86_AVX512DQ;
        }
        if (zmm_state && ((ebx & (1u << 30)) != 0)) {
            mask |= KAIN_CPU_FEATURE_X86_AVX512BW;
        }
        if (zmm_state && ((ebx & (1u << 31)) != 0)) {
            mask |= KAIN_CPU_FEATURE_X86_AVX512VL;
        }
    }

    return mask;
#else
    return 0;
#endif
}

uint64_t abi_cpu_feature_mask(void) {
    if (!g_kain_native_cpu_feature_mask_initialized) {
        g_kain_native_cpu_feature_mask = abi_cpu_detect_feature_mask();
        g_kain_native_cpu_feature_mask_initialized = 1;
    }
    return g_kain_native_cpu_feature_mask;
}

uint64_t abi_cpu_feature_fingerprint(void) {
    uint64_t mask = abi_cpu_feature_mask();
    uint64_t mixed = mask + 0x9e3779b97f4a7c15ull;
    mixed ^= mixed >> 30;
    mixed *= 0xbf58476d1ce4e5b9ull;
    mixed ^= mixed >> 27;
    mixed *= 0x94d049bb133111ebull;
    mixed ^= mixed >> 31;
    return mixed;
}

uint64_t abi_cpu_capability_mask_for_key(const char* capability_key) {
    if (capability_key == 0) {
        return 0;
    }

    if (abi_text_equals(capability_key, "cpu.x86.sse2") ||
        abi_text_equals(capability_key, "x86.sse2") ||
        abi_text_equals(capability_key, "sse2")) {
        return KAIN_CPU_FEATURE_X86_SSE2;
    }
    if (abi_text_equals(capability_key, "cpu.x86.avx") ||
        abi_text_equals(capability_key, "x86.avx") ||
        abi_text_equals(capability_key, "avx")) {
        return KAIN_CPU_FEATURE_X86_AVX;
    }
    if (abi_text_equals(capability_key, "cpu.x86.avx2") ||
        abi_text_equals(capability_key, "x86.avx2") ||
        abi_text_equals(capability_key, "avx2")) {
        return KAIN_CPU_FEATURE_X86_AVX2;
    }
    if (abi_text_equals(capability_key, "cpu.x86.avx512f") ||
        abi_text_equals(capability_key, "x86.avx512f") ||
        abi_text_equals(capability_key, "avx512f")) {
        return KAIN_CPU_FEATURE_X86_AVX512F;
    }
    if (abi_text_equals(capability_key, "cpu.x86.avx512dq") ||
        abi_text_equals(capability_key, "x86.avx512dq") ||
        abi_text_equals(capability_key, "avx512dq")) {
        return KAIN_CPU_FEATURE_X86_AVX512DQ;
    }
    if (abi_text_equals(capability_key, "cpu.x86.avx512bw") ||
        abi_text_equals(capability_key, "x86.avx512bw") ||
        abi_text_equals(capability_key, "avx512bw")) {
        return KAIN_CPU_FEATURE_X86_AVX512BW;
    }
    if (abi_text_equals(capability_key, "cpu.x86.avx512vl") ||
        abi_text_equals(capability_key, "x86.avx512vl") ||
        abi_text_equals(capability_key, "avx512vl")) {
        return KAIN_CPU_FEATURE_X86_AVX512VL;
    }
    if (abi_text_equals(capability_key, "cpu.x86.avx512") ||
        abi_text_equals(capability_key, "x86.avx512") ||
        abi_text_equals(capability_key, "avx512")) {
        return KAIN_CPU_FEATURE_X86_AVX512F;
    }
    if (abi_text_equals(capability_key, "cpu.x86.fma") ||
        abi_text_equals(capability_key, "x86.fma") ||
        abi_text_equals(capability_key, "fma")) {
        return KAIN_CPU_FEATURE_X86_FMA;
    }
    if (abi_text_equals(capability_key, "cpu.x86.bmi2") ||
        abi_text_equals(capability_key, "x86.bmi2") ||
        abi_text_equals(capability_key, "bmi2")) {
        return KAIN_CPU_FEATURE_X86_BMI2;
    }

    return 0;
}

int64_t abi_cpu_has_capability(const char* capability_key) {
    uint64_t required = abi_cpu_capability_mask_for_key(capability_key);
    uint64_t present;
    if (required == 0) {
        return 0;
    }
    present = abi_cpu_feature_mask();
    return ((present & required) == required) ? 1 : 0;
}
