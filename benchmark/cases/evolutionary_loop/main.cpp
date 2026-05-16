#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(_MSC_VER)
#define BENCH_NOINLINE __declspec(noinline)
#else
#define BENCH_NOINLINE __attribute__((noinline))
#endif

constexpr std::int64_t ITERATIONS = 2'000'000;
constexpr std::int64_t EXPECTED = 403'591'996;
constexpr std::int64_t MODULUS = 1'000'000'007;

BENCH_NOINLINE std::int64_t scalar_lane(std::int64_t value) {
    return ((value * 31) + 7) % MODULUS;
}

BENCH_NOINLINE std::int64_t wide_lane(std::int64_t value) {
    return ((value * 31) + 7) % MODULUS;
}

BENCH_NOINLINE bool cpu_supports_avx2() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#if defined(_MSC_VER)
    int cpu_info[4] = {0, 0, 0, 0};
    __cpuid(cpu_info, 0);
    if (cpu_info[0] < 7) {
        return false;
    }

    __cpuidex(cpu_info, 1, 0);
    const bool has_osxsave = (cpu_info[2] & (1 << 27)) != 0;
    const bool has_avx = (cpu_info[2] & (1 << 28)) != 0;
    if (!has_osxsave || !has_avx) {
        return false;
    }

    const unsigned __int64 xcr0 = _xgetbv(0);
    if ((xcr0 & 0x6U) != 0x6U) {
        return false;
    }

    __cpuidex(cpu_info, 7, 0);
    return (cpu_info[1] & (1 << 5)) != 0;
#elif defined(__clang__) || defined(__GNUC__)
    return __builtin_cpu_supports("avx2");
#endif
#endif
    return false;
}

BENCH_NOINLINE std::int64_t choose(std::int64_t value) {
    if (cpu_supports_avx2()) {
        return wide_lane(value);
    }
    return scalar_lane(value);
}

BENCH_NOINLINE std::int64_t mix(std::int64_t value) {
    return ((value * 17) + 11) % MODULUS;
}

BENCH_NOINLINE std::int64_t pipeline(std::int64_t value) {
    return mix(choose(value));
}

int main() {
    std::int64_t acc = 1;
    std::int64_t i = 0;
    while (i < ITERATIONS) {
        acc = pipeline(acc + i);
        ++i;
    }
    const volatile std::int64_t* observed_ptr = &acc;
    return *observed_ptr == EXPECTED ? 0 : 1;
}
