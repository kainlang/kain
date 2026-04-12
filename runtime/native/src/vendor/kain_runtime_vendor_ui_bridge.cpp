#include "../../include/kain_runtime_vendor_ui_bridge.h"
#include "../../include/kain_runtime_vendor_lane.h"

#if KAIN_VENDOR_HAS_IMGUI && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
#include <imgui.h>
#endif

#if KAIN_VENDOR_HAS_YOGA && !defined(KAIN_RUNTIME_VENDOR_STUBS_ONLY)
#include <yoga/Yoga.h>
#endif

extern "C" {

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
    return "rmlui-staged";
}

int kain_vendor_rmlui_probe(void) {
    return 0;
}

const char* kain_vendor_skia_version_string(void) {
    return "skia-core-staged";
}

int kain_vendor_skia_probe(void) {
    return 0;
}

const char* kain_vendor_slint_version_string(void) {
    return "slint-ui-staged";
}

int kain_vendor_slint_probe(void) {
    return 0;
}

const char* kain_vendor_qt_version_string(void) {
    return "qt-staged";
}

int kain_vendor_qt_probe(void) {
    return 0;
}

const char* kain_vendor_cef_version_string(void) {
    return "cef-staged";
}

int kain_vendor_cef_probe(void) {
    return 0;
}

} /* extern "C" */
