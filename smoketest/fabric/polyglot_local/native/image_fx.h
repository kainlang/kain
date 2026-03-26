#if defined(_WIN32)
#define IMAGEFX_EXPORT __declspec(dllexport)
#else
#define IMAGEFX_EXPORT
#endif

#include <stddef.h>
#include <stdint.h>

IMAGEFX_EXPORT uint64_t imagefx_checksum(const uint8_t* pixels, size_t len);
IMAGEFX_EXPORT void imagefx_halo_rgba(uint8_t* pixels, size_t len, int accent);
IMAGEFX_EXPORT const char* imagefx_signature(int width, int height, uint64_t checksum);
