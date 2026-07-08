// SSE2 intrinsic stubs for UCRT wmemcmp inline linkage
// The UCRT <wchar.h> wmemcmp inline uses SSE2 intrinsics. With
// clang++/lld-link on Windows, these may be emitted as undefined
// extern symbols when -nostdlib is used. Provide definitions.
//
// This file uses clang vector extensions instead of <emmintrin.h>
// to avoid conflicts with the static inline definitions there.

typedef short __v8hi __attribute__((__vector_size__(16)));
typedef char __v16qi __attribute__((__vector_size__(16)));
typedef long long __m128i __attribute__((__vector_size__(16), __aligned__(16)));

extern "C" {

__attribute__((used)) __m128i __cdecl _mm_loadu_si128(__m128i const *__p) {
    return *__p;
}

__attribute__((used)) __m128i __cdecl _mm_cmpeq_epi16(__m128i __a, __m128i __b) {
    return (__m128i)((__v8hi)__a == (__v8hi)__b);
}

__attribute__((used)) int __cdecl _mm_movemask_epi8(__m128i __a) {
    return __builtin_ia32_pmovmskb128((__v16qi)__a);
}

}
