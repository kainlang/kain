#include "../../include/cpu.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

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

int64_t abi_cpu_pause(void) {
#if CPU_X86 && defined(_MSC_VER)
    _mm_pause();
#elif CPU_X86 && (defined(__GNUC__) || defined(__clang__))
    __asm__ volatile("pause");
#endif
    return 0;
}

uint64_t abi_cpu_rdtsc(void) {
#if CPU_X86 && defined(_MSC_VER)
    return (uint64_t)__rdtsc();
#elif CPU_X86 && (defined(__GNUC__) || defined(__clang__))
    unsigned int lo = 0;
    unsigned int hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
#else
    return 0;
#endif
}

uint64_t abi_cpu_cpuid_lane(int64_t leaf, int64_t subleaf, int64_t lane) {
#if CPU_X86
    int out[4] = {0, 0, 0, 0};
    if (lane < 0 || lane > 3) {
        return 0;
    }
    if (!abi_cpuid((int)leaf, (int)subleaf, out)) {
        return 0;
    }
    return (uint64_t)(uint32_t)out[lane];
#else
    (void)leaf;
    (void)subleaf;
    (void)lane;
    return 0;
#endif
}

static int abi_prefetch_locality(int64_t locality) {
    if (locality <= 0) {
        return 0;
    }
    if (locality == 1) {
        return 1;
    }
    if (locality == 2) {
        return 2;
    }
    return 3;
}

void abi_cpu_prefetch_read(const void* ptr, int64_t locality) {
#if defined(__GNUC__) || defined(__clang__)
    switch (abi_prefetch_locality(locality)) {
    case 0:
        __builtin_prefetch(ptr, 0, 0);
        break;
    case 1:
        __builtin_prefetch(ptr, 0, 1);
        break;
    case 2:
        __builtin_prefetch(ptr, 0, 2);
        break;
    default:
        __builtin_prefetch(ptr, 0, 3);
        break;
    }
#elif defined(_MSC_VER) && CPU_X86
    (void)locality;
    _mm_prefetch((const char*)ptr, _MM_HINT_T0);
#else
    (void)ptr;
    (void)locality;
#endif
}

void abi_cpu_prefetch_write(const void* ptr, int64_t locality) {
#if defined(__GNUC__) || defined(__clang__)
    switch (abi_prefetch_locality(locality)) {
    case 0:
        __builtin_prefetch(ptr, 1, 0);
        break;
    case 1:
        __builtin_prefetch(ptr, 1, 1);
        break;
    case 2:
        __builtin_prefetch(ptr, 1, 2);
        break;
    default:
        __builtin_prefetch(ptr, 1, 3);
        break;
    }
#elif defined(_MSC_VER) && CPU_X86
    (void)locality;
    _mm_prefetch((const char*)ptr, _MM_HINT_T0);
#else
    (void)ptr;
    (void)locality;
#endif
}

int64_t abi_cpu_logical_count(void) {
#if defined(_WIN32)
    DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return count == 0 ? 1 : (int64_t)count;
#elif defined(_SC_NPROCESSORS_ONLN)
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (int64_t)count : 1;
#else
    return 1;
#endif
}

int64_t abi_cpu_current_thread_id(void) {
#if defined(_WIN32)
    return (int64_t)GetCurrentThreadId();
#elif defined(__unix__) || defined(__APPLE__)
    return (int64_t)(uintptr_t)pthread_self();
#else
    return 0;
#endif
}

int64_t abi_cpu_set_current_thread_affinity(int64_t core_index) {
    if (core_index < 0) {
        return -1;
    }
#if defined(_WIN32)
    if (core_index >= (int64_t)(sizeof(DWORD_PTR) * 8u)) {
        return -1;
    }
    DWORD_PTR mask = ((DWORD_PTR)1u) << (DWORD_PTR)core_index;
    return SetThreadAffinityMask(GetCurrentThread(), mask) == 0 ? -1 : 0;
#elif defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((int)core_index, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0 ? 0 : -1;
#else
    (void)core_index;
    return -1;
#endif
}

int64_t abi_vm_page_size(void) {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize > 0 ? (int64_t)info.dwPageSize : 4096;
#elif defined(_SC_PAGESIZE)
    long page_size = sysconf(_SC_PAGESIZE);
    return page_size > 0 ? (int64_t)page_size : 4096;
#else
    return 4096;
#endif
}

void* abi_vm_map(int64_t byte_count) {
    if (byte_count <= 0) {
        return 0;
    }
#if defined(_WIN32)
    return VirtualAlloc(0, (SIZE_T)byte_count, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#elif defined(MAP_PRIVATE) && defined(MAP_ANON)
    void* ptr = mmap(0, (size_t)byte_count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return ptr == MAP_FAILED ? 0 : ptr;
#elif defined(MAP_PRIVATE) && defined(MAP_ANONYMOUS)
    void* ptr = mmap(0, (size_t)byte_count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return ptr == MAP_FAILED ? 0 : ptr;
#else
    return malloc((size_t)byte_count);
#endif
}

int64_t abi_vm_unmap(void* ptr, int64_t byte_count) {
    if (ptr == 0) {
        return 0;
    }
#if defined(_WIN32)
    (void)byte_count;
    return VirtualFree(ptr, 0, MEM_RELEASE) != 0 ? 0 : -1;
#elif defined(MAP_PRIVATE) && (defined(MAP_ANON) || defined(MAP_ANONYMOUS))
    if (byte_count <= 0) {
        return -1;
    }
    return munmap(ptr, (size_t)byte_count) == 0 ? 0 : -1;
#else
    (void)byte_count;
    free(ptr);
    return 0;
#endif
}

static int abi_vm_protection_mode(int64_t mode) {
#if defined(_WIN32)
    switch (mode) {
    case 0:
        return PAGE_NOACCESS;
    case 1:
        return PAGE_READONLY;
    case 2:
        return PAGE_READWRITE;
    case 3:
        return PAGE_EXECUTE_READ;
    case 4:
        return PAGE_EXECUTE_READWRITE;
    default:
        return PAGE_READWRITE;
    }
#elif defined(PROT_NONE)
    switch (mode) {
    case 0:
        return PROT_NONE;
    case 1:
        return PROT_READ;
    case 2:
        return PROT_READ | PROT_WRITE;
    case 3:
        return PROT_READ | PROT_EXEC;
    case 4:
        return PROT_READ | PROT_WRITE | PROT_EXEC;
    default:
        return PROT_READ | PROT_WRITE;
    }
#else
    (void)mode;
    return 0;
#endif
}

int64_t abi_vm_protect(void* ptr, int64_t byte_count, int64_t mode) {
    if (ptr == 0 || byte_count <= 0) {
        return -1;
    }
#if defined(_WIN32)
    DWORD old_protect = 0;
    return VirtualProtect(ptr, (SIZE_T)byte_count, (DWORD)abi_vm_protection_mode(mode), &old_protect) != 0 ? 0 : -1;
#elif defined(PROT_NONE)
    return mprotect(ptr, (size_t)byte_count, abi_vm_protection_mode(mode)) == 0 ? 0 : -1;
#else
    (void)ptr;
    (void)byte_count;
    (void)mode;
    return -1;
#endif
}
