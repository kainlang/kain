#include "../../include/async.h"
#include "../../include/attrition.h"

int64_t abi_actor_send(int64_t actor_id, const char* message_name, const char* data_payload);
int abi_actor_state_invalid(int64_t actor_id);

static long long kain_py_call_internal_active(
    long long target,
    const char* attr_name,
    long long args,
    long long kwargs,
    int raw_result,
    KainPythonRegionHandle* region
) {
    PyObject* callable;
    PyObject* positional = NULL;
    PyObject* keyword = NULL;
    PyObject* result = NULL;
    callable = kain_py_resolve_target(target);
    if (!callable) {
        return 0;
    }
    if (attr_name && attr_name[0]) {
        PyObject* attr_target = region
            ? kain_py_region_cached_attr(region, callable, attr_name)
            : g_kain_python_api.PyObject_GetAttrString(callable, attr_name);
        g_kain_python_api.Py_DecRef(callable);
        callable = attr_target;
        if (!callable) {
            kain_py_clear_error();
            return 0;
        }
    }
    positional = kain_py_any_to_tuple(args);
    keyword = kain_py_any_to_kwargs(kwargs);
    if (!positional) {
        g_kain_python_api.Py_DecRef(callable);
        return 0;
    }
    result = g_kain_python_api.PyObject_Call(callable, positional, keyword);
    g_kain_python_api.Py_DecRef(callable);
    g_kain_python_api.Py_DecRef(positional);
    if (keyword) {
        g_kain_python_api.Py_DecRef(keyword);
    }
    if (!result) {
        kain_py_clear_error();
        return 0;
    }
    return raw_result ? kain_py_wrap_result(result) : kain_py_wrap_materialized_result(result);
}

static long long kain_py_call_internal(long long target, long long attr, long long args, long long kwargs, int raw_result) {
    KainPythonGilScope scope = kain_py_gil_enter();
    long long wrapped = 0;
    char* attr_text = NULL;
    if (!scope.active) {
        return 0;
    }
    if (!kain_py_any_is_null_tag(attr)) {
        attr_text = json_any_to_string(attr);
    }
    wrapped = kain_py_call_internal_active(
        target,
        attr_text ? attr_text : NULL,
        args,
        kwargs,
        raw_result,
        NULL
    );
    kain_py_gil_exit(&scope);
    return wrapped;
}

static long long kain_py_import_internal_active(
    const char* module_name,
    const char* importer_file,
    KainPythonRegionHandle* region
) {
    PyObject* module;
    if (!module_name || !module_name[0]) {
        return 0;
    }
    module = region
        ? kain_py_region_cached_import(region, module_name, importer_file)
        : NULL;
    if (!module) {
        kain_py_prepare_import_context(importer_file);
        module = g_kain_python_api.PyImport_ImportModule(module_name);
    }
    if (!module) {
        kain_py_clear_error();
        return 0;
    }
    return kain_py_wrap_result(module);
}

static long long kain_py_import_internal(const char* module_name, const char* importer_file) {
    KainPythonGilScope scope = kain_py_gil_enter();
    long long wrapped = 0;
    if (!scope.active || !module_name || !module_name[0]) {
        return 0;
    }
    wrapped = kain_py_import_internal_active(module_name, importer_file, NULL);
    kain_py_gil_exit(&scope);
    return wrapped;
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
    return kain_py_call_internal(target, 4LL, args, kwargs, 0);
}

long long py_call_attr_args(long long target, long long attr, long long args, long long kwargs) {
    return kain_py_call_internal(target, attr, args, kwargs, 0);
}

long long py_call_raw_args(long long target, long long args) {
    return kain_py_call_internal(target, 4LL, args, 4LL, 1);
}

long long py_call_raw_attr(long long target, long long attr, long long args) {
    return kain_py_call_internal(target, attr, args, 4LL, 1);
}

long long py_call_raw_f64_trunc_i64(long long target, double arg) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* callable;
    PyObject* positional = NULL;
    PyObject* arg_obj = NULL;
    PyObject* result = NULL;
    PyObject* coerced = NULL;
    long long value = 0;
    if (!scope.active) {
        return 0;
    }
    callable = kain_py_resolve_target(target);
    if (!callable) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    positional = g_kain_python_api.PyTuple_New(1);
    if (!positional) {
        g_kain_python_api.Py_DecRef(callable);
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    arg_obj = g_kain_python_api.PyFloat_FromDouble(arg);
    if (!arg_obj) {
        g_kain_python_api.Py_DecRef(positional);
        g_kain_python_api.Py_DecRef(callable);
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    if (g_kain_python_api.PyTuple_SetItem(positional, 0, arg_obj) != 0) {
        g_kain_python_api.Py_DecRef(arg_obj);
        g_kain_python_api.Py_DecRef(positional);
        g_kain_python_api.Py_DecRef(callable);
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    result = g_kain_python_api.PyObject_Call(callable, positional, NULL);
    g_kain_python_api.Py_DecRef(positional);
    g_kain_python_api.Py_DecRef(callable);
    if (!result) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    coerced = g_kain_python_api.PyNumber_Long(result);
    g_kain_python_api.Py_DecRef(result);
    if (!coerced) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    value = g_kain_python_api.PyLong_AsLongLong(coerced);
    g_kain_python_api.Py_DecRef(coerced);
    kain_py_gil_exit(&scope);
    return value;
}

static long long kain_py_getattr_internal_active(
    long long target,
    const char* name,
    KainPythonRegionHandle* region
) {
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(target);
    KainPythonImageHandle* image_handle = kain_py_as_image_handle(target);
    PyObject* object;
    PyObject* attr;
    if (!name) {
        return 0;
    }
    if (tensor_handle) {
        if (kain_py_tensor_has_virtual_attr(name)) {
            return kain_py_tensor_attr_value(tensor_handle, name);
        }
        object = tensor_handle->object;
        g_kain_python_api.Py_IncRef(object);
    } else if (image_handle) {
        long long tagged = kain_py_image_attr_value(image_handle, name);
        if (tagged != 0 || strcmp(name, "source_backend") == 0) {
            return tagged;
        }
        object = image_handle->object;
        g_kain_python_api.Py_IncRef(object);
    } else {
        object = kain_py_resolve_target(target);
    }
    if (!object) {
        return 0;
    }
    attr = region
        ? kain_py_region_cached_attr(region, object, name)
        : g_kain_python_api.PyObject_GetAttrString(object, name);
    g_kain_python_api.Py_DecRef(object);
    if (!attr) {
        kain_py_clear_error();
        return 0;
    }
    return kain_py_wrap_result(attr);
}

long long py_getattr(long long target, char* name) {
    return py_getattr_raw(target, name);
}

long long py_getattr_raw(long long target, char* name) {
    KainPythonGilScope scope = kain_py_gil_enter();
    long long wrapped = 0;
    if (!scope.active || !name) {
        return 0;
    }
    wrapped = kain_py_getattr_internal_active(target, name, NULL);
    kain_py_gil_exit(&scope);
    return wrapped;
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

typedef enum {
    KAIN_PY_ASYNC_SETTLEMENT_PENDING = 0,
    KAIN_PY_ASYNC_SETTLEMENT_OK = 1,
    KAIN_PY_ASYNC_SETTLEMENT_ERROR = 2,
    KAIN_PY_ASYNC_SETTLEMENT_CANCELLED = 3,
} KainPythonAsyncSettlementKind;

#ifdef _WIN32
typedef CRITICAL_SECTION KainPythonAsyncMutex;
#else
typedef pthread_mutex_t KainPythonAsyncMutex;
#endif

struct KainPythonAsyncFutureHandle {
    KainTaskId task_id;
    atomic_int close_requested;
    atomic_int cancel_requested;
    atomic_int worker_started;
    atomic_int worker_done;
    atomic_int settlement_once;
    atomic_int settlement_kind;
    long long settled_value;
    char* settled_message;
    PyObject* awaitable;
    PyObject* event_loop;
    PyObject* scheduled_task;
    KainPythonAsyncMutex lock;
    int thread_started;
#ifdef _WIN32
    HANDLE thread_handle;
#else
    pthread_t thread_handle;
#endif
};

struct KainPythonActorCallbackHandle {
    int64_t actor_id;
    uint64_t callback_id;
    char* message_name;
    char* queue_name;
    char* callable_name;
    KainPythonAsyncMutex lock;
    atomic_int close_requested;
    atomic_int dispatcher_done;
    atomic_llong delivered_count;
    int thread_started;
#ifdef _WIN32
    HANDLE thread_handle;
#else
    pthread_t thread_handle;
#endif
};

static atomic_uint_least64_t g_kain_py_actor_callback_ids = 1u;

static void kain_py_async_mutex_init(KainPythonAsyncMutex* mutex) {
#ifdef _WIN32
    InitializeCriticalSection(mutex);
#else
    pthread_mutex_init(mutex, NULL);
#endif
}

static void kain_py_async_mutex_destroy(KainPythonAsyncMutex* mutex) {
#ifdef _WIN32
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

static void kain_py_async_mutex_lock(KainPythonAsyncMutex* mutex) {
#ifdef _WIN32
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

static void kain_py_async_mutex_unlock(KainPythonAsyncMutex* mutex) {
#ifdef _WIN32
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

static void kain_py_async_sleep_millis(unsigned long long delay_ms) {
#ifdef _WIN32
    Sleep((DWORD)delay_ms);
#else
    struct timespec delay;
    delay.tv_sec = (time_t)(delay_ms / 1000u);
    delay.tv_nsec = (long)((delay_ms % 1000u) * 1000000u);
    nanosleep(&delay, NULL);
#endif
}

static PyObject* kain_py_none_object_owned(void) {
    PyObject* globals;
    PyObject* none_obj;
    globals = kain_py_main_dict();
    if (!globals) {
        return NULL;
    }
    none_obj = g_kain_python_api.PyRun_StringFlags("None", KAIN_PY_EVAL_INPUT, globals, globals, NULL);
    g_kain_python_api.Py_DecRef(globals);
    return none_obj;
}

static int kain_py_set_main_global_owned(const char* name, PyObject* value) {
    PyObject* globals;
    int status;
    if (!name || !name[0] || !value) {
        return -1;
    }
    globals = kain_py_main_dict();
    if (!globals) {
        return -1;
    }
    status = g_kain_python_api.PyDict_SetItemString(globals, name, value);
    g_kain_python_api.Py_DecRef(globals);
    if (status != 0) {
        kain_py_clear_error();
        return -1;
    }
    return 0;
}

static char* kain_py_render_pyobject_text(PyObject* object) {
    PyObject* text_obj;
    const char* utf8;
    char* out;
    if (!object) {
        return string_new("python async error");
    }
    text_obj = g_kain_python_api.PyObject_Str(object);
    if (!text_obj) {
        kain_py_clear_error();
        return string_new("python async error");
    }
    utf8 = g_kain_python_api.PyUnicode_AsUTF8(text_obj);
    if (!utf8) {
        kain_py_clear_error();
        g_kain_python_api.Py_DecRef(text_obj);
        return string_new("python async error");
    }
    out = string_new((char*)utf8);
    g_kain_python_api.Py_DecRef(text_obj);
    return out ? out : string_new("python async error");
}

static int kain_py_truthy_object(PyObject* object, int fallback) {
    int result;
    if (!object) {
        return fallback;
    }
    result = g_kain_python_api.PyObject_IsTrue(object);
    if (result < 0) {
        kain_py_clear_error();
        return fallback;
    }
    return result != 0;
}

static KainPythonAsyncFutureHandle* kain_py_as_async_future_handle(long long value) {
    value = kain_py_unbox_tagged_handle(value, KAIN_RC_TYPE_PY_ASYNC_FUTURE);
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_ASYNC_FUTURE)
        ? (KainPythonAsyncFutureHandle*)(intptr_t)value
        : NULL;
}

static KainPythonActorCallbackHandle* kain_py_as_actor_callback_handle(long long value) {
    value = kain_py_unbox_tagged_handle(value, KAIN_RC_TYPE_PY_ACTOR_CALLBACK);
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_ACTOR_CALLBACK)
        ? (KainPythonActorCallbackHandle*)(intptr_t)value
        : NULL;
}

static int kain_py_async_future_effective_state(const KainPythonAsyncFutureHandle* future) {
    int state;
    int settlement_kind;
    if (!future) {
        return -1;
    }
    state = (int)kain_task_get_state(future->task_id);
    settlement_kind = atomic_load_explicit(&future->settlement_kind, memory_order_acquire);
    if (state == KAIN_TASK_STATE_COMPLETED ||
        state == KAIN_TASK_STATE_CANCELLED ||
        state == KAIN_TASK_STATE_FAILED) {
        return state;
    }
    if (settlement_kind == KAIN_PY_ASYNC_SETTLEMENT_OK) {
        return KAIN_TASK_STATE_COMPLETED;
    }
    if (settlement_kind == KAIN_PY_ASYNC_SETTLEMENT_CANCELLED) {
        return KAIN_TASK_STATE_CANCELLED;
    }
    if (settlement_kind == KAIN_PY_ASYNC_SETTLEMENT_ERROR) {
        return KAIN_TASK_STATE_FAILED;
    }
    return state;
}

static void kain_py_async_future_release_python_refs(KainPythonAsyncFutureHandle* future) {
    PyObject* awaitable = NULL;
    PyObject* event_loop = NULL;
    PyObject* scheduled_task = NULL;
    KainPythonGilScope scope;
    if (!future) {
        return;
    }
    kain_py_async_mutex_lock(&future->lock);
    awaitable = future->awaitable;
    future->awaitable = NULL;
    event_loop = future->event_loop;
    future->event_loop = NULL;
    scheduled_task = future->scheduled_task;
    future->scheduled_task = NULL;
    kain_py_async_mutex_unlock(&future->lock);
    if (!awaitable && !event_loop && !scheduled_task) {
        return;
    }
    scope = kain_py_gil_enter();
    if (scope.active) {
        if (awaitable) {
            g_kain_python_api.Py_DecRef(awaitable);
        }
        if (event_loop) {
            g_kain_python_api.Py_DecRef(event_loop);
        }
        if (scheduled_task) {
            g_kain_python_api.Py_DecRef(scheduled_task);
        }
        kain_py_gil_exit(&scope);
        return;
    }
    if (awaitable) {
        future->awaitable = awaitable;
    }
    if (event_loop) {
        future->event_loop = event_loop;
    }
    if (scheduled_task) {
        future->scheduled_task = scheduled_task;
    }
}

static int kain_py_async_future_settle(
    KainPythonAsyncFutureHandle* future,
    KainPythonAsyncSettlementKind settlement_kind,
    long long settled_value,
    char* settled_message
) {
    int expected = 0;
    if (!future) {
        if (settled_value != 0) {
            kain_py_any_release(settled_value);
        }
        if (settled_message) {
            rc_release(settled_message);
        }
        return 0;
    }
    if (!atomic_compare_exchange_strong_explicit(
            &future->settlement_once,
            &expected,
            1,
            memory_order_acq_rel,
            memory_order_acquire)) {
        if (settled_value != 0) {
            kain_py_any_release(settled_value);
        }
        if (settled_message) {
            rc_release(settled_message);
        }
        return 0;
    }
    kain_py_async_mutex_lock(&future->lock);
    future->settled_value = settled_value;
    future->settled_message = settled_message;
    atomic_store_explicit(&future->settlement_kind, settlement_kind, memory_order_release);
    kain_py_async_mutex_unlock(&future->lock);
    return 1;
}

static int kain_py_async_future_schedule_cancel_locked(KainPythonAsyncFutureHandle* future) {
    PyObject* event_loop = NULL;
    PyObject* scheduled_task = NULL;
    PyObject* cancel_callable = NULL;
    PyObject* scheduled = NULL;
    int status = -1;
    if (!future) {
        return -1;
    }
    if (future->event_loop == NULL || future->scheduled_task == NULL) {
        return -1;
    }
    event_loop = future->event_loop;
    scheduled_task = future->scheduled_task;
    g_kain_python_api.Py_IncRef(event_loop);
    g_kain_python_api.Py_IncRef(scheduled_task);
    cancel_callable = g_kain_python_api.PyObject_GetAttrString(scheduled_task, "cancel");
    if (cancel_callable != NULL) {
        scheduled = kain_py_call_method1_owned(event_loop, "call_soon_threadsafe", cancel_callable);
        if (scheduled != NULL) {
            g_kain_python_api.Py_DecRef(scheduled);
            status = 0;
        } else {
            kain_py_clear_error();
        }
    } else {
        kain_py_clear_error();
    }
    g_kain_python_api.Py_DecRef(scheduled_task);
    g_kain_python_api.Py_DecRef(event_loop);
    return status;
}

static KainPollResult kain_py_async_future_task_fn(
    KainFutureContext* context,
    void* user_data,
    void** result
) {
    KainPythonAsyncFutureHandle* future = (KainPythonAsyncFutureHandle*)user_data;
    int settlement_kind;
    (void)context;
    if (result) {
        *result = NULL;
    }
    if (!future) {
        return KAIN_POLL_ERROR;
    }
    settlement_kind = atomic_load_explicit(&future->settlement_kind, memory_order_acquire);
    if (settlement_kind == KAIN_PY_ASYNC_SETTLEMENT_PENDING) {
        return KAIN_POLL_PENDING;
    }
    if (settlement_kind == KAIN_PY_ASYNC_SETTLEMENT_OK) {
        return KAIN_POLL_READY;
    }
    return KAIN_POLL_ERROR;
}

static long long kain_py_async_future_envelope(const char* status_text, long long value, const char* message, int state) {
    long long envelope = json_object_new();
    long long status_value = kain_py_string_tag(status_text ? status_text : "error");
    json_object_set(envelope, "status", status_value);
    kain_py_any_release(status_value);
    json_object_set(envelope, "state", KAIN_PY_JSON_INT(state));
    if (value != 0) {
        json_object_set(envelope, "value", value);
    }
    if (message != NULL && message[0] != '\0') {
        long long message_value = kain_py_string_tag(message);
        json_object_set(envelope, "message", message_value);
        kain_py_any_release(message_value);
    }
    return envelope;
}

static long long kain_py_async_future_build_result(KainPythonAsyncFutureHandle* future) {
    int settlement_kind;
    int state;
    long long settled_value = 0;
    const char* settled_message = NULL;
    if (!future) {
        return kain_py_async_future_envelope(
            "error",
            0,
            "python async future handle was invalid",
            -1
        );
    }
    kain_py_async_mutex_lock(&future->lock);
    settlement_kind = atomic_load_explicit(&future->settlement_kind, memory_order_acquire);
    settled_value = future->settled_value;
    settled_message = future->settled_message;
    state = kain_py_async_future_effective_state(future);
    kain_py_async_mutex_unlock(&future->lock);
    if (settlement_kind == KAIN_PY_ASYNC_SETTLEMENT_OK) {
        return kain_py_async_future_envelope("ok", settled_value, NULL, state);
    }
    if (settlement_kind == KAIN_PY_ASYNC_SETTLEMENT_CANCELLED) {
        return kain_py_async_future_envelope(
            "cancelled",
            0,
            settled_message ? settled_message : "python async future cancelled",
            state
        );
    }
    if (settlement_kind == KAIN_PY_ASYNC_SETTLEMENT_ERROR) {
        return kain_py_async_future_envelope(
            "error",
            0,
            settled_message ? settled_message : "python async future failed",
            state
        );
    }
    return kain_py_async_future_envelope(
        "pending",
        0,
        "python async future still pending",
        state
    );
}

static int kain_py_async_future_cancel_internal(KainPythonAsyncFutureHandle* future) {
    KainPythonGilScope scope;
    if (!future) {
        return -1;
    }
    atomic_store_explicit(&future->cancel_requested, 1, memory_order_release);
    scope = kain_py_gil_enter();
    if (scope.active) {
        kain_py_async_mutex_lock(&future->lock);
        (void)kain_py_async_future_schedule_cancel_locked(future);
        kain_py_async_mutex_unlock(&future->lock);
        kain_py_gil_exit(&scope);
    }
    if (kain_py_async_future_settle(
            future,
            KAIN_PY_ASYNC_SETTLEMENT_CANCELLED,
            0,
            string_new("python async future cancelled")) != 0) {
        KainDiagnostic diag;
        kain_diagnostic_init(&diag);
        (void)kain_task_cancel(future->task_id, &diag);
        (void)kain_task_poll(future->task_id, NULL, &diag);
        return 0;
    }
    return 0;
}

static int kain_py_async_future_shutdown(KainPythonAsyncFutureHandle* future) {
    int dispose_status = 0;
    unsigned int spin_count = 0u;
    if (!future) {
        return -1;
    }
    if (atomic_exchange_explicit(&future->close_requested, 1, memory_order_acq_rel) != 0) {
        return 0;
    }
    (void)kain_py_async_future_cancel_internal(future);
    if (future->thread_started) {
#ifdef _WIN32
        if (future->thread_handle != NULL) {
            WaitForSingleObject(future->thread_handle, INFINITE);
            CloseHandle(future->thread_handle);
            future->thread_handle = NULL;
        }
#else
        pthread_join(future->thread_handle, NULL);
#endif
        future->thread_started = 0;
    }
    kain_py_async_future_release_python_refs(future);
    if (future->task_id != KAIN_TASK_ID_INVALID) {
        while ((dispose_status = kain_attrition_async_dispose_task(future->task_id)) == -2 && spin_count < 2048u) {
            KainDiagnostic diag;
            kain_diagnostic_init(&diag);
            (void)kain_task_cancel(future->task_id, &diag);
            (void)kain_task_poll(future->task_id, NULL, &diag);
            kain_py_async_sleep_millis(1u);
            spin_count += 1u;
        }
        if (dispose_status == 0 || dispose_status == -1) {
            future->task_id = KAIN_TASK_ID_INVALID;
        }
    }
    kain_py_async_mutex_lock(&future->lock);
    if (future->settled_value != 0) {
        kain_py_any_release(future->settled_value);
        future->settled_value = 0;
    }
    if (future->settled_message != NULL) {
        rc_release(future->settled_message);
        future->settled_message = NULL;
    }
    atomic_store_explicit(&future->settlement_kind, KAIN_PY_ASYNC_SETTLEMENT_CANCELLED, memory_order_release);
    kain_py_async_mutex_unlock(&future->lock);
    return 0;
}

static void kain_py_async_future_destructor(void* ptr) {
    KainPythonAsyncFutureHandle* future = (KainPythonAsyncFutureHandle*)ptr;
    if (!future) {
        return;
    }
    (void)kain_py_async_future_shutdown(future);
    kain_py_async_mutex_destroy(&future->lock);
}

#ifdef _WIN32
static DWORD WINAPI kain_py_async_future_thread_proc(LPVOID param)
#else
static void* kain_py_async_future_thread_proc(void* param)
#endif
{
    KainPythonAsyncFutureHandle* future = (KainPythonAsyncFutureHandle*)param;
    KainPythonGilScope scope;
    PyObject* asyncio_module = NULL;
    PyObject* local_awaitable = NULL;
    PyObject* event_loop = NULL;
    PyObject* scheduled_task = NULL;
    PyObject* ignored = NULL;
    PyObject* result = NULL;
    long long settled_value = 0;
    char* settled_message = NULL;
    int cancelled = 0;
    if (!future) {
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }
    atomic_store_explicit(&future->worker_started, 1, memory_order_release);
    scope = kain_py_gil_enter();
    if (!scope.active) {
        (void)kain_py_async_future_settle(
            future,
            KAIN_PY_ASYNC_SETTLEMENT_ERROR,
            0,
            string_new("python async worker could not acquire the gil")
        );
        goto finish_without_gil;
    }
    kain_py_async_mutex_lock(&future->lock);
    if (future->awaitable != NULL) {
        local_awaitable = future->awaitable;
        g_kain_python_api.Py_IncRef(local_awaitable);
    }
    kain_py_async_mutex_unlock(&future->lock);
    if (local_awaitable == NULL) {
        (void)kain_py_async_future_settle(
            future,
            KAIN_PY_ASYNC_SETTLEMENT_ERROR,
            0,
            string_new("python async future was missing an awaitable")
        );
        kain_py_gil_exit(&scope);
        goto finish_without_gil;
    }
    asyncio_module = g_kain_python_api.PyImport_ImportModule("asyncio");
    if (asyncio_module == NULL) {
        kain_py_clear_error();
        (void)kain_py_async_future_settle(
            future,
            KAIN_PY_ASYNC_SETTLEMENT_ERROR,
            0,
            string_new("python asyncio import failed")
        );
        g_kain_python_api.Py_DecRef(local_awaitable);
        kain_py_gil_exit(&scope);
        goto finish_without_gil;
    }
    event_loop = kain_py_call_method0_owned(asyncio_module, "new_event_loop");
    if (event_loop == NULL) {
        kain_py_clear_error();
        (void)kain_py_async_future_settle(
            future,
            KAIN_PY_ASYNC_SETTLEMENT_ERROR,
            0,
            string_new("python async worker could not create an event loop")
        );
        g_kain_python_api.Py_DecRef(asyncio_module);
        g_kain_python_api.Py_DecRef(local_awaitable);
        kain_py_gil_exit(&scope);
        goto finish_without_gil;
    }
    g_kain_python_api.Py_IncRef(event_loop);
    ignored = kain_py_call_method1_owned(asyncio_module, "set_event_loop", event_loop);
    if (ignored == NULL) {
        kain_py_clear_error();
        (void)kain_py_async_future_settle(
            future,
            KAIN_PY_ASYNC_SETTLEMENT_ERROR,
            0,
            string_new("python async worker could not bind the event loop")
        );
        g_kain_python_api.Py_DecRef(event_loop);
        g_kain_python_api.Py_DecRef(asyncio_module);
        g_kain_python_api.Py_DecRef(local_awaitable);
        kain_py_gil_exit(&scope);
        goto finish_without_gil;
    }
    g_kain_python_api.Py_DecRef(ignored);
    ignored = NULL;
    g_kain_python_api.Py_IncRef(local_awaitable);
    scheduled_task = kain_py_call_method1_owned(asyncio_module, "ensure_future", local_awaitable);
    if (scheduled_task == NULL) {
        kain_py_clear_error();
        (void)kain_py_async_future_settle(
            future,
            KAIN_PY_ASYNC_SETTLEMENT_ERROR,
            0,
            string_new("python awaitable could not be scheduled")
        );
        g_kain_python_api.Py_DecRef(event_loop);
        g_kain_python_api.Py_DecRef(asyncio_module);
        g_kain_python_api.Py_DecRef(local_awaitable);
        kain_py_gil_exit(&scope);
        goto finish_without_gil;
    }
    kain_py_async_mutex_lock(&future->lock);
    if (future->event_loop == NULL) {
        g_kain_python_api.Py_IncRef(event_loop);
        future->event_loop = event_loop;
    }
    if (future->scheduled_task == NULL) {
        g_kain_python_api.Py_IncRef(scheduled_task);
        future->scheduled_task = scheduled_task;
    }
    if (atomic_load_explicit(&future->cancel_requested, memory_order_acquire) != 0) {
        (void)kain_py_async_future_schedule_cancel_locked(future);
    }
    kain_py_async_mutex_unlock(&future->lock);
    g_kain_python_api.Py_IncRef(scheduled_task);
    result = kain_py_call_method1_owned(event_loop, "run_until_complete", scheduled_task);
    if (result != NULL) {
        settled_value = kain_py_materialize_result(result, 0, 1);
        if (settled_value == 0) {
            settled_message = string_new("python async result materialization failed");
            (void)kain_py_async_future_settle(
                future,
                KAIN_PY_ASYNC_SETTLEMENT_ERROR,
                0,
                settled_message
            );
        } else {
            (void)kain_py_async_future_settle(
                future,
                KAIN_PY_ASYNC_SETTLEMENT_OK,
                settled_value,
                NULL
            );
            settled_value = 0;
        }
    } else {
        PyObject* cancelled_obj;
        PyObject* exception_obj;
        kain_py_clear_error();
        cancelled_obj = kain_py_call_method0_owned(scheduled_task, "cancelled");
        cancelled = kain_py_truthy_object(cancelled_obj, 0);
        if (cancelled_obj != NULL) {
            g_kain_python_api.Py_DecRef(cancelled_obj);
        }
        if (cancelled) {
            (void)kain_py_async_future_settle(
                future,
                KAIN_PY_ASYNC_SETTLEMENT_CANCELLED,
                0,
                string_new("python async future cancelled")
            );
        } else {
            exception_obj = kain_py_call_method0_owned(scheduled_task, "exception");
            if (exception_obj != NULL) {
                settled_message = kain_py_render_pyobject_text(exception_obj);
                g_kain_python_api.Py_DecRef(exception_obj);
            } else {
                kain_py_clear_error();
                settled_message = string_new("python async future failed");
            }
            (void)kain_py_async_future_settle(
                future,
                KAIN_PY_ASYNC_SETTLEMENT_ERROR,
                0,
                settled_message
            );
        }
    }
    if (scheduled_task != NULL) {
        kain_py_async_mutex_lock(&future->lock);
        if (future->scheduled_task == scheduled_task) {
            g_kain_python_api.Py_DecRef(future->scheduled_task);
            future->scheduled_task = NULL;
        }
        kain_py_async_mutex_unlock(&future->lock);
    }
    if (event_loop != NULL) {
        PyObject* close_result;
        PyObject* none_obj = kain_py_none_object_owned();
        if (none_obj != NULL) {
            ignored = kain_py_call_method1_owned(asyncio_module, "set_event_loop", none_obj);
            if (ignored != NULL) {
                g_kain_python_api.Py_DecRef(ignored);
            } else {
                kain_py_clear_error();
            }
        }
        close_result = kain_py_call_method0_owned(event_loop, "close");
        if (close_result != NULL) {
            g_kain_python_api.Py_DecRef(close_result);
        } else {
            kain_py_clear_error();
        }
        kain_py_async_mutex_lock(&future->lock);
        if (future->event_loop == event_loop) {
            g_kain_python_api.Py_DecRef(future->event_loop);
            future->event_loop = NULL;
        }
        kain_py_async_mutex_unlock(&future->lock);
    }
    kain_py_async_mutex_lock(&future->lock);
    if (future->awaitable == local_awaitable) {
        g_kain_python_api.Py_DecRef(future->awaitable);
        future->awaitable = NULL;
    }
    kain_py_async_mutex_unlock(&future->lock);
    if (scheduled_task != NULL) {
        g_kain_python_api.Py_DecRef(scheduled_task);
    }
    if (event_loop != NULL) {
        g_kain_python_api.Py_DecRef(event_loop);
    }
    if (asyncio_module != NULL) {
        g_kain_python_api.Py_DecRef(asyncio_module);
    }
    if (local_awaitable != NULL) {
        g_kain_python_api.Py_DecRef(local_awaitable);
    }
    kain_py_gil_exit(&scope);
finish_without_gil:
    atomic_store_explicit(&future->worker_done, 1, memory_order_release);
    if (future->task_id != KAIN_TASK_ID_INVALID) {
        KainDiagnostic diag;
        kain_diagnostic_init(&diag);
        if (atomic_load_explicit(&future->settlement_kind, memory_order_acquire) == KAIN_PY_ASYNC_SETTLEMENT_CANCELLED) {
            (void)kain_task_cancel(future->task_id, &diag);
        }
        (void)kain_task_poll(future->task_id, NULL, &diag);
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static KainPythonAsyncFutureHandle* kain_py_async_future_create_from_owned_awaitable(PyObject* awaitable) {
    KainPythonAsyncFutureHandle* future;
    KainTaskSpawnConfig config;
    KainDiagnostic diag;
    if (!awaitable) {
        return NULL;
    }
    future = (KainPythonAsyncFutureHandle*)kain_alloc_rc(
        sizeof(KainPythonAsyncFutureHandle),
        KAIN_RC_TYPE_PY_ASYNC_FUTURE
    );
    if (!future) {
        g_kain_python_api.Py_DecRef(awaitable);
        return NULL;
    }
    memset(future, 0, sizeof(*future));
    future->awaitable = awaitable;
    kain_py_async_mutex_init(&future->lock);
    atomic_init(&future->close_requested, 0);
    atomic_init(&future->cancel_requested, 0);
    atomic_init(&future->worker_started, 0);
    atomic_init(&future->worker_done, 0);
    atomic_init(&future->settlement_once, 0);
    atomic_init(&future->settlement_kind, KAIN_PY_ASYNC_SETTLEMENT_PENDING);
    KAIN_set_destructor(future, kain_py_async_future_destructor);
    kain_task_spawn_config_init(&config);
    config.task_fn = kain_py_async_future_task_fn;
    config.user_data = future;
    kain_diagnostic_init(&diag);
    future->task_id = kain_task_spawn(&config, &diag);
    if (future->task_id == KAIN_TASK_ID_INVALID) {
        (void)kain_py_async_future_settle(
            future,
            KAIN_PY_ASYNC_SETTLEMENT_ERROR,
            0,
            string_new("python async future task spawn failed")
        );
        return future;
    }
#ifdef _WIN32
    future->thread_handle = CreateThread(NULL, 0, kain_py_async_future_thread_proc, future, 0, NULL);
    future->thread_started = future->thread_handle != NULL;
#else
    future->thread_started = pthread_create(&future->thread_handle, NULL, kain_py_async_future_thread_proc, future) == 0;
#endif
    if (!future->thread_started) {
        (void)kain_py_async_future_settle(
            future,
            KAIN_PY_ASYNC_SETTLEMENT_ERROR,
            0,
            string_new("python async worker thread failed to start")
        );
        (void)kain_task_poll(future->task_id, NULL, &diag);
    } else {
        (void)kain_task_poll(future->task_id, NULL, &diag);
    }
    return future;
}

static long long kain_py_async_call_from_target(long long target, const char* attr_name, long long args) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* callable;
    PyObject* positional = NULL;
    PyObject* result = NULL;
    KainPythonAsyncFutureHandle* future = NULL;
    if (!scope.active) {
        return 0;
    }
    callable = kain_py_resolve_target(target);
    if (!callable) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    if (attr_name && attr_name[0]) {
        PyObject* attr_target = g_kain_python_api.PyObject_GetAttrString(callable, attr_name);
        g_kain_python_api.Py_DecRef(callable);
        callable = attr_target;
        if (!callable) {
            kain_py_clear_error();
            kain_py_gil_exit(&scope);
            return 0;
        }
    }
    positional = kain_py_any_to_tuple(args);
    if (!positional) {
        g_kain_python_api.Py_DecRef(callable);
        kain_py_gil_exit(&scope);
        return 0;
    }
    result = g_kain_python_api.PyObject_Call(callable, positional, NULL);
    g_kain_python_api.Py_DecRef(positional);
    g_kain_python_api.Py_DecRef(callable);
    if (!result) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        return 0;
    }
    future = kain_py_async_future_create_from_owned_awaitable(result);
    kain_py_gil_exit(&scope);
    return future ? (long long)(intptr_t)future : 0;
}

long long py_call_async_args(long long target, long long args) {
    return kain_py_async_call_from_target(target, NULL, args);
}

long long py_call_async_attr(long long target, char* attr, long long args) {
    return kain_py_async_call_from_target(target, attr, args);
}

long long py_awaitable_future(long long awaitable) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* awaitable_object;
    KainPythonAsyncFutureHandle* future;
    if (!scope.active) {
        return 0;
    }
    awaitable_object = kain_py_resolve_target(awaitable);
    if (!awaitable_object) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    future = kain_py_async_future_create_from_owned_awaitable(awaitable_object);
    kain_py_gil_exit(&scope);
    return future ? (long long)(intptr_t)future : 0;
}

long long py_future_state(long long future_value) {
    KainPythonAsyncFutureHandle* future = kain_py_as_async_future_handle(future_value);
    return future ? (long long)kain_py_async_future_effective_state(future) : -1;
}

int py_future_done(long long future_value) {
    int state;
    KainPythonAsyncFutureHandle* future = kain_py_as_async_future_handle(future_value);
    if (!future) {
        return 0;
    }
    state = kain_py_async_future_effective_state(future);
    return state == KAIN_TASK_STATE_COMPLETED ||
        state == KAIN_TASK_STATE_CANCELLED ||
        state == KAIN_TASK_STATE_FAILED;
}

long long py_future_await(long long future_value) {
    KainPythonAsyncFutureHandle* future = kain_py_as_async_future_handle(future_value);
    unsigned int spin_count = 0u;
    if (!future) {
        return kain_py_async_future_envelope(
            "error",
            0,
            "python async future handle was invalid",
            -1
        );
    }
    while (atomic_load_explicit(&future->settlement_kind, memory_order_acquire) == KAIN_PY_ASYNC_SETTLEMENT_PENDING) {
        KainDiagnostic diag;
        if (kain_task_get_state(future->task_id) == KAIN_TASK_STATE_PENDING ||
            kain_task_get_state(future->task_id) == KAIN_TASK_STATE_READY) {
            kain_diagnostic_init(&diag);
            (void)kain_task_poll(future->task_id, NULL, &diag);
        }
        kain_py_async_sleep_millis(1u);
        spin_count += 1u;
        if (spin_count > 8192u &&
            atomic_load_explicit(&future->worker_done, memory_order_acquire) != 0) {
            break;
        }
    }
    return kain_py_async_future_build_result(future);
}

int py_future_cancel(long long future_value) {
    KainPythonAsyncFutureHandle* future = kain_py_as_async_future_handle(future_value);
    return kain_py_async_future_cancel_internal(future);
}

int py_future_close(long long future_value) {
    KainPythonAsyncFutureHandle* future = kain_py_as_async_future_handle(future_value);
    return kain_py_async_future_shutdown(future);
}

static void kain_py_actor_callback_release_globals(KainPythonActorCallbackHandle* callback) {
    KainPythonGilScope scope;
    PyObject* none_obj;
    if (!callback) {
        return;
    }
    scope = kain_py_gil_enter();
    if (!scope.active) {
        return;
    }
    none_obj = kain_py_none_object_owned();
    if (none_obj != NULL) {
        if (callback->queue_name && callback->queue_name[0]) {
            (void)kain_py_set_main_global_owned(callback->queue_name, none_obj);
        }
        if (callback->callable_name && callback->callable_name[0]) {
            (void)kain_py_set_main_global_owned(callback->callable_name, none_obj);
        }
        g_kain_python_api.Py_DecRef(none_obj);
    }
    kain_py_gil_exit(&scope);
}

static char* kain_py_actor_callback_payload_text(
    KainPythonActorCallbackHandle* callback,
    PyObject* item
) {
    PyObject* args_obj;
    PyObject* kwargs_obj;
    long long args_value;
    long long kwargs_value;
    long long payload;
    char* text;
    if (!callback || !item) {
        return NULL;
    }
    args_obj = g_kain_python_api.PyTuple_GetItem(item, 0);
    kwargs_obj = g_kain_python_api.PyTuple_GetItem(item, 1);
    if (!args_obj || !kwargs_obj) {
        kain_py_clear_error();
        return NULL;
    }
    args_value = kain_py_materialize_borrowed(args_obj, 0);
    kwargs_value = kain_py_materialize_borrowed(kwargs_obj, 0);
    payload = json_object_new();
    json_object_set(payload, "callback_id", KAIN_PY_JSON_INT((long long)callback->callback_id));
    json_object_set(payload, "args", args_value);
    json_object_set(payload, "kwargs", kwargs_value);
    kain_py_any_release(args_value);
    kain_py_any_release(kwargs_value);
    text = json_string(payload);
    json_release(payload);
    return text;
}

static int kain_py_actor_callback_shutdown(KainPythonActorCallbackHandle* callback) {
    if (!callback) {
        return -1;
    }
    if (atomic_exchange_explicit(&callback->close_requested, 1, memory_order_acq_rel) != 0) {
        return 0;
    }
    if (callback->thread_started) {
#ifdef _WIN32
        if (callback->thread_handle != NULL) {
            WaitForSingleObject(callback->thread_handle, INFINITE);
            CloseHandle(callback->thread_handle);
            callback->thread_handle = NULL;
        }
#else
        pthread_join(callback->thread_handle, NULL);
#endif
        callback->thread_started = 0;
    }
    kain_py_actor_callback_release_globals(callback);
    return 0;
}

static void kain_py_actor_callback_destructor(void* ptr) {
    KainPythonActorCallbackHandle* callback = (KainPythonActorCallbackHandle*)ptr;
    if (!callback) {
        return;
    }
    (void)kain_py_actor_callback_shutdown(callback);
    if (callback->message_name) {
        free(callback->message_name);
        callback->message_name = NULL;
    }
    if (callback->queue_name) {
        free(callback->queue_name);
        callback->queue_name = NULL;
    }
    if (callback->callable_name) {
        free(callback->callable_name);
        callback->callable_name = NULL;
    }
    kain_py_async_mutex_destroy(&callback->lock);
}

#ifdef _WIN32
static DWORD WINAPI kain_py_actor_callback_thread_proc(LPVOID param)
#else
static void* kain_py_actor_callback_thread_proc(void* param)
#endif
{
    KainPythonActorCallbackHandle* callback = (KainPythonActorCallbackHandle*)param;
    while (callback != NULL &&
           atomic_load_explicit(&callback->close_requested, memory_order_acquire) == 0) {
        KainPythonGilScope scope = kain_py_gil_enter();
        char* payload_text = NULL;
        if (scope.active) {
            PyObject* queue = kain_py_lookup_name(callback->queue_name);
            if (queue != NULL) {
                Py_ssize_t queue_size = g_kain_python_api.PyList_Size(queue);
                if (queue_size > 0) {
                    PyObject* index = g_kain_python_api.PyLong_FromLongLong(0);
                    PyObject* item = NULL;
                    if (index != NULL) {
                        item = kain_py_call_method1_owned(queue, "pop", index);
                    }
                    if (item != NULL) {
                        payload_text = kain_py_actor_callback_payload_text(callback, item);
                        g_kain_python_api.Py_DecRef(item);
                    } else {
                        kain_py_clear_error();
                    }
                } else if (queue_size < 0) {
                    kain_py_clear_error();
                }
                g_kain_python_api.Py_DecRef(queue);
            } else {
                kain_py_clear_error();
            }
            kain_py_gil_exit(&scope);
        }
        if (payload_text != NULL) {
            if (abi_actor_send(callback->actor_id, callback->message_name, payload_text) == 0) {
                atomic_fetch_add_explicit(&callback->delivered_count, 1, memory_order_relaxed);
            }
            free(payload_text);
            continue;
        }
        kain_py_async_sleep_millis(2u);
    }
    atomic_store_explicit(&callback->dispatcher_done, 1, memory_order_release);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

long long py_actor_callback_register(long long actor_id, char* message_name) {
    KainPythonActorCallbackHandle* callback;
    KainPythonGilScope scope;
    PyObject* queue = NULL;
    PyObject* none_obj = NULL;
    char code[768];
    if (actor_id <= 0 || !message_name || !message_name[0] || abi_actor_state_invalid(actor_id)) {
        return 0;
    }
    callback = (KainPythonActorCallbackHandle*)kain_alloc_rc(
        sizeof(KainPythonActorCallbackHandle),
        KAIN_RC_TYPE_PY_ACTOR_CALLBACK
    );
    if (!callback) {
        return 0;
    }
    memset(callback, 0, sizeof(*callback));
    callback->actor_id = actor_id;
    callback->callback_id = atomic_fetch_add_explicit(&g_kain_py_actor_callback_ids, 1u, memory_order_relaxed);
    kain_py_async_mutex_init(&callback->lock);
    atomic_init(&callback->close_requested, 0);
    atomic_init(&callback->dispatcher_done, 0);
    atomic_init(&callback->delivered_count, 0);
    KAIN_set_destructor(callback, kain_py_actor_callback_destructor);
    callback->message_name = kain_py_dup_cstr(message_name);
    if (!callback->message_name) {
        rc_release(callback);
        return 0;
    }
    callback->queue_name = (char*)malloc(96u);
    callback->callable_name = (char*)malloc(96u);
    if (!callback->queue_name || !callback->callable_name) {
        rc_release(callback);
        return 0;
    }
    snprintf(callback->queue_name, 96u, "__kain_py_actor_callback_queue_%llu", (unsigned long long)callback->callback_id);
    snprintf(callback->callable_name, 96u, "__kain_py_actor_callback_fn_%llu", (unsigned long long)callback->callback_id);
    scope = kain_py_gil_enter();
    if (!scope.active) {
        rc_release(callback);
        return 0;
    }
    queue = g_kain_python_api.PyList_New(0);
    if (!queue) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        rc_release(callback);
        return 0;
    }
    if (kain_py_set_main_global_owned(callback->queue_name, queue) != 0) {
        g_kain_python_api.Py_DecRef(queue);
        kain_py_gil_exit(&scope);
        rc_release(callback);
        return 0;
    }
    g_kain_python_api.Py_DecRef(queue);
    snprintf(
        code,
        sizeof(code),
        "def %s(*args, **kwargs):\n"
        "    %s.append((args, kwargs))\n"
        "    return None\n",
        callback->callable_name,
        callback->queue_name
    );
    none_obj = kain_py_main_dict();
    if (!none_obj) {
        kain_py_gil_exit(&scope);
        rc_release(callback);
        return 0;
    }
    queue = g_kain_python_api.PyRun_StringFlags(code, KAIN_PY_FILE_INPUT, none_obj, none_obj, NULL);
    g_kain_python_api.Py_DecRef(none_obj);
    if (!queue) {
        kain_py_clear_error();
        kain_py_gil_exit(&scope);
        rc_release(callback);
        return 0;
    }
    g_kain_python_api.Py_DecRef(queue);
    kain_py_gil_exit(&scope);
#ifdef _WIN32
    callback->thread_handle = CreateThread(NULL, 0, kain_py_actor_callback_thread_proc, callback, 0, NULL);
    callback->thread_started = callback->thread_handle != NULL;
#else
    callback->thread_started = pthread_create(&callback->thread_handle, NULL, kain_py_actor_callback_thread_proc, callback) == 0;
#endif
    if (!callback->thread_started) {
        (void)kain_py_actor_callback_shutdown(callback);
        rc_release(callback);
        return 0;
    }
    return (long long)(intptr_t)callback;
}

long long py_actor_callback_function(long long callback_value) {
    KainPythonActorCallbackHandle* callback = kain_py_as_actor_callback_handle(callback_value);
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* callable;
    long long wrapped = 0;
    if (!callback || !scope.active) {
        return 0;
    }
    callable = kain_py_lookup_name(callback->callable_name);
    if (!callable) {
        kain_py_gil_exit(&scope);
        return 0;
    }
    wrapped = kain_py_wrap_result(callable);
    kain_py_gil_exit(&scope);
    return wrapped;
}

int py_actor_callback_close(long long callback_value) {
    KainPythonActorCallbackHandle* callback = kain_py_as_actor_callback_handle(callback_value);
    return kain_py_actor_callback_shutdown(callback);
}

long long py_actor_callback_delivered_count(long long callback_value) {
    KainPythonActorCallbackHandle* callback = kain_py_as_actor_callback_handle(callback_value);
    if (!callback) {
        return 0;
    }
    return atomic_load_explicit(&callback->delivered_count, memory_order_acquire);
}
