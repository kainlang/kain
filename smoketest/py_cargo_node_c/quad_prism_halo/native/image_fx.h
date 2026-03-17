#if defined(_WIN32)
#define IMAGEFX_EXPORT __declspec(dllexport)
#else
#define IMAGEFX_EXPORT
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct ImageWorkspace ImageWorkspace;

IMAGEFX_EXPORT int64_t imagefx_checksum(const uint8_t* pixels, size_t len);
IMAGEFX_EXPORT void imagefx_halo_rgba(uint8_t* pixels, size_t len, int accent);
IMAGEFX_EXPORT const char* imagefx_signature(int width, int height, int64_t checksum, int accent);
IMAGEFX_EXPORT ImageWorkspace* imagefx_workspace_create(int width, int height);
IMAGEFX_EXPORT int imagefx_workspace_area(ImageWorkspace* workspace);
IMAGEFX_EXPORT void imagefx_workspace_destroy(ImageWorkspace* workspace);
