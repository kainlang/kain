#if defined(_WIN32)
#define MODELER_OPS_EXPORT __declspec(dllexport)
#else
#define MODELER_OPS_EXPORT
#endif

#include <stddef.h>
#include <stdint.h>

MODELER_OPS_EXPORT void modeler_stamp_highlight(uint8_t* pixels, size_t len, int accent);
MODELER_OPS_EXPORT const char* modeler_signature(int width, int height, int accent);
