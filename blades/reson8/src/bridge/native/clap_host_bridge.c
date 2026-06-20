// clap_host_bridge.c — CLAP host wrapper for reson8
//
// CLAP is already a flat C ABI — no COM, no C++ inheritance.
// This bridge handles: module loading (dlopen/LoadLibrary), entry discovery,
// handle lifecycle, and param/proc dispatch through the clap_plugin struct.

#include <clap/clap.h>

// We don't include ext/ headers yet — the vendor path setup in KAIN.toml
// will make these resolvable. For now, we use the core clap.h.
// Extensions (audio-effect, params, gui) will be resolved at runtime via
// clap_plugin::get_extension().

#include "clap_host_bridge.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#define DL_HANDLE HMODULE
#define DL_OPEN(path) LoadLibraryA(path)
#define DL_SYM(handle, name) (void*)GetProcAddress(handle, name)
#define DL_CLOSE(handle) FreeLibrary(handle)
#else
#include <dlfcn.h>
#define DL_HANDLE void*
#define DL_OPEN(path) dlopen(path, RTLD_LAZY)
#define DL_SYM(handle, name) dlsym(handle, name)
#define DL_CLOSE(handle) dlclose(handle)
#endif

// ── Scanned entries ──
#define MAX_ENTRIES 256
static char g_entry_paths[MAX_ENTRIES][512];
static char g_entry_names[MAX_ENTRIES][256];
static int  g_entry_count = 0;

// ── Loaded instances ──
#define MAX_INSTANCES 64
typedef struct {
    char            path[512];
    DL_HANDLE       module;
    clap_plugin_entry_t* entry;
    clap_plugin_t*  plugin;
    clap_host_t     host;
    int             active;
    int             sample_rate;
    int             block_size;
} ClapInstance;

static ClapInstance g_instances[MAX_INSTANCES];
static int g_instance_count = 0;
static char g_last_error[512] = "";
static int  g_last_status = 0;

// ── Forward ──
static const clap_host_t g_clap_host;

// ── Host callbacks (stubs — the Kain side drives everything) ──
static void host_log(const clap_host_t* host, clap_log_severity severity,
                     const char* msg) {
    (void)host; (void)severity;
    snprintf(g_last_error, sizeof(g_last_error), "CLAP: %s", msg);
}
static void host_request_restart(const clap_host_t* host) {
    (void)host; // Kain actor handles restart
}
static void host_request_process(const clap_host_t* host) {
    (void)host; // Kain drives process calls
}
static void host_request_callback(const clap_host_t* host) {
    (void)host;
}
static const clap_host_t g_clap_host = {
    .clap_version = CLAP_VERSION,
    .name         = "reson8",
    .vendor       = "reson8 DAW",
    .url          = "https://reson8.dev",
    .version      = "0.1.0",
    .get_extension = NULL,
    .request_restart = host_request_restart,
    .request_process = host_request_process,
    .request_callback = host_request_callback,
};

// ── Scan ──

int clap_host_scan_directory(const char* path, const char** out_names, int capacity)
{
    // Platform-dependent directory enumeration.
    // For now, stub — full implementation will use:
    //   Windows: FindFirstFile/FindNextFile
    //   macOS: CFBundle or directory enumeration
    //   Linux: opendir/readdir
    (void)path; (void)out_names; (void)capacity;
    g_last_status = 0;
    return g_entry_count;
}

const char* clap_host_entry_path(int index)
{
    if (index >= 0 && index < g_entry_count) return g_entry_paths[index];
    return NULL;
}

// ── Load / Unload ──

int clap_host_load(int entry_index)
{
    if (entry_index < 0 || entry_index >= g_entry_count) {
        snprintf(g_last_error, sizeof(g_last_error), "Invalid entry index %d", entry_index);
        g_last_status = -1;
        return -1;
    }
    return clap_host_load_path(g_entry_paths[entry_index]);
}

int clap_host_load_path(const char* plugin_path)
{
    if (g_instance_count >= MAX_INSTANCES) {
        snprintf(g_last_error, sizeof(g_last_error), "Max instances reached (%d)", MAX_INSTANCES);
        g_last_status = -2;
        return -2;
    }

    DL_HANDLE module = DL_OPEN(plugin_path);
    if (!module) {
        snprintf(g_last_error, sizeof(g_last_error), "Failed to load: %s", plugin_path);
        g_last_status = -3;
        return -3;
    }

    clap_plugin_entry_t* entry = (clap_plugin_entry_t*)DL_SYM(module, "clap_entry");
    if (!entry) {
        snprintf(g_last_error, sizeof(g_last_error), "No clap_entry in: %s", plugin_path);
        DL_CLOSE(module);
        g_last_status = -4;
        return -4;
    }

    if (!entry->init || !entry->init(plugin_path) || !entry->get_factory) {
        DL_CLOSE(module);
        snprintf(g_last_error, sizeof(g_last_error), "Entry init/get_factory failed: %s", plugin_path);
        g_last_status = -5;
        return -5;
    }

    clap_plugin_factory_t* factory =
        (clap_plugin_factory_t*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!factory || !factory->create_plugin) {
        entry->deinit();
        DL_CLOSE(module);
        snprintf(g_last_error, sizeof(g_last_error), "No factory in: %s", plugin_path);
        g_last_status = -6;
        return -6;
    }

    clap_plugin_t* plugin = factory->create_plugin(&g_clap_host, plugin_path);
    if (!plugin || !plugin->init || !plugin->init(plugin)) {
        entry->deinit();
        DL_CLOSE(module);
        snprintf(g_last_error, sizeof(g_last_error), "create_plugin/init failed: %s", plugin_path);
        g_last_status = -7;
        return -7;
    }

    int handle = g_instance_count++;
    ClapInstance* inst = &g_instances[handle];
    memset(inst, 0, sizeof(ClapInstance));
    strncpy(inst->path, plugin_path, sizeof(inst->path) - 1);
    inst->module = module;
    inst->entry  = entry;
    inst->plugin = plugin;

    g_last_status = 0;
    return handle + 1; // 1-based handles
}

void clap_host_unload(int instance_handle)
{
    int idx = instance_handle - 1;
    if (idx < 0 || idx >= g_instance_count) return;

    ClapInstance* inst = &g_instances[idx];
    if (inst->plugin && inst->plugin->destroy) {
        inst->plugin->destroy(inst->plugin);
    }
    if (inst->entry && inst->entry->deinit) {
        inst->entry->deinit();
    }
    if (inst->module) {
        DL_CLOSE(inst->module);
    }
    memset(inst, 0, sizeof(ClapInstance));
    g_last_status = 0;
}

// ── Plugin info ──

#define GET_INST(idx) \
    int idx = instance_handle - 1; \
    if (idx < 0 || idx >= g_instance_count || !g_instances[idx].plugin) { \
        g_last_status = -8; return ""; }

const char* clap_host_name(int instance_handle) {
    GET_INST(i);
    return g_instances[i].plugin->desc->name;
}
const char* clap_host_vendor(int instance_handle) {
    GET_INST(i);
    return g_instances[i].plugin->desc->vendor;
}
const char* clap_host_version(int instance_handle) {
    GET_INST(i);
    return g_instances[i].plugin->desc->version;
}
const char* clap_host_description(int instance_handle) {
    GET_INST(i);
    return g_instances[i].plugin->desc->description;
}
int clap_host_feature_count(int instance_handle) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 0;
    int count = 0;
    const char* const* features = g_instances[i].plugin->desc->features;
    while (features && features[count]) count++;
    return count;
}
const char* clap_host_feature(int instance_handle, int index) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return "";
    const char* const* features = g_instances[i].plugin->desc->features;
    if (features && features[index]) return features[index];
    return "";
}

// ── Activation ──

int clap_host_activate(int instance_handle, int sample_rate,
                        int min_block_size, int max_block_size)
{
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count || !g_instances[i].plugin) {
        g_last_status = -9; return -9;
    }
    ClapInstance* inst = &g_instances[i];
    if (!inst->plugin->activate) {
        g_last_status = -10; return -10;
    }
    if (inst->plugin->activate(inst->plugin, (double)sample_rate,
                                (uint32_t)min_block_size,
                                (uint32_t)max_block_size)) {
        inst->active = 1;
        inst->sample_rate = sample_rate;
        inst->block_size = max_block_size;
        g_last_status = 0;
        return 0;
    }
    g_last_status = -11;
    return -11;
}

int clap_host_deactivate(int instance_handle)
{
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) { g_last_status = -12; return -12; }
    ClapInstance* inst = &g_instances[i];
    if (inst->plugin->deactivate) {
        inst->plugin->deactivate(inst->plugin);
    }
    inst->active = 0;
    g_last_status = 0;
    return 0;
}

// ── Processing ──

int clap_host_process(int instance_handle, const float* input, float* output,
                       int frames, int input_count, int output_count)
{
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count || !g_instances[i].active) {
        g_last_status = -13; return -13;
    }

    ClapInstance* inst = &g_instances[i];
    clap_process_t process = {0};

    // Audio buffers — CLAP uses float** for non-interleaved or float* for interleaved
    float* in_bufs[2]  = {NULL, NULL};
    float* out_bufs[2] = {NULL, NULL};

    // Use interleaved (CLAP_AUDIO_INPUT_PORT_TYPE)
    // Actually CLAP uses non-interleaved by default — we deinterleave/reinterleave
    float* temp_in  = NULL;
    float* temp_out = NULL;

    if (input_count > 0 && input) {
        temp_in = (float*)malloc((size_t)(frames * input_count) * sizeof(float));
        // Deinterleave input into per-channel buffers
        for (int ch = 0; ch < input_count && ch < 2; ch++) {
            in_bufs[ch] = temp_in + ch * frames;
            for (int f = 0; f < frames; f++) {
                in_bufs[ch][f] = input[f * input_count + ch];
            }
        }
    }

    if (output_count > 0) {
        temp_out = (float*)calloc((size_t)(frames * output_count), sizeof(float));
        for (int ch = 0; ch < output_count && ch < 2; ch++) {
            out_bufs[ch] = temp_out + ch * frames;
        }
    }

    process.audio_inputs_count  = (input_count > 0) ? 1 : 0;
    process.audio_outputs_count = (output_count > 0) ? 1 : 0;

    clap_audio_buffer_t in_audio  = { in_bufs, (uint32_t)frames, (uint32_t)frames };
    clap_audio_buffer_t out_audio = { out_bufs, (uint32_t)frames, (uint32_t)frames };

    process.audio_inputs  = &in_audio;
    process.audio_outputs = &out_audio;
    process.frames_count  = (uint32_t)frames;
    process.transport     = NULL;

    clap_process_status status = inst->plugin->process(inst->plugin, &process);

    // Reinterleave output
    if (output && temp_out) {
        for (int ch = 0; ch < output_count; ch++) {
            for (int f = 0; f < frames; f++) {
                output[f * output_count + ch] = out_bufs[ch][f];
            }
        }
    }

    free(temp_in);
    free(temp_out);

    g_last_status = 0;
    return (int)status;
}

// ── Parameters ──

int clap_host_param_count(int instance_handle) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 0;
    clap_plugin_t* p = g_instances[i].plugin;
    return p->params_count ? (int)p->params_count(p) : 0;
}

int clap_host_param_id(int instance_handle, int index) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return index;
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_info) return index;
    clap_param_info_t info;
    if (p->params_info(p, (uint32_t)index, &info)) return (int)info.id;
    return index;
}

const char* clap_host_param_name(int instance_handle, int param_id) {
    static char buf[256];
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return "";
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_info) { snprintf(buf, sizeof(buf), "p%d", param_id); return buf; }
    clap_param_info_t info;
    if (p->params_info(p, (uint32_t)param_id, &info)) {
        snprintf(buf, sizeof(buf), "%s", info.name);
        return buf;
    }
    snprintf(buf, sizeof(buf), "p%d", param_id);
    return buf;
}

const char* clap_host_param_module(int instance_handle, int param_id) {
    static char buf[256];
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return "";
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_info) { buf[0]='\0'; return buf; }
    clap_param_info_t info;
    if (p->params_info(p, (uint32_t)param_id, &info)) {
        snprintf(buf, sizeof(buf), "%s", info.module);
        return buf;
    }
    buf[0] = '\0';
    return buf;
}

double clap_host_param_value(int instance_handle, int param_id) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 0.0;
    clap_plugin_t* p = g_instances[i].plugin;
    return p->params_value ? p->params_value(p, (clap_id)param_id) : 0.0;
}

double clap_host_param_default(int instance_handle, int param_id) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 0.5;
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_info) return 0.5;
    clap_param_info_t info;
    if (p->params_info(p, (uint32_t)param_id, &info)) return info.default_value;
    return 0.5;
}

double clap_host_param_min(int instance_handle, int param_id) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 0.0;
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_info) return 0.0;
    clap_param_info_t info;
    if (p->params_info(p, (uint32_t)param_id, &info)) return info.min_value;
    return 0.0;
}

double clap_host_param_max(int instance_handle, int param_id) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 1.0;
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_info) return 1.0;
    clap_param_info_t info;
    if (p->params_info(p, (uint32_t)param_id, &info)) return info.max_value;
    return 1.0;
}

int clap_host_set_param(int instance_handle, int param_id, double value) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) { g_last_status = -14; return -14; }
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_value) { g_last_status = -15; return -15; }
    p->params_value(p, (clap_id)param_id, value);
    g_last_status = 0;
    return 0;
}

int clap_host_param_is_stepped(int instance_handle, int param_id) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 0;
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_info) return 0;
    clap_param_info_t info;
    if (p->params_info(p, (uint32_t)param_id, &info))
        return (info.flags & CLAP_PARAM_IS_STEPPED) ? 1 : 0;
    return 0;
}

int clap_host_param_is_periodic(int instance_handle, int param_id) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 0;
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_info) return 0;
    clap_param_info_t info;
    if (p->params_info(p, (uint32_t)param_id, &info))
        return (info.flags & CLAP_PARAM_IS_PERIODIC) ? 1 : 0;
    return 0;
}

int clap_host_param_is_hidden(int instance_handle, int param_id) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 0;
    clap_plugin_t* p = g_instances[i].plugin;
    if (!p->params_info) return 0;
    clap_param_info_t info;
    if (p->params_info(p, (uint32_t)param_id, &info))
        return (info.flags & CLAP_PARAM_IS_HIDDEN) ? 1 : 0;
    return 0;
}

// ── State ──

int clap_host_state_save(int instance_handle, char* buffer, int buffer_size)
{
    (void)instance_handle; (void)buffer; (void)buffer_size;
    // Requires clap_plugin_state extension — stub
    g_last_status = -16;
    return -16;
}

int clap_host_state_load(int instance_handle, const char* buffer, int buffer_size)
{
    (void)instance_handle; (void)buffer; (void)buffer_size;
    g_last_status = -17;
    return -17;
}

// ── GUI ──

int clap_host_gui_open(int instance_handle, void* parent_window)
{
    (void)instance_handle; (void)parent_window;
    // Requires clap_plugin_gui extension
    g_last_status = -18;
    return -18;
}

int clap_host_gui_close(int instance_handle)
{
    (void)instance_handle;
    g_last_status = -19;
    return -19;
}

int clap_host_gui_is_open(int instance_handle)   { (void)instance_handle; return 0; }
int clap_host_gui_can_resize(int instance_handle) { (void)instance_handle; return 0; }
int clap_host_gui_get_size(int instance_handle, int* width, int* height) {
    (void)instance_handle; *width = 0; *height = 0; return -20;
}
int clap_host_gui_set_size(int instance_handle, int width, int height) {
    (void)instance_handle; (void)width; (void)height; return -21;
}

// ── Latency ──

int clap_host_latency(int instance_handle) {
    int i = instance_handle - 1;
    if (i < 0 || i >= g_instance_count) return 0;
    clap_plugin_t* p = g_instances[i].plugin;
    return p->latency ? (int)p->latency(p) : 0;
}

const char* clap_host_last_error(void) { return g_last_error; }
int clap_host_last_status(void)        { return g_last_status; }
