#include "../../include/base.h"
#include "../../include/json.h"

#include <stddef.h>

#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif

#define KAIN_PY_FILE_INPUT 257
#define KAIN_PY_EVAL_INPUT 258

#define KAIN_RC_TYPE_PY_OBJECT  UINT64_C(0x4b50594f424a0001)
#define KAIN_RC_TYPE_PY_TENSOR  UINT64_C(0x4b505954454e0001)
#define KAIN_RC_TYPE_PY_IMAGE   UINT64_C(0x4b5059494d470001)
#define KAIN_PY_JSON_INT(value)  ((((int64_t)(value)) << 3) | 1LL)
#define KAIN_PY_JSON_BOOL(value) ((((int64_t)((value) != 0)) << 3) | 2LL)
#define KAIN_PY_JSON_NULL        4LL

typedef intptr_t Py_ssize_t;
typedef struct _object PyObject;
typedef int PyGILState_STATE;

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
    PyObject* (*PyList_New)(Py_ssize_t);
    int (*PyList_Append)(PyObject*, PyObject*);
    int (*PyList_Insert)(PyObject*, Py_ssize_t, PyObject*);
    Py_ssize_t (*PyList_Size)(PyObject*);
    PyObject* (*PyList_GetItem)(PyObject*, Py_ssize_t);
    PyObject* (*PyDict_New)(void);
    int (*PyDict_SetItemString)(PyObject*, const char*, PyObject*);
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
} KainPythonApi;

typedef struct {
    PyObject* object;
} KainPythonObjectHandle;

typedef struct {
    PyObject* object;
    long long* shape;
    long long ndim;
    char ownership[8];
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
    int active;
    PyGILState_STATE state;
} KainPythonGilScope;

static KainPythonApi g_kain_python_api;
static int g_kain_python_load_attempted = 0;

void* kain_alloc_rc(size_t size, long long type_tag);
void rc_retain(void* ptr);
void KAIN_set_destructor(void* ptr, void (*dtor)(void*));
void rc_release(void* ptr);
long long py_getattr_raw(long long target, char* name);
long long kain_tensor_from_py_shared(long long target);
long long kain_image_from_py_shared(long long target);

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
    RcHeader* header = kain_py_rc_header(ptr);
    return header != NULL &&
        header->magic == KAIN_RC_MAGIC_ALIVE &&
        header->type_tag == type_tag;
}

static int kain_py_any_is_string_tag(long long value) {
    return (value & 7LL) == 3LL;
}

static int kain_py_any_is_null_tag(long long value) {
    return (value & 7LL) == 4LL;
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
    KAIN_LOAD_PY_API(PyList_New);
    KAIN_LOAD_PY_API(PyList_Append);
    KAIN_LOAD_PY_API(PyList_Insert);
    KAIN_LOAD_PY_API(PyList_Size);
    KAIN_LOAD_PY_API(PyList_GetItem);
    KAIN_LOAD_PY_API(PyDict_New);
    KAIN_LOAD_PY_API(PyDict_SetItemString);
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
        kain_py_prepend_sys_path(parent);
        free(parent);
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
}

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
    PyObject* shape_obj;
    Py_ssize_t shape_len;
    Py_ssize_t index;
    if (!object) {
        return NULL;
    }
    tensor = (KainPythonTensorHandle*)kain_alloc_rc(sizeof(KainPythonTensorHandle), KAIN_RC_TYPE_PY_TENSOR);
    if (!tensor) {
        return NULL;
    }
    memset(tensor, 0, sizeof(*tensor));
    tensor->object = object;
    strncpy_s(tensor->ownership, sizeof(tensor->ownership), ownership ? ownership : "shared", _TRUNCATE);
    shape_obj = g_kain_python_api.PyObject_GetAttrString(object, "shape");
    if (!shape_obj) {
        kain_py_clear_error();
        shape_len = 0;
    } else {
        shape_len = g_kain_python_api.PySequence_Size(shape_obj);
        if (shape_len < 0) {
            kain_py_clear_error();
            shape_len = 0;
        }
    }
    tensor->ndim = (long long)shape_len;
    if (shape_len > 0) {
        tensor->shape = (long long*)calloc((size_t)shape_len, sizeof(long long));
        for (index = 0; index < shape_len; ++index) {
            PyObject* item = g_kain_python_api.PySequence_GetItem(shape_obj, index);
            if (item) {
                PyObject* coerced = g_kain_python_api.PyNumber_Long(item);
                if (coerced) {
                    tensor->shape[index] = g_kain_python_api.PyLong_AsLongLong(coerced);
                    g_kain_python_api.Py_DecRef(coerced);
                } else {
                    kain_py_clear_error();
                }
                g_kain_python_api.Py_DecRef(item);
            } else {
                kain_py_clear_error();
            }
        }
    }
    if (shape_obj) {
        g_kain_python_api.Py_DecRef(shape_obj);
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

static const char* kain_py_storage_from_dtype(const char* dtype) {
    if (!dtype || !dtype[0]) {
        return "unknown";
    }
    if (strcmp(dtype, "uint8") == 0 || strcmp(dtype, "ubyte") == 0) {
        return "u8";
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
    if (!layout || width < 0 || channels < 0 || item_size <= 0) {
        return 0;
    }
    if (strcmp(layout, "HW") == 0) {
        return width * item_size;
    }
    if (strcmp(layout, "HWC") == 0 || strcmp(layout, "NHWC") == 0) {
        return width * channels * item_size;
    }
    if (strcmp(layout, "CHW") == 0 || strcmp(layout, "NCHW") == 0) {
        return width * item_size;
    }
    return 0;
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
    image->pixel_count = image->batch * image->width * image->height;
    image->byte_length = byte_length > 0
        ? byte_length
        : image->pixel_count * image->channels * item_size;
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
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_OBJECT)
        ? (KainPythonObjectHandle*)(intptr_t)value
        : NULL;
}

static KainPythonTensorHandle* kain_py_as_tensor_handle(long long value) {
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_TENSOR)
        ? (KainPythonTensorHandle*)(intptr_t)value
        : NULL;
}

static KainPythonImageHandle* kain_py_as_image_handle(long long value) {
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_IMAGE)
        ? (KainPythonImageHandle*)(intptr_t)value
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

static long long kain_py_wrap_result(PyObject* object) {
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

static long long kain_py_image_attr_value(const KainPythonImageHandle* image, const char* name) {
    if (!image || !name) {
        return 0;
    }
    if (strcmp(name, "shape") == 0) {
        return (long long)(intptr_t)image->shape;
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

static long long kain_py_call_internal(long long target, long long attr, long long args, long long kwargs) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* callable;
    PyObject* attr_name = NULL;
    PyObject* positional = NULL;
    PyObject* keyword = NULL;
    PyObject* result = NULL;
    if (!scope.active) {
        return 0;
    }
    callable = kain_py_resolve_target(target);
    if (!callable) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    if (!kain_py_any_is_null_tag(attr)) {
        char* attr_text = json_any_to_string(attr);
        PyObject* attr_target = g_kain_python_api.PyObject_GetAttrString(callable, attr_text ? attr_text : "");
        g_kain_python_api.Py_DecRef(callable);
        callable = attr_target;
        if (!callable) {
            kain_py_clear_error();
            kain_py_gil_exit(&scope);
            return 0;
        }
        attr_name = NULL;
    }
    positional = kain_py_any_to_tuple(args);
    keyword = kain_py_any_to_kwargs(kwargs);
    if (!positional) {
        g_kain_python_api.Py_DecRef(callable);
        kain_py_gil_exit(&scope);
        return 0;
    }
    result = g_kain_python_api.PyObject_Call(callable, positional, keyword);
    g_kain_python_api.Py_DecRef(callable);
    g_kain_python_api.Py_DecRef(positional);
    if (keyword) {
        g_kain_python_api.Py_DecRef(keyword);
    }
    (void)attr_name;
    if (!result) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    {
        long long wrapped = kain_py_wrap_result(result);
        kain_py_gil_exit(&scope);
        return wrapped;
    }
}

static long long kain_py_import_internal(const char* module_name, const char* importer_file) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* module;
    if (!scope.active || !module_name || !module_name[0]) {
        return 0;
    }
    kain_py_prepare_import_context(importer_file);
    module = g_kain_python_api.PyImport_ImportModule(module_name);
    if (!module) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    {
        long long wrapped = kain_py_wrap_result(module);
        kain_py_gil_exit(&scope);
        return wrapped;
    }
}

static long long kain_py_import_member_internal(const char* module_name, const char* member_name, const char* importer_file) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* module;
    PyObject* member;
    char nested_name[1024];
    if (!scope.active || !module_name || !member_name) {
        return 0;
    }
    kain_py_prepare_import_context(importer_file);
    module = g_kain_python_api.PyImport_ImportModule(module_name);
    if (!module) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    member = g_kain_python_api.PyObject_GetAttrString(module, member_name);
    g_kain_python_api.Py_DecRef(module);
    if (member) {
        long long wrapped = kain_py_wrap_result(member);
        kain_py_gil_exit(&scope);
        return wrapped;
    }
    kain_py_clear_error();
    snprintf(nested_name, sizeof(nested_name), "%s.%s", module_name, member_name);
    member = g_kain_python_api.PyImport_ImportModule(nested_name);
    if (!member) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    {
        long long wrapped = kain_py_wrap_result(member);
        kain_py_gil_exit(&scope);
        return wrapped;
    }
}

static PyObject* kain_py_tensor_index_key(long long indices_any) {
    int kind = json_any_kind(indices_any);
    if (kind == KAIN_JSON_KIND_INT) {
        return g_kain_python_api.PyLong_FromLongLong(json_any_to_int(indices_any));
    }
    if (kind == KAIN_JSON_KIND_ARRAY) {
        long long len = json_array_len(indices_any);
        long long index;
        PyObject* key = g_kain_python_api.PyTuple_New((Py_ssize_t)len);
        if (!key) {
            return NULL;
        }
        for (index = 0; index < len; ++index) {
            PyObject* item = g_kain_python_api.PyLong_FromLongLong(json_any_to_int(json_array_get(indices_any, index)));
            if (!item) {
                g_kain_python_api.Py_DecRef(key);
                return NULL;
            }
            g_kain_python_api.PyTuple_SetItem(key, (Py_ssize_t)index, item);
        }
        return key;
    }
    return NULL;
}

long long to_int(long long value) {
    KainPythonObjectHandle* object_handle = kain_py_as_object_handle(value);
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(value);
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(value);
    if (object_handle || tensor_handle || image_handle) {
        KainPythonGilScope scope = kain_py_gil_enter();
        PyObject* object = object_handle
            ? object_handle->object
            : (tensor_handle ? tensor_handle->object : image_handle->object);
        PyObject* coerced;
        PyObject* rendered = NULL;
        long long result = 0;
        if (!scope.active) {
            return 0;
        }
        if (kain_py_trace_enabled()) {
            rendered = g_kain_python_api.PyObject_Str(object);
        }
        coerced = g_kain_python_api.PyNumber_Long(object);
        if (coerced) {
            result = g_kain_python_api.PyLong_AsLongLong(coerced);
            g_kain_python_api.Py_DecRef(coerced);
        } else {
            kain_py_clear_error();
        }
        if (rendered) {
            const char* utf8 = g_kain_python_api.PyUnicode_AsUTF8(rendered);
            fprintf(stderr, "[kain-py] to_int object=%s -> %lld\n", utf8 ? utf8 : "<null>", result);
            g_kain_python_api.Py_DecRef(rendered);
        }
        kain_py_gil_exit(&scope);
        return result;
    }
    return json_any_to_int(value);
}

double to_float(long long value) {
    KainPythonObjectHandle* object_handle = kain_py_as_object_handle(value);
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(value);
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(value);
    if (object_handle || tensor_handle || image_handle) {
        KainPythonGilScope scope = kain_py_gil_enter();
        PyObject* object = object_handle
            ? object_handle->object
            : (tensor_handle ? tensor_handle->object : image_handle->object);
        PyObject* coerced;
        double result = 0.0;
        if (!scope.active) {
            return 0.0;
        }
        coerced = g_kain_python_api.PyNumber_Float(object);
        if (coerced) {
            result = g_kain_python_api.PyFloat_AsDouble(coerced);
            g_kain_python_api.Py_DecRef(coerced);
        } else {
            kain_py_clear_error();
        }
        kain_py_gil_exit(&scope);
        return result;
    }
    return json_any_to_float(value);
}

char* to_string_any(long long value) {
    KainPythonObjectHandle* object_handle = kain_py_as_object_handle(value);
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(value);
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(value);
    if (object_handle || tensor_handle || image_handle) {
        KainPythonGilScope scope = kain_py_gil_enter();
        PyObject* object = object_handle
            ? object_handle->object
            : (tensor_handle ? tensor_handle->object : image_handle->object);
        PyObject* text_obj;
        const char* utf8;
        char* out;
        if (!scope.active) {
            return string_new("<python>");
        }
        text_obj = g_kain_python_api.PyObject_Str(object);
        if (!text_obj) {
            kain_py_clear_error();
            kain_py_gil_exit(&scope);
            return string_new("<python>");
        }
        utf8 = g_kain_python_api.PyUnicode_AsUTF8(text_obj);
        if (!utf8) {
            kain_py_clear_error();
            g_kain_python_api.Py_DecRef(text_obj);
            kain_py_gil_exit(&scope);
            return string_new("<python>");
        }
        out = string_new((char*)utf8);
        g_kain_python_api.Py_DecRef(text_obj);
        kain_py_gil_exit(&scope);
        return out;
    }
    return json_any_to_string(value);
}

void py_exec(char* code) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* globals;
    PyObject* result;
    if (!scope.active || !code) {
        return;
    }
    globals = kain_py_main_dict();
    if (!globals) {
        kain_py_gil_exit(&scope);
        return;
    }
    result = g_kain_python_api.PyRun_StringFlags(code, KAIN_PY_FILE_INPUT, globals, globals, NULL);
    if (result) {
        g_kain_python_api.Py_DecRef(result);
    } else {
        kain_py_clear_error();
    }
    g_kain_python_api.Py_DecRef(globals);
    kain_py_gil_exit(&scope);
}

long long py_eval(char* code) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* globals;
    PyObject* result;
    if (!scope.active || !code) {
        return 0;
    }
    globals = kain_py_main_dict();
    if (!globals) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    result = g_kain_python_api.PyRun_StringFlags(code, KAIN_PY_EVAL_INPUT, globals, globals, NULL);
    g_kain_python_api.Py_DecRef(globals);
    if (!result) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    {
        long long wrapped = kain_py_wrap_result(result);
        kain_py_gil_exit(&scope);
        return wrapped;
    }
}

long long py_eval_raw(char* code) {
    return py_eval(code);
}

long long py_import(char* module_name) {
    return kain_py_import_internal(module_name, NULL);
}

long long py_import_with_context(char* module_name, char* importer_file) {
    return kain_py_import_internal(module_name, importer_file);
}

long long py_import_from_with_context(char* module_name, char* member_name, char* importer_file) {
    return kain_py_import_member_internal(module_name, member_name, importer_file);
}

long long py_call_args(long long target, long long args, long long kwargs) {
    return kain_py_call_internal(target, 4LL, args, kwargs);
}

long long py_call_attr_args(long long target, long long attr, long long args, long long kwargs) {
    return kain_py_call_internal(target, attr, args, kwargs);
}

long long py_call_raw_args(long long target, long long args) {
    return kain_py_call_internal(target, 4LL, args, 4LL);
}

long long py_call_raw_attr(long long target, long long attr, long long args) {
    return kain_py_call_internal(target, attr, args, 4LL);
}

long long py_getattr(long long target, char* name) {
    return py_getattr_raw(target, name);
}

long long py_getattr_raw(long long target, char* name) {
    KainPythonGilScope scope = kain_py_gil_enter();
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(target);
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(target);
    PyObject* object;
    PyObject* attr;
    if (!scope.active || !name) {
        return 0;
    }
    if (tensor_handle) {
        if (strcmp(name, "shape") == 0) {
            long long raw = (long long)(intptr_t)tensor_handle->shape;
            kain_py_gil_exit(&scope);
            return raw;
        }
        if (strcmp(name, "ownership") == 0) {
            long long tagged = kain_py_string_tag(tensor_handle->ownership);
            kain_py_gil_exit(&scope);
            return tagged;
        }
        object = tensor_handle->object;
        g_kain_python_api.Py_IncRef(object);
    } else if (image_handle) {
        long long tagged = kain_py_image_attr_value(image_handle, name);
        if (tagged != 0 || strcmp(name, "source_backend") == 0) {
            kain_py_gil_exit(&scope);
            return tagged;
        }
        object = image_handle->object;
        g_kain_python_api.Py_IncRef(object);
    } else {
        object = kain_py_resolve_target(target);
    }
    if (!object) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    attr = g_kain_python_api.PyObject_GetAttrString(object, name);
    g_kain_python_api.Py_DecRef(object);
    if (!attr) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    {
        long long wrapped = kain_py_wrap_result(attr);
        kain_py_gil_exit(&scope);
        return wrapped;
    }
}

void py_setattr(long long target, char* name, long long value) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* object;
    PyObject* py_value;
    if (!scope.active || !name) {
        return;
    }
    object = kain_py_resolve_target(target);
    py_value = kain_py_any_to_pyobject(value);
    if (!object || !py_value) {
        if (object) {
            g_kain_python_api.Py_DecRef(object);
        }
        if (py_value) {
            g_kain_python_api.Py_DecRef(py_value);
        }
        kain_py_gil_exit(&scope);
        return;
    }
    g_kain_python_api.PyObject_SetAttrString(object, name, py_value);
    g_kain_python_api.Py_DecRef(py_value);
    g_kain_python_api.Py_DecRef(object);
    kain_py_clear_error();
    kain_py_gil_exit(&scope);
}

int py_hasattr(long long target, char* name) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* object;
    int result = 0;
    if (!scope.active || !name) {
        return 0;
    }
    object = kain_py_resolve_target(target);
    if (!object) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    result = g_kain_python_api.PyObject_HasAttrString(object, name);
    g_kain_python_api.Py_DecRef(object);
    kain_py_clear_error();
    kain_py_gil_exit(&scope);
    return result != 0;
}

long long kain_tensor_from_py(long long target) {
    return kain_tensor_from_py_shared(target);
}

long long kain_tensor_from_py_shared(long long target) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* object;
    KainPythonTensorHandle* tensor;
    if (!scope.active) {
        return 0;
    }
    object = kain_py_resolve_target(target);
    if (!object) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    tensor = kain_py_wrap_tensor(object, "shared");
    if (!tensor) {
        g_kain_python_api.Py_DecRef(object);
        kain_py_gil_exit(&scope);
        return 0;
    }
    KAIN_set_destructor(tensor, kain_py_tensor_destructor);
    kain_py_gil_exit(&scope);
    return (long long)(intptr_t)tensor;
}

long long kain_tensor_from_py_owned(long long target) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* object;
    PyObject* copy_method;
    PyObject* empty_args;
    PyObject* copied;
    KainPythonTensorHandle* tensor;
    if (!scope.active) {
        return 0;
    }
    object = kain_py_resolve_target(target);
    if (!object) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    copy_method = g_kain_python_api.PyObject_GetAttrString(object, "copy");
    g_kain_python_api.Py_DecRef(object);
    if (!copy_method) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    empty_args = g_kain_python_api.PyTuple_New(0);
    copied = empty_args ? g_kain_python_api.PyObject_Call(copy_method, empty_args, NULL) : NULL;
    if (empty_args) {
        g_kain_python_api.Py_DecRef(empty_args);
    }
    g_kain_python_api.Py_DecRef(copy_method);
    if (!copied) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    tensor = kain_py_wrap_tensor(copied, "owned");
    if (!tensor) {
        g_kain_python_api.Py_DecRef(copied);
        kain_py_gil_exit(&scope);
        return 0;
    }
    KAIN_set_destructor(tensor, kain_py_tensor_destructor);
    kain_py_gil_exit(&scope);
    return (long long)(intptr_t)tensor;
}

long long kain_tensor_info(long long tensor) {
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(tensor);
    if (!tensor_handle) {
        return 0;
    }
    rc_retain(tensor_handle);
    return tensor;
}

long long kain_tensor_get(long long tensor, long long indices) {
    KainPythonGilScope scope = kain_py_gil_enter();
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(tensor);
    PyObject* key;
    PyObject* value;
    if (!scope.active || !tensor_handle) {
        return 0;
    }
    key = kain_py_tensor_index_key(indices);
    if (!key) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    value = g_kain_python_api.PyObject_GetItem(tensor_handle->object, key);
    g_kain_python_api.Py_DecRef(key);
    if (!value) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    {
        long long wrapped = kain_py_wrap_result(value);
        kain_py_gil_exit(&scope);
        return wrapped;
    }
}

void kain_tensor_set(long long tensor, long long indices, long long value) {
    KainPythonGilScope scope = kain_py_gil_enter();
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(tensor);
    PyObject* key;
    PyObject* py_value;
    if (!scope.active || !tensor_handle) {
        return;
    }
    key = kain_py_tensor_index_key(indices);
    py_value = kain_py_any_to_pyobject(value);
    if (!key || !py_value) {
        if (key) {
            g_kain_python_api.Py_DecRef(key);
        }
        if (py_value) {
            g_kain_python_api.Py_DecRef(py_value);
        }
        kain_py_gil_exit(&scope);
        return;
    }
    if (g_kain_python_api.PyObject_SetItem(tensor_handle->object, key, py_value) != 0) {
        if (kain_py_trace_enabled()) {
            fprintf(stderr, "[kain-py] PyObject_SetItem failed\n");
        }
        kain_py_clear_error();
    } else if (kain_py_trace_enabled()) {
        PyObject* observed = g_kain_python_api.PyObject_GetItem(tensor_handle->object, key);
        if (observed) {
            PyObject* observed_text = g_kain_python_api.PyObject_Str(observed);
            const char* utf8 = observed_text ? g_kain_python_api.PyUnicode_AsUTF8(observed_text) : NULL;
            fprintf(stderr, "[kain-py] tensor_set ok ownership=%s value=%s\n", tensor_handle->ownership, utf8 ? utf8 : "<null>");
            if (observed_text) {
                g_kain_python_api.Py_DecRef(observed_text);
            }
            g_kain_python_api.Py_DecRef(observed);
        } else {
            fprintf(stderr, "[kain-py] tensor_set ok but readback failed\n");
            kain_py_clear_error();
        }
    }
    g_kain_python_api.Py_DecRef(py_value);
    g_kain_python_api.Py_DecRef(key);
    kain_py_gil_exit(&scope);
}

long long kain_tensor_to_py(long long tensor, char* backend) {
    KainPythonGilScope scope = kain_py_gil_enter();
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(tensor);
    KainPythonObjectHandle* wrapped;
    (void)backend;
    if (!scope.active || !tensor_handle) {
        return 0;
    }
    g_kain_python_api.Py_IncRef(tensor_handle->object);
    wrapped = kain_py_wrap_object(tensor_handle->object);
    if (!wrapped) {
        g_kain_python_api.Py_DecRef(tensor_handle->object);
        kain_py_gil_exit(&scope);
        return 0;
    }
    kain_py_finalize_wrap(wrapped, kain_py_object_destructor);
    kain_py_gil_exit(&scope);
    return (long long)(intptr_t)wrapped;
}

static PyObject* kain_py_full_slice(void) {
    PyObject* globals = kain_py_main_dict();
    PyObject* slice_obj = globals
        ? g_kain_python_api.PyRun_StringFlags("slice(None)", KAIN_PY_EVAL_INPUT, globals, globals, NULL)
        : NULL;
    if (globals) {
        g_kain_python_api.Py_DecRef(globals);
    }
    if (!slice_obj) {
        kain_py_clear_error();
    }
    return slice_obj;
}

static PyObject* kain_py_image_pixel_key(
    const KainPythonImageHandle* image,
    long long batch,
    long long x,
    long long y
) {
    PyObject* key = NULL;
    PyObject* full = NULL;
    if (!image || x < 0 || y < 0 || x >= image->width || y >= image->height) {
        return NULL;
    }
    if (batch < 0 || batch >= image->batch) {
        return NULL;
    }
    if (strcmp(image->layout, "HW") == 0 || strcmp(image->layout, "HWC") == 0) {
        key = g_kain_python_api.PyTuple_New(2);
        if (!key) {
            return NULL;
        }
        g_kain_python_api.PyTuple_SetItem(key, 0, g_kain_python_api.PyLong_FromLongLong(y));
        g_kain_python_api.PyTuple_SetItem(key, 1, g_kain_python_api.PyLong_FromLongLong(x));
        return key;
    }
    if (strcmp(image->layout, "NHWC") == 0) {
        key = g_kain_python_api.PyTuple_New(3);
        if (!key) {
            return NULL;
        }
        g_kain_python_api.PyTuple_SetItem(key, 0, g_kain_python_api.PyLong_FromLongLong(batch));
        g_kain_python_api.PyTuple_SetItem(key, 1, g_kain_python_api.PyLong_FromLongLong(y));
        g_kain_python_api.PyTuple_SetItem(key, 2, g_kain_python_api.PyLong_FromLongLong(x));
        return key;
    }
    full = kain_py_full_slice();
    if (!full) {
        return NULL;
    }
    if (strcmp(image->layout, "CHW") == 0) {
        key = g_kain_python_api.PyTuple_New(3);
        if (!key) {
            g_kain_python_api.Py_DecRef(full);
            return NULL;
        }
        g_kain_python_api.PyTuple_SetItem(key, 0, full);
        g_kain_python_api.PyTuple_SetItem(key, 1, g_kain_python_api.PyLong_FromLongLong(y));
        g_kain_python_api.PyTuple_SetItem(key, 2, g_kain_python_api.PyLong_FromLongLong(x));
        return key;
    }
    if (strcmp(image->layout, "NCHW") == 0) {
        key = g_kain_python_api.PyTuple_New(4);
        if (!key) {
            g_kain_python_api.Py_DecRef(full);
            return NULL;
        }
        g_kain_python_api.PyTuple_SetItem(key, 0, g_kain_python_api.PyLong_FromLongLong(batch));
        g_kain_python_api.PyTuple_SetItem(key, 1, full);
        g_kain_python_api.PyTuple_SetItem(key, 2, g_kain_python_api.PyLong_FromLongLong(y));
        g_kain_python_api.PyTuple_SetItem(key, 3, g_kain_python_api.PyLong_FromLongLong(x));
        return key;
    }
    g_kain_python_api.Py_DecRef(full);
    return NULL;
}

static long long kain_py_pyobject_to_any(PyObject* object) {
    PyObject* coerced;
    if (!object) {
        return 0;
    }
    coerced = g_kain_python_api.PyNumber_Long(object);
    if (coerced) {
        long long value = g_kain_python_api.PyLong_AsLongLong(coerced);
        g_kain_python_api.Py_DecRef(coerced);
        g_kain_python_api.Py_DecRef(object);
        return KAIN_PY_JSON_INT(value);
    }
    kain_py_clear_error();
    coerced = g_kain_python_api.PyNumber_Float(object);
    if (coerced) {
        double value = g_kain_python_api.PyFloat_AsDouble(coerced);
        g_kain_python_api.Py_DecRef(coerced);
        g_kain_python_api.Py_DecRef(object);
        return json_box_float(value);
    }
    kain_py_clear_error();
    {
        Py_ssize_t len = g_kain_python_api.PySequence_Size(object);
        if (len >= 0) {
            long long array = json_array_new();
            Py_ssize_t index;
            for (index = 0; index < len; ++index) {
                PyObject* item = g_kain_python_api.PySequence_GetItem(object, index);
                if (!item) {
                    kain_py_clear_error();
                    g_kain_python_api.Py_DecRef(object);
                    return array;
                }
                json_array_push(array, kain_py_pyobject_to_any(item));
            }
            g_kain_python_api.Py_DecRef(object);
            return array;
        }
    }
    kain_py_clear_error();
    return kain_py_wrap_result(object);
}

long long kain_image_from_py(long long target) {
    return kain_image_from_py_shared(target);
}

long long kain_image_from_py_shared(long long target) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* object;
    KainPythonImageHandle* image;
    if (!scope.active) {
        return 0;
    }
    object = kain_py_resolve_target(target);
    if (!object) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    image = kain_py_wrap_image(object, "shared");
    if (!image) {
        g_kain_python_api.Py_DecRef(object);
        kain_py_gil_exit(&scope);
        return 0;
    }
    KAIN_set_destructor(image, kain_py_image_destructor);
    kain_py_gil_exit(&scope);
    return (long long)(intptr_t)image;
}

long long kain_image_from_py_owned(long long target) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* object;
    PyObject* copy_method;
    PyObject* empty_args;
    PyObject* copied;
    KainPythonImageHandle* image;
    if (!scope.active) {
        return 0;
    }
    object = kain_py_resolve_target(target);
    if (!object) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    copy_method = g_kain_python_api.PyObject_GetAttrString(object, "copy");
    g_kain_python_api.Py_DecRef(object);
    if (!copy_method) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    empty_args = g_kain_python_api.PyTuple_New(0);
    copied = empty_args ? g_kain_python_api.PyObject_Call(copy_method, empty_args, NULL) : NULL;
    if (empty_args) {
        g_kain_python_api.Py_DecRef(empty_args);
    }
    g_kain_python_api.Py_DecRef(copy_method);
    if (!copied) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    image = kain_py_wrap_image(copied, "owned");
    if (!image) {
        g_kain_python_api.Py_DecRef(copied);
        kain_py_gil_exit(&scope);
        return 0;
    }
    KAIN_set_destructor(image, kain_py_image_destructor);
    kain_py_gil_exit(&scope);
    return (long long)(intptr_t)image;
}

long long kain_image_info(long long image) {
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(image);
    if (!image_handle) {
        return 0;
    }
    rc_retain(image_handle);
    return image;
}

long long kain_image_pixel(long long image, long long x, long long y) {
    KainPythonGilScope scope = kain_py_gil_enter();
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(image);
    PyObject* key;
    PyObject* value;
    if (!scope.active || !image_handle) {
        return 0;
    }
    key = kain_py_image_pixel_key(image_handle, 0, x, y);
    if (!key) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    value = g_kain_python_api.PyObject_GetItem(image_handle->object, key);
    g_kain_python_api.Py_DecRef(key);
    if (!value) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    {
        long long any = kain_py_pyobject_to_any(value);
        kain_py_gil_exit(&scope);
        return any;
    }
}

void kain_image_set_pixel(long long image, long long x, long long y, long long value) {
    KainPythonGilScope scope = kain_py_gil_enter();
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(image);
    PyObject* key;
    PyObject* py_value;
    if (!scope.active || !image_handle) {
        return;
    }
    key = kain_py_image_pixel_key(image_handle, 0, x, y);
    py_value = kain_py_any_to_pyobject(value);
    if (!key || !py_value) {
        if (key) {
            g_kain_python_api.Py_DecRef(key);
        }
        if (py_value) {
            g_kain_python_api.Py_DecRef(py_value);
        }
        kain_py_gil_exit(&scope);
        return;
    }
    if (g_kain_python_api.PyObject_SetItem(image_handle->object, key, py_value) != 0) {
        kain_py_clear_error();
    }
    g_kain_python_api.Py_DecRef(py_value);
    g_kain_python_api.Py_DecRef(key);
    kain_py_gil_exit(&scope);
}

long long kain_image_to_py(long long image, char* backend) {
    KainPythonGilScope scope = kain_py_gil_enter();
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(image);
    KainPythonObjectHandle* wrapped;
    (void)backend;
    if (!scope.active || !image_handle) {
        return 0;
    }
    g_kain_python_api.Py_IncRef(image_handle->object);
    wrapped = kain_py_wrap_object(image_handle->object);
    if (!wrapped) {
        g_kain_python_api.Py_DecRef(image_handle->object);
        kain_py_gil_exit(&scope);
        return 0;
    }
    kain_py_finalize_wrap(wrapped, kain_py_object_destructor);
    kain_py_gil_exit(&scope);
    return (long long)(intptr_t)wrapped;
}
