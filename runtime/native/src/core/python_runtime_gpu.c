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
    return (long long)(intptr_t)tensor_handle;
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
    return kain_py_wrap_materialized_result(object);
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
    return (long long)(intptr_t)image_handle;
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

long long kain_shared_image_from_py(long long target) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* object;
    PyObject* export_target;
    PyObject* bytes_obj = NULL;
    Py_buffer borrowed_view;
    KainPythonImageHandle* image;
    unsigned char* byte_values = NULL;
    long long byte_length = 0;
    long long shape_handle = 0;
    long long strides_handle = 0;
    long long labels = 0;
    long long handle = 0;
    long long item_size = 1;
    long long row_stride = 0;
    long long zero_copy_owner = 0;
    char backend[32];
    const char* storage;
    const char* pixel_format;
    if (!scope.active) {
        return 0;
    }
    object = kain_py_resolve_target(target);
    if (!object) {
        if (kain_py_trace_enabled()) {
            fprintf(stderr, "[kain-py] shared_image: resolve_target failed\n");
        }
        kain_py_gil_exit(&scope);
        return 0;
    }
    export_target = kain_py_export_buffer_target(object, backend, sizeof(backend));
    if (!export_target) {
        if (kain_py_trace_enabled()) {
            fprintf(stderr, "[kain-py] shared_image: export_target failed\n");
        }
        g_kain_python_api.Py_DecRef(object);
        kain_py_gil_exit(&scope);
        return 0;
    }
    memset(&borrowed_view, 0, sizeof(borrowed_view));
    image = kain_py_wrap_image(export_target, "shared");
    if (!image) {
        g_kain_python_api.Py_DecRef(export_target);
        g_kain_python_api.Py_DecRef(object);
        kain_py_gil_exit(&scope);
        return 0;
    }
    KAIN_set_destructor(image, kain_py_image_destructor);
    item_size = kain_py_attr_int(image->object, "itemsize", 1);
    storage = kain_py_storage_from_dtype(image->dtype);
    if (strcmp(storage, "u8") == 0) {
        shape_handle = kain_py_image_shape_handle(image);
        strides_handle = kain_py_image_strides_handle(image);
        row_stride = image->row_stride > 0 ? image->row_stride : kain_py_default_image_row_stride(
            image->layout,
            image->width,
            image->channels,
            item_size > 0 ? item_size : 1
        );
        zero_copy_owner = kain_py_borrowed_buffer_owner_create(image->object, &borrowed_view);
        if (zero_copy_owner && shape_handle && row_stride > 0) {
            labels = kain_py_build_shared_labels("python", backend[0] ? backend : "image");
            pixel_format = kain_py_pixel_format(image->channels, image->dtype);
            handle = kain_shared_image_create_borrowed(
                (const unsigned char*)borrowed_view.buf,
                (long long)borrowed_view.len,
                image->width,
                image->height,
                image->channels,
                image->layout,
                pixel_format,
                "image/x-kain-raster",
                row_stride,
                "raster",
                "srgb",
                image->channels == 4 ? "straight" : "opaque",
                "python",
                backend[0] ? backend : NULL,
                "shared",
                labels,
                shape_handle,
                strides_handle,
                zero_copy_owner
            );
            if (kain_py_trace_enabled()) {
                fprintf(
                    stderr,
                    "[kain-py] shared_image: zero-copy adopted bytes=%lld labels=%p owner=%p handle=%p pixel_format=%s\n",
                    (long long)borrowed_view.len,
                    (void*)(intptr_t)labels,
                    (void*)(intptr_t)zero_copy_owner,
                    (void*)(intptr_t)handle,
                    pixel_format ? pixel_format : "<null>"
                );
            }
        }
        if (!handle) {
            bytes_obj = kain_py_call_method0_owned(image->object, "tobytes");
            if (kain_py_trace_enabled()) {
                fprintf(
                    stderr,
                    "[kain-py] shared_image: backend=%s layout=%s dtype=%s item_size=%lld row_stride=%lld shape_handle=%p strides_handle=%p zero_copy_owner=%p bytes_obj=%p\n",
                    backend[0] ? backend : "<none>",
                    image->layout,
                    image->dtype,
                    item_size,
                    row_stride,
                    (void*)(intptr_t)shape_handle,
                    (void*)(intptr_t)strides_handle,
                    (void*)(intptr_t)zero_copy_owner,
                    (void*)bytes_obj
                );
            }
            if (bytes_obj &&
                kain_py_extract_byte_sequence(bytes_obj, &byte_values, &byte_length) &&
                shape_handle &&
                row_stride > 0) {
                if (!labels) {
                    labels = kain_py_build_shared_labels("python", backend[0] ? backend : "image");
                }
                pixel_format = kain_py_pixel_format(image->channels, image->dtype);
                handle = kain_shared_image_create_owned(
                    byte_values,
                    byte_length,
                    image->width,
                    image->height,
                    image->channels,
                    image->layout,
                    pixel_format,
                    "image/x-kain-raster",
                    row_stride,
                    "raster",
                    "srgb",
                    image->channels == 4 ? "straight" : "opaque",
                    "python",
                    backend[0] ? backend : NULL,
                    "owned",
                    labels,
                    shape_handle,
                    strides_handle
                );
                if (kain_py_trace_enabled()) {
                    fprintf(
                        stderr,
                        "[kain-py] shared_image: extracted_bytes=%lld labels=%p handle=%p pixel_format=%s\n",
                        byte_length,
                        (void*)(intptr_t)labels,
                        (void*)(intptr_t)handle,
                        pixel_format ? pixel_format : "<null>"
                    );
                }
            }
        }
        if (!handle && kain_py_trace_enabled()) {
            fprintf(
                stderr,
                "[kain-py] shared_image: extraction gate failed bytes_obj=%p shape_handle=%p row_stride=%lld\n",
                (void*)bytes_obj,
                (void*)(intptr_t)shape_handle,
                row_stride
            );
        }
    }
    if (labels) {
        rc_release((void*)(intptr_t)labels);
    }
    if (shape_handle) {
        rc_release((void*)(intptr_t)shape_handle);
    }
    if (strides_handle) {
        rc_release((void*)(intptr_t)strides_handle);
    }
    free(byte_values);
    if (bytes_obj) {
        g_kain_python_api.Py_DecRef(bytes_obj);
    }
    if (zero_copy_owner) {
        kain_interop_zero_copy_owner_release(zero_copy_owner);
    }
    rc_release(image);
    g_kain_python_api.Py_DecRef(object);
    kain_py_gil_exit(&scope);
    return handle;
}
