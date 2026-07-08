// SipHash reference implementation (public domain)
// By Jean-Philippe Aumasson and Daniel J. Bernstein
// https://github.com/veorq/SipHash
//
// llvm-kain: Restored for SipHash support (deleted during Phase 3/4 stripping)

#include <cstddef>
#include <cstdint>

#define SIPROUND                            \
  do {                                     \
    v0 += v1; v1 = ROTL(v1, 13);           \
    v1 ^= v0; v0 = ROTL(v0, 32);           \
    v2 += v3; v3 = ROTL(v3, 16);           \
    v3 ^= v2;                               \
    v0 += v3; v3 = ROTL(v3, 21);           \
    v3 ^= v0;                               \
    v2 += v1; v1 = ROTL(v1, 17);           \
    v1 ^= v2; v2 = ROTL(v2, 32);           \
  } while(0)

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

template <int cROUNDS, int dROUNDS>
inline void siphash(const uint8_t *in, size_t inlen, const uint8_t *k,
                    uint8_t *out) {
  uint64_t v0 = 0x736f6d6570736575ULL;
  uint64_t v1 = 0x646f72616e646f6dULL;
  uint64_t v2 = 0x6c7967656e657261ULL;
  uint64_t v3 = 0x7465646279746573ULL;
  uint64_t k0 = ((uint64_t *)k)[0];
  uint64_t k1 = ((uint64_t *)k)[1];
  uint64_t m;
  int i;
  const uint8_t *end = in + inlen - (inlen % sizeof(uint64_t));
  const int left = inlen & 7;
  uint64_t b = ((uint64_t)inlen) << 56;
  v3 ^= k1;
  v2 ^= k0;
  v1 ^= k1;
  v0 ^= k0;

  for (; in != end; in += 8) {
    m = ((uint64_t *)in)[0];
    v3 ^= m;
    for (i = 0; i < cROUNDS; ++i)
      SIPROUND;
    v0 ^= m;
  }

  switch (left) {
  case 7:
    b |= ((uint64_t)in[6]) << 48;
    /* FALLTHRU */
  case 6:
    b |= ((uint64_t)in[5]) << 40;
    /* FALLTHRU */
  case 5:
    b |= ((uint64_t)in[4]) << 32;
    /* FALLTHRU */
  case 4:
    b |= ((uint64_t)in[3]) << 24;
    /* FALLTHRU */
  case 3:
    b |= ((uint64_t)in[2]) << 16;
    /* FALLTHRU */
  case 2:
    b |= ((uint64_t)in[1]) << 8;
    /* FALLTHRU */
  case 1:
    b |= ((uint64_t)in[0]);
    break;
  case 0:
    break;
  }

  v3 ^= b;
  for (i = 0; i < cROUNDS; ++i)
    SIPROUND;
  v0 ^= b;
  v2 ^= 0xff;
  for (i = 0; i < dROUNDS; ++i)
    SIPROUND;
  b = v0 ^ v1 ^ v2 ^ v3;
  ((uint64_t *)out)[0] = b;
}
