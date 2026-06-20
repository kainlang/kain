// clap_host_bridge.h — CLAP host API wrapper for reson8
//
// CLAP (https://github.com/free-audio/clap) is already a flat C ABI.
// This bridge provides handle lifecycle management, scanning, and a
// consistent API surface that matches the VST3 bridge for pluggable dispatch.
//
// Vendored CLAP SDK: ../../3rdparty/clap/

#ifndef KAIN_CLAP_HOST_BRIDGE_H
#define KAIN_CLAP_HOST_BRIDGE_H

#if defined(_WIN32)
#define KAIN_CLAP_EXPORT __declspec(dllexport)
#else
#define KAIN_CLAP_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ── Plugin scan ──
KAIN_CLAP_EXPORT int clap_host_scan_directory(
    const char* path,
    const char** out_names,
    int capacity
);
KAIN_CLAP_EXPORT const char* clap_host_entry_path(int index);

// ── Load / Unload ──
KAIN_CLAP_EXPORT int clap_host_load(int entry_index);
KAIN_CLAP_EXPORT int clap_host_load_path(const char* plugin_path);
KAIN_CLAP_EXPORT void clap_host_unload(int instance_handle);

// ── Plugin info ──
KAIN_CLAP_EXPORT const char* clap_host_name(int instance_handle);
KAIN_CLAP_EXPORT const char* clap_host_vendor(int instance_handle);
KAIN_CLAP_EXPORT const char* clap_host_version(int instance_handle);
KAIN_CLAP_EXPORT const char* clap_host_description(int instance_handle);
KAIN_CLAP_EXPORT int clap_host_feature_count(int instance_handle);
KAIN_CLAP_EXPORT const char* clap_host_feature(int instance_handle, int index);

// ── Activation ──
KAIN_CLAP_EXPORT int clap_host_activate(int instance_handle,
                                         int sample_rate,
                                         int min_block_size,
                                         int max_block_size);
KAIN_CLAP_EXPORT int clap_host_deactivate(int instance_handle);

// ── Processing ──
KAIN_CLAP_EXPORT int clap_host_process(int instance_handle,
                                        const float* input,
                                        float* output,
                                        int frames,
                                        int input_count,  // mono=1, stereo=2
                                        int output_count);

// ── Parameters ──
KAIN_CLAP_EXPORT int clap_host_param_count(int instance_handle);
KAIN_CLAP_EXPORT int clap_host_param_id(int instance_handle, int index);
KAIN_CLAP_EXPORT const char* clap_host_param_name(int instance_handle, int param_id);
KAIN_CLAP_EXPORT const char* clap_host_param_module(int instance_handle, int param_id);
KAIN_CLAP_EXPORT double clap_host_param_value(int instance_handle, int param_id);
KAIN_CLAP_EXPORT double clap_host_param_default(int instance_handle, int param_id);
KAIN_CLAP_EXPORT double clap_host_param_min(int instance_handle, int param_id);
KAIN_CLAP_EXPORT double clap_host_param_max(int instance_handle, int param_id);
KAIN_CLAP_EXPORT int clap_host_set_param(int instance_handle, int param_id, double value);
KAIN_CLAP_EXPORT int clap_host_param_is_stepped(int instance_handle, int param_id);
KAIN_CLAP_EXPORT int clap_host_param_is_periodic(int instance_handle, int param_id);
KAIN_CLAP_EXPORT int clap_host_param_is_hidden(int instance_handle, int param_id);

// ── State (preset save/load) ──
KAIN_CLAP_EXPORT int clap_host_state_save(int instance_handle,
                                           char* buffer, int buffer_size);
KAIN_CLAP_EXPORT int clap_host_state_load(int instance_handle,
                                           const char* buffer, int buffer_size);

// ── GUI ──
KAIN_CLAP_EXPORT int clap_host_gui_open(int instance_handle, void* parent_window);
KAIN_CLAP_EXPORT int clap_host_gui_close(int instance_handle);
KAIN_CLAP_EXPORT int clap_host_gui_is_open(int instance_handle);
KAIN_CLAP_EXPORT int clap_host_gui_can_resize(int instance_handle);
KAIN_CLAP_EXPORT int clap_host_gui_get_size(int instance_handle,
                                             int* width, int* height);
KAIN_CLAP_EXPORT int clap_host_gui_set_size(int instance_handle,
                                             int width, int height);

// ── Latency ──
KAIN_CLAP_EXPORT int clap_host_latency(int instance_handle);

// ── Error ──
KAIN_CLAP_EXPORT const char* clap_host_last_error(void);
KAIN_CLAP_EXPORT int clap_host_last_status(void);

#ifdef __cplusplus
}
#endif

#endif // KAIN_CLAP_HOST_BRIDGE_H
