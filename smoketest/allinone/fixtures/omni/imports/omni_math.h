#if defined(_WIN32)
#define OMNI_EXPORT __declspec(dllexport)
#else
#define OMNI_EXPORT
#endif

OMNI_EXPORT int omni_add(int a, int b);
OMNI_EXPORT double omni_scale(int value, double factor);
OMNI_EXPORT const char* omni_label(int id);
