#ifndef PLATFORM_LIBRARY_H
#define PLATFORM_LIBRARY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_PLATFORM_LIBRARY_MAX_HANDLES 128

typedef enum KainPlatformLibraryStatus {
    KAIN_PLATFORM_LIBRARY_OK = 0,
    KAIN_PLATFORM_LIBRARY_INVALID_ARGUMENT = -1,
    KAIN_PLATFORM_LIBRARY_CAPACITY_EXCEEDED = -2,
    KAIN_PLATFORM_LIBRARY_OPEN_FAILED = -3,
    KAIN_PLATFORM_LIBRARY_SYMBOL_NOT_FOUND = -4,
    KAIN_PLATFORM_LIBRARY_INVALID_HANDLE = -5,
} KainPlatformLibraryStatus;

int64_t abi_platform_current_kind(void);
const char* abi_platform_current_name(void);
int64_t abi_platform_current_service_mask(void);
int64_t abi_platform_current_optional_service_mask(void);

int64_t abi_platform_library_open(const char* path);
int64_t abi_platform_library_close(int64_t handle);
int64_t abi_platform_library_resolve(int64_t handle, const char* symbol_name);
int64_t abi_platform_library_is_valid(int64_t handle);
int64_t abi_platform_library_live_count(void);
int64_t abi_platform_library_last_status(void);
const char* abi_platform_library_last_error_kind(void);
const char* abi_platform_library_last_error_message(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_LIBRARY_H */
