#if defined(_WIN32)
#define IMAGEFX_EXPORT __declspec(dllexport)
#else
#define IMAGEFX_EXPORT
#endif

#include <stddef.h>
#include <stdint.h>

typedef struct ImageWorkspace ImageWorkspace;

IMAGEFX_EXPORT uint64_t imagefx_checksum(const uint8_t* pixels, size_t len);
IMAGEFX_EXPORT void imagefx_invert_rgba(uint8_t* pixels, size_t len);
IMAGEFX_EXPORT const char* imagefx_signature(int width, int height, uint64_t checksum);
IMAGEFX_EXPORT ImageWorkspace* imagefx_workspace_create(int width, int height);
IMAGEFX_EXPORT int imagefx_workspace_area(ImageWorkspace* workspace);
IMAGEFX_EXPORT void imagefx_workspace_destroy(ImageWorkspace* workspace);
