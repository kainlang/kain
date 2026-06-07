#include "../include/platform_library.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define PLATFORM_TEST_LIBRARY "kernel32.dll"
#define PLATFORM_TEST_SYMBOL "GetCurrentProcessId"
typedef DWORD(WINAPI* PlatformTestProcessIdFn)(void);
#elif defined(__APPLE__)
#include <unistd.h>
#define PLATFORM_TEST_LIBRARY "/usr/lib/libSystem.B.dylib"
#define PLATFORM_TEST_SYMBOL "getpid"
typedef pid_t (*PlatformTestProcessIdFn)(void);
#else
#include <unistd.h>
#define PLATFORM_TEST_LIBRARY "libc.so.6"
#define PLATFORM_TEST_SYMBOL "getpid"
typedef pid_t (*PlatformTestProcessIdFn)(void);
#endif

static int expect_true(int condition, const char* label) {
    if (!condition) {
        fprintf(stderr, "platform library test failed: %s\n", label);
        fprintf(stderr, "last status: %lld\n", (long long)abi_platform_library_last_status());
        fprintf(stderr, "last kind: %s\n", abi_platform_library_last_error_kind());
        fprintf(stderr, "last message: %s\n", abi_platform_library_last_error_message());
        return 0;
    }
    return 1;
}

int main(void) {
    int64_t handle;
    int64_t symbol;
    int64_t close_status;

    if (!expect_true(abi_platform_library_live_count() == 0, "starts with no live libraries")) {
        return 1;
    }

    handle = abi_platform_library_open(PLATFORM_TEST_LIBRARY);
    if (!expect_true(handle > 0, "opens platform test library")) {
        return 1;
    }
    if (!expect_true(abi_platform_library_is_valid(handle) == 1, "opened handle is valid")) {
        return 1;
    }
    if (!expect_true(abi_platform_library_live_count() == 1, "live count tracks open library")) {
        return 1;
    }

    symbol = abi_platform_library_resolve(handle, PLATFORM_TEST_SYMBOL);
    if (!expect_true(symbol != 0, "resolves known symbol")) {
        return 1;
    }
    {
        PlatformTestProcessIdFn typed_thunk = (PlatformTestProcessIdFn)(intptr_t)symbol;
        int64_t process_id = (int64_t)typed_thunk();
        if (!expect_true(process_id > 0, "typed thunk calls through resolved symbol")) {
            return 1;
        }
    }

    close_status = abi_platform_library_close(handle);
    if (!expect_true(close_status == 0, "closes library")) {
        return 1;
    }
    if (!expect_true(abi_platform_library_is_valid(handle) == 0, "closed handle is stale")) {
        return 1;
    }
    if (!expect_true(abi_platform_library_live_count() == 0, "live count returns to zero")) {
        return 1;
    }
    if (!expect_true(
            abi_platform_library_resolve(handle, PLATFORM_TEST_SYMBOL) == KAIN_PLATFORM_LIBRARY_INVALID_HANDLE,
            "stale handle cannot resolve symbol"
        )) {
        return 1;
    }

    return 0;
}
