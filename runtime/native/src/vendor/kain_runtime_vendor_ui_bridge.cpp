#include "../../include/kain_runtime_vendor_ui_bridge.h"
#include "../../include/kain_runtime_vendor_lane.h"

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

#if KAIN_VENDOR_HAS_IMGUI && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
#include <imgui.h>
#endif

#if KAIN_VENDOR_HAS_YOGA && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
#include <yoga/Yoga.h>
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

static int kain_vendor_env_has_qt_runtime(void) {
    const char* env_keys[] = {
        "KAIN_UI_NATIVE_QT_RUNTIME",
        "KAIN_QT_QML_RUNTIME",
        "KAIN_QT_RUNTIME",
        "QT_RUNTIME"
    };
    for (const char* env_key : env_keys) {
        const char* candidate = std::getenv(env_key);
        if (candidate != nullptr && candidate[0] != '\0' && KAIN_VENDOR_ACCESS(candidate, 0) == 0) {
            return 1;
        }
    }
    return 0;
}

static int kain_vendor_external_qt_runtime_available(void) {
    if (kain_vendor_env_has_qt_runtime()) {
        return 1;
    }
    return kain_vendor_path_has_binary("qmlscene") || kain_vendor_path_has_binary("qml");
}

static int kain_vendor_env_or_binary_available(
    const char* const* env_keys,
    size_t env_key_count,
    const char* const* binary_names,
    size_t binary_count
) {
    size_t index;

    if (env_keys != nullptr) {
        for (index = 0; index < env_key_count; ++index) {
            const char* candidate = std::getenv(env_keys[index]);
            if (candidate != nullptr && candidate[0] != '\0' && KAIN_VENDOR_ACCESS(candidate, 0) == 0) {
                return 1;
            }
        }
    }

    if (binary_names != nullptr) {
        for (index = 0; index < binary_count; ++index) {
            if (kain_vendor_path_has_binary(binary_names[index])) {
                return 1;
            }
        }
    }

    return 0;
}

#if KAIN_VENDOR_HAS_IMGUI && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
const char* kain_vendor_imgui_version_string(void) {
    return ImGui::GetVersion();
}

int kain_vendor_imgui_probe(void) {
    bool ok = false;
    IMGUI_CHECKVERSION();
    ImGuiContext* context = ImGui::CreateContext();
    if (context != nullptr) {
        ok = ImGui::GetVersion() != nullptr;
        ImGui::DestroyContext(context);
    }
    return ok ? 1 : 0;
}
#else
const char* kain_vendor_imgui_version_string(void) {
    return "imgui-unavailable";
}

int kain_vendor_imgui_probe(void) {
    return 0;
}
#endif

#if KAIN_VENDOR_HAS_YOGA && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
const char* kain_vendor_yoga_version_string(void) {
    return "yoga-runtime";
}

int kain_vendor_yoga_probe(void) {
    YGConfigRef config = YGConfigNew();
    YGNodeRef node = nullptr;
    if (config == nullptr) {
        return 0;
    }

    node = YGNodeNewWithConfig(config);
    if (node == nullptr) {
        YGConfigFree(config);
        return 0;
    }

    YGNodeStyleSetWidth(node, 128.0f);
    YGNodeStyleSetHeight(node, 48.0f);
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);

    YGNodeFree(node);
    YGConfigFree(config);
    return 1;
}
#else
const char* kain_vendor_yoga_version_string(void) {
    return "yoga-unavailable";
}

int kain_vendor_yoga_probe(void) {
    return 0;
}
#endif

const char* kain_vendor_rmlui_version_string(void) {
    static const char* const env_keys[] = {"KAIN_RMLUI_RUNTIME", "RMLUI_RUNTIME"};
    static const char* const binary_names[] = {"rmlui"};
    return kain_vendor_env_or_binary_available(env_keys, 2, binary_names, 1)
        ? "rmlui-bridge"
        : "rmlui-bridge-unavailable";
}

int kain_vendor_rmlui_probe(void) {
    static const char* const env_keys[] = {"KAIN_RMLUI_RUNTIME", "RMLUI_RUNTIME"};
    static const char* const binary_names[] = {"rmlui"};
    return kain_vendor_env_or_binary_available(env_keys, 2, binary_names, 1);
}

const char* kain_vendor_skia_version_string(void) {
    static const char* const env_keys[] = {"KAIN_SKIA_RUNTIME", "SKIA_RUNTIME"};
    static const char* const binary_names[] = {"skia", "skia-viewer"};
    return kain_vendor_env_or_binary_available(env_keys, 2, binary_names, 2)
        ? "skia-bridge"
        : "skia-bridge-unavailable";
}

int kain_vendor_skia_probe(void) {
    static const char* const env_keys[] = {"KAIN_SKIA_RUNTIME", "SKIA_RUNTIME"};
    static const char* const binary_names[] = {"skia", "skia-viewer"};
    return kain_vendor_env_or_binary_available(env_keys, 2, binary_names, 2);
}

const char* kain_vendor_slint_version_string(void) {
    static const char* const env_keys[] = {"KAIN_SLINT_RUNTIME", "SLINT_RUNTIME"};
    static const char* const binary_names[] = {"slint-viewer", "slint"};
    return kain_vendor_env_or_binary_available(env_keys, 2, binary_names, 2)
        ? "slint-bridge"
        : "slint-bridge-unavailable";
}

int kain_vendor_slint_probe(void) {
    static const char* const env_keys[] = {"KAIN_SLINT_RUNTIME", "SLINT_RUNTIME"};
    static const char* const binary_names[] = {"slint-viewer", "slint"};
    return kain_vendor_env_or_binary_available(env_keys, 2, binary_names, 2);
}

const char* kain_vendor_qt_version_string(void) {
    return kain_vendor_external_qt_runtime_available()
        ? "qt-external-runtime"
        : "qt-external-runtime-unavailable";
}

int kain_vendor_qt_probe(void) {
    return kain_vendor_external_qt_runtime_available();
}

const char* kain_vendor_cef_version_string(void) {
    static const char* const env_keys[] = {"KAIN_CEF_RUNTIME", "CEF_RUNTIME"};
    static const char* const binary_names[] = {"cefclient", "cefsimple"};
    return kain_vendor_env_or_binary_available(env_keys, 2, binary_names, 2)
        ? "cef-bridge"
        : "cef-bridge-unavailable";
}

int kain_vendor_cef_probe(void) {
    static const char* const env_keys[] = {"KAIN_CEF_RUNTIME", "CEF_RUNTIME"};
    static const char* const binary_names[] = {"cefclient", "cefsimple"};
    return kain_vendor_env_or_binary_available(env_keys, 2, binary_names, 2);
}

} /* extern "C" */
