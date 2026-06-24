#include "../../include/cpu.h"

#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/*
 * 64-bit token signature for capability key dispatch.
 * Packs (len, first, second, second_last, last) into 40 bits at bits 24-63.
 * XOR is equivalent to OR when fields don't overlap; using XOR matches the
 * Z3 proof semantics.
 *
 * Proof: runtime/native/src/core/z3/proofs/native-cpu-capability-token-signatures-are-collision-free.yaml
 */
#define CPU_CAP_SIG64(len, first, second, second_last, last) \
    (((uint64_t)(len) << 56) ^ ((uint64_t)(first) << 48) ^ \
     ((uint64_t)(second) << 40) ^ ((uint64_t)(second_last) << 32) ^ \
     ((uint64_t)(last) << 24))

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

/*
 * Compute the extended 5-field token signature for a capability key string.
 * For keys shorter than 2 bytes, missing fields are zero-padded.
 */
static uint64_t cpu_cap_sig64(const char* key) {
    size_t len = strlen(key);
    unsigned char first = len > 0 ? (unsigned char)key[0] : 0;
    unsigned char second = len > 1 ? (unsigned char)key[1] : 0;
    unsigned char second_last = len > 1 ? (unsigned char)key[len - 2] : 0;
    unsigned char last = len > 0 ? (unsigned char)key[len - 1] : 0;
    return (((uint64_t)len << 56) ^
            ((uint64_t)first << 48) ^
            ((uint64_t)second << 40) ^
            ((uint64_t)second_last << 32) ^
            ((uint64_t)last << 24));
}

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

/*
 * Proof: runtime/native/src/core/z3/proofs/native-cpu-capability-token-signatures-are-collision-free.yaml
 *
 * The Z3 proof verifies that all 30 capability keys (10 feature groups x 3
 * alias forms each) produce unique extended 5-param sig64 values.  We dispatch
 * by computed sig64 first, then strcmp-verify only the matched group's alias
 * strings as a defensive measure against novel keys that happen to share the
 * same (len, first, second, second_last, last) tuple.
 */
uint64_t abi_cpu_capability_mask_for_key(const char* capability_key) {
    uint64_t sig;

    if (capability_key == 0) {
        return 0;
    }

    sig = cpu_cap_sig64(capability_key);

    switch (sig) {
    case CPU_CAP_SIG64(12, 'c', 'p', 'e', '2'):
    case CPU_CAP_SIG64(8,  'x', '8', 'e', '2'):
    case CPU_CAP_SIG64(4,  's', 's', 'e', '2'):
        if (abi_text_equals(capability_key, "cpu.x86.sse2") ||
            abi_text_equals(capability_key, "x86.sse2") ||
            abi_text_equals(capability_key, "sse2"))
            return KAIN_CPU_FEATURE_X86_SSE2;
        return 0;

    case CPU_CAP_SIG64(11, 'c', 'p', 'v', 'x'):
    case CPU_CAP_SIG64(7,  'x', '8', 'v', 'x'):
    case CPU_CAP_SIG64(3,  'a', 'v', 'v', 'x'):
        if (abi_text_equals(capability_key, "cpu.x86.avx") ||
            abi_text_equals(capability_key, "x86.avx") ||
            abi_text_equals(capability_key, "avx"))
            return KAIN_CPU_FEATURE_X86_AVX;
        return 0;

    case CPU_CAP_SIG64(12, 'c', 'p', 'x', '2'):
    case CPU_CAP_SIG64(8,  'x', '8', 'x', '2'):
    case CPU_CAP_SIG64(4,  'a', 'v', 'x', '2'):
        if (abi_text_equals(capability_key, "cpu.x86.avx2") ||
            abi_text_equals(capability_key, "x86.avx2") ||
            abi_text_equals(capability_key, "avx2"))
            return KAIN_CPU_FEATURE_X86_AVX2;
        return 0;

    case CPU_CAP_SIG64(15, 'c', 'p', '2', 'f'):
    case CPU_CAP_SIG64(11, 'x', '8', '2', 'f'):
    case CPU_CAP_SIG64(7,  'a', 'v', '2', 'f'):
        if (abi_text_equals(capability_key, "cpu.x86.avx512f") ||
            abi_text_equals(capability_key, "x86.avx512f") ||
            abi_text_equals(capability_key, "avx512f"))
            return KAIN_CPU_FEATURE_X86_AVX512F;
        return 0;

    case CPU_CAP_SIG64(16, 'c', 'p', 'd', 'q'):
    case CPU_CAP_SIG64(12, 'x', '8', 'd', 'q'):
    case CPU_CAP_SIG64(8,  'a', 'v', 'd', 'q'):
        if (abi_text_equals(capability_key, "cpu.x86.avx512dq") ||
            abi_text_equals(capability_key, "x86.avx512dq") ||
            abi_text_equals(capability_key, "avx512dq"))
            return KAIN_CPU_FEATURE_X86_AVX512DQ;
        return 0;

    case CPU_CAP_SIG64(16, 'c', 'p', 'b', 'w'):
    case CPU_CAP_SIG64(12, 'x', '8', 'b', 'w'):
    case CPU_CAP_SIG64(8,  'a', 'v', 'b', 'w'):
        if (abi_text_equals(capability_key, "cpu.x86.avx512bw") ||
            abi_text_equals(capability_key, "x86.avx512bw") ||
            abi_text_equals(capability_key, "avx512bw"))
            return KAIN_CPU_FEATURE_X86_AVX512BW;
        return 0;

    case CPU_CAP_SIG64(16, 'c', 'p', 'v', 'l'):
    case CPU_CAP_SIG64(12, 'x', '8', 'v', 'l'):
    case CPU_CAP_SIG64(8,  'a', 'v', 'v', 'l'):
        if (abi_text_equals(capability_key, "cpu.x86.avx512vl") ||
            abi_text_equals(capability_key, "x86.avx512vl") ||
            abi_text_equals(capability_key, "avx512vl"))
            return KAIN_CPU_FEATURE_X86_AVX512VL;
        return 0;

    case CPU_CAP_SIG64(14, 'c', 'p', '1', '2'):
    case CPU_CAP_SIG64(10, 'x', '8', '1', '2'):
    case CPU_CAP_SIG64(6,  'a', 'v', '1', '2'):
        if (abi_text_equals(capability_key, "cpu.x86.avx512") ||
            abi_text_equals(capability_key, "x86.avx512") ||
            abi_text_equals(capability_key, "avx512"))
            return KAIN_CPU_FEATURE_X86_AVX512F;
        return 0;

    case CPU_CAP_SIG64(11, 'c', 'p', 'm', 'a'):
    case CPU_CAP_SIG64(7,  'x', '8', 'm', 'a'):
    case CPU_CAP_SIG64(3,  'f', 'm', 'm', 'a'):
        if (abi_text_equals(capability_key, "cpu.x86.fma") ||
            abi_text_equals(capability_key, "x86.fma") ||
            abi_text_equals(capability_key, "fma"))
            return KAIN_CPU_FEATURE_X86_FMA;
        return 0;

    case CPU_CAP_SIG64(12, 'c', 'p', 'i', '2'):
    case CPU_CAP_SIG64(8,  'x', '8', 'i', '2'):
    case CPU_CAP_SIG64(4,  'b', 'm', 'i', '2'):
        if (abi_text_equals(capability_key, "cpu.x86.bmi2") ||
            abi_text_equals(capability_key, "x86.bmi2") ||
            abi_text_equals(capability_key, "bmi2"))
            return KAIN_CPU_FEATURE_X86_BMI2;
        return 0;

    default:
        return 0;
    }
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

static int64_t abi_cpu_detect_cache_line_bytes(void) {
#if CPU_X86
    int leaf[4] = {0, 0, 0, 0};
    if (abi_cpuid((int)0x80000006u, 0, leaf)) {
        int64_t cache_line = (int64_t)((uint32_t)leaf[2] & 0xffu);
        if (cache_line > 0) {
            return cache_line;
        }
    }
#endif
    return 64;
}

#if defined(_WIN32)
static int64_t abi_windows_count_topology_relation(LOGICAL_PROCESSOR_RELATIONSHIP relation) {
    DWORD bytes = 0;
    BYTE* cursor = 0;
    BYTE* end = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX buffer = 0;
    int64_t count = -1;

    if (GetLogicalProcessorInformationEx(relation, 0, &bytes) != 0 ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
        bytes == 0) {
        return -1;
    }

    buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)malloc((size_t)bytes);
    if (buffer == 0) {
        return -1;
    }
    if (GetLogicalProcessorInformationEx(relation, buffer, &bytes) == 0) {
        free(buffer);
        return -1;
    }

    count = 0;
    cursor = (BYTE*)buffer;
    end = cursor + bytes;
    while (cursor < end) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info =
            (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)cursor;
        count += 1;
        cursor += info->Size;
    }

    free(buffer);
    return count;
}
#endif

/* Proof: runtime/native/src/core/z3/proofs/native-vm-byte-count-fits-size-t-before-platform-cast.yaml */
static int abi_vm_byte_count_fits_platform(int64_t byte_count) {
    return byte_count > 0 && (uint64_t)byte_count <= (uint64_t)SIZE_MAX;
}

static int64_t abi_vm_default_huge_page_bytes(void) {
#if defined(_WIN32)
    SIZE_T large_page_bytes = GetLargePageMinimum();
    return large_page_bytes > 0 ? (int64_t)large_page_bytes : 0;
#else
    return 2ll * 1024ll * 1024ll;
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

int64_t abi_cpu_core_count(void) {
#if defined(_WIN32)
    int64_t count = abi_windows_count_topology_relation(RelationProcessorCore);
    if (count > 0) {
        return count;
    }
#endif
    return abi_cpu_logical_count();
}

int64_t abi_cpu_package_count(void) {
#if defined(_WIN32)
    int64_t count = abi_windows_count_topology_relation(RelationProcessorPackage);
    if (count > 0) {
        return count;
    }
#endif
    return 1;
}

int64_t abi_cpu_cache_line_bytes(void) {
    return abi_cpu_detect_cache_line_bytes();
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

int64_t abi_cpu_current_thread_affinity_mask(void) {
#if defined(_WIN32)
    GROUP_AFFINITY affinity;
    memset(&affinity, 0, sizeof(affinity));
    if (GetThreadGroupAffinity(GetCurrentThread(), &affinity) != 0) {
        return (int64_t)affinity.Mask;
    }
    {
        DWORD_PTR process_mask = 0;
        DWORD_PTR system_mask = 0;
        if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask) != 0) {
            return (int64_t)process_mask;
        }
    }
    return -1;
#elif defined(__linux__)
    cpu_set_t set;
    uint64_t mask = 0;
    int cpu_index = 0;
    CPU_ZERO(&set);
    if (pthread_getaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
        return -1;
    }
    for (cpu_index = 0; cpu_index < CPU_SETSIZE && cpu_index < 64; cpu_index += 1) {
        if (CPU_ISSET(cpu_index, &set)) {
            mask |= (uint64_t)1u << (uint64_t)cpu_index;
        }
    }
    return (int64_t)mask;
#else
    return -1;
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
    /* Proof: runtime/native/src/core/z3/proofs/native-cpu-affinity-mask-shift-stays-within-word.yaml */
    {
        DWORD_PTR mask = ((DWORD_PTR)1u) << (DWORD_PTR)core_index;
        return SetThreadAffinityMask(GetCurrentThread(), mask) == 0 ? -1 : 0;
    }
#elif defined(__linux__)
    cpu_set_t set;
    if (core_index >= CPU_SETSIZE) {
        return -1;
    }
    CPU_ZERO(&set);
    CPU_SET((int)core_index, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0 ? 0 : -1;
#else
    (void)core_index;
    return -1;
#endif
}

int64_t abi_cpu_numa_node_count(void) {
#if defined(_WIN32)
    ULONG highest_node = 0;
    return GetNumaHighestNodeNumber(&highest_node) != 0 ? (int64_t)highest_node + 1 : 1;
#else
    return 1;
#endif
}

int64_t abi_cpu_current_numa_node(void) {
#if defined(_WIN32)
    PROCESSOR_NUMBER processor;
    USHORT node = 0;
    GetCurrentProcessorNumberEx(&processor);
    return GetNumaProcessorNodeEx(&processor, &node) != 0 ? (int64_t)node : 0;
#else
    return 0;
#endif
}

int64_t abi_cpu_bind_current_thread_to_numa(int64_t node_index) {
    if (node_index < 0) {
        return -1;
    }
    if (abi_cpu_numa_node_count() <= 1) {
        return node_index == 0 ? 0 : -1;
    }
    return -1;
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

void* abi_vm_reserve(int64_t byte_count) {
    if (!abi_vm_byte_count_fits_platform(byte_count)) {
        return 0;
    }
#if defined(_WIN32)
    return VirtualAlloc(0, (SIZE_T)byte_count, MEM_RESERVE, PAGE_NOACCESS);
#elif defined(MAP_PRIVATE) && defined(MAP_ANON)
    {
        void* ptr = mmap(0, (size_t)byte_count, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
        return ptr == MAP_FAILED ? 0 : ptr;
    }
#elif defined(MAP_PRIVATE) && defined(MAP_ANONYMOUS)
    {
        void* ptr =
            mmap(0, (size_t)byte_count, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return ptr == MAP_FAILED ? 0 : ptr;
    }
#else
    return malloc((size_t)byte_count);
#endif
}

int64_t abi_vm_commit(void* ptr, int64_t byte_count) {
    if (ptr == 0 || !abi_vm_byte_count_fits_platform(byte_count)) {
        return -1;
    }
#if defined(_WIN32)
    return VirtualAlloc(ptr, (SIZE_T)byte_count, MEM_COMMIT, PAGE_READWRITE) == ptr ? 0 : -1;
#elif defined(PROT_NONE)
    return mprotect(ptr, (size_t)byte_count, PROT_READ | PROT_WRITE) == 0 ? 0 : -1;
#else
    (void)ptr;
    (void)byte_count;
    return 0;
#endif
}

int64_t abi_vm_decommit(void* ptr, int64_t byte_count) {
    if (ptr == 0 || !abi_vm_byte_count_fits_platform(byte_count)) {
        return -1;
    }
#if defined(_WIN32)
    return VirtualFree(ptr, (SIZE_T)byte_count, MEM_DECOMMIT) != 0 ? 0 : -1;
#elif defined(PROT_NONE)
    int protected_ok = mprotect(ptr, (size_t)byte_count, PROT_NONE) == 0 ? 0 : -1;
#if defined(MADV_DONTNEED)
    if (protected_ok == 0) {
        (void)madvise(ptr, (size_t)byte_count, MADV_DONTNEED);
    }
#endif
    return protected_ok;
#else
    (void)ptr;
    (void)byte_count;
    return 0;
#endif
}

int64_t abi_vm_release(void* ptr, int64_t byte_count) {
    if (ptr == 0) {
        return 0;
    }
#if defined(_WIN32)
    (void)byte_count;
    return VirtualFree(ptr, 0, MEM_RELEASE) != 0 ? 0 : -1;
#elif defined(MAP_PRIVATE) && (defined(MAP_ANON) || defined(MAP_ANONYMOUS))
    if (!abi_vm_byte_count_fits_platform(byte_count)) {
        return -1;
    }
    return munmap(ptr, (size_t)byte_count) == 0 ? 0 : -1;
#else
    (void)byte_count;
    free(ptr);
    return 0;
#endif
}

int64_t abi_vm_lock(void* ptr, int64_t byte_count) {
    if (ptr == 0 || !abi_vm_byte_count_fits_platform(byte_count)) {
        return -1;
    }
#if defined(_WIN32)
    return VirtualLock(ptr, (SIZE_T)byte_count) != 0 ? 0 : -1;
#elif defined(__unix__) || defined(__APPLE__)
    return mlock(ptr, (size_t)byte_count) == 0 ? 0 : -1;
#else
    (void)ptr;
    (void)byte_count;
    return -1;
#endif
}

int64_t abi_vm_unlock(void* ptr, int64_t byte_count) {
    if (ptr == 0 || !abi_vm_byte_count_fits_platform(byte_count)) {
        return -1;
    }
#if defined(_WIN32)
    return VirtualUnlock(ptr, (SIZE_T)byte_count) != 0 ? 0 : -1;
#elif defined(__unix__) || defined(__APPLE__)
    return munlock(ptr, (size_t)byte_count) == 0 ? 0 : -1;
#else
    (void)ptr;
    (void)byte_count;
    return -1;
#endif
}

void* abi_vm_map_huge(int64_t byte_count) {
    int64_t huge_page_bytes = abi_vm_default_huge_page_bytes();
    /* Proof: runtime/native/src/core/z3/proofs/native-vm-huge-page-byte-count-must-align-to-large-page-granularity.yaml */
    if (!abi_vm_byte_count_fits_platform(byte_count) ||
        huge_page_bytes <= 0 ||
        (byte_count % huge_page_bytes) != 0) {
        return 0;
    }
#if defined(_WIN32)
    return VirtualAlloc(
        0,
        (SIZE_T)byte_count,
        MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
        PAGE_READWRITE);
#elif defined(MAP_PRIVATE) && defined(MAP_HUGETLB) && defined(MAP_ANONYMOUS)
    {
        void* ptr = mmap(
            0,
            (size_t)byte_count,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
            -1,
            0);
        return ptr == MAP_FAILED ? 0 : ptr;
    }
#else
    return 0;
#endif
}

void* abi_vm_map(int64_t byte_count) {
    void* ptr = abi_vm_reserve(byte_count);
    if (ptr == 0) {
        return 0;
    }
    if (abi_vm_commit(ptr, byte_count) != 0) {
        (void)abi_vm_release(ptr, byte_count);
        return 0;
    }
    return ptr;
}

int64_t abi_vm_unmap(void* ptr, int64_t byte_count) {
    return abi_vm_release(ptr, byte_count);
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
    if (ptr == 0 || !abi_vm_byte_count_fits_platform(byte_count)) {
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
