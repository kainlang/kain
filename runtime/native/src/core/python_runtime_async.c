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
