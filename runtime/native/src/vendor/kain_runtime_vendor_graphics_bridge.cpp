#include "../../include/kain_runtime_vendor_graphics_bridge.h"
#include "../../include/kain_runtime_vendor_lane.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <io.h>
#define KAIN_VENDOR_ACCESS _access
#define KAIN_VENDOR_PATH_SEPARATOR ';'
#else
#include <unistd.h>
#define KAIN_VENDOR_ACCESS access
#define KAIN_VENDOR_PATH_SEPARATOR ':'
#endif

#if KAIN_VENDOR_HAS_BGFX && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
#include <bgfx/c99/bgfx.h>
#include <bgfx/defines.h>
#endif

#if KAIN_VENDOR_HAS_BIMG && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
#include <bimg/bimg.h>
#endif

extern "C" {

static int kain_vendor_path_has_binary(const char* binary_name) {
    const char* path_value = std::getenv("PATH");
    if (path_value == nullptr || binary_name == nullptr || binary_name[0] == '\0') {
        return 0;
    }

    std::string path_list(path_value);
    size_t start = 0;
    while (start <= path_list.size()) {
        size_t separator = path_list.find(KAIN_VENDOR_PATH_SEPARATOR, start);
        std::string directory = separator == std::string::npos
            ? path_list.substr(start)
            : path_list.substr(start, separator - start);
        if (!directory.empty()) {
            std::string candidate = directory;
#if defined(_WIN32)
            candidate += "\\\\";
            candidate += binary_name;
            if (KAIN_VENDOR_ACCESS(candidate.c_str(), 0) == 0) {
                return 1;
            }
            candidate += ".exe";
            if (KAIN_VENDOR_ACCESS(candidate.c_str(), 0) == 0) {
                return 1;
            }
#else
            candidate += "/";
            candidate += binary_name;
            if (KAIN_VENDOR_ACCESS(candidate.c_str(), X_OK) == 0) {
                return 1;
            }
#endif
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    return 0;
}

static int kain_vendor_env_path_available(const char* env_key) {
    const char* candidate = std::getenv(env_key);
    return candidate != nullptr && candidate[0] != '\0' && KAIN_VENDOR_ACCESS(candidate, 0) == 0;
}

static int kain_vendor_any_env_path_available(const char* const* env_keys, size_t env_key_count) {
    size_t index;

    if (!env_keys || env_key_count == 0) {
        return 0;
    }

    for (index = 0; index < env_key_count; ++index) {
        if (kain_vendor_env_path_available(env_keys[index])) {
            return 1;
        }
    }

    return 0;
}

static int kain_vendor_any_binary_available(const char* const* binary_names, size_t binary_count) {
    size_t index;

    if (!binary_names || binary_count == 0) {
        return 0;
    }

    for (index = 0; index < binary_count; ++index) {
        if (kain_vendor_path_has_binary(binary_names[index])) {
            return 1;
        }
    }

    return 0;
}

static int kain_vendor_runtime_available(
    const char* const* env_keys,
    size_t env_key_count,
    const char* const* binary_names,
    size_t binary_count
) {
    return kain_vendor_any_env_path_available(env_keys, env_key_count) ||
        kain_vendor_any_binary_available(binary_names, binary_count);
}

#if KAIN_VENDOR_HAS_BGFX && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
const char* kain_vendor_bgfx_version_string(void) {
    static char version_buffer[32];
    static int initialized = 0;

    if (!initialized) {
        std::snprintf(version_buffer, sizeof(version_buffer), "api-%u", BGFX_API_VERSION);
        initialized = 1;
    }

    return version_buffer;
}

int kain_vendor_bgfx_probe(void) {
    return bgfx_get_interface(BGFX_API_VERSION) != nullptr;
}
#else
const char* kain_vendor_bgfx_version_string(void) {
    return "bgfx-unavailable";
}

int kain_vendor_bgfx_probe(void) {
    return 0;
}
#endif

const char* kain_vendor_filament_version_string(void) {
    static const char* const env_keys[] = {"KAIN_FILAMENT_RUNTIME", "FILAMENT_RUNTIME"};
    static const char* const binary_names[] = {"filament", "filament-viewer"};
    return kain_vendor_runtime_available(env_keys, 2, binary_names, 2)
        ? "filament-bridge"
        : "filament-bridge-unavailable";
}

int kain_vendor_filament_probe(void) {
    static const char* const env_keys[] = {"KAIN_FILAMENT_RUNTIME", "FILAMENT_RUNTIME"};
    static const char* const binary_names[] = {"filament", "filament-viewer"};
    return kain_vendor_runtime_available(env_keys, 2, binary_names, 2);
}

const char* kain_vendor_diligent_version_string(void) {
    static const char* const env_keys[] = {"KAIN_DILIGENT_RUNTIME", "DILIGENT_RUNTIME"};
    static const char* const binary_names[] = {"diligent", "diligent-viewer", "diligent-sample"};
    return kain_vendor_runtime_available(env_keys, 2, binary_names, 3)
        ? "diligent-bridge"
        : "diligent-bridge-unavailable";
}

int kain_vendor_diligent_probe(void) {
    static const char* const env_keys[] = {"KAIN_DILIGENT_RUNTIME", "DILIGENT_RUNTIME"};
    static const char* const binary_names[] = {"diligent", "diligent-viewer", "diligent-sample"};
    return kain_vendor_runtime_available(env_keys, 2, binary_names, 3);
}

const char* kain_vendor_forge_version_string(void) {
    static const char* const env_keys[] = {"KAIN_FORGE_RUNTIME", "FORGE_RUNTIME"};
    static const char* const binary_names[] = {"forge"};
    return kain_vendor_runtime_available(env_keys, 2, binary_names, 1)
        ? "forge-bridge"
        : "forge-bridge-unavailable";
}

int kain_vendor_forge_probe(void) {
    static const char* const env_keys[] = {"KAIN_FORGE_RUNTIME", "FORGE_RUNTIME"};
    static const char* const binary_names[] = {"forge"};
    return kain_vendor_runtime_available(env_keys, 2, binary_names, 1);
}

#if KAIN_VENDOR_HAS_BIMG && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
const char* kain_vendor_bimg_version_string(void) {
    return "bimg-runtime";
}

int kain_vendor_bimg_probe(void) {
    return bimg::getBitsPerPixel(bimg::TextureFormat::RGBA8) > 0 ? 1 : 0;
}
#else
const char* kain_vendor_bimg_version_string(void) {
    return "bimg-unavailable";
}

int kain_vendor_bimg_probe(void) {
    return 0;
}
#endif

} /* extern "C" */
