#if defined(_WIN32)
#define BLADE_FILTER_EXPORT __declspec(dllexport)
#else
#define BLADE_FILTER_EXPORT
#endif

#include <stddef.h>
#include <stdint.h>

BLADE_FILTER_EXPORT int64_t blade_filter_checksum(const uint8_t* pixels, size_t len);
BLADE_FILTER_EXPORT void blade_filter_apply_rgba(uint8_t* pixels, size_t len, int accent);
BLADE_FILTER_EXPORT const char* blade_filter_signature(int width, int height, int64_t checksum);
