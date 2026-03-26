#if defined(_WIN32)
#define BEACON_EXPORT __declspec(dllexport)
#else
#define BEACON_EXPORT
#endif

BEACON_EXPORT int beacon_add(int a, int b);
BEACON_EXPORT _Bool beacon_is_even(int value);
BEACON_EXPORT double beacon_scale(int value, double factor);
BEACON_EXPORT const char* beacon_label(int id);

// Unsupported on purpose right now: pointer-heavy declarations should be classified as stubbed.
BEACON_EXPORT const unsigned char* beacon_payload_bytes(int id, int* byte_count);
BEACON_EXPORT void beacon_fill_buffer(float* values, int count);
