#ifndef SMOKETEST_C_ABI_ALBUM_H
#define SMOKETEST_C_ABI_ALBUM_H

#if defined(_WIN32)
#define SMOKETEST_C_ABI_EXPORT __declspec(dllexport)
#else
#define SMOKETEST_C_ABI_EXPORT
#endif

SMOKETEST_C_ABI_EXPORT int smoketest_c_abi_album_score(int seed, int rounds);
SMOKETEST_C_ABI_EXPORT int smoketest_c_abi_album_command_count(int seed, int rounds);
SMOKETEST_C_ABI_EXPORT int smoketest_c_abi_album_ring_tail(int seed, int rounds);
SMOKETEST_C_ABI_EXPORT int smoketest_c_abi_album_signature_span(int seed, int rounds);
SMOKETEST_C_ABI_EXPORT const char* smoketest_c_abi_album_signature(int seed, int rounds);
SMOKETEST_C_ABI_EXPORT _Bool smoketest_c_abi_album_hot(int seed, int rounds);

#endif
