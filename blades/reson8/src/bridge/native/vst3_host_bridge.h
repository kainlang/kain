// vst3_host_bridge.h — Flat C API over Steinberg VST3 SDK for reson8
//
// Wraps the VST3 SDK (vendored at ../../3rdparty/vst3_sdk/) into a flat C API.
// All COM initialization, FUnknown casting, and factory enumeration happens
// inside vst3_host_bridge.c. Kain sees only flat functions and integer handles.
//
// The VST3 SDK is C++/COM. This bridge compiles the relevant host classes
// internally and exposes a pure C surface.

#ifndef KAIN_VST3_HOST_BRIDGE_H
#define KAIN_VST3_HOST_BRIDGE_H

#if defined(_WIN32)
#define KAIN_VST3_EXPORT __declspec(dllexport)
#else
#define KAIN_VST3_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ── Handle types (opaque integers — bridge owns the real objects) ──
// A "factory_handle" identifies a loaded VST3 module and its plugin factory.
// An "instance_handle" is one instantiated plugin.

// ── Plugin scan ──
// Scan a directory for VST3 bundles (.vst3 folders on Windows, .vst3 bundles on macOS).
// Returns count of discovered plugins, fills provided arrays up to capacity.
KAIN_VST3_EXPORT int vst3_host_scan_directory(
    const char* path,
    const char** out_names,    // array of plugin name strings (bridge allocates)
    const char** out_paths,    // array of plugin path strings
    int capacity
);

// ── Factory ──
// Load a VST3 module and get its plugin factory.
// Returns factory_handle (>= 1) or negative on error.
KAIN_VST3_EXPORT int vst3_host_load_factory(const char* plugin_path);

// Get info about a plugin class in the factory.
KAIN_VST3_EXPORT int vst3_host_class_count(int factory_handle);
KAIN_VST3_EXPORT const char* vst3_host_class_name(int factory_handle, int class_index);
KAIN_VST3_EXPORT int vst3_host_class_category(int factory_handle, int class_index); // 0=audio_effect, 1=instrument, 2=controller
KAIN_VST3_EXPORT const char* vst3_host_class_vendor(int factory_handle, int class_index);
KAIN_VST3_EXPORT const char* vst3_host_class_version(int factory_handle, int class_index);

// Release a factory and its loaded module.
KAIN_VST3_EXPORT void vst3_host_release_factory(int factory_handle);

// ── Instance ──
// Create a plugin instance from a factory. Returns instance_handle (>= 1) or error.
KAIN_VST3_EXPORT int vst3_host_create_instance(int factory_handle, int class_index);

// Get the controller component (editor) for an instance.
KAIN_VST3_EXPORT int vst3_host_create_controller(int instance_handle);

// ── Audio processing ──
// Setup: set sampling rate and max block size before calling process.
KAIN_VST3_EXPORT int vst3_host_setup_processing(int instance_handle, int sample_rate, int max_block_size);

// Activate/deactivate the audio processing.
KAIN_VST3_EXPORT int vst3_host_activate(int instance_handle, int activate); // 1=on, 0=off

// Process a block of audio. Input and output are interleaved float arrays.
// frames must be <= max_block_size set in setup_processing.
KAIN_VST3_EXPORT int vst3_host_process(
    int instance_handle,
    const float* input,
    float* output,
    int frames
);

// ── Parameters ──
KAIN_VST3_EXPORT int vst3_host_param_count(int instance_handle);
KAIN_VST3_EXPORT int vst3_host_param_id(int instance_handle, int index);
KAIN_VST3_EXPORT const char* vst3_host_param_name(int instance_handle, int param_id);
KAIN_VST3_EXPORT const char* vst3_host_param_unit(int instance_handle, int param_id);
KAIN_VST3_EXPORT double vst3_host_param_value(int instance_handle, int param_id);
KAIN_VST3_EXPORT double vst3_host_param_default(int instance_handle, int param_id);
KAIN_VST3_EXPORT int vst3_host_set_param(int instance_handle, int param_id, double value);
KAIN_VST3_EXPORT int vst3_host_param_step_count(int instance_handle, int param_id); // 0 = continuous

// ── Editor (GUI) ──
// Open the plugin's editor window. parent_hwnd is the host window handle.
KAIN_VST3_EXPORT int vst3_host_open_editor(int instance_handle, void* parent_hwnd);
KAIN_VST3_EXPORT int vst3_host_close_editor(int instance_handle);
KAIN_VST3_EXPORT int vst3_host_editor_open(int instance_handle); // 1 = visible

// ── Lifecycle ──
KAIN_VST3_EXPORT void vst3_host_release_instance(int instance_handle);

// ── Info ──
KAIN_VST3_EXPORT int vst3_host_latency_samples(int instance_handle);
KAIN_VST3_EXPORT int vst3_host_tail_samples(int instance_handle);
KAIN_VST3_EXPORT int vst3_host_bus_count(int instance_handle, int input); // 1=input, 0=output

// ── Error ──
KAIN_VST3_EXPORT const char* vst3_host_last_error(void);
KAIN_VST3_EXPORT int vst3_host_last_status(void);

#ifdef __cplusplus
}
#endif

#endif // KAIN_VST3_HOST_BRIDGE_H
