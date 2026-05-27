#include "../../include/base.h"
#include "../../include/interop_contracts.h"
#include "../../include/interop_zero_copy.h"
#include "../../include/json.h"

#include <limits.h>
#include <stddef.h>
#include <stdatomic.h>

#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif

#define KAIN_PY_FILE_INPUT 257
#define KAIN_PY_EVAL_INPUT 258
#define KAIN_PY_IMPORT_DIR_CACHE 4096
#define KAIN_PYBUF_WRITABLE 0x0001
#define KAIN_PYBUF_FORMAT   0x0004
#define KAIN_PYBUF_ND       0x0008
#define KAIN_PYBUF_STRIDES  0x0018

#define KAIN_RC_TYPE_PY_OBJECT  UINT64_C(0x4b50594f424a0001)
#define KAIN_RC_TYPE_PY_TENSOR  UINT64_C(0x4b505954454e0001)
#define KAIN_RC_TYPE_PY_IMAGE   UINT64_C(0x4b5059494d470001)
#define KAIN_RC_TYPE_PY_BUFFER_VIEW UINT64_C(0x4b50594256490001)
#define KAIN_RC_TYPE_PY_REGION UINT64_C(0x4b50595245470001)
#define KAIN_RC_TYPE_PY_ASYNC_FUTURE UINT64_C(0x4b50594153594e01)
#define KAIN_RC_TYPE_PY_ACTOR_CALLBACK UINT64_C(0x4b50594143544201)
#define KAIN_PY_REGION_IMPORT_CACHE 64u
#define KAIN_PY_REGION_ATTR_CACHE 128u
#define KAIN_PY_JSON_INT(value)  ((((int64_t)(value)) << 3) | 1LL)
#define KAIN_PY_JSON_BOOL(value) ((((int64_t)((value) != 0)) << 3) | 2LL)
#define KAIN_PY_JSON_NULL        4LL

typedef intptr_t Py_ssize_t;
typedef struct _object PyObject;
typedef int PyGILState_STATE;
typedef struct {
    void* buf;
    PyObject* obj;
    Py_ssize_t len;
    Py_ssize_t itemsize;
    int readonly;
    int ndim;
    char* format;
    Py_ssize_t* shape;
    Py_ssize_t* strides;
    Py_ssize_t* suboffsets;
    void* internal;
} Py_buffer;

typedef struct {
    int ready;
#ifdef _WIN32
    HMODULE dll;
#else
    void* dll;
#endif
    int (*Py_IsInitialized)(void);
    void (*Py_Initialize)(void);
    PyGILState_STATE (*PyGILState_Ensure)(void);
    void (*PyGILState_Release)(PyGILState_STATE);
    PyObject* (*PyImport_ImportModule)(const char*);
    PyObject* (*PyObject_GetAttrString)(PyObject*, const char*);
    int (*PyObject_HasAttrString)(PyObject*, const char*);
    int (*PyObject_SetAttrString)(PyObject*, const char*, PyObject*);
    PyObject* (*PyObject_Call)(PyObject*, PyObject*, PyObject*);
    int (*PyObject_SetItem)(PyObject*, PyObject*, PyObject*);
    PyObject* (*PyObject_GetItem)(PyObject*, PyObject*);
    PyObject* (*PyObject_Str)(PyObject*);
    PyObject* (*PyObject_Repr)(PyObject*);
    int (*PyObject_IsTrue)(PyObject*);
    PyObject* (*PyTuple_New)(Py_ssize_t);
    int (*PyTuple_SetItem)(PyObject*, Py_ssize_t, PyObject*);
    PyObject* (*PyTuple_GetItem)(PyObject*, Py_ssize_t);
    PyObject* (*PyList_New)(Py_ssize_t);
    int (*PyList_Append)(PyObject*, PyObject*);
    int (*PyList_Insert)(PyObject*, Py_ssize_t, PyObject*);
    Py_ssize_t (*PyList_Size)(PyObject*);
    PyObject* (*PyList_GetItem)(PyObject*, Py_ssize_t);
    PyObject* (*PyDict_New)(void);
    int (*PyDict_SetItemString)(PyObject*, const char*, PyObject*);
    PyObject* (*PyMapping_Items)(PyObject*);
    Py_ssize_t (*PySequence_Size)(PyObject*);
    PyObject* (*PySequence_GetItem)(PyObject*, Py_ssize_t);
    PyObject* (*PyUnicode_FromString)(const char*);
    const char* (*PyUnicode_AsUTF8)(PyObject*);
    PyObject* (*PyLong_FromLongLong)(long long);
    long long (*PyLong_AsLongLong)(PyObject*);
    PyObject* (*PyFloat_FromDouble)(double);
    double (*PyFloat_AsDouble)(PyObject*);
    PyObject* (*PyBool_FromLong)(long);
    PyObject* (*PyNumber_Long)(PyObject*);
    PyObject* (*PyNumber_Float)(PyObject*);
    PyObject* (*PyRun_StringFlags)(const char*, int, PyObject*, PyObject*, void*);
    PyObject* (*PyErr_Occurred)(void);
    void (*PyErr_Clear)(void);
    void (*Py_IncRef)(PyObject*);
    void (*Py_DecRef)(PyObject*);
    int (*PyObject_GetBuffer)(PyObject*, Py_buffer*, int);
    void (*PyBuffer_Release)(Py_buffer*);
} KainPythonApi;

typedef struct {
    PyObject* object;
} KainPythonObjectHandle;

typedef struct KainPythonAsyncFutureHandle KainPythonAsyncFutureHandle;
typedef struct KainPythonActorCallbackHandle KainPythonActorCallbackHandle;

typedef struct {
    int active;
    PyGILState_STATE state;
} KainPythonGilScope;

typedef struct KainPythonRegionHandle KainPythonRegionHandle;
typedef struct KainPythonBufferViewHandle KainPythonBufferViewHandle;

static long long kain_py_string_tag(const char* text);

typedef struct {
    char* module_name;
    PyObject* module;
} KainPythonRegionImportCacheEntry;

typedef struct {
    PyObject* owner;
    char* attr_name;
    PyObject* value;
} KainPythonRegionAttrCacheEntry;

struct KainPythonBufferViewHandle {
    Py_buffer view;
    long long item_count;
    long long item_size;
    int c_contiguous;
    int writable;
    KainPythonRegionHandle* region_owner;
    size_t region_slot;
};

typedef struct {
    PyObject* object;
    long long* shape;
    long long* strides;
    long long ndim;
    long long stride_count;
    long long element_size;
    long long element_count;
    long long byte_length;
    long long device_ordinal;
    long long device_pointer;
    long long device_type_code;
    int host_accessible;
    int contiguous;
    int writable;
    int dlpack_capable;
    int cuda_array_interface_version;
    char ownership[8];
    char dtype[24];
    char element_type[24];
    char source_backend[32];
    char device[32];
    char device_kind[24];
    char interop_lane[32];
} KainPythonTensorHandle;

typedef struct {
    PyObject* object;
    long long* shape;
    long long ndim;
    long long batch;
    long long width;
    long long height;
    long long channels;
    long long pixel_count;
    long long byte_length;
    long long row_stride;
    int zero_copy;
    char ownership[8];
    char dtype[24];
    char layout[8];
    char storage[16];
    char source_backend[16];
} KainPythonImageHandle;

typedef struct {
    Py_buffer view;
} KainPythonBorrowedBufferOwner;

struct KainPythonRegionHandle {
    KainPythonGilScope scope;
    int active;
    KainPythonRegionImportCacheEntry imports[KAIN_PY_REGION_IMPORT_CACHE];
    size_t import_count;
    KainPythonRegionAttrCacheEntry attrs[KAIN_PY_REGION_ATTR_CACHE];
    size_t attr_count;
    KainPythonBufferViewHandle** open_views;
    size_t open_view_count;
    size_t open_view_capacity;
    uint64_t import_cache_hits;
    uint64_t import_cache_misses;
    uint64_t attr_cache_hits;
    uint64_t attr_cache_misses;
    uint64_t views_opened;
    uint64_t views_released;
    uint64_t call_count;
    uint64_t generic_call_count;
    uint64_t fast_call_count;
};

static KainPythonApi g_kain_python_api;
static int g_kain_python_load_attempted = 0;
static int g_kain_python_default_import_context_ready = 0;
static char g_kain_python_last_importer_dir[KAIN_PY_IMPORT_DIR_CACHE];

void* kain_alloc_rc(size_t size, long long type_tag);
void rc_retain(void* ptr);
void KAIN_set_destructor(void* ptr, void (*dtor)(void*));
void rc_release(void* ptr);
KainArray* array_new(long long cap);
void array_push(KainArray* arr, long long val);
long long py_getattr_raw(long long target, char* name);
long long kain_tensor_from_py_shared(long long target);
long long kain_image_from_py_shared(long long target);
static int kain_py_copy_source_backend(PyObject* object, char* dest, size_t dest_size);
static int kain_py_checked_mul_i64(long long left, long long right, long long* out_value);
static int64_t kain_py_array_handle_from_values(const long long* values, long long len);
static int64_t kain_py_json_array_from_values(const long long* values, long long len);
static int64_t kain_py_compact_strides_handle(const long long* shape, long long len);
static int64_t kain_py_tensor_shape_handle(const KainPythonTensorHandle* tensor);
static int64_t kain_py_tensor_strides_handle(const KainPythonTensorHandle* tensor);
static int kain_py_tensor_has_virtual_attr(const char* name);
static long long kain_py_tensor_attr_value(const KainPythonTensorHandle* tensor, const char* name);
static const char* kain_py_storage_from_dtype(const char* dtype);
static PyObject* kain_py_resolve_target(long long value);
static PyObject* kain_py_call_method0_owned(PyObject* object, const char* name);
static long long kain_py_call_internal_active(
    long long target,
    const char* attr_name,
    long long args,
    long long kwargs,
    int raw_result,
    KainPythonRegionHandle* region
);
static long long kain_py_import_internal_active(
    const char* module_name,
    const char* importer_file,
    KainPythonRegionHandle* region
);
static int kain_py_copy_dtype_name(PyObject* object, char* dest, size_t dest_size);
static int kain_py_read_attr_int_sequence(PyObject* object, const char* name, long long** out_values, long long* out_len);
static long long kain_py_element_size_from_storage(const char* element_type);
static int kain_py_read_sequence_int_values(PyObject* sequence, long long** out_values, long long* out_len);
static long long kain_py_method0_int(PyObject* object, const char* name, long long fallback);
static int kain_py_shape_element_count(const long long* shape, long long len, long long* out_count);
static long long* kain_py_copy_long_long_buffer(const long long* values, long long len);
static long long* kain_py_compact_strides_values(const long long* shape, long long len);
static int kain_py_stride_values_look_like_bytes(const long long* values, long long len, long long element_size);
static void kain_py_stride_values_to_elements(long long* values, long long len, long long element_size);
static int kain_py_tensor_strides_are_compact(const KainPythonTensorHandle* tensor);
static void kain_py_tensor_capture_device_metadata(PyObject* object, KainPythonTensorHandle* tensor);
static long long kain_py_attr_int(PyObject* object, const char* name, long long fallback);
static long long kain_py_getattr_internal_active(long long target, const char* name, KainPythonRegionHandle* region);
static long long kain_py_buffer_view_from_target_active(long long target, KainPythonRegionHandle* region);
static void kain_py_any_retain(long long value);
static void kain_py_any_release(long long value);

static int kain_py_trace_enabled(void) {
#ifdef _WIN32
    char buffer[8];
    DWORD len = GetEnvironmentVariableA("KAIN_PY_TRACE", buffer, (DWORD)sizeof(buffer));
    return len > 0 && len < sizeof(buffer);
#else
    const char* value = getenv("KAIN_PY_TRACE");
    return value && value[0];
#endif
}

static RcHeader* kain_py_rc_header(const void* ptr) {
    if (!ptr) {
        return NULL;
    }
    return ((RcHeader*)ptr) - 1;
}

static int kain_py_type_tag_matches(const void* ptr, long long type_tag) {
    RcHeader* header;
    if (!ptr || (((uintptr_t)ptr) & 7u) != 0u) {
        return 0;
    }
    header = kain_py_rc_header(ptr);
    return header != NULL &&
        header->magic == KAIN_RC_MAGIC_ALIVE &&
        header->type_tag == type_tag;
}

static long long kain_py_unbox_tagged_handle(long long value, long long type_tag) {
    long long payload;
    if ((value & 7LL) != 1LL) {
        return value;
    }
    payload = value >> 3;
    return kain_py_type_tag_matches((void*)(intptr_t)payload, type_tag) ? payload : value;
}

static int kain_py_any_is_string_tag(long long value) {
    return (value & 7LL) == 3LL;
}

static int kain_py_any_is_null_tag(long long value) {
    return (value & 7LL) == 4LL;
}

static void kain_py_any_retain(long long value) {
    if (value == 0 || kain_py_any_is_null_tag(value)) {
        return;
    }
    if ((value & 7LL) == 3LL) {
        return;
    }
    if ((value & 7LL) == 0LL) {
        rc_retain((void*)(intptr_t)value);
        return;
    }
    json_retain(value);
}

static void kain_py_any_release(long long value) {
    if (value == 0 || kain_py_any_is_null_tag(value)) {
        return;
    }
    if ((value & 7LL) == 3LL) {
        free((void*)(intptr_t)(value & ~7LL));
        return;
    }
    if ((value & 7LL) == 0LL) {
        rc_release((void*)(intptr_t)value);
        return;
    }
    json_release(value);
}

static char* kain_py_dup_cstr(const char* text) {
    size_t length;
    char* out;
    if (!text) {
        text = "";
    }
    length = strlen(text);
    out = (char*)malloc(length + 1u);
    if (!out) {
        return NULL;
    }
    memcpy(out, text, length + 1u);
    return out;
}

static char* kain_py_parent_dir(const char* path) {
    char* copy;
    char* cursor;
    if (!path || !path[0]) {
        return NULL;
    }
    copy = kain_py_dup_cstr(path);
    if (!copy) {
        return NULL;
    }
    cursor = copy + strlen(copy);
    while (cursor > copy) {
        char ch = cursor[-1];
        if (ch == '\\' || ch == '/') {
            cursor[-1] = '\0';
            return copy;
        }
        cursor -= 1;
    }
    free(copy);
    return NULL;
}

static void* kain_py_load_symbol(const char* name) {
#ifdef _WIN32
    return g_kain_python_api.dll ? (void*)GetProcAddress(g_kain_python_api.dll, name) : NULL;
#else
    return g_kain_python_api.dll ? dlsym(g_kain_python_api.dll, name) : NULL;
#endif
}

#ifdef _WIN32
static HMODULE kain_py_try_load_library(const char* path) {
    return path && path[0] ? LoadLibraryA(path) : NULL;
}

static HMODULE kain_py_try_directory_known_dlls(const char* directory) {
    static const char* dlls[] = {
        "python313.dll",
        "python312.dll",
        "python311.dll",
        "python310.dll",
        "python39.dll"
    };
    size_t i;
    if (!directory || !directory[0]) {
        return NULL;
    }
    for (i = 0; i < sizeof(dlls) / sizeof(dlls[0]); ++i) {
        char candidate[MAX_PATH];
        snprintf(candidate, sizeof(candidate), "%s\\%s", directory, dlls[i]);
        g_kain_python_api.dll = kain_py_try_load_library(candidate);
        if (g_kain_python_api.dll) {
            return g_kain_python_api.dll;
        }
    }
    return NULL;
}

static HMODULE kain_py_try_python_executable_family(const char* executable_path, int search_path) {
    char resolved[MAX_PATH];
    char* directory;
    DWORD len;
    if (!executable_path || !executable_path[0]) {
        return NULL;
    }
    if (search_path) {
        len = SearchPathA(NULL, executable_path, NULL, (DWORD)sizeof(resolved), resolved, NULL);
        if (len == 0 || len >= sizeof(resolved)) {
            return NULL;
        }
        directory = kain_py_parent_dir(resolved);
    } else {
        directory = kain_py_parent_dir(executable_path);
    }
    if (!directory) {
        return NULL;
    }
    g_kain_python_api.dll = kain_py_try_directory_known_dlls(directory);
    free(directory);
    return g_kain_python_api.dll;
}
#else
static void* kain_py_try_load_library(const char* path) {
    return path && path[0] ? dlopen(path, RTLD_NOW | RTLD_LOCAL) : NULL;
}
#endif

static int kain_py_load_api(void) {
    if (g_kain_python_api.ready) {
        return 1;
    }
    if (g_kain_python_load_attempted) {
        return 0;
    }
    g_kain_python_load_attempted = 1;

#ifdef _WIN32
    {
        char local_app_data[MAX_PATH];
        DWORD len = GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, (DWORD)sizeof(local_app_data));
        static const char* bare_names[] = {
            "python313.dll",
            "python312.dll",
            "python311.dll",
            "python310.dll",
            "python39.dll"
        };
        char* env_candidate = NULL;
        size_t env_length = 0u;
        char* env_python_exe = NULL;
        size_t env_python_exe_length = 0u;
        size_t i;
        _dupenv_s(&env_candidate, &env_length, "KAIN_PYTHON_DLL");
        if (!g_kain_python_api.dll && env_candidate && env_candidate[0]) {
            g_kain_python_api.dll = kain_py_try_load_library(env_candidate);
        }
        if (env_candidate) {
            free(env_candidate);
        }
        _dupenv_s(&env_python_exe, &env_python_exe_length, "KAIN_PYTHON_EXE");
        if (!g_kain_python_api.dll && env_python_exe && env_python_exe[0]) {
            g_kain_python_api.dll = kain_py_try_python_executable_family(env_python_exe, 0);
        }
        if (env_python_exe) {
            free(env_python_exe);
        }
        if (!g_kain_python_api.dll) {
            g_kain_python_api.dll = kain_py_try_python_executable_family("python.exe", 1);
        }
        if (!g_kain_python_api.dll) {
            g_kain_python_api.dll = kain_py_try_python_executable_family("python3.exe", 1);
        }
        if (!g_kain_python_api.dll && len > 0 && len < sizeof(local_app_data)) {
            const char* versions[] = {"Python313", "Python312", "Python311", "Python310", "Python39"};
            const char* dlls[] = {"python313.dll", "python312.dll", "python311.dll", "python310.dll", "python39.dll"};
            for (i = 0; i < sizeof(versions) / sizeof(versions[0]); ++i) {
                char candidate[MAX_PATH];
                snprintf(
                    candidate,
                    sizeof(candidate),
                    "%s\\Programs\\Python\\%s\\%s",
                    local_app_data,
                    versions[i],
                    dlls[i]
                );
                g_kain_python_api.dll = kain_py_try_load_library(candidate);
                if (g_kain_python_api.dll) {
                    break;
                }
            }
        }
        for (i = 0; !g_kain_python_api.dll && i < sizeof(bare_names) / sizeof(bare_names[0]); ++i) {
            g_kain_python_api.dll = kain_py_try_load_library(bare_names[i]);
        }
    }
#else
    {
        static const char* candidates[] = {
            "libpython3.13.so",
            "libpython3.12.so",
            "libpython3.11.so",
            "libpython3.10.so",
            "libpython3.9.so"
        };
        const char* env_candidate = getenv("KAIN_PYTHON_DLL");
        size_t i;
        if (!g_kain_python_api.dll && env_candidate && env_candidate[0]) {
            g_kain_python_api.dll = kain_py_try_load_library(env_candidate);
        }
        for (i = 0; !g_kain_python_api.dll && i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
            g_kain_python_api.dll = kain_py_try_load_library(candidates[i]);
        }
    }
#endif

    if (!g_kain_python_api.dll) {
        return 0;
    }

#define KAIN_LOAD_PY_API(name)                                                                 \
    do {                                                                                       \
        g_kain_python_api.name = (void*)kain_py_load_symbol(#name);                            \
        if (!g_kain_python_api.name) {                                                         \
            return 0;                                                                          \
        }                                                                                      \
    } while (0)

    KAIN_LOAD_PY_API(Py_IsInitialized);
    KAIN_LOAD_PY_API(Py_Initialize);
    KAIN_LOAD_PY_API(PyGILState_Ensure);
    KAIN_LOAD_PY_API(PyGILState_Release);
    KAIN_LOAD_PY_API(PyImport_ImportModule);
    KAIN_LOAD_PY_API(PyObject_GetAttrString);
    KAIN_LOAD_PY_API(PyObject_HasAttrString);
    KAIN_LOAD_PY_API(PyObject_SetAttrString);
    KAIN_LOAD_PY_API(PyObject_Call);
    KAIN_LOAD_PY_API(PyObject_SetItem);
    KAIN_LOAD_PY_API(PyObject_GetItem);
    KAIN_LOAD_PY_API(PyObject_Str);
    KAIN_LOAD_PY_API(PyObject_Repr);
    KAIN_LOAD_PY_API(PyObject_IsTrue);
    KAIN_LOAD_PY_API(PyTuple_New);
    KAIN_LOAD_PY_API(PyTuple_SetItem);
    KAIN_LOAD_PY_API(PyTuple_GetItem);
    KAIN_LOAD_PY_API(PyList_New);
    KAIN_LOAD_PY_API(PyList_Append);
    KAIN_LOAD_PY_API(PyList_Insert);
    KAIN_LOAD_PY_API(PyList_Size);
    KAIN_LOAD_PY_API(PyList_GetItem);
    KAIN_LOAD_PY_API(PyDict_New);
    KAIN_LOAD_PY_API(PyDict_SetItemString);
    KAIN_LOAD_PY_API(PyMapping_Items);
    KAIN_LOAD_PY_API(PySequence_Size);
    KAIN_LOAD_PY_API(PySequence_GetItem);
    KAIN_LOAD_PY_API(PyUnicode_FromString);
    KAIN_LOAD_PY_API(PyUnicode_AsUTF8);
    KAIN_LOAD_PY_API(PyLong_FromLongLong);
    KAIN_LOAD_PY_API(PyLong_AsLongLong);
    KAIN_LOAD_PY_API(PyFloat_FromDouble);
    KAIN_LOAD_PY_API(PyFloat_AsDouble);
    KAIN_LOAD_PY_API(PyBool_FromLong);
    KAIN_LOAD_PY_API(PyNumber_Long);
    KAIN_LOAD_PY_API(PyNumber_Float);
    KAIN_LOAD_PY_API(PyRun_StringFlags);
    KAIN_LOAD_PY_API(PyErr_Occurred);
    KAIN_LOAD_PY_API(PyErr_Clear);
    KAIN_LOAD_PY_API(Py_IncRef);
    KAIN_LOAD_PY_API(Py_DecRef);
    KAIN_LOAD_PY_API(PyObject_GetBuffer);
    KAIN_LOAD_PY_API(PyBuffer_Release);

#undef KAIN_LOAD_PY_API

    if (!g_kain_python_api.Py_IsInitialized()) {
        g_kain_python_api.Py_Initialize();
    }
    g_kain_python_api.ready = 1;
    return 1;
}

static void kain_py_clear_error(void) {
    if (g_kain_python_api.ready && g_kain_python_api.PyErr_Occurred()) {
        g_kain_python_api.PyErr_Clear();
    }
}

static KainPythonGilScope kain_py_gil_enter(void) {
    KainPythonGilScope scope;
    scope.active = 0;
    scope.state = 0;
    if (!kain_py_load_api()) {
        return scope;
    }
    scope.state = g_kain_python_api.PyGILState_Ensure();
    scope.active = 1;
    return scope;
}

static void kain_py_gil_exit(KainPythonGilScope* scope) {
    if (scope && scope->active) {
        g_kain_python_api.PyGILState_Release(scope->state);
        scope->active = 0;
    }
}

static PyObject* kain_py_main_dict(void) {
    PyObject* main_module = g_kain_python_api.PyImport_ImportModule("__main__");
    PyObject* dict_obj;
    if (!main_module) {
        kain_py_clear_error();
        return NULL;
    }
    dict_obj = g_kain_python_api.PyObject_GetAttrString(main_module, "__dict__");
    g_kain_python_api.Py_DecRef(main_module);
    if (!dict_obj) {
        kain_py_clear_error();
        return NULL;
    }
    return dict_obj;
}

static void kain_py_prepend_sys_path(const char* root) {
    PyObject* sys_module;
    PyObject* sys_path;
    PyObject* root_text;
    Py_ssize_t size;
    Py_ssize_t index;
    if (!root || !root[0]) {
        return;
    }
    sys_module = g_kain_python_api.PyImport_ImportModule("sys");
    if (!sys_module) {
        kain_py_clear_error();
        return;
    }
    sys_path = g_kain_python_api.PyObject_GetAttrString(sys_module, "path");
    g_kain_python_api.Py_DecRef(sys_module);
    if (!sys_path) {
        kain_py_clear_error();
        return;
    }
    size = g_kain_python_api.PyList_Size(sys_path);
    for (index = 0; index < size; ++index) {
        PyObject* entry = g_kain_python_api.PyList_GetItem(sys_path, index);
        const char* utf8 = entry ? g_kain_python_api.PyUnicode_AsUTF8(entry) : NULL;
        if (!utf8) {
            kain_py_clear_error();
            continue;
        }
        if (strcmp(utf8, root) == 0) {
            g_kain_python_api.Py_DecRef(sys_path);
            return;
        }
    }
    root_text = g_kain_python_api.PyUnicode_FromString(root);
    if (root_text) {
        g_kain_python_api.PyList_Insert(sys_path, 0, root_text);
        g_kain_python_api.Py_DecRef(root_text);
    } else {
        kain_py_clear_error();
    }
    g_kain_python_api.Py_DecRef(sys_path);
}

static void kain_py_prepare_import_context(const char* importer_file) {
    char* parent = kain_py_parent_dir(importer_file);
    if (parent) {
        if (strcmp(parent, g_kain_python_last_importer_dir) != 0) {
            kain_py_prepend_sys_path(parent);
            strncpy_s(
                g_kain_python_last_importer_dir,
                sizeof(g_kain_python_last_importer_dir),
                parent,
                _TRUNCATE
            );
        }
        free(parent);
    }
    if (g_kain_python_default_import_context_ready) {
        return;
    }
#ifdef _WIN32
    {
        char cwd[MAX_PATH];
        DWORD len = GetCurrentDirectoryA((DWORD)sizeof(cwd), cwd);
        if (len > 0 && len < sizeof(cwd)) {
            char cwd_src[MAX_PATH];
            kain_py_prepend_sys_path(cwd);
            snprintf(cwd_src, sizeof(cwd_src), "%s\\src", cwd);
            kain_py_prepend_sys_path(cwd_src);
        }
    }
#else
    {
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            char cwd_src[4096];
            kain_py_prepend_sys_path(cwd);
            snprintf(cwd_src, sizeof(cwd_src), "%s/src", cwd);
            kain_py_prepend_sys_path(cwd_src);
        }
    }
#endif
    g_kain_python_default_import_context_ready = 1;
}

static const char* kain_py_dtype_from_buffer_format(const char* format) {
    if (!format) {
        return NULL;
    }
    while (*format == '@' || *format == '=' || *format == '<' || *format == '>' || *format == '!') {
        format += 1;
    }
    switch (*format) {
        case '?':
            return "bool";
        case 'b':
            return "int8";
        case 'B':
            return "uint8";
        case 'h':
            return "int16";
        case 'H':
            return "uint16";
        case 'i':
            return "int32";
        case 'I':
            return "uint32";
        case 'l':
            return "int64";
        case 'L':
            return "uint64";
        case 'q':
            return "int64";
        case 'Q':
            return "uint64";
        case 'f':
            return "float32";
        case 'd':
            return "float64";
        default:
            return NULL;
    }
}

static int kain_py_buffer_view_is_c_contiguous(const Py_buffer* view) {
    long long expected_stride;
    int index;
    if (!view || view->len < 0) {
        return 0;
    }
    if (view->itemsize <= 0) {
        return view->len == 0;
    }
    if (view->ndim <= 0 || !view->shape) {
        return 1;
    }
    if (!view->strides) {
        return 1;
    }
    expected_stride = (long long)view->itemsize;
    index = view->ndim - 1;
    while (index >= 0) {
        long long dim = (long long)view->shape[index];
        long long stride = (long long)view->strides[index];
        if (dim < 0) {
            return 0;
        }
        if (dim == 0) {
            return 1;
        }
        if (dim > 1 && stride != expected_stride) {
            return 0;
        }
        if (!kain_py_checked_mul_i64(expected_stride, dim > 0 ? dim : 1, &expected_stride)) {
            return 0;
        }
        if (index == 0) {
            break;
        }
        index -= 1;
    }
    return 1;
}

static int kain_py_buffer_shape_handles_from_view(
    const Py_buffer* view,
    long long item_size,
    int64_t* out_shape_handle,
    int64_t* out_strides_handle
) {
    long long* shape_values = NULL;
    long long ndim = 0;
    int ok = 0;
    if (out_shape_handle) {
        *out_shape_handle = 0;
    }
    if (out_strides_handle) {
        *out_strides_handle = 0;
    }
    if (!view || !out_shape_handle || !out_strides_handle) {
        return 0;
    }
    if (view->ndim > 0 && view->shape) {
        Py_ssize_t index;
        ndim = (long long)view->ndim;
        shape_values = (long long*)calloc((size_t)ndim, sizeof(long long));
        if (!shape_values) {
            return 0;
        }
        for (index = 0; index < view->ndim; ++index) {
            long long dim = (long long)view->shape[index];
            if (dim < 0) {
                free(shape_values);
                return 0;
            }
            shape_values[index] = dim;
        }
    } else {
        long long inferred[1];
        long long byte_length = (long long)view->len;
        if (item_size <= 0) {
            item_size = 1;
        }
        if (byte_length < 0) {
            return 0;
        }
        if (item_size > 0 && byte_length > 0 && (byte_length % item_size) != 0) {
            return 0;
        }
        inferred[0] = (item_size > 0 && byte_length > 0) ? (byte_length / item_size) : byte_length;
        *out_shape_handle = kain_py_array_handle_from_values(inferred, 1);
        *out_strides_handle = kain_py_compact_strides_handle(inferred, 1);
        return *out_shape_handle != 0 && *out_strides_handle != 0;
    }
    *out_shape_handle = kain_py_array_handle_from_values(shape_values, ndim);
    *out_strides_handle = kain_py_compact_strides_handle(shape_values, ndim);
    ok = *out_shape_handle != 0 && *out_strides_handle != 0;
    free(shape_values);
    return ok;
}

static void kain_py_borrowed_buffer_owner_release(void* state) {
    KainPythonBorrowedBufferOwner* owner = (KainPythonBorrowedBufferOwner*)state;
    KainPythonGilScope scope;
    if (!owner) {
        return;
    }
    scope = kain_py_gil_enter();
    if (scope.active && g_kain_python_api.PyBuffer_Release) {
        g_kain_python_api.PyBuffer_Release(&owner->view);
    }
    kain_py_gil_exit(&scope);
    free(owner);
}

static int64_t kain_py_borrowed_buffer_owner_create(PyObject* object, Py_buffer* out_view) {
    KainPythonBorrowedBufferOwner* owner;
    int64_t owner_handle;
    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
    }
    if (!object || !g_kain_python_api.PyObject_GetBuffer || !g_kain_python_api.PyBuffer_Release) {
        return 0;
    }
    owner = (KainPythonBorrowedBufferOwner*)calloc(1u, sizeof(KainPythonBorrowedBufferOwner));
    if (!owner) {
        return 0;
    }
    if (g_kain_python_api.PyObject_GetBuffer(
            object,
            &owner->view,
            KAIN_PYBUF_STRIDES | KAIN_PYBUF_FORMAT
        ) != 0) {
        kain_py_clear_error();
        free(owner);
        return 0;
    }
    if (owner->view.itemsize <= 0) {
        owner->view.itemsize = 1;
    }
    if (owner->view.len < 0 ||
        (owner->view.len > 0 && owner->view.buf == NULL) ||
        !kain_py_buffer_view_is_c_contiguous(&owner->view)) {
        g_kain_python_api.PyBuffer_Release(&owner->view);
        free(owner);
        return 0;
    }
    if (out_view) {
        *out_view = owner->view;
    }
    owner_handle = kain_interop_zero_copy_owner_create(owner, kain_py_borrowed_buffer_owner_release);
    if (!owner_handle) {
        return 0;
    }
    return owner_handle;
}

// Keep one translation unit for the Python runtime hot lane, but split the
// edit seams so parallel agents can work buffers, regions, async, and GPU
// without dogpiling one giant file.
#include "python_runtime_region.c"

static KainPythonObjectHandle* kain_py_wrap_object(PyObject* object) {
    KainPythonObjectHandle* handle;
    if (!object) {
        return NULL;
    }
    handle = (KainPythonObjectHandle*)kain_alloc_rc(sizeof(KainPythonObjectHandle), KAIN_RC_TYPE_PY_OBJECT);
    if (!handle) {
        return NULL;
    }
    handle->object = object;
    KAIN_set_destructor(handle, NULL);
    return handle;
}

static void kain_py_object_destructor(void* payload) {
    KainPythonObjectHandle* handle = (KainPythonObjectHandle*)payload;
    KainPythonGilScope scope;
    if (!handle || !handle->object) {
        return;
    }
    scope = kain_py_gil_enter();
    if (scope.active) {
        g_kain_python_api.Py_DecRef(handle->object);
    }
    kain_py_gil_exit(&scope);
}

static KainPythonTensorHandle* kain_py_wrap_tensor(PyObject* object, const char* ownership) {
    KainPythonTensorHandle* tensor;
    PyObject* flags_obj;
    PyObject* writable_obj;
    if (!object) {
        return NULL;
    }
    tensor = (KainPythonTensorHandle*)kain_alloc_rc(sizeof(KainPythonTensorHandle), KAIN_RC_TYPE_PY_TENSOR);
    if (!tensor) {
        return NULL;
    }
    memset(tensor, 0, sizeof(*tensor));
    tensor->object = object;
    tensor->host_accessible = 1;
    tensor->contiguous = 1;
    tensor->writable = 1;
    tensor->device_type_code = 1;
    strncpy_s(tensor->ownership, sizeof(tensor->ownership), ownership ? ownership : "shared", _TRUNCATE);
    if (!kain_py_read_attr_int_sequence(object, "shape", &tensor->shape, &tensor->ndim)) {
        tensor->shape = NULL;
        tensor->ndim = 0;
    }
    if (!kain_py_copy_dtype_name(object, tensor->dtype, sizeof(tensor->dtype))) {
        strncpy_s(tensor->dtype, sizeof(tensor->dtype), "unknown", _TRUNCATE);
    }
    strncpy_s(
        tensor->element_type,
        sizeof(tensor->element_type),
        kain_py_storage_from_dtype(tensor->dtype),
        _TRUNCATE
    );
    tensor->element_size = kain_py_attr_int(object, "itemsize", 0);
    if (tensor->element_size <= 0) {
        tensor->element_size = kain_py_method0_int(object, "element_size", 0);
    }
    if (tensor->element_size <= 0) {
        tensor->element_size = kain_py_element_size_from_storage(tensor->element_type);
    }
    if (!kain_py_copy_source_backend(object, tensor->source_backend, sizeof(tensor->source_backend)) ||
        tensor->source_backend[0] == '\0' ||
        strcmp(tensor->source_backend, "__main__") == 0) {
        strncpy_s(tensor->source_backend, sizeof(tensor->source_backend), "python", _TRUNCATE);
    }
    if (!kain_py_read_attr_int_sequence(object, "strides", &tensor->strides, &tensor->stride_count)) {
        tensor->strides = NULL;
        tensor->stride_count = 0;
    }
    if (!tensor->strides && g_kain_python_api.PyObject_HasAttrString(object, "stride") > 0) {
        PyObject* stride_result = kain_py_call_method0_owned(object, "stride");
        if (stride_result) {
            if (!kain_py_read_sequence_int_values(stride_result, &tensor->strides, &tensor->stride_count)) {
                tensor->strides = NULL;
                tensor->stride_count = 0;
            }
            g_kain_python_api.Py_DecRef(stride_result);
        }
    }
    if (tensor->strides &&
        tensor->stride_count == tensor->ndim &&
        kain_py_stride_values_look_like_bytes(tensor->strides, tensor->stride_count, tensor->element_size)) {
        kain_py_stride_values_to_elements(tensor->strides, tensor->stride_count, tensor->element_size);
    }
    if (!tensor->strides && tensor->shape && tensor->ndim > 0) {
        tensor->strides = kain_py_compact_strides_values(tensor->shape, tensor->ndim);
        tensor->stride_count = tensor->strides ? tensor->ndim : 0;
    }
    if (tensor->shape && tensor->ndim > 0) {
        (void)kain_py_shape_element_count(tensor->shape, tensor->ndim, &tensor->element_count);
    }
    if (tensor->element_count <= 0) {
        tensor->element_count = kain_py_attr_int(object, "size", 0);
    }
    if (tensor->element_count <= 0) {
        tensor->element_count = kain_py_method0_int(object, "numel", 0);
    }
    if (!tensor->shape && tensor->element_count > 0) {
        long long inferred[1];
        inferred[0] = tensor->element_count;
        tensor->shape = kain_py_copy_long_long_buffer(inferred, 1);
        tensor->ndim = tensor->shape ? 1 : 0;
        if (!tensor->strides) {
            tensor->strides = kain_py_compact_strides_values(inferred, 1);
            tensor->stride_count = tensor->strides ? 1 : 0;
        }
    }
    tensor->byte_length = kain_py_attr_int(object, "nbytes", 0);
    if (tensor->byte_length <= 0 &&
        tensor->element_count > 0 &&
        tensor->element_size > 0) {
        (void)kain_py_checked_mul_i64(
            tensor->element_count,
            tensor->element_size,
            &tensor->byte_length
        );
    }
    if (g_kain_python_api.PyObject_HasAttrString(object, "is_contiguous") > 0) {
        PyObject* contiguous_obj = kain_py_call_method0_owned(object, "is_contiguous");
        if (contiguous_obj) {
            int truth = g_kain_python_api.PyObject_IsTrue(contiguous_obj);
            tensor->contiguous = truth > 0 ? 1 : 0;
            if (truth < 0) {
                kain_py_clear_error();
            }
            g_kain_python_api.Py_DecRef(contiguous_obj);
        }
    } else if (tensor->strides) {
        tensor->contiguous = kain_py_tensor_strides_are_compact(tensor);
    }
    flags_obj = g_kain_python_api.PyObject_GetAttrString(object, "flags");
    if (flags_obj) {
        writable_obj = g_kain_python_api.PyObject_GetAttrString(flags_obj, "writeable");
        if (writable_obj) {
            int truth = g_kain_python_api.PyObject_IsTrue(writable_obj);
            tensor->writable = truth > 0 ? 1 : 0;
            if (truth < 0) {
                kain_py_clear_error();
            }
            g_kain_python_api.Py_DecRef(writable_obj);
        } else {
            kain_py_clear_error();
        }
        g_kain_python_api.Py_DecRef(flags_obj);
    } else {
        kain_py_clear_error();
    }
    kain_py_tensor_capture_device_metadata(object, tensor);
    if (!tensor->strides && tensor->shape && tensor->ndim > 0) {
        tensor->strides = kain_py_compact_strides_values(tensor->shape, tensor->ndim);
        tensor->stride_count = tensor->strides ? tensor->ndim : 0;
    }
    if (tensor->element_count <= 0 && tensor->shape && tensor->ndim > 0) {
        (void)kain_py_shape_element_count(tensor->shape, tensor->ndim, &tensor->element_count);
    }
    if (tensor->byte_length <= 0 &&
        tensor->element_count > 0 &&
        tensor->element_size > 0) {
        (void)kain_py_checked_mul_i64(
            tensor->element_count,
            tensor->element_size,
            &tensor->byte_length
        );
    }
    if (tensor->strides && g_kain_python_api.PyObject_HasAttrString(object, "is_contiguous") <= 0) {
        tensor->contiguous = kain_py_tensor_strides_are_compact(tensor);
    }
    if (tensor->dlpack_capable && !tensor->interop_lane[0]) {
        strncpy_s(tensor->interop_lane, sizeof(tensor->interop_lane), "dlpack", _TRUNCATE);
    }
    KAIN_set_destructor(tensor, NULL);
    return tensor;
}

static void kain_py_tensor_destructor(void* payload) {
    KainPythonTensorHandle* tensor = (KainPythonTensorHandle*)payload;
    KainPythonGilScope scope;
    if (!tensor) {
        return;
    }
    if (tensor->shape) {
        free(tensor->shape);
        tensor->shape = NULL;
    }
    if (tensor->strides) {
        free(tensor->strides);
        tensor->strides = NULL;
    }
    if (!tensor->object) {
        return;
    }
    scope = kain_py_gil_enter();
    if (scope.active) {
        g_kain_python_api.Py_DecRef(tensor->object);
    }
    kain_py_gil_exit(&scope);
}

static int kain_py_small_channel_count(long long value) {
    return value >= 1 && value <= 4;
}

static int kain_py_checked_mul_i64(long long left, long long right, long long* out_value) {
    if (!out_value || left < 0 || right < 0) {
        return 0;
    }
    if (left == 0 || right == 0) {
        *out_value = 0;
        return 1;
    }
    if (left > (LLONG_MAX / right)) {
        return 0;
    }
    *out_value = left * right;
    return 1;
}

static const char* kain_py_storage_from_dtype(const char* dtype) {
    if (!dtype || !dtype[0]) {
        return "unknown";
    }
    if (strcmp(dtype, "int8") == 0 || strcmp(dtype, "byte") == 0) {
        return "i8";
    }
    if (strcmp(dtype, "uint8") == 0 || strcmp(dtype, "ubyte") == 0) {
        return "u8";
    }
    if (strcmp(dtype, "int16") == 0) {
        return "i16";
    }
    if (strcmp(dtype, "uint16") == 0) {
        return "u16";
    }
    if (strcmp(dtype, "int32") == 0) {
        return "i32";
    }
    if (strcmp(dtype, "uint32") == 0) {
        return "u32";
    }
    if (strcmp(dtype, "int64") == 0) {
        return "i64";
    }
    if (strcmp(dtype, "uint64") == 0) {
        return "u64";
    }
    if (strcmp(dtype, "float32") == 0) {
        return "f32";
    }
    if (strcmp(dtype, "float64") == 0 || strcmp(dtype, "double") == 0) {
        return "f64";
    }
    if (strcmp(dtype, "bool") == 0 || strcmp(dtype, "bool_") == 0) {
        return "bool";
    }
    return dtype;
}

static int kain_py_copy_python_text(PyObject* object, char* dest, size_t dest_size) {
    PyObject* text_obj;
    const char* utf8;
    if (!object || !dest || dest_size == 0u) {
        return 0;
    }
    text_obj = g_kain_python_api.PyObject_Str(object);
    if (!text_obj) {
        kain_py_clear_error();
        return 0;
    }
    utf8 = g_kain_python_api.PyUnicode_AsUTF8(text_obj);
    if (!utf8) {
        kain_py_clear_error();
        g_kain_python_api.Py_DecRef(text_obj);
        return 0;
    }
    strncpy_s(dest, dest_size, utf8, _TRUNCATE);
    g_kain_python_api.Py_DecRef(text_obj);
    return 1;
}

static int kain_py_copy_attr_text(PyObject* object, const char* name, char* dest, size_t dest_size) {
    PyObject* attr;
    int ok;
    if (!object || !name || !dest || dest_size == 0u) {
        return 0;
    }
    attr = g_kain_python_api.PyObject_GetAttrString(object, name);
    if (!attr) {
        kain_py_clear_error();
        return 0;
    }
    ok = kain_py_copy_python_text(attr, dest, dest_size);
    g_kain_python_api.Py_DecRef(attr);
    return ok;
}

static PyObject* kain_py_call_method0_owned(PyObject* object, const char* name) {
    PyObject* method;
    PyObject* args;
    PyObject* result;
    if (!object || !name) {
        return NULL;
    }
    method = g_kain_python_api.PyObject_GetAttrString(object, name);
    if (!method) {
        kain_py_clear_error();
        return NULL;
    }
    args = g_kain_python_api.PyTuple_New(0);
    if (!args) {
        g_kain_python_api.Py_DecRef(method);
        kain_py_clear_error();
        return NULL;
    }
    result = g_kain_python_api.PyObject_Call(method, args, NULL);
    g_kain_python_api.Py_DecRef(args);
    g_kain_python_api.Py_DecRef(method);
    if (!result) {
        kain_py_clear_error();
        return NULL;
    }
    return result;
}

static PyObject* kain_py_call_method1_owned(PyObject* object, const char* name, PyObject* arg0) {
    PyObject* method;
    PyObject* args;
    PyObject* result;
    if (!object || !name || !arg0) {
        return NULL;
    }
    method = g_kain_python_api.PyObject_GetAttrString(object, name);
    if (!method) {
        kain_py_clear_error();
        return NULL;
    }
    args = g_kain_python_api.PyTuple_New(1);
    if (!args) {
        g_kain_python_api.Py_DecRef(method);
        kain_py_clear_error();
        return NULL;
    }
    g_kain_python_api.Py_IncRef(arg0);
    if (g_kain_python_api.PyTuple_SetItem(args, 0, arg0) != 0) {
        g_kain_python_api.Py_DecRef(arg0);
        g_kain_python_api.Py_DecRef(args);
        g_kain_python_api.Py_DecRef(method);
        kain_py_clear_error();
        return NULL;
    }
    result = g_kain_python_api.PyObject_Call(method, args, NULL);
    g_kain_python_api.Py_DecRef(args);
    g_kain_python_api.Py_DecRef(method);
    if (!result) {
        kain_py_clear_error();
        return NULL;
    }
    return result;
}

static int kain_py_copy_dtype_name(PyObject* object, char* dest, size_t dest_size) {
    PyObject* dtype;
    int ok = 0;
    if (!object || !dest || dest_size == 0u) {
        return 0;
    }
    dtype = g_kain_python_api.PyObject_GetAttrString(object, "dtype");
    if (!dtype) {
        kain_py_clear_error();
        return 0;
    }
    ok = kain_py_copy_attr_text(dtype, "name", dest, dest_size);
    if (!ok) {
        ok = kain_py_copy_python_text(dtype, dest, dest_size);
    }
    g_kain_python_api.Py_DecRef(dtype);
    return ok;
}

static int kain_py_read_attr_int_sequence(
    PyObject* object,
    const char* name,
    long long** out_values,
    long long* out_len
) {
    PyObject* sequence;
    Py_ssize_t len;
    Py_ssize_t index;
    long long* values = NULL;
    if (out_values) {
        *out_values = NULL;
    }
    if (out_len) {
        *out_len = 0;
    }
    if (!object || !name) {
        return 0;
    }
    sequence = g_kain_python_api.PyObject_GetAttrString(object, name);
    if (!sequence) {
        kain_py_clear_error();
        return 0;
    }
    len = g_kain_python_api.PySequence_Size(sequence);
    if (len < 0) {
        g_kain_python_api.Py_DecRef(sequence);
        kain_py_clear_error();
        return 0;
    }
    if (len > 0) {
        values = (long long*)calloc((size_t)len, sizeof(long long));
        if (!values) {
            g_kain_python_api.Py_DecRef(sequence);
            return 0;
        }
        for (index = 0; index < len; ++index) {
            PyObject* item = g_kain_python_api.PySequence_GetItem(sequence, index);
            PyObject* coerced;
            if (!item) {
                free(values);
                g_kain_python_api.Py_DecRef(sequence);
                kain_py_clear_error();
                return 0;
            }
            coerced = g_kain_python_api.PyNumber_Long(item);
            g_kain_python_api.Py_DecRef(item);
            if (!coerced) {
                free(values);
                g_kain_python_api.Py_DecRef(sequence);
                kain_py_clear_error();
                return 0;
            }
            values[index] = g_kain_python_api.PyLong_AsLongLong(coerced);
            g_kain_python_api.Py_DecRef(coerced);
        }
    }
    g_kain_python_api.Py_DecRef(sequence);
    if (out_values) {
        *out_values = values;
    } else if (values) {
        free(values);
    }
    if (out_len) {
        *out_len = (long long)len;
    }
    return 1;
}

static long long kain_py_element_size_from_storage(const char* element_type) {
    if (!element_type || !element_type[0]) {
        return 1;
    }
    if (strcmp(element_type, "bool") == 0 ||
        strcmp(element_type, "u8") == 0 ||
        strcmp(element_type, "i8") == 0) {
        return 1;
    }
    if (strcmp(element_type, "u16") == 0 ||
        strcmp(element_type, "i16") == 0) {
        return 2;
    }
    if (strcmp(element_type, "u32") == 0 ||
        strcmp(element_type, "i32") == 0 ||
        strcmp(element_type, "f32") == 0) {
        return 4;
    }
    if (strcmp(element_type, "u64") == 0 ||
        strcmp(element_type, "i64") == 0 ||
        strcmp(element_type, "f64") == 0) {
        return 8;
    }
    return 1;
}

static int kain_py_read_sequence_int_values(
    PyObject* sequence,
    long long** out_values,
    long long* out_len
) {
    Py_ssize_t len;
    Py_ssize_t index;
    long long* values = NULL;
    if (out_values) {
        *out_values = NULL;
    }
    if (out_len) {
        *out_len = 0;
    }
    if (!sequence) {
        return 0;
    }
    len = g_kain_python_api.PySequence_Size(sequence);
    if (len < 0) {
        kain_py_clear_error();
        return 0;
    }
    if (len > 0) {
        values = (long long*)calloc((size_t)len, sizeof(long long));
        if (!values) {
            return 0;
        }
        for (index = 0; index < len; ++index) {
            PyObject* item = g_kain_python_api.PySequence_GetItem(sequence, index);
            PyObject* coerced;
            if (!item) {
                free(values);
                kain_py_clear_error();
                return 0;
            }
            coerced = g_kain_python_api.PyNumber_Long(item);
            g_kain_python_api.Py_DecRef(item);
            if (!coerced) {
                free(values);
                kain_py_clear_error();
                return 0;
            }
            values[index] = g_kain_python_api.PyLong_AsLongLong(coerced);
            g_kain_python_api.Py_DecRef(coerced);
        }
    }
    if (out_values) {
        *out_values = values;
    } else if (values) {
        free(values);
    }
    if (out_len) {
        *out_len = (long long)len;
    }
    return 1;
}

static PyObject* kain_py_mapping_get_item_string_owned(PyObject* mapping, const char* key) {
    PyObject* py_key;
    PyObject* value;
    if (!mapping || !key) {
        return NULL;
    }
    py_key = g_kain_python_api.PyUnicode_FromString(key);
    if (!py_key) {
        kain_py_clear_error();
        return NULL;
    }
    value = g_kain_python_api.PyObject_GetItem(mapping, py_key);
    g_kain_python_api.Py_DecRef(py_key);
    if (!value) {
        kain_py_clear_error();
        return NULL;
    }
    return value;
}

static long long kain_py_method0_int(PyObject* object, const char* name, long long fallback) {
    PyObject* result;
    PyObject* coerced;
    long long value = fallback;
    if (!object || !name) {
        return fallback;
    }
    result = kain_py_call_method0_owned(object, name);
    if (!result) {
        return fallback;
    }
    coerced = g_kain_python_api.PyNumber_Long(result);
    g_kain_python_api.Py_DecRef(result);
    if (!coerced) {
        kain_py_clear_error();
        return fallback;
    }
    value = g_kain_python_api.PyLong_AsLongLong(coerced);
    g_kain_python_api.Py_DecRef(coerced);
    return value;
}

static int kain_py_copy_typestr_dtype(const char* typestr, char* dest, size_t dest_size) {
    size_t len;
    size_t index = 0u;
    char kind = '\0';
    int width;
    if (!dest || dest_size == 0u) {
        return 0;
    }
    dest[0] = '\0';
    if (!typestr || !typestr[0]) {
        return 0;
    }
    len = strlen(typestr);
    while (index < len) {
        char ch = typestr[index];
        if (ch == 'f' || ch == 'i' || ch == 'u' || ch == 'b' || ch == '?') {
            kind = ch;
            index += 1u;
            break;
        }
        index += 1u;
    }
    if (kind == '\0') {
        return 0;
    }
    if (kind == '?') {
        kind = 'b';
        width = 1;
    } else {
        if (index >= len) {
            return 0;
        }
        width = atoi(typestr + index);
        if (width <= 0) {
            return 0;
        }
    }
    if (kind == 'f' && width == 4) {
        strncpy_s(dest, dest_size, "float32", _TRUNCATE);
        return 1;
    }
    if (kind == 'f' && width == 8) {
        strncpy_s(dest, dest_size, "float64", _TRUNCATE);
        return 1;
    }
    if (kind == 'i' && width == 1) {
        strncpy_s(dest, dest_size, "int8", _TRUNCATE);
        return 1;
    }
    if (kind == 'i' && width == 2) {
        strncpy_s(dest, dest_size, "int16", _TRUNCATE);
        return 1;
    }
    if (kind == 'i' && width == 4) {
        strncpy_s(dest, dest_size, "int32", _TRUNCATE);
        return 1;
    }
    if (kind == 'i' && width == 8) {
        strncpy_s(dest, dest_size, "int64", _TRUNCATE);
        return 1;
    }
    if (kind == 'u' && width == 1) {
        strncpy_s(dest, dest_size, "uint8", _TRUNCATE);
        return 1;
    }
    if (kind == 'u' && width == 2) {
        strncpy_s(dest, dest_size, "uint16", _TRUNCATE);
        return 1;
    }
    if (kind == 'u' && width == 4) {
        strncpy_s(dest, dest_size, "uint32", _TRUNCATE);
        return 1;
    }
    if (kind == 'u' && width == 8) {
        strncpy_s(dest, dest_size, "uint64", _TRUNCATE);
        return 1;
    }
    if (kind == 'b' && width == 1) {
        strncpy_s(dest, dest_size, "bool", _TRUNCATE);
        return 1;
    }
    return 0;
}

static int kain_py_parse_device_string(
    const char* device_text,
    char* device_kind,
    size_t device_kind_size,
    long long* out_ordinal
) {
    const char* colon;
    if (out_ordinal) {
        *out_ordinal = 0;
    }
    if (!device_text || !device_text[0] || !device_kind || device_kind_size == 0u) {
        return 0;
    }
    device_kind[0] = '\0';
    colon = strchr(device_text, ':');
    if (colon) {
        size_t prefix_len = (size_t)(colon - device_text);
        if (prefix_len > 0u) {
            snprintf(device_kind, device_kind_size, "%.*s", (int)prefix_len, device_text);
        }
        if (out_ordinal) {
            *out_ordinal = _strtoi64(colon + 1, NULL, 10);
        }
        return 1;
    }
    strncpy_s(device_kind, device_kind_size, device_text, _TRUNCATE);
    return 1;
}

static const char* kain_py_device_kind_from_dlpack_code(long long code) {
    if (code == 1) {
        return "cpu";
    }
    if (code == 2) {
        return "cuda";
    }
    return "";
}

static int kain_py_shape_element_count(const long long* shape, long long len, long long* out_count) {
    long long index;
    long long count = 1;
    if (out_count) {
        *out_count = 0;
    }
    if (!shape || len <= 0 || !out_count) {
        return 0;
    }
    for (index = 0; index < len; ++index) {
        long long dim = shape[index];
        if (dim < 0) {
            return 0;
        }
        if (!kain_py_checked_mul_i64(count, dim, &count)) {
            return 0;
        }
    }
    *out_count = count;
    return 1;
}

static long long* kain_py_copy_long_long_buffer(const long long* values, long long len) {
    long long* copy;
    if (!values || len <= 0) {
        return NULL;
    }
    copy = (long long*)calloc((size_t)len, sizeof(long long));
    if (!copy) {
        return NULL;
    }
    memcpy(copy, values, (size_t)len * sizeof(long long));
    return copy;
}

static long long* kain_py_compact_strides_values(const long long* shape, long long len) {
    long long* values;
    long long stride = 1;
    long long index;
    if (!shape || len <= 0) {
        return NULL;
    }
    values = (long long*)calloc((size_t)len, sizeof(long long));
    if (!values) {
        return NULL;
    }
    index = len - 1;
    while (index >= 0) {
        values[index] = stride;
        if (!kain_py_checked_mul_i64(stride, shape[index] > 0 ? shape[index] : 1, &stride)) {
            free(values);
            return NULL;
        }
        if (index == 0) {
            break;
        }
        index -= 1;
    }
    return values;
}

static int kain_py_stride_values_look_like_bytes(
    const long long* values,
    long long len,
    long long element_size
) {
    long long index;
    if (!values || len <= 0 || element_size <= 1) {
        return 0;
    }
    for (index = 0; index < len; ++index) {
        if (values[index] > 0 && (values[index] % element_size) != 0) {
            return 0;
        }
    }
    return 1;
}

static void kain_py_stride_values_to_elements(long long* values, long long len, long long element_size) {
    long long index;
    if (!values || len <= 0 || element_size <= 1) {
        return;
    }
    for (index = 0; index < len; ++index) {
        if (values[index] > 0) {
            values[index] /= element_size;
        }
    }
}

static int kain_py_tensor_strides_are_compact(const KainPythonTensorHandle* tensor) {
    long long* expected;
    long long index;
    int matches = 1;
    if (!tensor || !tensor->shape || !tensor->strides || tensor->ndim <= 0 || tensor->stride_count != tensor->ndim) {
        return 0;
    }
    expected = kain_py_compact_strides_values(tensor->shape, tensor->ndim);
    if (!expected) {
        return 0;
    }
    for (index = 0; index < tensor->ndim; ++index) {
        if (expected[index] != tensor->strides[index]) {
            matches = 0;
            break;
        }
    }
    free(expected);
    return matches;
}

static int kain_py_tensor_capture_cuda_array_interface(PyObject* object, KainPythonTensorHandle* tensor) {
    const char* storage;
    PyObject* interface_obj;
    PyObject* data_obj;
    PyObject* shape_obj;
    PyObject* strides_obj;
    PyObject* version_obj;
    PyObject* typestr_obj;
    if (!object || !tensor || g_kain_python_api.PyObject_HasAttrString(object, "__cuda_array_interface__") <= 0) {
        return 0;
    }
    interface_obj = g_kain_python_api.PyObject_GetAttrString(object, "__cuda_array_interface__");
    if (!interface_obj) {
        kain_py_clear_error();
        return 0;
    }
    version_obj = kain_py_mapping_get_item_string_owned(interface_obj, "version");
    if (version_obj) {
        PyObject* coerced = g_kain_python_api.PyNumber_Long(version_obj);
        if (coerced) {
            tensor->cuda_array_interface_version = (int)g_kain_python_api.PyLong_AsLongLong(coerced);
            g_kain_python_api.Py_DecRef(coerced);
        } else {
            kain_py_clear_error();
        }
        g_kain_python_api.Py_DecRef(version_obj);
    }
    data_obj = kain_py_mapping_get_item_string_owned(interface_obj, "data");
    if (data_obj) {
        PyObject* pointer_obj = g_kain_python_api.PySequence_GetItem(data_obj, 0);
        if (pointer_obj) {
            PyObject* coerced = g_kain_python_api.PyNumber_Long(pointer_obj);
            if (coerced) {
                tensor->device_pointer = g_kain_python_api.PyLong_AsLongLong(coerced);
                g_kain_python_api.Py_DecRef(coerced);
            } else {
                kain_py_clear_error();
            }
            g_kain_python_api.Py_DecRef(pointer_obj);
        } else {
            kain_py_clear_error();
        }
        g_kain_python_api.Py_DecRef(data_obj);
    }
    shape_obj = kain_py_mapping_get_item_string_owned(interface_obj, "shape");
    if (shape_obj && !tensor->shape) {
        (void)kain_py_read_sequence_int_values(shape_obj, &tensor->shape, &tensor->ndim);
        g_kain_python_api.Py_DecRef(shape_obj);
    } else if (shape_obj) {
        g_kain_python_api.Py_DecRef(shape_obj);
    }
    strides_obj = kain_py_mapping_get_item_string_owned(interface_obj, "strides");
    if (strides_obj && !tensor->strides) {
        if (kain_py_read_sequence_int_values(strides_obj, &tensor->strides, &tensor->stride_count) &&
            kain_py_stride_values_look_like_bytes(tensor->strides, tensor->stride_count, tensor->element_size)) {
            kain_py_stride_values_to_elements(tensor->strides, tensor->stride_count, tensor->element_size);
        }
        g_kain_python_api.Py_DecRef(strides_obj);
    } else if (strides_obj) {
        g_kain_python_api.Py_DecRef(strides_obj);
    }
    typestr_obj = kain_py_mapping_get_item_string_owned(interface_obj, "typestr");
    if (typestr_obj && !tensor->dtype[0]) {
        const char* utf8 = g_kain_python_api.PyUnicode_AsUTF8(typestr_obj);
        if (utf8) {
            (void)kain_py_copy_typestr_dtype(utf8, tensor->dtype, sizeof(tensor->dtype));
        } else {
            kain_py_clear_error();
        }
        g_kain_python_api.Py_DecRef(typestr_obj);
    } else if (typestr_obj) {
        g_kain_python_api.Py_DecRef(typestr_obj);
    }
    if (tensor->dtype[0]) {
        storage = kain_py_storage_from_dtype(tensor->dtype);
        if (!tensor->element_type[0] || strcmp(tensor->element_type, "unknown") == 0) {
            strncpy_s(tensor->element_type, sizeof(tensor->element_type), storage, _TRUNCATE);
        }
        if (tensor->element_size <= 0) {
            tensor->element_size = kain_py_element_size_from_storage(storage);
        }
    }
    if (!tensor->strides && tensor->shape && tensor->ndim > 0) {
        tensor->strides = kain_py_compact_strides_values(tensor->shape, tensor->ndim);
        tensor->stride_count = tensor->strides ? tensor->ndim : 0;
    }
    if (tensor->element_count <= 0 && tensor->shape && tensor->ndim > 0) {
        (void)kain_py_shape_element_count(tensor->shape, tensor->ndim, &tensor->element_count);
    }
    if (tensor->byte_length <= 0 &&
        tensor->element_count > 0 &&
        tensor->element_size > 0) {
        (void)kain_py_checked_mul_i64(
            tensor->element_count,
            tensor->element_size,
            &tensor->byte_length
        );
    }
    if (!tensor->device_kind[0]) {
        strncpy_s(tensor->device_kind, sizeof(tensor->device_kind), "cuda", _TRUNCATE);
    }
    tensor->device_type_code = 2;
    tensor->host_accessible = 0;
    if (!tensor->interop_lane[0]) {
        strncpy_s(tensor->interop_lane, sizeof(tensor->interop_lane), "cuda_array_interface", _TRUNCATE);
    }
    g_kain_python_api.Py_DecRef(interface_obj);
    return 1;
}

static void kain_py_tensor_capture_device_metadata(PyObject* object, KainPythonTensorHandle* tensor) {
    PyObject* device_attr;
    if (!object || !tensor) {
        return;
    }
    tensor->dlpack_capable = g_kain_python_api.PyObject_HasAttrString(object, "__dlpack__") > 0 ? 1 : 0;
    if (g_kain_python_api.PyObject_HasAttrString(object, "__dlpack_device__") > 0) {
        PyObject* device_tuple = kain_py_call_method0_owned(object, "__dlpack_device__");
        if (device_tuple) {
            PyObject* type_obj = g_kain_python_api.PySequence_GetItem(device_tuple, 0);
            PyObject* ordinal_obj = g_kain_python_api.PySequence_GetItem(device_tuple, 1);
            if (type_obj) {
                PyObject* coerced = g_kain_python_api.PyNumber_Long(type_obj);
                if (coerced) {
                    tensor->device_type_code = g_kain_python_api.PyLong_AsLongLong(coerced);
                    g_kain_python_api.Py_DecRef(coerced);
                } else {
                    kain_py_clear_error();
                }
                g_kain_python_api.Py_DecRef(type_obj);
            } else {
                kain_py_clear_error();
            }
            if (ordinal_obj) {
                PyObject* coerced = g_kain_python_api.PyNumber_Long(ordinal_obj);
                if (coerced) {
                    tensor->device_ordinal = g_kain_python_api.PyLong_AsLongLong(coerced);
                    g_kain_python_api.Py_DecRef(coerced);
                } else {
                    kain_py_clear_error();
                }
                g_kain_python_api.Py_DecRef(ordinal_obj);
            } else {
                kain_py_clear_error();
            }
            if (!tensor->device_kind[0]) {
                const char* kind = kain_py_device_kind_from_dlpack_code(tensor->device_type_code);
                if (kind[0]) {
                    strncpy_s(tensor->device_kind, sizeof(tensor->device_kind), kind, _TRUNCATE);
                }
            }
            if (!tensor->interop_lane[0]) {
                strncpy_s(tensor->interop_lane, sizeof(tensor->interop_lane), "dlpack_device", _TRUNCATE);
            }
            if (tensor->device_type_code == 2) {
                tensor->host_accessible = 0;
            }
            g_kain_python_api.Py_DecRef(device_tuple);
        }
    }
    device_attr = g_kain_python_api.PyObject_GetAttrString(object, "device");
    if (device_attr) {
        if (kain_py_copy_python_text(device_attr, tensor->device, sizeof(tensor->device))) {
            (void)kain_py_parse_device_string(
                tensor->device,
                tensor->device_kind,
                sizeof(tensor->device_kind),
                &tensor->device_ordinal
            );
            if (_stricmp(tensor->device_kind, "cuda") == 0) {
                tensor->device_type_code = 2;
                tensor->host_accessible = 0;
            }
        }
        g_kain_python_api.Py_DecRef(device_attr);
    } else {
        kain_py_clear_error();
    }
    if (kain_py_tensor_capture_cuda_array_interface(object, tensor)) {
        if (!tensor->device[0]) {
            if (tensor->device_ordinal > 0) {
                snprintf(tensor->device, sizeof(tensor->device), "%s:%lld", tensor->device_kind, tensor->device_ordinal);
            } else {
                strncpy_s(tensor->device, sizeof(tensor->device), tensor->device_kind, _TRUNCATE);
            }
        }
        return;
    }
    if (!tensor->device_kind[0]) {
        strncpy_s(tensor->device_kind, sizeof(tensor->device_kind), "cpu", _TRUNCATE);
    }
    if (!tensor->device[0]) {
        strncpy_s(tensor->device, sizeof(tensor->device), tensor->device_kind, _TRUNCATE);
    }
}

static int64_t kain_py_tensor_shape_handle(const KainPythonTensorHandle* tensor) {
    if (!tensor || !tensor->shape || tensor->ndim <= 0) {
        return 0;
    }
    return kain_py_json_array_from_values(tensor->shape, tensor->ndim);
}

static int64_t kain_py_tensor_strides_handle(const KainPythonTensorHandle* tensor) {
    if (!tensor || !tensor->strides || tensor->stride_count <= 0) {
        return 0;
    }
    return kain_py_json_array_from_values(tensor->strides, tensor->stride_count);
}

static int kain_py_tensor_has_virtual_attr(const char* name) {
    if (!name) {
        return 0;
    }
    return strcmp(name, "shape") == 0 ||
        strcmp(name, "strides") == 0 ||
        strcmp(name, "ownership") == 0 ||
        strcmp(name, "dtype") == 0 ||
        strcmp(name, "element_type") == 0 ||
        strcmp(name, "source_runtime") == 0 ||
        strcmp(name, "source_backend") == 0 ||
        strcmp(name, "device") == 0 ||
        strcmp(name, "device_kind") == 0 ||
        strcmp(name, "interop_lane") == 0 ||
        strcmp(name, "ndim") == 0 ||
        strcmp(name, "element_size") == 0 ||
        strcmp(name, "element_count") == 0 ||
        strcmp(name, "byte_length") == 0 ||
        strcmp(name, "device_ordinal") == 0 ||
        strcmp(name, "device_pointer") == 0 ||
        strcmp(name, "device_type_code") == 0 ||
        strcmp(name, "host_accessible") == 0 ||
        strcmp(name, "is_contiguous") == 0 ||
        strcmp(name, "writable") == 0 ||
        strcmp(name, "dlpack_capable") == 0 ||
        strcmp(name, "cuda_array_interface_version") == 0;
}

static long long kain_py_tensor_attr_value(const KainPythonTensorHandle* tensor, const char* name) {
    if (!tensor || !name) {
        return 0;
    }
    if (strcmp(name, "shape") == 0) {
        return kain_py_tensor_shape_handle(tensor);
    }
    if (strcmp(name, "strides") == 0) {
        return kain_py_tensor_strides_handle(tensor);
    }
    if (strcmp(name, "ownership") == 0) {
        return kain_py_string_tag(tensor->ownership);
    }
    if (strcmp(name, "dtype") == 0) {
        return kain_py_string_tag(tensor->dtype);
    }
    if (strcmp(name, "element_type") == 0) {
        return kain_py_string_tag(tensor->element_type);
    }
    if (strcmp(name, "source_runtime") == 0) {
        return kain_py_string_tag("python");
    }
    if (strcmp(name, "source_backend") == 0) {
        return tensor->source_backend[0] ? kain_py_string_tag(tensor->source_backend) : KAIN_PY_JSON_NULL;
    }
    if (strcmp(name, "device") == 0) {
        return tensor->device[0] ? kain_py_string_tag(tensor->device) : KAIN_PY_JSON_NULL;
    }
    if (strcmp(name, "device_kind") == 0) {
        return tensor->device_kind[0] ? kain_py_string_tag(tensor->device_kind) : KAIN_PY_JSON_NULL;
    }
    if (strcmp(name, "interop_lane") == 0) {
        return tensor->interop_lane[0] ? kain_py_string_tag(tensor->interop_lane) : KAIN_PY_JSON_NULL;
    }
    if (strcmp(name, "ndim") == 0) {
        return KAIN_PY_JSON_INT(tensor->ndim);
    }
    if (strcmp(name, "element_size") == 0) {
        return KAIN_PY_JSON_INT(tensor->element_size);
    }
    if (strcmp(name, "element_count") == 0) {
        return KAIN_PY_JSON_INT(tensor->element_count);
    }
    if (strcmp(name, "byte_length") == 0) {
        return KAIN_PY_JSON_INT(tensor->byte_length);
    }
    if (strcmp(name, "device_ordinal") == 0) {
        return KAIN_PY_JSON_INT(tensor->device_ordinal);
    }
    if (strcmp(name, "device_pointer") == 0) {
        return KAIN_PY_JSON_INT(tensor->device_pointer);
    }
    if (strcmp(name, "device_type_code") == 0) {
        return KAIN_PY_JSON_INT(tensor->device_type_code);
    }
    if (strcmp(name, "host_accessible") == 0) {
        return KAIN_PY_JSON_BOOL(tensor->host_accessible);
    }
    if (strcmp(name, "is_contiguous") == 0) {
        return KAIN_PY_JSON_BOOL(tensor->contiguous);
    }
    if (strcmp(name, "writable") == 0) {
        return KAIN_PY_JSON_BOOL(tensor->writable);
    }
    if (strcmp(name, "dlpack_capable") == 0) {
        return KAIN_PY_JSON_BOOL(tensor->dlpack_capable);
    }
    if (strcmp(name, "cuda_array_interface_version") == 0) {
        return KAIN_PY_JSON_INT(tensor->cuda_array_interface_version);
    }
    return 0;
}

static int64_t kain_py_array_handle_from_values(const long long* values, long long len) {
    KainArray* array;
    long long index;
    array = array_new(len > 0 ? len : 1);
    if (!array) {
        return 0;
    }
    for (index = 0; index < len; ++index) {
        array_push(array, values[index]);
    }
    return (int64_t)(intptr_t)array;
}

static int64_t kain_py_json_array_from_values(const long long* values, long long len) {
    int64_t array;
    long long index;
    array = json_array_new();
    if (!array) {
        return 0;
    }
    for (index = 0; index < len; ++index) {
        json_array_push(array, KAIN_PY_JSON_INT(values[index]));
    }
    return array;
}

static int64_t kain_py_build_shared_labels(const char* first, const char* second) {
    KainArray* labels = array_new(2);
    if (!labels) {
        return 0;
    }
    array_push(labels, (int64_t)(intptr_t)string_new((char*)(first ? first : "")));
    array_push(labels, (int64_t)(intptr_t)string_new((char*)(second ? second : "")));
    return (int64_t)(intptr_t)labels;
}

static int kain_py_image_shape_values(
    const KainPythonImageHandle* image,
    long long* values,
    long long* out_count
) {
    long long count = 0;
    if (out_count) {
        *out_count = 0;
    }
    if (!image || !values || !out_count) {
        return 0;
    }
    if (strcmp(image->layout, "HW") == 0) {
        values[0] = image->height;
        values[1] = image->width;
        count = 2;
    } else if (strcmp(image->layout, "CHW") == 0) {
        values[0] = image->channels;
        values[1] = image->height;
        values[2] = image->width;
        count = 3;
    } else if (strcmp(image->layout, "NHWC") == 0) {
        values[0] = image->batch > 0 ? image->batch : 1;
        values[1] = image->height;
        values[2] = image->width;
        values[3] = image->channels;
        count = 4;
    } else if (strcmp(image->layout, "NCHW") == 0) {
        values[0] = image->batch > 0 ? image->batch : 1;
        values[1] = image->channels;
        values[2] = image->height;
        values[3] = image->width;
        count = 4;
    } else {
        values[0] = image->height;
        values[1] = image->width;
        values[2] = image->channels;
        count = image->channels > 1 ? 3 : 2;
    }
    *out_count = count;
    return 1;
}

static int64_t kain_py_image_shape_handle(const KainPythonImageHandle* image) {
    long long values[4];
    long long count = 0;
    if (!image) {
        return 0;
    }
    if (!kain_py_image_shape_values(image, values, &count)) {
        return 0;
    }
    return kain_py_array_handle_from_values(values, count);
}

static int64_t kain_py_compact_strides_handle(const long long* shape, long long len) {
    long long values[8];
    long long stride = 1;
    long long index;
    if (!shape || len <= 0 || len > (long long)(sizeof(values) / sizeof(values[0]))) {
        return 0;
    }
    index = len - 1;
    while (index >= 0) {
        values[index] = stride;
        if (!kain_py_checked_mul_i64(stride, shape[index] > 0 ? shape[index] : 1, &stride)) {
            return 0;
        }
        if (index == 0) {
            break;
        }
        index -= 1;
    }
    return kain_py_array_handle_from_values(values, len);
}

static int64_t kain_py_image_strides_handle(const KainPythonImageHandle* image) {
    long long values[4];
    long long count = 0;
    if (!image) {
        return 0;
    }
    if (!kain_py_image_shape_values(image, values, &count)) {
        return 0;
    }
    return kain_py_compact_strides_handle(values, count);
}

static const char* kain_py_pixel_format(long long channels, const char* dtype) {
    const char* suffix = "x";
    if (!dtype || !dtype[0]) {
        dtype = "unknown";
    }
    if (strcmp(dtype, "uint8") == 0 || strcmp(dtype, "ubyte") == 0) {
        suffix = "8";
    } else if (strcmp(dtype, "uint16") == 0) {
        suffix = "16";
    }
    if (channels == 1) {
        return strcmp(suffix, "x") == 0 ? "r" : (strcmp(suffix, "8") == 0 ? "r8" : "r16");
    }
    if (channels == 2) {
        return strcmp(suffix, "x") == 0 ? "rg" : (strcmp(suffix, "8") == 0 ? "rg8" : "rg16");
    }
    if (channels == 3) {
        return strcmp(suffix, "x") == 0 ? "rgb" : (strcmp(suffix, "8") == 0 ? "rgb8" : "rgb16");
    }
    if (channels == 4) {
        return strcmp(suffix, "x") == 0 ? "rgba" : (strcmp(suffix, "8") == 0 ? "rgba8" : "rgba16");
    }
    return "channels";
}

static int kain_py_extract_byte_sequence(PyObject* object, unsigned char** out_bytes, long long* out_len) {
    Py_ssize_t len;
    Py_ssize_t index;
    unsigned char* bytes;
    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_len) {
        *out_len = 0;
    }
    if (!object || !out_bytes || !out_len) {
        return 0;
    }
    len = g_kain_python_api.PySequence_Size(object);
    if (len < 0) {
        kain_py_clear_error();
        return 0;
    }
    bytes = len > 0 ? (unsigned char*)malloc((size_t)len) : NULL;
    if (len > 0 && !bytes) {
        return 0;
    }
    for (index = 0; index < len; ++index) {
        PyObject* item = g_kain_python_api.PySequence_GetItem(object, index);
        PyObject* coerced;
        long long value;
        if (!item) {
            free(bytes);
            kain_py_clear_error();
            return 0;
        }
        coerced = g_kain_python_api.PyNumber_Long(item);
        g_kain_python_api.Py_DecRef(item);
        if (!coerced) {
            free(bytes);
            kain_py_clear_error();
            return 0;
        }
        value = g_kain_python_api.PyLong_AsLongLong(coerced);
        g_kain_python_api.Py_DecRef(coerced);
        if (value < 0 || value > 255) {
            free(bytes);
            return 0;
        }
        bytes[index] = (unsigned char)value;
    }
    *out_bytes = bytes;
    *out_len = (long long)len;
    return 1;
}

static PyObject* kain_py_export_buffer_target(
    PyObject* object,
    char* backend,
    size_t backend_size
) {
    PyObject* detached;
    PyObject* cpu_tensor;
    PyObject* contiguous;
    PyObject* numpy_array;
    if (backend && backend_size > 0u) {
        backend[0] = '\0';
    }
    if (!object) {
        return NULL;
    }
    if (backend && backend_size > 0u) {
        kain_py_copy_source_backend(object, backend, backend_size);
    }
    if (backend && strcmp(backend, "torch") == 0) {
        detached = kain_py_call_method0_owned(object, "detach");
        if (!detached) {
            return NULL;
        }
        cpu_tensor = kain_py_call_method0_owned(detached, "cpu");
        g_kain_python_api.Py_DecRef(detached);
        if (!cpu_tensor) {
            return NULL;
        }
        contiguous = kain_py_call_method0_owned(cpu_tensor, "contiguous");
        g_kain_python_api.Py_DecRef(cpu_tensor);
        if (!contiguous) {
            return NULL;
        }
        numpy_array = kain_py_call_method0_owned(contiguous, "numpy");
        g_kain_python_api.Py_DecRef(contiguous);
        if (!numpy_array) {
            return NULL;
        }
        return numpy_array;
    }
    g_kain_python_api.Py_IncRef(object);
    return object;
}

static long long kain_py_attr_int(PyObject* object, const char* name, long long fallback) {
    PyObject* attr;
    PyObject* coerced;
    long long value = fallback;
    if (!object || !name) {
        return fallback;
    }
    attr = g_kain_python_api.PyObject_GetAttrString(object, name);
    if (!attr) {
        kain_py_clear_error();
        return fallback;
    }
    coerced = g_kain_python_api.PyNumber_Long(attr);
    if (coerced) {
        value = g_kain_python_api.PyLong_AsLongLong(coerced);
        g_kain_python_api.Py_DecRef(coerced);
    } else {
        kain_py_clear_error();
    }
    g_kain_python_api.Py_DecRef(attr);
    return value;
}

static int kain_py_read_shape(PyObject* object, long long** out_shape, long long* out_ndim) {
    PyObject* shape_obj;
    Py_ssize_t shape_len;
    Py_ssize_t index;
    long long* shape = NULL;
    if (out_shape) {
        *out_shape = NULL;
    }
    if (out_ndim) {
        *out_ndim = 0;
    }
    if (!object) {
        return 0;
    }
    shape_obj = g_kain_python_api.PyObject_GetAttrString(object, "shape");
    if (!shape_obj) {
        kain_py_clear_error();
        return 0;
    }
    shape_len = g_kain_python_api.PySequence_Size(shape_obj);
    if (shape_len < 0) {
        kain_py_clear_error();
        g_kain_python_api.Py_DecRef(shape_obj);
        return 0;
    }
    if (shape_len > 0) {
        shape = (long long*)calloc((size_t)shape_len, sizeof(long long));
        if (!shape) {
            g_kain_python_api.Py_DecRef(shape_obj);
            return 0;
        }
        for (index = 0; index < shape_len; ++index) {
            PyObject* item = g_kain_python_api.PySequence_GetItem(shape_obj, index);
            if (!item) {
                kain_py_clear_error();
                free(shape);
                g_kain_python_api.Py_DecRef(shape_obj);
                return 0;
            }
            {
                PyObject* coerced = g_kain_python_api.PyNumber_Long(item);
                g_kain_python_api.Py_DecRef(item);
                if (!coerced) {
                    kain_py_clear_error();
                    free(shape);
                    g_kain_python_api.Py_DecRef(shape_obj);
                    return 0;
                }
                shape[index] = g_kain_python_api.PyLong_AsLongLong(coerced);
                g_kain_python_api.Py_DecRef(coerced);
            }
        }
    }
    g_kain_python_api.Py_DecRef(shape_obj);
    if (out_shape) {
        *out_shape = shape;
    } else if (shape) {
        free(shape);
    }
    if (out_ndim) {
        *out_ndim = (long long)shape_len;
    }
    return 1;
}

static int kain_py_infer_image_layout(
    const long long* shape,
    long long ndim,
    char* layout,
    size_t layout_size,
    long long* batch,
    long long* height,
    long long* width,
    long long* channels
) {
    if (!shape || !layout || layout_size == 0u || !batch || !height || !width || !channels) {
        return 0;
    }
    if (ndim == 2) {
        strncpy_s(layout, layout_size, "HW", _TRUNCATE);
        *batch = 1;
        *height = shape[0];
        *width = shape[1];
        *channels = 1;
        return 1;
    }
    if (ndim == 3 && kain_py_small_channel_count(shape[2])) {
        strncpy_s(layout, layout_size, "HWC", _TRUNCATE);
        *batch = 1;
        *height = shape[0];
        *width = shape[1];
        *channels = shape[2];
        return 1;
    }
    if (ndim == 3 && kain_py_small_channel_count(shape[0])) {
        strncpy_s(layout, layout_size, "CHW", _TRUNCATE);
        *batch = 1;
        *height = shape[1];
        *width = shape[2];
        *channels = shape[0];
        return 1;
    }
    if (ndim == 4 && kain_py_small_channel_count(shape[3])) {
        strncpy_s(layout, layout_size, "NHWC", _TRUNCATE);
        *batch = shape[0];
        *height = shape[1];
        *width = shape[2];
        *channels = shape[3];
        return 1;
    }
    if (ndim == 4 && kain_py_small_channel_count(shape[1])) {
        strncpy_s(layout, layout_size, "NCHW", _TRUNCATE);
        *batch = shape[0];
        *height = shape[2];
        *width = shape[3];
        *channels = shape[1];
        return 1;
    }
    return 0;
}

static long long kain_py_default_image_row_stride(
    const char* layout,
    long long width,
    long long channels,
    long long item_size
) {
    long long pixels_per_row = 0;
    long long bytes_per_row = 0;
    if (!layout || width < 0 || channels < 0 || item_size <= 0) {
        return 0;
    }
    if (strcmp(layout, "HW") != 0 &&
        strcmp(layout, "HWC") != 0 &&
        strcmp(layout, "CHW") != 0 &&
        strcmp(layout, "NHWC") != 0 &&
        strcmp(layout, "NCHW") != 0) {
        return 0;
    }
    if (!kain_py_checked_mul_i64(width, channels > 0 ? channels : 1, &pixels_per_row) ||
        !kain_py_checked_mul_i64(pixels_per_row, item_size, &bytes_per_row)) {
        return 0;
    }
    return bytes_per_row;
}

static long long kain_py_abs_i64(long long value) {
    return value < 0 ? -value : value;
}

static long long kain_py_image_row_stride_from_strides(
    const char* layout,
    const long long* strides,
    long long ndim
) {
    if (!layout || !strides || ndim <= 0) {
        return 0;
    }
    if (strcmp(layout, "HW") == 0 || strcmp(layout, "HWC") == 0) {
        return ndim >= 1 ? kain_py_abs_i64(strides[0]) : 0;
    }
    if (strcmp(layout, "CHW") == 0) {
        return ndim >= 2 ? kain_py_abs_i64(strides[1]) : 0;
    }
    if (strcmp(layout, "NHWC") == 0) {
        return ndim >= 2 ? kain_py_abs_i64(strides[1]) : 0;
    }
    if (strcmp(layout, "NCHW") == 0) {
        return ndim >= 3 ? kain_py_abs_i64(strides[2]) : 0;
    }
    return 0;
}

static int kain_py_copy_source_backend(PyObject* object, char* dest, size_t dest_size) {
    PyObject* klass;
    PyObject* module_name;
    int ok = 0;
    if (!object || !dest || dest_size == 0u) {
        return 0;
    }
    klass = g_kain_python_api.PyObject_GetAttrString(object, "__class__");
    if (!klass) {
        kain_py_clear_error();
        return 0;
    }
    module_name = g_kain_python_api.PyObject_GetAttrString(klass, "__module__");
    g_kain_python_api.Py_DecRef(klass);
    if (!module_name) {
        kain_py_clear_error();
        return 0;
    }
    if (kain_py_copy_python_text(module_name, dest, dest_size)) {
        if (strstr(dest, "numpy") != NULL) {
            strncpy_s(dest, dest_size, "numpy", _TRUNCATE);
        } else if (strstr(dest, "torch") != NULL) {
            strncpy_s(dest, dest_size, "torch", _TRUNCATE);
        }
        ok = 1;
    }
    g_kain_python_api.Py_DecRef(module_name);
    return ok;
}

static KainPythonImageHandle* kain_py_wrap_image(PyObject* object, const char* ownership) {
    KainPythonImageHandle* image;
    long long* strides = NULL;
    long long strides_ndim = 0;
    long long item_size = 1;
    long long byte_length = 0;
    if (!object) {
        return NULL;
    }
    image = (KainPythonImageHandle*)kain_alloc_rc(sizeof(KainPythonImageHandle), KAIN_RC_TYPE_PY_IMAGE);
    if (!image) {
        return NULL;
    }
    memset(image, 0, sizeof(*image));
    image->object = object;
    strncpy_s(image->ownership, sizeof(image->ownership), ownership ? ownership : "shared", _TRUNCATE);
    image->zero_copy = ownership == NULL || strcmp(ownership, "owned") != 0;
    if (!kain_py_read_shape(object, &image->shape, &image->ndim)) {
        rc_release(image);
        return NULL;
    }
    if (!kain_py_infer_image_layout(
            image->shape,
            image->ndim,
            image->layout,
            sizeof(image->layout),
            &image->batch,
            &image->height,
            &image->width,
            &image->channels)) {
        rc_release(image);
        return NULL;
    }
    item_size = kain_py_attr_int(object, "itemsize", 1);
    byte_length = kain_py_attr_int(object, "nbytes", 0);
    if (!kain_py_checked_mul_i64(image->batch > 0 ? image->batch : 1, image->width, &image->pixel_count) ||
        !kain_py_checked_mul_i64(image->pixel_count, image->height, &image->pixel_count)) {
        rc_release(image);
        return NULL;
    }
    if (byte_length > 0) {
        image->byte_length = byte_length;
    } else {
        long long channel_bytes = 0;
        if (!kain_py_checked_mul_i64(image->pixel_count, image->channels > 0 ? image->channels : 1, &channel_bytes) ||
            !kain_py_checked_mul_i64(channel_bytes, item_size > 0 ? item_size : 1, &image->byte_length)) {
            rc_release(image);
            return NULL;
        }
    }
    if (!kain_py_copy_attr_text(object, "dtype", image->dtype, sizeof(image->dtype))) {
        strncpy_s(image->dtype, sizeof(image->dtype), "unknown", _TRUNCATE);
    } else {
        PyObject* dtype_obj = g_kain_python_api.PyObject_GetAttrString(object, "dtype");
        if (dtype_obj) {
            char narrowed[24];
            if (kain_py_copy_attr_text(dtype_obj, "name", narrowed, sizeof(narrowed))) {
                strncpy_s(image->dtype, sizeof(image->dtype), narrowed, _TRUNCATE);
            }
            g_kain_python_api.Py_DecRef(dtype_obj);
        } else {
            kain_py_clear_error();
        }
    }
    strncpy_s(
        image->storage,
        sizeof(image->storage),
        kain_py_storage_from_dtype(image->dtype),
        _TRUNCATE);
    {
        PyObject* strides_obj = g_kain_python_api.PyObject_GetAttrString(object, "strides");
        if (strides_obj) {
            Py_ssize_t stride_len = g_kain_python_api.PySequence_Size(strides_obj);
            if (stride_len > 0) {
                long long* stride_values = (long long*)calloc((size_t)stride_len, sizeof(long long));
                Py_ssize_t index;
                if (stride_values) {
                    for (index = 0; index < stride_len; ++index) {
                        PyObject* item = g_kain_python_api.PySequence_GetItem(strides_obj, index);
                        if (!item) {
                            kain_py_clear_error();
                            break;
                        }
                        {
                            PyObject* coerced = g_kain_python_api.PyNumber_Long(item);
                            g_kain_python_api.Py_DecRef(item);
                            if (!coerced) {
                                kain_py_clear_error();
                                break;
                            }
                            stride_values[index] = g_kain_python_api.PyLong_AsLongLong(coerced);
                            g_kain_python_api.Py_DecRef(coerced);
                        }
                    }
                    if (index == stride_len) {
                        image->row_stride = kain_py_image_row_stride_from_strides(
                            image->layout,
                            stride_values,
                            (long long)stride_len);
                    }
                    free(stride_values);
                }
            }
            g_kain_python_api.Py_DecRef(strides_obj);
        } else {
            kain_py_clear_error();
        }
    }
    if (image->row_stride <= 0) {
        image->row_stride = kain_py_default_image_row_stride(
            image->layout,
            image->width,
            image->channels,
            item_size);
    }
    if (!kain_py_copy_source_backend(object, image->source_backend, sizeof(image->source_backend))) {
        image->source_backend[0] = '\0';
    }
    KAIN_set_destructor(image, NULL);
    return image;
}

static void kain_py_image_destructor(void* payload) {
    KainPythonImageHandle* image = (KainPythonImageHandle*)payload;
    KainPythonGilScope scope;
    if (!image) {
        return;
    }
    if (image->shape) {
        free(image->shape);
        image->shape = NULL;
    }
    if (!image->object) {
        return;
    }
    scope = kain_py_gil_enter();
    if (scope.active) {
        g_kain_python_api.Py_DecRef(image->object);
    }
    kain_py_gil_exit(&scope);
}

static int kain_py_finalize_wrap(KainPythonObjectHandle* handle, void (*destructor)(void*)) {
    if (!handle) {
        return 0;
    }
    KAIN_set_destructor(handle, destructor);
    return 1;
}

static KainPythonObjectHandle* kain_py_as_object_handle(long long value) {
    value = kain_py_unbox_tagged_handle(value, KAIN_RC_TYPE_PY_OBJECT);
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_OBJECT)
        ? (KainPythonObjectHandle*)(intptr_t)value
        : NULL;
}

static KainPythonTensorHandle* kain_py_as_tensor_handle(long long value) {
    value = kain_py_unbox_tagged_handle(value, KAIN_RC_TYPE_PY_TENSOR);
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_TENSOR)
        ? (KainPythonTensorHandle*)(intptr_t)value
        : NULL;
}

static KainPythonImageHandle* kain_py_as_image_handle(long long value) {
    value = kain_py_unbox_tagged_handle(value, KAIN_RC_TYPE_PY_IMAGE);
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_IMAGE)
        ? (KainPythonImageHandle*)(intptr_t)value
        : NULL;
}

static KainPythonBufferViewHandle* kain_py_as_buffer_view_handle(long long value) {
    value = kain_py_unbox_tagged_handle(value, KAIN_RC_TYPE_PY_BUFFER_VIEW);
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_BUFFER_VIEW)
        ? (KainPythonBufferViewHandle*)(intptr_t)value
        : NULL;
}

static PyObject* kain_py_lookup_name(const char* name) {
    PyObject* main_module;
    PyObject* attr;
    if (!name || !name[0]) {
        return NULL;
    }
    main_module = g_kain_python_api.PyImport_ImportModule("__main__");
    if (main_module) {
        attr = g_kain_python_api.PyObject_GetAttrString(main_module, name);
        g_kain_python_api.Py_DecRef(main_module);
        if (attr) {
            return attr;
        }
        kain_py_clear_error();
    } else {
        kain_py_clear_error();
    }
    return g_kain_python_api.PyImport_ImportModule(name);
}

static PyObject* kain_py_any_to_pyobject(long long value);

static PyObject* kain_py_any_to_tuple(long long value) {
    PyObject* tuple_obj;
    int kind = json_any_kind(value);
    if (kain_py_any_is_null_tag(value)) {
        return g_kain_python_api.PyTuple_New(0);
    }
    if (kind != KAIN_JSON_KIND_ARRAY) {
        PyObject* single = g_kain_python_api.PyTuple_New(1);
        PyObject* item = kain_py_any_to_pyobject(value);
        if (!single || !item) {
            if (single) {
                g_kain_python_api.Py_DecRef(single);
            }
            if (item) {
                g_kain_python_api.Py_DecRef(item);
            }
            return NULL;
        }
        g_kain_python_api.PyTuple_SetItem(single, 0, item);
        return single;
    }
    {
        long long len = json_array_len(value);
        long long index;
        tuple_obj = g_kain_python_api.PyTuple_New((Py_ssize_t)len);
        if (!tuple_obj) {
            return NULL;
        }
        for (index = 0; index < len; ++index) {
            PyObject* item = kain_py_any_to_pyobject(json_array_get(value, index));
            if (!item) {
                g_kain_python_api.Py_DecRef(tuple_obj);
                return NULL;
            }
            g_kain_python_api.PyTuple_SetItem(tuple_obj, (Py_ssize_t)index, item);
        }
        return tuple_obj;
    }
}

static PyObject* kain_py_any_to_kwargs(long long value) {
    if (kain_py_any_is_null_tag(value)) {
        return NULL;
    }
    if (kain_py_as_object_handle(value)) {
        KainPythonObjectHandle* handle = kain_py_as_object_handle(value);
        g_kain_python_api.Py_IncRef(handle->object);
        return handle->object;
    }
    return NULL;
}

static PyObject* kain_py_any_to_pyobject(long long value) {
    KainPythonObjectHandle* object_handle = kain_py_as_object_handle(value);
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(value);
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(value);
    int kind;
    if (object_handle) {
        g_kain_python_api.Py_IncRef(object_handle->object);
        return object_handle->object;
    }
    if (tensor_handle) {
        g_kain_python_api.Py_IncRef(tensor_handle->object);
        return tensor_handle->object;
    }
    if (image_handle) {
        g_kain_python_api.Py_IncRef(image_handle->object);
        return image_handle->object;
    }
    if (kain_py_any_is_null_tag(value)) {
        PyObject* globals = kain_py_main_dict();
        PyObject* none_obj = globals
            ? g_kain_python_api.PyRun_StringFlags("None", KAIN_PY_EVAL_INPUT, globals, globals, NULL)
            : NULL;
        if (globals) {
            g_kain_python_api.Py_DecRef(globals);
        }
        return none_obj;
    }
    kind = json_any_kind(value);
    switch (kind) {
        case KAIN_JSON_KIND_BOOL:
            return g_kain_python_api.PyBool_FromLong(json_any_to_int(value) != 0);
        case KAIN_JSON_KIND_INT:
            return g_kain_python_api.PyLong_FromLongLong(json_any_to_int(value));
        case KAIN_JSON_KIND_FLOAT:
            return g_kain_python_api.PyFloat_FromDouble(json_any_to_float(value));
        case KAIN_JSON_KIND_STRING:
            return g_kain_python_api.PyUnicode_FromString(json_any_to_string(value));
        case KAIN_JSON_KIND_ARRAY: {
            long long len = json_array_len(value);
            long long index;
            PyObject* list = g_kain_python_api.PyList_New(0);
            if (!list) {
                return NULL;
            }
            for (index = 0; index < len; ++index) {
                PyObject* item = kain_py_any_to_pyobject(json_array_get(value, index));
                if (!item) {
                    g_kain_python_api.Py_DecRef(list);
                    return NULL;
                }
                g_kain_python_api.PyList_Append(list, item);
                g_kain_python_api.Py_DecRef(item);
            }
            return list;
        }
        default:
            break;
    }
    if (kain_py_any_is_string_tag(value)) {
        return g_kain_python_api.PyUnicode_FromString((const char*)(intptr_t)(value & ~7LL));
    }
    return NULL;
}

static PyObject* kain_py_resolve_target(long long value) {
    if (kain_py_any_is_string_tag(value) || json_any_kind(value) == KAIN_JSON_KIND_STRING) {
        return kain_py_lookup_name(json_any_to_string(value));
    }
    return kain_py_any_to_pyobject(value);
}

static long long kain_py_wrap_owned_object(PyObject* object) {
    KainPythonObjectHandle* handle;
    if (!object) {
        kain_py_clear_error();
        return 0;
    }
    handle = kain_py_wrap_object(object);
    if (!handle) {
        g_kain_python_api.Py_DecRef(object);
        return 0;
    }
    kain_py_finalize_wrap(handle, kain_py_object_destructor);
    return (long long)(intptr_t)handle;
}

static long long kain_py_string_tag(const char* text) {
    char* owned = string_new((char*)(text ? text : ""));
    long long bits;
    if (!owned) {
        return 0;
    }
    bits = (long long)(intptr_t)owned;
    return bits | 3LL;
}

static int kain_py_copy_type_name(PyObject* object, char* dest, size_t dest_size) {
    PyObject* klass;
    PyObject* name;
    const char* utf8;
    if (!object || !dest || dest_size == 0u) {
        return 0;
    }
    dest[0] = '\0';
    klass = g_kain_python_api.PyObject_GetAttrString(object, "__class__");
    if (!klass) {
        kain_py_clear_error();
        return 0;
    }
    name = g_kain_python_api.PyObject_GetAttrString(klass, "__name__");
    g_kain_python_api.Py_DecRef(klass);
    if (!name) {
        kain_py_clear_error();
        return 0;
    }
    utf8 = g_kain_python_api.PyUnicode_AsUTF8(name);
    if (!utf8) {
        kain_py_clear_error();
        g_kain_python_api.Py_DecRef(name);
        return 0;
    }
    snprintf(dest, dest_size, "%s", utf8);
    g_kain_python_api.Py_DecRef(name);
    return 1;
}

static int kain_py_type_name_is(const char* type_name, const char* expected) {
    return type_name && expected && strcmp(type_name, expected) == 0;
}

static int kain_py_try_unicode_tag(PyObject* object, long long* out) {
    const char* utf8;
    if (!object || !out) {
        return 0;
    }
    utf8 = g_kain_python_api.PyUnicode_AsUTF8(object);
    if (!utf8) {
        kain_py_clear_error();
        return 0;
    }
    *out = kain_py_string_tag(utf8);
    return 1;
}

static int kain_py_try_long_value(PyObject* object, long long* out) {
    PyObject* coerced;
    if (!object || !out) {
        return 0;
    }
    coerced = g_kain_python_api.PyNumber_Long(object);
    if (!coerced) {
        kain_py_clear_error();
        return 0;
    }
    *out = g_kain_python_api.PyLong_AsLongLong(coerced);
    if (g_kain_python_api.PyErr_Occurred()) {
        kain_py_clear_error();
    }
    g_kain_python_api.Py_DecRef(coerced);
    return 1;
}

static int kain_py_try_float_tag(PyObject* object, long long* out) {
    PyObject* coerced;
    double value;
    if (!object || !out) {
        return 0;
    }
    coerced = g_kain_python_api.PyNumber_Float(object);
    if (!coerced) {
        kain_py_clear_error();
        return 0;
    }
    value = g_kain_python_api.PyFloat_AsDouble(coerced);
    if (g_kain_python_api.PyErr_Occurred()) {
        kain_py_clear_error();
        g_kain_python_api.Py_DecRef(coerced);
        return 0;
    }
    g_kain_python_api.Py_DecRef(coerced);
    *out = json_box_float(value);
    return 1;
}

static int kain_py_should_keep_raw_host(PyObject* object, const char* type_name) {
    if (!object) {
        return 0;
    }
    if (kain_py_type_name_is(type_name, "ndarray")) {
        return 1;
    }
    if (g_kain_python_api.PyObject_HasAttrString(object, "__array_interface__") > 0) {
        return 1;
    }
    if (g_kain_python_api.PyObject_HasAttrString(object, "__cuda_array_interface__") > 0) {
        return 1;
    }
    if (g_kain_python_api.PyObject_HasAttrString(object, "__dlpack__") > 0) {
        return 1;
    }
    return 0;
}

static long long kain_py_materialize_result(PyObject* object, int raw_mode, int boxed_scalars);

static long long kain_py_materialize_borrowed(PyObject* object, int raw_mode) {
    if (!object) {
        return 0;
    }
    g_kain_python_api.Py_IncRef(object);
    return kain_py_materialize_result(object, raw_mode, 1);
}

static long long kain_py_sequence_to_json_array(PyObject* object, int raw_mode) {
    Py_ssize_t len = g_kain_python_api.PySequence_Size(object);
    long long array;
    Py_ssize_t index;
    if (len < 0) {
        kain_py_clear_error();
        return 0;
    }
    array = json_array_new();
    for (index = 0; index < len; ++index) {
        PyObject* item = g_kain_python_api.PySequence_GetItem(object, index);
        if (!item) {
            kain_py_clear_error();
            return array;
        }
        json_array_push(array, kain_py_materialize_result(item, raw_mode, 1));
    }
    return array;
}

static const char* kain_py_key_utf8(PyObject* key, PyObject** owned_text) {
    const char* utf8;
    if (owned_text) {
        *owned_text = NULL;
    }
    if (!key) {
        return "";
    }
    utf8 = g_kain_python_api.PyUnicode_AsUTF8(key);
    if (utf8) {
        return utf8;
    }
    kain_py_clear_error();
    if (!owned_text) {
        return "";
    }
    *owned_text = g_kain_python_api.PyObject_Str(key);
    if (!*owned_text) {
        kain_py_clear_error();
        return "";
    }
    utf8 = g_kain_python_api.PyUnicode_AsUTF8(*owned_text);
    if (!utf8) {
        kain_py_clear_error();
        return "";
    }
    return utf8;
}

static long long kain_py_mapping_to_json_object(PyObject* object, int raw_mode) {
    PyObject* items = g_kain_python_api.PyMapping_Items(object);
    Py_ssize_t len;
    Py_ssize_t index;
    long long out;
    if (!items) {
        kain_py_clear_error();
        return 0;
    }
    len = g_kain_python_api.PyList_Size(items);
    if (len < 0) {
        kain_py_clear_error();
        len = g_kain_python_api.PySequence_Size(items);
    }
    if (len < 0) {
        kain_py_clear_error();
        g_kain_python_api.Py_DecRef(items);
        return 0;
    }
    out = json_object_new();
    for (index = 0; index < len; ++index) {
        PyObject* pair = g_kain_python_api.PyList_GetItem(items, index);
        PyObject* owned_pair = NULL;
        PyObject* key = NULL;
        PyObject* value = NULL;
        PyObject* owned_key_text = NULL;
        const char* key_text;
        if (!pair) {
            kain_py_clear_error();
            owned_pair = g_kain_python_api.PySequence_GetItem(items, index);
            pair = owned_pair;
        }
        if (!pair) {
            kain_py_clear_error();
            continue;
        }
        key = g_kain_python_api.PyTuple_GetItem(pair, 0);
        value = g_kain_python_api.PyTuple_GetItem(pair, 1);
        if (!key || !value) {
            kain_py_clear_error();
            if (owned_pair) {
                g_kain_python_api.Py_DecRef(owned_pair);
            }
            continue;
        }
        key_text = kain_py_key_utf8(key, &owned_key_text);
        json_object_set(out, key_text ? key_text : "", kain_py_materialize_borrowed(value, raw_mode));
        if (owned_key_text) {
            g_kain_python_api.Py_DecRef(owned_key_text);
        }
        if (owned_pair) {
            g_kain_python_api.Py_DecRef(owned_pair);
        }
    }
    g_kain_python_api.Py_DecRef(items);
    return out;
}

static long long kain_py_materialize_result(PyObject* object, int raw_mode, int boxed_scalars) {
    char type_name[96];
    long long tagged;
    PyObject* listed;
    if (!object) {
        kain_py_clear_error();
        return 0;
    }
    if (!kain_py_copy_type_name(object, type_name, sizeof(type_name))) {
        type_name[0] = '\0';
    }
    if (kain_py_trace_enabled()) {
        PyObject* rendered = g_kain_python_api.PyObject_Repr(object);
        const char* text = rendered ? g_kain_python_api.PyUnicode_AsUTF8(rendered) : NULL;
        fprintf(stderr, "[kain-py] materialize raw=%d type=%s value=%s\n", raw_mode, type_name[0] ? type_name : "<unknown>", text ? text : "<repr>");
        if (rendered) {
            g_kain_python_api.Py_DecRef(rendered);
        }
    }
    if (kain_py_type_name_is(type_name, "NoneType")) {
        g_kain_python_api.Py_DecRef(object);
        return boxed_scalars ? KAIN_PY_JSON_NULL : 0;
    }
    if (kain_py_type_name_is(type_name, "bool")) {
        int truth = g_kain_python_api.PyObject_IsTrue(object);
        if (truth < 0) {
            kain_py_clear_error();
            truth = 0;
        }
        g_kain_python_api.Py_DecRef(object);
        return boxed_scalars ? KAIN_PY_JSON_BOOL(truth) : (truth != 0 ? 1 : 0);
    }
    if (kain_py_type_name_is(type_name, "str") && kain_py_try_unicode_tag(object, &tagged)) {
        g_kain_python_api.Py_DecRef(object);
        return tagged;
    }
    if (raw_mode && kain_py_should_keep_raw_host(object, type_name)) {
        return kain_py_wrap_owned_object(object);
    }
    if (kain_py_type_name_is(type_name, "dict")) {
        long long out = kain_py_mapping_to_json_object(object, raw_mode);
        g_kain_python_api.Py_DecRef(object);
        return out;
    }
    if (
        kain_py_type_name_is(type_name, "list") ||
        kain_py_type_name_is(type_name, "tuple") ||
        kain_py_type_name_is(type_name, "bytes") ||
        kain_py_type_name_is(type_name, "bytearray")
    ) {
        long long out = kain_py_sequence_to_json_array(object, raw_mode);
        g_kain_python_api.Py_DecRef(object);
        return out;
    }
    if (kain_py_type_name_is(type_name, "int") && kain_py_try_long_value(object, &tagged)) {
        g_kain_python_api.Py_DecRef(object);
        return boxed_scalars ? KAIN_PY_JSON_INT(tagged) : tagged;
    }
    if (kain_py_type_name_is(type_name, "float") && kain_py_try_float_tag(object, &tagged)) {
        g_kain_python_api.Py_DecRef(object);
        return tagged;
    }
    if (!raw_mode && g_kain_python_api.PyObject_HasAttrString(object, "tolist") > 0) {
        listed = kain_py_call_method0_owned(object, "tolist");
        if (listed) {
            g_kain_python_api.Py_DecRef(object);
            return kain_py_materialize_result(listed, 0, boxed_scalars);
        }
    }
    if (!raw_mode) {
        long long out = kain_py_mapping_to_json_object(object, raw_mode);
        if (out != 0) {
            g_kain_python_api.Py_DecRef(object);
            return out;
        }
        out = kain_py_sequence_to_json_array(object, raw_mode);
        if (out != 0) {
            g_kain_python_api.Py_DecRef(object);
            return out;
        }
    }
    if (kain_py_try_unicode_tag(object, &tagged)) {
        g_kain_python_api.Py_DecRef(object);
        return tagged;
    }
    return kain_py_wrap_owned_object(object);
}

static long long kain_py_wrap_result(PyObject* object) {
    return kain_py_materialize_result(object, 1, 0);
}

static long long kain_py_wrap_materialized_result(PyObject* object) {
    return kain_py_materialize_result(object, 0, 0);
}

static long long kain_py_image_attr_value(const KainPythonImageHandle* image, const char* name) {
    if (!image || !name) {
        return 0;
    }
    if (strcmp(name, "shape") == 0) {
        return kain_py_json_array_from_values(image->shape, image->ndim);
    }
    if (strcmp(name, "ownership") == 0) {
        return kain_py_string_tag(image->ownership);
    }
    if (strcmp(name, "dtype") == 0) {
        return kain_py_string_tag(image->dtype);
    }
    if (strcmp(name, "layout") == 0) {
        return kain_py_string_tag(image->layout);
    }
    if (strcmp(name, "storage") == 0) {
        return kain_py_string_tag(image->storage);
    }
    if (strcmp(name, "source_runtime") == 0) {
        return kain_py_string_tag("python");
    }
    if (strcmp(name, "source_backend") == 0) {
        return image->source_backend[0] ? kain_py_string_tag(image->source_backend) : KAIN_PY_JSON_NULL;
    }
    if (strcmp(name, "width") == 0) {
        return KAIN_PY_JSON_INT(image->width);
    }
    if (strcmp(name, "height") == 0) {
        return KAIN_PY_JSON_INT(image->height);
    }
    if (strcmp(name, "channels") == 0) {
        return KAIN_PY_JSON_INT(image->channels);
    }
    if (strcmp(name, "batch") == 0) {
        return KAIN_PY_JSON_INT(image->batch);
    }
    if (strcmp(name, "pixel_count") == 0) {
        return KAIN_PY_JSON_INT(image->pixel_count);
    }
    if (strcmp(name, "byte_length") == 0) {
        return KAIN_PY_JSON_INT(image->byte_length);
    }
    if (strcmp(name, "row_stride") == 0) {
        return KAIN_PY_JSON_INT(image->row_stride);
    }
    if (strcmp(name, "zero_copy") == 0) {
        return KAIN_PY_JSON_BOOL(image->zero_copy);
    }
    return 0;
}

#include "python_runtime_async.c"
#include "python_runtime_buffers.c"
#include "python_runtime_gpu.c"
