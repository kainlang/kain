// vst3_host_bridge.c — VST3 SDK host wrapper for reson8
//
// Compiles against vendored VST3 SDK at ../../3rdparty/vst3_sdk/
// Wraps the COM-heavy VST3 host API into flat C functions.
//
// The VST3 SDK is C++/COM. This file is compiled as C++ (or as C with
// internal C++ helpers) and exposes a pure C surface via extern "C".

// ── VST3 SDK includes ──
// Paths are relative to this file; the KAIN.toml [c_ffi] adds the SDK roots.
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ustring.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugfactory.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include "vst3_host_bridge.h"

#include <map>
#include <string>
#include <vector>
#include <cstring>

// ── Internal handle table ──
// We use integer handles to hide COM pointers from the C API.

struct FactoryEntry {
    std::string module_path;
    VST3::Hosting::Module::Ptr module;
    VST3::Hosting::PluginFactory factory;
    bool loaded;
};

struct InstanceEntry {
    int factory_handle;
    VST3::Hosting::Module::Ptr module; // keep module alive
    Steinberg::Vst::IComponent* component;
    Steinberg::Vst::IAudioProcessor* processor;
    Steinberg::Vst::IEditController* controller;
    bool activated;
    int sample_rate;
    int block_size;
    bool editor_open;
};

static std::map<int, FactoryEntry>  g_factories;
static std::map<int, InstanceEntry> g_instances;
static int g_next_factory_handle  = 1;
static int g_next_instance_handle = 1;
static char g_last_error[512] = "";
static int  g_last_status = 0;

// ── Helpers ──
static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}

// ── Plugin scanning ──

int vst3_host_scan_directory(const char* path, const char** out_names,
                              const char** out_paths, int capacity)
{
    // VST3 scanning: enumerate .vst3 bundles in the directory.
    // This is a platform-dependent operation — file system enumeration.
    // For now, return 0 (no plugins found — scanning requires FS enumeration
    // which will be fleshed out in the full implementation).
    (void)path;
    (void)out_names;
    (void)out_paths;
    (void)capacity;
    g_last_status = 0;
    return 0;
}

// ── Factory lifecycle ──

int vst3_host_load_factory(const char* plugin_path)
{
    std::string error;
    auto module = VST3::Hosting::Module::create(plugin_path, error);
    if (!module) {
        set_error("Failed to load module: %s — %s", plugin_path, error.c_str());
        g_last_status = -1;
        return -1;
    }

    auto factory = VST3::Hosting::PluginFactory(module);
    if (!factory.getFactory()) {
        set_error("No plugin factory in module: %s", plugin_path);
        g_last_status = -2;
        return -2;
    }

    int handle = g_next_factory_handle++;
    FactoryEntry entry;
    entry.module_path = plugin_path;
    entry.module = module;
    entry.factory = factory;
    entry.loaded = true;
    g_factories[handle] = entry;

    g_last_status = 0;
    return handle;
}

int vst3_host_class_count(int factory_handle)
{
    auto it = g_factories.find(factory_handle);
    if (it == g_factories.end() || !it->second.loaded) {
        set_error("Invalid factory handle: %d", factory_handle);
        g_last_status = -3;
        return -1;
    }
    int count = it->second.factory.getClassCount();
    g_last_status = 0;
    return count;
}

const char* vst3_host_class_name(int factory_handle, int class_index)
{
    static char name_buf[256];
    auto it = g_factories.find(factory_handle);
    if (it == g_factories.end()) {
        return "(invalid factory)";
    }
    if (class_index < 0 || class_index >= it->second.factory.getClassCount()) {
        return "(invalid class index)";
    }
    Steinberg::Vst::ClassInfo info;
    if (it->second.factory.getClassInfo(class_index, &info) != Steinberg::kResultOk) {
        return "(getClassInfo failed)";
    }
    // Convert TChar (UTF-16 on Windows) to UTF-8
    Steinberg::String str(info.name);
    str.copyTo8(name_buf, 0, sizeof(name_buf));
    return name_buf;
}

const char* vst3_host_class_category(int factory_handle, int class_index)
{
    auto it = g_factories.find(factory_handle);
    if (it == g_factories.end()) { return "invalid"; }
    Steinberg::Vst::ClassInfo info;
    if (it->second.factory.getClassInfo(class_index, &info) != Steinberg::kResultOk) {
        return "error";
    }
    // cardinals: kVstAudioEffectClass etc.
    return info.category;
}

const char* vst3_host_class_vendor(int factory_handle, int class_index)
{
    static char buf[256];
    auto it = g_factories.find(factory_handle);
    if (it == g_factories.end()) { return "(no factory)"; }
    Steinberg::Vst::ClassInfo info;
    if (it->second.factory.getClassInfo(class_index, &info) != Steinberg::kResultOk) {
        return "(error)";
    }
    Steinberg::String str(info.vendor);
    str.copyTo8(buf, 0, sizeof(buf));
    return buf;
}

const char* vst3_host_class_version(int factory_handle, int class_index)
{
    static char buf[64];
    auto it = g_factories.find(factory_handle);
    if (it == g_factories.end()) { return "0"; }
    Steinberg::Vst::ClassInfo info;
    if (it->second.factory.getClassInfo(class_index, &info) != Steinberg::kResultOk) {
        return "0";
    }
    Steinberg::String str(info.version);
    str.copyTo8(buf, 0, sizeof(buf));
    return buf;
}

void vst3_host_release_factory(int factory_handle)
{
    auto it = g_factories.find(factory_handle);
    if (it != g_factories.end()) {
        it->second.module.reset();
        g_factories.erase(it);
    }
    g_last_status = 0;
}

// ── Instance lifecycle ──

int vst3_host_create_instance(int factory_handle, int class_index)
{
    auto it = g_factories.find(factory_handle);
    if (it == g_factories.end() || !it->second.loaded) {
        set_error("Invalid factory handle: %d", factory_handle);
        g_last_status = -4;
        return -4;
    }

    Steinberg::Vst::ClassInfo info;
    if (it->second.factory.getClassInfo(class_index, &info) != Steinberg::kResultOk) {
        set_error("getClassInfo failed for index %d", class_index);
        g_last_status = -5;
        return -5;
    }

    auto component = it->second.factory.createInstance<Steinberg::Vst::IComponent>(info.cid);
    if (!component) {
        set_error("createInstance failed for class %s", info.name);
        g_last_status = -6;
        return -6;
    }

    // Query IAudioProcessor
    Steinberg::Vst::IAudioProcessor* processor = nullptr;
    component->queryInterface(Steinberg::Vst::IAudioProcessor::iid,
                              (void**)&processor);

    int handle = g_next_instance_handle++;
    InstanceEntry entry;
    entry.factory_handle = factory_handle;
    entry.module = it->second.module;
    entry.component = component;
    entry.processor = processor;
    entry.controller = nullptr;
    entry.activated = false;
    entry.sample_rate = 44100;
    entry.block_size = 256;
    entry.editor_open = false;
    g_instances[handle] = entry;

    g_last_status = 0;
    return handle;
}

int vst3_host_create_controller(int instance_handle)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) {
        set_error("Invalid instance handle: %d", instance_handle);
        g_last_status = -7;
        return -7;
    }

    if (it->second.controller) {
        return 0; // already created
    }

    Steinberg::Vst::IEditController* controller = nullptr;
    Steinberg::FUnknownPtr<Steinberg::Vst::IComponent> comp(it->second.component);
    if (comp.getUnknown()) {
        // Get controller from component
    }

    // For now: no controller (parameter access still works via IComponent)
    g_last_status = 0;
    return 0;
}

void vst3_host_release_instance(int instance_handle)
{
    auto it = g_instances.find(instance_handle);
    if (it != g_instances.end()) {
        if (it->second.controller) {
            it->second.controller->release();
        }
        if (it->second.processor) {
            it->second.processor->release();
        }
        if (it->second.component) {
            it->second.component->release();
        }
        g_instances.erase(it);
    }
    g_last_status = 0;
}

// ── Audio processing ──

int vst3_host_setup_processing(int instance_handle, int sample_rate, int max_block_size)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) {
        g_last_status = -8;
        return -8;
    }

    it->second.sample_rate = sample_rate;
    it->second.block_size = max_block_size;

    Steinberg::Vst::ProcessSetup setup;
    setup.processMode = Steinberg::Vst::kRealtime;
    setup.symbolicSampleSize = Steinberg::Vst::kSample32;
    setup.maxSamplesPerBlock = max_block_size;
    setup.sampleRate = (double)sample_rate;

    if (it->second.processor) {
        if (it->second.processor->setupProcessing(setup) != Steinberg::kResultOk) {
            set_error("setupProcessing failed");
            g_last_status = -9;
            return -9;
        }
    }

    g_last_status = 0;
    return 0;
}

int vst3_host_activate(int instance_handle, int activate)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) {
        g_last_status = -10;
        return -10;
    }

    if (it->second.component) {
        if (it->second.component->activateBus(Steinberg::Vst::kAudio,
                                               Steinberg::Vst::kInput, 0,
                                               activate ? 1 : 0) != Steinberg::kResultOk) {
            // May not have audio inputs — not an error for instruments
        }
        if (it->second.component->activateBus(Steinberg::Vst::kAudio,
                                               Steinberg::Vst::kOutput, 0,
                                               activate ? 1 : 0) != Steinberg::kResultOk) {
            // May not have audio outputs
        }
        it->second.component->setActive(activate ? Steinberg::TBool(1) : Steinberg::TBool(0));
    }

    it->second.activated = (activate != 0);
    g_last_status = 0;
    return 0;
}

int vst3_host_process(int instance_handle, const float* input, float* output, int frames)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end() || !it->second.processor) {
        g_last_status = -11;
        return -11;
    }

    // Build ProcessData
    VST3::Hosting::ProcessData pd;
    pd.prepare(*(it->second.component), frames, Steinberg::Vst::kSample32);

    // Fill input
    if (input && pd.inputs[0].buffer) {
        memcpy(pd.inputs[0].buffer, input, (size_t)(frames * pd.inputs[0].numChannels) * sizeof(float));
    }

    // Process
    if (it->second.processor->process(pd.processData) != Steinberg::kResultOk) {
        set_error("process() failed");
        g_last_status = -12;
        return -12;
    }

    // Copy output
    if (output && pd.outputs[0].buffer) {
        memcpy(output, pd.outputs[0].buffer, (size_t)(frames * pd.outputs[0].numChannels) * sizeof(float));
    }

    g_last_status = 0;
    return 0;
}

// ── Parameters ──

int vst3_host_param_count(int instance_handle)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) { return 0; }

    if (it->second.controller) {
        return it->second.controller->getParameterCount();
    }
    // Fallback: try component
    return 0; // Component doesn't expose parameter count directly in VST3
}

int vst3_host_param_id(int instance_handle, int index)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) { return -1; }

    if (it->second.controller) {
        Steinberg::Vst::ParameterInfo info;
        if (it->second.controller->getParameterInfo(index, info) == Steinberg::kResultOk) {
            return (int)info.id;
        }
    }
    return index; // fallback
}

const char* vst3_host_param_name(int instance_handle, int param_id)
{
    static char buf[256];
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) { return "(no instance)"; }

    if (it->second.controller) {
        Steinberg::Vst::ParameterInfo info;
        int count = it->second.controller->getParameterCount();
        for (int i = 0; i < count; i++) {
            if (it->second.controller->getParameterInfo(i, info) == Steinberg::kResultOk
                && info.id == param_id) {
                Steinberg::String str(info.title);
                str.copyTo8(buf, 0, sizeof(buf));
                return buf;
            }
        }
    }
    snprintf(buf, sizeof(buf), "param_%d", param_id);
    return buf;
}

const char* vst3_host_param_unit(int instance_handle, int param_id)
{
    static char buf[64];
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) { return ""; }

    if (it->second.controller) {
        Steinberg::Vst::ParameterInfo info;
        int count = it->second.controller->getParameterCount();
        for (int i = 0; i < count; i++) {
            if (it->second.controller->getParameterInfo(i, info) == Steinberg::kResultOk
                && info.id == param_id) {
                Steinberg::String str(info.units);
                str.copyTo8(buf, 0, sizeof(buf));
                return buf;
            }
        }
    }
    return "";
}

double vst3_host_param_value(int instance_handle, int param_id)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) { return 0.0; }

    if (it->second.controller) {
        return it->second.controller->getParamNormalized((Steinberg::Vst::ParamID)param_id);
    }
    return 0.0;
}

double vst3_host_param_default(int instance_handle, int param_id)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) { return 0.0; }

    if (it->second.controller) {
        Steinberg::Vst::ParameterInfo info;
        int count = it->second.controller->getParameterCount();
        for (int i = 0; i < count; i++) {
            if (it->second.controller->getParameterInfo(i, info) == Steinberg::kResultOk
                && info.id == param_id) {
                return (double)info.defaultNormalizedValue;
            }
        }
    }
    return 0.5;
}

int vst3_host_set_param(int instance_handle, int param_id, double value)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) {
        g_last_status = -13;
        return -13;
    }

    if (it->second.controller) {
        if (it->second.controller->setParamNormalized(
                (Steinberg::Vst::ParamID)param_id, value) == Steinberg::kResultOk) {
            g_last_status = 0;
            return 0;
        }
    }
    g_last_status = -14;
    return -14;
}

int vst3_host_param_step_count(int instance_handle, int param_id)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) { return 0; }

    if (it->second.controller) {
        Steinberg::Vst::ParameterInfo info;
        int count = it->second.controller->getParameterCount();
        for (int i = 0; i < count; i++) {
            if (it->second.controller->getParameterInfo(i, info) == Steinberg::kResultOk
                && info.id == param_id) {
                return (info.stepCount == 0) ? 0 : info.stepCount;
            }
        }
    }
    return 0; // continuous
}

// ── Editor ──

int vst3_host_open_editor(int instance_handle, void* parent_hwnd)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) { g_last_status = -15; return -15; }

    if (it->second.controller) {
        // VST3 editor uses IPtr<IPlugView>
        // controller->createView(...) returns IPlugView*
        // Then attach to parent HWND
        it->second.editor_open = true;
        g_last_status = 0;
        return 0;
    }
    g_last_status = -16;
    return -16;
}

int vst3_host_close_editor(int instance_handle)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) { g_last_status = -17; return -17; }
    it->second.editor_open = false;
    g_last_status = 0;
    return 0;
}

int vst3_host_editor_open(int instance_handle)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end()) return 0;
    return it->second.editor_open ? 1 : 0;
}

// ── Info ──

int vst3_host_latency_samples(int instance_handle)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end() || !it->second.component) return 0;
    return (int)it->second.component->getLatencySamples();
}

int vst3_host_tail_samples(int instance_handle)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end() || !it->second.processor) return 0;
    return (int)it->second.processor->getTailSamples();
}

int vst3_host_bus_count(int instance_handle, int input)
{
    auto it = g_instances.find(instance_handle);
    if (it == g_instances.end() || !it->second.component) return 0;
    return it->second.component->getBusCount(
        Steinberg::Vst::kAudio,
        input ? Steinberg::Vst::kInput : Steinberg::Vst::kOutput
    );
}

const char* vst3_host_last_error(void) { return g_last_error; }
int vst3_host_last_status(void)        { return g_last_status; }
