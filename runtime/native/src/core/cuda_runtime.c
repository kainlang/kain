#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/cuda_runtime.h"
#include "../../include/base.h"
#include "../../include/win32.h"

#ifdef _WIN32
typedef HMODULE KainCudaRuntimeLibrary;
#else
#include <dlfcn.h>
typedef void* KainCudaRuntimeLibrary;
#endif

#define KAIN_CUDA_RUNTIME_PATH_MAX 1024u
#define KAIN_CUDA_RUNTIME_KIND_MAX 64u
#define KAIN_CUDA_RUNTIME_MESSAGE_MAX 512u

typedef int (*KainGpuRuntimeDispatchPtxPersistedFn)(
    const KainGpuRuntimeDispatchRequest* request,
    KainGpuRuntimeDispatchResult* result
);

static int64_t g_cuda_last_status = 0;
static int64_t g_cuda_last_dispatch_invocations = 0;
static int64_t g_cuda_last_tensor_binding_count = 0;
static int64_t g_cuda_last_stream_binding_count = 0;
static int64_t g_cuda_last_neural_node_count = 0;
static int64_t g_cuda_last_output_binding_count = 0;
static int64_t g_cuda_last_total_output_bytes = 0;
static char g_cuda_last_error_kind[KAIN_CUDA_RUNTIME_KIND_MAX] = "ok";
static char g_cuda_last_error_message[KAIN_CUDA_RUNTIME_MESSAGE_MAX] = "";
static char g_cuda_last_runtime_library_path[KAIN_CUDA_RUNTIME_PATH_MAX] = "";
static char g_cuda_last_shader_bundle_path[KAIN_CUDA_RUNTIME_PATH_MAX] = "";
static char g_cuda_last_compute_residency_path[KAIN_CUDA_RUNTIME_PATH_MAX] = "";

static void cuda_copy_text(char* destination, size_t capacity, const char* source) {
    if (destination == NULL || capacity == 0u) {
        return;
    }
    if (source == NULL) {
        source = "";
    }
    snprintf(destination, capacity, "%s", source);
}

static int64_t cuda_set_status(
    int64_t status,
    const char* kind,
    const char* message
) {
    g_cuda_last_status = status;
    cuda_copy_text(
        g_cuda_last_error_kind,
        sizeof(g_cuda_last_error_kind),
        kind
    );
    cuda_copy_text(
        g_cuda_last_error_message,
        sizeof(g_cuda_last_error_message),
        message
    );
    return status;
}

static int64_t cuda_ok(const char* message) {
    return cuda_set_status(0, "ok", message ? message : "");
}

static void cuda_reset_dispatch_stats(void) {
    g_cuda_last_dispatch_invocations = 0;
    g_cuda_last_tensor_binding_count = 0;
    g_cuda_last_stream_binding_count = 0;
    g_cuda_last_neural_node_count = 0;
    g_cuda_last_output_binding_count = 0;
    g_cuda_last_total_output_bytes = 0;
}

static void cuda_store_paths(
    const char* runtime_library_path,
    const char* shader_bundle_path,
    const char* compute_residency_path
) {
    cuda_copy_text(
        g_cuda_last_runtime_library_path,
        sizeof(g_cuda_last_runtime_library_path),
        runtime_library_path
    );
    cuda_copy_text(
        g_cuda_last_shader_bundle_path,
        sizeof(g_cuda_last_shader_bundle_path),
        shader_bundle_path
    );
    cuda_copy_text(
        g_cuda_last_compute_residency_path,
        sizeof(g_cuda_last_compute_residency_path),
        compute_residency_path
    );
}

static int cuda_file_exists(const char* path) {
    FILE* file = NULL;
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    if (fopen_s(&file, path, "rb") != 0 || file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

static int cuda_build_executable_sibling_path(
    const char* file_name,
    char* out_path,
    size_t out_cap
) {
    char exe_path[KAIN_CUDA_RUNTIME_PATH_MAX];
    char* last_backslash;
    char* last_slash;
    char* last_sep;
    size_t prefix_len;
    size_t file_name_len;

    if (file_name == NULL || file_name[0] == '\0' || out_path == NULL || out_cap == 0u) {
        return 0;
    }
    out_path[0] = '\0';
#ifdef _WIN32
    if (!kain_win32_get_executable_path(exe_path, sizeof(exe_path))) {
        return 0;
    }
#else
    {
        ssize_t length = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1u);
        if (length <= 0 || (size_t)length >= sizeof(exe_path)) {
            return 0;
        }
        exe_path[length] = '\0';
    }
#endif

    last_backslash = strrchr(exe_path, '\\');
    last_slash = strrchr(exe_path, '/');
    last_sep = last_backslash;
    if (last_sep == NULL || (last_slash != NULL && last_slash > last_sep)) {
        last_sep = last_slash;
    }
    if (last_sep == NULL) {
        return 0;
    }

    prefix_len = (size_t)(last_sep - exe_path) + 1u;
    file_name_len = strlen(file_name);
    if (prefix_len + file_name_len + 1u > out_cap) {
        return 0;
    }

    memcpy(out_path, exe_path, prefix_len);
    memcpy(out_path + prefix_len, file_name, file_name_len + 1u);
    return 1;
}

static int cuda_resolve_env_path(
    const char* env_name,
    char* out_path,
    size_t out_cap
) {
    char* value = NULL;
    if (env_name == NULL || env_name[0] == '\0' || out_path == NULL || out_cap == 0u) {
        return 0;
    }
    out_path[0] = '\0';
    value = kain_env_dup(env_name);
    if (value == NULL || value[0] == '\0') {
        kain_env_free(value);
        return 0;
    }
    cuda_copy_text(out_path, out_cap, value);
    kain_env_free(value);
    return 1;
}

static int cuda_resolve_artifact_path(
    const char* primary_env,
    const char* fallback_env,
    const char* file_name,
    char* out_path,
    size_t out_cap
) {
    if (out_path == NULL || out_cap == 0u) {
        return 0;
    }
    out_path[0] = '\0';
    if (cuda_resolve_env_path(primary_env, out_path, out_cap) && cuda_file_exists(out_path)) {
        return 1;
    }
    if (cuda_resolve_env_path(fallback_env, out_path, out_cap) && cuda_file_exists(out_path)) {
        return 1;
    }
    if (cuda_build_executable_sibling_path(file_name, out_path, out_cap) &&
        cuda_file_exists(out_path)) {
        return 1;
    }
    out_path[0] = '\0';
    return 0;
}

static int cuda_resolve_runtime_library_path(char* out_path, size_t out_cap) {
    if (out_path == NULL || out_cap == 0u) {
        return 0;
    }
    out_path[0] = '\0';
    if (cuda_resolve_env_path(KAIN_GPU_RUNTIME_LIBRARY_ENV, out_path, out_cap) &&
        cuda_file_exists(out_path)) {
        return 1;
    }
#ifdef _WIN32
    if (cuda_build_executable_sibling_path(
            KAIN_GPU_RUNTIME_WINDOWS_DLL,
            out_path,
            out_cap
        ) &&
        cuda_file_exists(out_path)) {
        return 1;
    }
#else
    if (cuda_build_executable_sibling_path(KAIN_GPU_RUNTIME_LINUX_SO, out_path, out_cap) &&
        cuda_file_exists(out_path)) {
        return 1;
    }
#endif
    out_path[0] = '\0';
    return 0;
}

static int cuda_open_driver_probe(void) {
#ifdef _WIN32
    HMODULE library = LoadLibraryA("nvcuda.dll");
    if (library == NULL) {
        return 0;
    }
    FreeLibrary(library);
    return 1;
#else
    void* library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
    if (library == NULL) {
        return 0;
    }
    dlclose(library);
    return 1;
#endif
}

static int cuda_open_runtime_library(
    const char* path,
    KainCudaRuntimeLibrary* out_library,
    char* out_message,
    size_t out_message_cap
) {
    if (out_library == NULL) {
        return 0;
    }
    *out_library = NULL;
    if (path == NULL || path[0] == '\0') {
        cuda_copy_text(out_message, out_message_cap, "kain-gpu-runtime library path was empty");
        return 0;
    }
#ifdef _WIN32
    *out_library = LoadLibraryA(path);
    if (*out_library == NULL) {
        DWORD error_code = GetLastError();
        snprintf(
            out_message,
            out_message_cap,
            "failed to load kain-gpu-runtime '%s' (Win32 error %lu)",
            path,
            (unsigned long)error_code
        );
        return 0;
    }
#else
    dlerror();
    *out_library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (*out_library == NULL) {
        const char* error_text = dlerror();
        snprintf(
            out_message,
            out_message_cap,
            "failed to load kain-gpu-runtime '%s': %s",
            path,
            error_text ? error_text : "unknown dynamic loader error"
        );
        return 0;
    }
#endif
    cuda_copy_text(out_message, out_message_cap, "");
    return 1;
}

static void cuda_close_runtime_library(KainCudaRuntimeLibrary library) {
    if (library == NULL) {
        return;
    }
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
}

static void* cuda_resolve_runtime_symbol(
    KainCudaRuntimeLibrary library,
    const char* symbol_name,
    char* out_message,
    size_t out_message_cap
) {
    void* symbol = NULL;
    if (library == NULL || symbol_name == NULL || symbol_name[0] == '\0') {
        cuda_copy_text(out_message, out_message_cap, "invalid runtime symbol lookup");
        return NULL;
    }
#ifdef _WIN32
    symbol = (void*)GetProcAddress(library, symbol_name);
#else
    dlerror();
    symbol = dlsym(library, symbol_name);
#endif
    if (symbol == NULL) {
        snprintf(
            out_message,
            out_message_cap,
            "kain-gpu-runtime did not export '%s'",
            symbol_name
        );
        return NULL;
    }
    cuda_copy_text(out_message, out_message_cap, "");
    return symbol;
}

// ============================================================================
//                    PIPELINE CACHE — Linked List Storage
// ============================================================================
// PipelineCacheEntry caches compiled GPU pipelines (VkPipeline / CUfunction)
// so they can be reused across multiple dispatch calls.
// Managed by the GPU executor, populated during pipeline registration.
// TODO: mutex for thread safety (Phase 1: single-threaded)

typedef struct PipelineCacheEntry {
    char library_name[256];
    char compute_key[256];
    void* module_handle;       // VkShaderModule* or CUmodule
    void* pipeline_handle;     // VkPipeline* or CUfunction
    uint32_t dispatch_size[3]; // [x, y, z] workgroup count
    uint32_t ref_count;        // Reference count for library sharing
    struct PipelineCacheEntry* next;
} PipelineCacheEntry;

// Global pipeline cache — a singly-linked list
static PipelineCacheEntry* g_pipeline_cache_head = NULL;

/// Find a pipeline cache entry by library name and compute key.
/// Returns NULL if not found.
static PipelineCacheEntry* pipeline_cache_find(
    const char* library_name,
    const char* compute_key
) {
    PipelineCacheEntry* entry = g_pipeline_cache_head;
    while (entry != NULL) {
        if (strncmp(entry->library_name, library_name, sizeof(entry->library_name) - 1) == 0 &&
            strncmp(entry->compute_key, compute_key, sizeof(entry->compute_key) - 1) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/// Allocate and prepend a new cache entry. Does NOT check for duplicates.
/// The pipeline handles (module_handle, pipeline_handle) are filled by the
/// GPU executor during dispatch.
static PipelineCacheEntry* pipeline_cache_insert(
    const char* library_name,
    const char* compute_key,
    uint32_t dispatch_x,
    uint32_t dispatch_y,
    uint32_t dispatch_z
) {
    PipelineCacheEntry* entry = (PipelineCacheEntry*)calloc(1, sizeof(PipelineCacheEntry));
    if (entry == NULL) return NULL;

    strncpy(entry->library_name, library_name, sizeof(entry->library_name) - 1);
    strncpy(entry->compute_key, compute_key, sizeof(entry->compute_key) - 1);
    entry->dispatch_size[0] = dispatch_x;
    entry->dispatch_size[1] = dispatch_y;
    entry->dispatch_size[2] = dispatch_z;
    entry->module_handle = NULL;
    entry->pipeline_handle = NULL;
    entry->ref_count = 1;
    entry->next = NULL;

    // Prepend to global list
    if (g_pipeline_cache_head == NULL) {
        g_pipeline_cache_head = entry;
    } else {
        entry->next = g_pipeline_cache_head;
        g_pipeline_cache_head = entry;
    }

    return entry;
}

/// Free all pipeline cache entries. Called during runtime shutdown.
/// Does NOT release the underlying Vulkan/CUDA handles — that's the GPU
/// executor's job.
/// Declared in cuda_runtime.h for shutdown wiring.
void kain_cuda_pipeline_cache_free_all(void) {
    PipelineCacheEntry* entry = g_pipeline_cache_head;
    while (entry != NULL) {
        PipelineCacheEntry* next = entry->next;
        free(entry);
        entry = next;
    }
    g_pipeline_cache_head = NULL;
}

static int64_t cuda_dispatch_internal(
    const char* shader_bundle_path_override,
    const char* compute_residency_path_override,
    const char* compute_key,
    int64_t dispatch_x,
    int64_t dispatch_y,
    int64_t dispatch_z
) {
    char runtime_library_path[KAIN_CUDA_RUNTIME_PATH_MAX];
    char shader_bundle_path[KAIN_CUDA_RUNTIME_PATH_MAX];
    char compute_residency_path[KAIN_CUDA_RUNTIME_PATH_MAX];
    char message[KAIN_CUDA_RUNTIME_MESSAGE_MAX];
    KainCudaRuntimeLibrary runtime_library = NULL;
    KainGpuRuntimeDispatchPtxPersistedFn dispatch_fn = NULL;
    KainGpuRuntimeDispatchRequest request;
    KainGpuRuntimeDispatchResult dispatch_result;
    int call_status;

    cuda_reset_dispatch_stats();
    cuda_store_paths("", "", "");

    if ((dispatch_x != 0 || dispatch_y != 0 || dispatch_z != 0) &&
        (dispatch_x <= 0 || dispatch_y <= 0 || dispatch_z <= 0 ||
         dispatch_x > UINT32_MAX || dispatch_y > UINT32_MAX || dispatch_z > UINT32_MAX)) {
        return cuda_set_status(
            -1,
            "invalid_argument",
            "gpu dispatch dimensions must be positive u32 values when provided"
        );
    }
    if (compute_key == NULL || compute_key[0] == '\0') {
        return cuda_set_status(
            -1,
            "invalid_argument",
            "cuda compute dispatch requires a non-empty compute key"
        );
    }
    if (shader_bundle_path_override != NULL && shader_bundle_path_override[0] != '\0') {
        cuda_copy_text(shader_bundle_path, sizeof(shader_bundle_path), shader_bundle_path_override);
    } else if (!cuda_resolve_artifact_path(
                   KAIN_CUDA_SHADER_BUNDLE_ENV,
                   "KAIN_UI_NATIVE_SHADER_BUNDLE",
                   KAIN_CUDA_SHADER_BUNDLE_FILE_NAME,
                   shader_bundle_path,
                   sizeof(shader_bundle_path)
               )) {
        return cuda_set_status(
            -1,
            "shader_bundle_missing",
            "unable to resolve the CUDA shader bundle path"
        );
    }
    if (compute_residency_path_override != NULL && compute_residency_path_override[0] != '\0') {
        cuda_copy_text(
            compute_residency_path,
            sizeof(compute_residency_path),
            compute_residency_path_override
        );
    } else if (!cuda_resolve_artifact_path(
                   KAIN_CUDA_COMPUTE_RESIDENCY_ENV,
                   KAIN_COMPUTE_RESIDENCY_ENV,
                   KAIN_CUDA_COMPUTE_RESIDENCY_FILE_NAME,
                   compute_residency_path,
                   sizeof(compute_residency_path)
               )) {
        return cuda_set_status(
            -1,
            "compute_residency_missing",
            "unable to resolve the CUDA compute residency path"
        );
    }
    if (!cuda_file_exists(shader_bundle_path)) {
        return cuda_set_status(
            -1,
            "shader_bundle_missing",
            "CUDA shader bundle path does not exist"
        );
    }
    if (!cuda_file_exists(compute_residency_path)) {
        return cuda_set_status(
            -1,
            "compute_residency_missing",
            "CUDA compute residency path does not exist"
        );
    }
    if (!cuda_resolve_runtime_library_path(
            runtime_library_path,
            sizeof(runtime_library_path)
        )) {
        return cuda_set_status(
            -1,
            "runtime_library_missing",
            "unable to resolve kain-gpu-runtime for CUDA dispatch"
        );
    }
    if (!cuda_open_runtime_library(
            runtime_library_path,
            &runtime_library,
            message,
            sizeof(message)
        )) {
        return cuda_set_status(-1, "runtime_library_open_failed", message);
    }

    dispatch_fn = (KainGpuRuntimeDispatchPtxPersistedFn)cuda_resolve_runtime_symbol(
        runtime_library,
        "kain_gpu_runtime_dispatch_nvidia_ptx_primary_compute_persisted",
        message,
        sizeof(message)
    );
    if (dispatch_fn == NULL) {
        cuda_close_runtime_library(runtime_library);
        return cuda_set_status(-1, "runtime_symbol_missing", message);
    }

    ZeroMemory(&request, sizeof(request));
    ZeroMemory(&dispatch_result, sizeof(dispatch_result));
    request.shader_bundle_path = shader_bundle_path;
    request.compute_residency_path = compute_residency_path;
    request.compute_key = compute_key;
    if (dispatch_x > 0 && dispatch_y > 0 && dispatch_z > 0) {
        request.dispatch_size[0] = (unsigned int)dispatch_x;
        request.dispatch_size[1] = (unsigned int)dispatch_y;
        request.dispatch_size[2] = (unsigned int)dispatch_z;
    }
    call_status = dispatch_fn(&request, &dispatch_result);

    cuda_store_paths(
        runtime_library_path,
        shader_bundle_path,
        compute_residency_path
    );
    g_cuda_last_dispatch_invocations = (int64_t)dispatch_result.dispatch_invocations;
    g_cuda_last_tensor_binding_count = (int64_t)dispatch_result.tensor_binding_count;
    g_cuda_last_stream_binding_count = (int64_t)dispatch_result.stream_binding_count;
    g_cuda_last_neural_node_count = (int64_t)dispatch_result.neural_node_count;
    g_cuda_last_output_binding_count = (int64_t)dispatch_result.output_binding_count;
    g_cuda_last_total_output_bytes = (int64_t)dispatch_result.total_output_bytes;

    cuda_close_runtime_library(runtime_library);

    if (call_status != 0 || dispatch_result.status_code != 0) {
        return cuda_set_status(
            dispatch_result.status_code != 0 ? (int64_t)dispatch_result.status_code : -1,
            "dispatch_failed",
            dispatch_result.message[0] ? dispatch_result.message : "CUDA dispatch failed"
        );
    }
    return cuda_ok(
        dispatch_result.message[0] ? dispatch_result.message : "cuda dispatch ok"
    );
}

int abi_cuda_driver_available(void) {
    return cuda_open_driver_probe();
}

int abi_cuda_runtime_library_available(void) {
    char runtime_library_path[KAIN_CUDA_RUNTIME_PATH_MAX];
    KainCudaRuntimeLibrary runtime_library = NULL;
    char message[KAIN_CUDA_RUNTIME_MESSAGE_MAX];
    int available;

    if (!cuda_resolve_runtime_library_path(
            runtime_library_path,
            sizeof(runtime_library_path)
        )) {
        return 0;
    }
    available = cuda_open_runtime_library(
        runtime_library_path,
        &runtime_library,
        message,
        sizeof(message)
    );
    cuda_close_runtime_library(runtime_library);
    return available;
}

int abi_cuda_runtime_ready(void) {
    char shader_bundle_path[KAIN_CUDA_RUNTIME_PATH_MAX];
    char compute_residency_path[KAIN_CUDA_RUNTIME_PATH_MAX];
    return abi_cuda_driver_available() &&
        abi_cuda_runtime_library_available() &&
        cuda_resolve_artifact_path(
            KAIN_CUDA_SHADER_BUNDLE_ENV,
            "KAIN_UI_NATIVE_SHADER_BUNDLE",
            KAIN_CUDA_SHADER_BUNDLE_FILE_NAME,
            shader_bundle_path,
            sizeof(shader_bundle_path)
        ) &&
        cuda_resolve_artifact_path(
            KAIN_CUDA_COMPUTE_RESIDENCY_ENV,
            KAIN_COMPUTE_RESIDENCY_ENV,
            KAIN_CUDA_COMPUTE_RESIDENCY_FILE_NAME,
            compute_residency_path,
            sizeof(compute_residency_path)
        );
}

const char* abi_cuda_runtime_library_path(void) {
    char path[KAIN_CUDA_RUNTIME_PATH_MAX];
    if (cuda_resolve_runtime_library_path(path, sizeof(path))) {
        return string_new(path);
    }
    return string_new("");
}

const char* abi_cuda_shader_bundle_path(void) {
    char path[KAIN_CUDA_RUNTIME_PATH_MAX];
    if (cuda_resolve_artifact_path(
            KAIN_CUDA_SHADER_BUNDLE_ENV,
            "KAIN_UI_NATIVE_SHADER_BUNDLE",
            KAIN_CUDA_SHADER_BUNDLE_FILE_NAME,
            path,
            sizeof(path)
        )) {
        return string_new(path);
    }
    return string_new("");
}

const char* abi_cuda_compute_residency_path(void) {
    char path[KAIN_CUDA_RUNTIME_PATH_MAX];
    if (cuda_resolve_artifact_path(
            KAIN_CUDA_COMPUTE_RESIDENCY_ENV,
            KAIN_COMPUTE_RESIDENCY_ENV,
            KAIN_CUDA_COMPUTE_RESIDENCY_FILE_NAME,
            path,
            sizeof(path)
        )) {
        return string_new(path);
    }
    return string_new("");
}

int64_t abi_cuda_dispatch_primary_compute(const char* compute_key) {
    return cuda_dispatch_internal(NULL, NULL, compute_key, 0, 0, 0);
}

int64_t abi_cuda_dispatch(
    const char* shader_bundle_path,
    const char* compute_residency_path,
    const char* compute_key
) {
    return cuda_dispatch_internal(
        shader_bundle_path,
        compute_residency_path,
        compute_key,
        0,
        0,
        0
    );
}

int64_t abi_gpu_dispatch(
    const char* compute_key,
    int64_t dispatch_x,
    int64_t dispatch_y,
    int64_t dispatch_z
) {
    return cuda_dispatch_internal(
        NULL,
        NULL,
        compute_key,
        dispatch_x,
        dispatch_y,
        dispatch_z
    );
}

int64_t abi_cuda_last_status(void) {
    return g_cuda_last_status;
}

const char* abi_cuda_last_error_kind(void) {
    return string_new(g_cuda_last_error_kind);
}

const char* abi_cuda_last_error_message(void) {
    return string_new(g_cuda_last_error_message);
}

int64_t abi_cuda_last_dispatch_invocations(void) {
    return g_cuda_last_dispatch_invocations;
}

int64_t abi_cuda_last_tensor_binding_count(void) {
    return g_cuda_last_tensor_binding_count;
}

int64_t abi_cuda_last_stream_binding_count(void) {
    return g_cuda_last_stream_binding_count;
}

int64_t abi_cuda_last_neural_node_count(void) {
    return g_cuda_last_neural_node_count;
}

int64_t abi_cuda_last_output_binding_count(void) {
    return g_cuda_last_output_binding_count;
}

int64_t abi_cuda_last_total_output_bytes(void) {
    return g_cuda_last_total_output_bytes;
}
