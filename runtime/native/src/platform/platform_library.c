#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/platform_library.h"
#include "../../include/platform.h"
#include "../../include/base.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef HMODULE KainPlatformLibraryOsHandle;
#else
#include <dlfcn.h>
typedef void* KainPlatformLibraryOsHandle;
#endif

#define KAIN_PLATFORM_LIBRARY_PATH_MAX 1024u
#define KAIN_PLATFORM_LIBRARY_KIND_MAX 64u
#define KAIN_PLATFORM_LIBRARY_MESSAGE_MAX 512u
#define KAIN_PLATFORM_LIBRARY_INDEX_BITS 16u
#define KAIN_PLATFORM_LIBRARY_INDEX_MASK 0xffffu
#define KAIN_PLATFORM_LIBRARY_MAX_GENERATION ((uint64_t)INT64_MAX >> KAIN_PLATFORM_LIBRARY_INDEX_BITS)

typedef struct KainPlatformLibrarySlot {
    int in_use;
    uint64_t generation;
    KainPlatformLibraryOsHandle os_handle;
    char path[KAIN_PLATFORM_LIBRARY_PATH_MAX];
} KainPlatformLibrarySlot;

static KainPlatformLibrarySlot g_platform_libraries[KAIN_PLATFORM_LIBRARY_MAX_HANDLES];
static int64_t g_platform_library_last_status = KAIN_PLATFORM_LIBRARY_OK;
static char g_platform_library_last_error_kind[KAIN_PLATFORM_LIBRARY_KIND_MAX] = "ok";
static char g_platform_library_last_error_message[KAIN_PLATFORM_LIBRARY_MESSAGE_MAX] = "";

static void abi_platform_library_copy_text(char* destination, size_t capacity, const char* source) {
    if (destination == NULL || capacity == 0u) {
        return;
    }
    if (source == NULL) {
        source = "";
    }
    snprintf(destination, capacity, "%s", source);
}

static int64_t abi_platform_library_set_status(
    int64_t status,
    const char* kind,
    const char* message
) {
    g_platform_library_last_status = status;
    abi_platform_library_copy_text(
        g_platform_library_last_error_kind,
        sizeof(g_platform_library_last_error_kind),
        kind
    );
    abi_platform_library_copy_text(
        g_platform_library_last_error_message,
        sizeof(g_platform_library_last_error_message),
        message
    );
    return status;
}

static int64_t abi_platform_library_ok(void) {
    return abi_platform_library_set_status(KAIN_PLATFORM_LIBRARY_OK, "ok", "");
}

static int abi_platform_library_find_free_slot(void) {
    int index;
    for (index = 0; index < KAIN_PLATFORM_LIBRARY_MAX_HANDLES; index += 1) {
        if (g_platform_libraries[index].in_use == 0) {
            return index;
        }
    }
    return -1;
}

static int64_t abi_platform_library_encode_handle(uint64_t generation, int index) {
    uint64_t encoded;
    if (
        generation == 0u ||
        generation > KAIN_PLATFORM_LIBRARY_MAX_GENERATION ||
        index < 0 ||
        index >= KAIN_PLATFORM_LIBRARY_MAX_HANDLES
    ) {
        return 0;
    }
    encoded = (generation << KAIN_PLATFORM_LIBRARY_INDEX_BITS) | (uint64_t)(index + 1);
    if (encoded == 0u || encoded > (uint64_t)INT64_MAX) {
        return 0;
    }
    return (int64_t)encoded;
}

static KainPlatformLibrarySlot* abi_platform_library_slot_from_handle(int64_t handle) {
    uint64_t encoded;
    uint64_t generation;
    uint64_t index_lane;
    size_t index;
    KainPlatformLibrarySlot* slot;

    if (handle <= 0) {
        return NULL;
    }
    encoded = (uint64_t)handle;
    index_lane = encoded & KAIN_PLATFORM_LIBRARY_INDEX_MASK;
    if (index_lane == 0u || index_lane > KAIN_PLATFORM_LIBRARY_MAX_HANDLES) {
        return NULL;
    }
    generation = encoded >> KAIN_PLATFORM_LIBRARY_INDEX_BITS;
    if (generation == 0u) {
        return NULL;
    }
    index = (size_t)(index_lane - 1u);
    slot = &g_platform_libraries[index];
    if (slot->in_use == 0 || slot->generation != generation || slot->os_handle == NULL) {
        return NULL;
    }
    return slot;
}

static int64_t abi_platform_library_set_os_error(const char* operation, const char* path) {
    char message[KAIN_PLATFORM_LIBRARY_MESSAGE_MAX];
#ifdef _WIN32
    DWORD error_code = GetLastError();
    snprintf(
        message,
        sizeof(message),
        "%s failed for '%s' with Win32 error %lu",
        operation ? operation : "platform library operation",
        path ? path : "",
        (unsigned long)error_code
    );
#else
    const char* error_text = dlerror();
    snprintf(
        message,
        sizeof(message),
        "%s failed for '%s': %s",
        operation ? operation : "platform library operation",
        path ? path : "",
        error_text ? error_text : "unknown dynamic loader error"
    );
#endif
    return abi_platform_library_set_status(
        KAIN_PLATFORM_LIBRARY_OPEN_FAILED,
        "open_failed",
        message
    );
}

int64_t abi_platform_current_kind(void) {
    return (int64_t)kain_platform_current_kind();
}

const char* abi_platform_current_name(void) {
    return string_new((char*)kain_platform_kind_name(kain_platform_current_kind()));
}

int64_t abi_platform_current_service_mask(void) {
    return (int64_t)kain_platform_current_service_mask();
}

int64_t abi_platform_current_optional_service_mask(void) {
    return (int64_t)kain_platform_current_optional_service_mask();
}

int64_t abi_platform_library_open(const char* path) {
    int slot_index;
    KainPlatformLibrarySlot* slot;
    KainPlatformLibraryOsHandle os_handle;
    int64_t encoded;

    if (path == NULL || path[0] == '\0') {
        return abi_platform_library_set_status(
            KAIN_PLATFORM_LIBRARY_INVALID_ARGUMENT,
            "invalid_argument",
            "platform library path must not be empty"
        );
    }

    slot_index = abi_platform_library_find_free_slot();
    if (slot_index < 0) {
        return abi_platform_library_set_status(
            KAIN_PLATFORM_LIBRARY_CAPACITY_EXCEEDED,
            "capacity_exceeded",
            "platform library handle table is full"
        );
    }

#ifdef _WIN32
    os_handle = LoadLibraryA(path);
#else
    dlerror();
    os_handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    if (os_handle == NULL) {
        return abi_platform_library_set_os_error("open", path);
    }

    slot = &g_platform_libraries[slot_index];
    slot->generation += 1u;
    if (slot->generation == 0u) {
        slot->generation = 1u;
    }
    encoded = abi_platform_library_encode_handle(slot->generation, slot_index);
    if (encoded == 0) {
#ifdef _WIN32
        FreeLibrary(os_handle);
#else
        dlclose(os_handle);
#endif
        return abi_platform_library_set_status(
            KAIN_PLATFORM_LIBRARY_CAPACITY_EXCEEDED,
            "handle_overflow",
            "platform library handle generation overflowed the Kain Int domain"
        );
    }

    slot->in_use = 1;
    slot->os_handle = os_handle;
    abi_platform_library_copy_text(slot->path, sizeof(slot->path), path);
    abi_platform_library_ok();
    return encoded;
}

int64_t abi_platform_library_close(int64_t handle) {
    KainPlatformLibrarySlot* slot = abi_platform_library_slot_from_handle(handle);
    if (slot == NULL) {
        return abi_platform_library_set_status(
            KAIN_PLATFORM_LIBRARY_INVALID_HANDLE,
            "invalid_handle",
            "platform library handle is not live"
        );
    }

#ifdef _WIN32
    if (!FreeLibrary(slot->os_handle)) {
        return abi_platform_library_set_os_error("close", slot->path);
    }
#else
    if (dlclose(slot->os_handle) != 0) {
        return abi_platform_library_set_os_error("close", slot->path);
    }
#endif

    slot->in_use = 0;
    slot->os_handle = NULL;
    slot->path[0] = '\0';
    return abi_platform_library_ok();
}

int64_t abi_platform_library_resolve(int64_t handle, const char* symbol_name) {
    KainPlatformLibrarySlot* slot = abi_platform_library_slot_from_handle(handle);
    void* symbol;
    char message[KAIN_PLATFORM_LIBRARY_MESSAGE_MAX];

    if (slot == NULL) {
        return abi_platform_library_set_status(
            KAIN_PLATFORM_LIBRARY_INVALID_HANDLE,
            "invalid_handle",
            "platform library handle is not live"
        );
    }
    if (symbol_name == NULL || symbol_name[0] == '\0') {
        return abi_platform_library_set_status(
            KAIN_PLATFORM_LIBRARY_INVALID_ARGUMENT,
            "invalid_argument",
            "platform library symbol name must not be empty"
        );
    }

#ifdef _WIN32
    symbol = (void*)GetProcAddress(slot->os_handle, symbol_name);
#else
    dlerror();
    symbol = dlsym(slot->os_handle, symbol_name);
#endif

    if (symbol == NULL) {
        snprintf(
            message,
            sizeof(message),
            "symbol '%s' was not found in '%s'",
            symbol_name,
            slot->path
        );
        return abi_platform_library_set_status(
            KAIN_PLATFORM_LIBRARY_SYMBOL_NOT_FOUND,
            "symbol_not_found",
            message
        );
    }

    abi_platform_library_ok();
    return (int64_t)(intptr_t)symbol;
}

int64_t abi_platform_library_is_valid(int64_t handle) {
    return abi_platform_library_slot_from_handle(handle) != NULL ? 1 : 0;
}

int64_t abi_platform_library_live_count(void) {
    int index;
    int64_t count = 0;
    for (index = 0; index < KAIN_PLATFORM_LIBRARY_MAX_HANDLES; index += 1) {
        if (g_platform_libraries[index].in_use != 0) {
            count += 1;
        }
    }
    return count;
}

int64_t abi_platform_library_last_status(void) {
    return g_platform_library_last_status;
}

const char* abi_platform_library_last_error_kind(void) {
    return string_new(g_platform_library_last_error_kind);
}

const char* abi_platform_library_last_error_message(void) {
    return string_new(g_platform_library_last_error_message);
}
