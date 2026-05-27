static int kain_py_buffer_view_item_count_from_view(const Py_buffer* view, long long* out_count) {
    long long count = 0;
    if (out_count) {
        *out_count = 0;
    }
    if (!view || !out_count || view->len < 0) {
        return 0;
    }
    if (view->ndim > 0 && view->shape) {
        Py_ssize_t index;
        count = 1;
        for (index = 0; index < view->ndim; ++index) {
            long long dim = (long long)view->shape[index];
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
    if (view->itemsize <= 0) {
        *out_count = (long long)view->len;
        return 1;
    }
    if ((view->len % view->itemsize) != 0) {
        return 0;
    }
    *out_count = (long long)(view->len / view->itemsize);
    return 1;
}

static void kain_py_buffer_view_destructor(void* payload) {
    KainPythonBufferViewHandle* handle = (KainPythonBufferViewHandle*)payload;
    if (!handle) {
        return;
    }
    if (handle->region_owner && handle->region_owner->active) {
        kain_py_buffer_view_close_active(handle);
        return;
    }
    {
        KainPythonGilScope scope = kain_py_gil_enter();
        if (scope.active) {
            kain_py_buffer_view_close_active(handle);
        }
        kain_py_gil_exit(&scope);
    }
}

static long long kain_py_buffer_view_from_target_active(
    long long target,
    KainPythonRegionHandle* region
) {
    PyObject* object;
    PyObject* export_target;
    KainPythonBufferViewHandle* handle = NULL;
    long long item_count = 0;
    Py_buffer borrowed_view;
    memset(&borrowed_view, 0, sizeof(borrowed_view));
    object = kain_py_resolve_target(target);
    if (!object) {
        return 0;
    }
    export_target = kain_py_export_buffer_target(object, NULL, 0u);
    g_kain_python_api.Py_DecRef(object);
    if (!export_target) {
        return 0;
    }
    if (!g_kain_python_api.PyObject_GetBuffer ||
        g_kain_python_api.PyObject_GetBuffer(export_target, &borrowed_view, KAIN_PYBUF_STRIDES) != 0) {
        g_kain_python_api.Py_DecRef(export_target);
        kain_py_clear_error();
        return 0;
    }
    g_kain_python_api.Py_DecRef(export_target);
    if (borrowed_view.len < 0 || (borrowed_view.len > 0 && borrowed_view.buf == NULL)) {
        g_kain_python_api.PyBuffer_Release(&borrowed_view);
        return 0;
    }
    if (borrowed_view.itemsize <= 0) {
        borrowed_view.itemsize = 1;
    }
    if (!kain_py_buffer_view_item_count_from_view(&borrowed_view, &item_count)) {
        g_kain_python_api.PyBuffer_Release(&borrowed_view);
        return 0;
    }
    handle = (KainPythonBufferViewHandle*)kain_alloc_rc(
        sizeof(KainPythonBufferViewHandle),
        KAIN_RC_TYPE_PY_BUFFER_VIEW
    );
    if (!handle) {
        g_kain_python_api.PyBuffer_Release(&borrowed_view);
        return 0;
    }
    memset(handle, 0, sizeof(*handle));
    handle->view = borrowed_view;
    handle->item_count = item_count;
    handle->item_size = (long long)borrowed_view.itemsize;
    handle->c_contiguous = kain_py_buffer_view_is_c_contiguous(&borrowed_view);
    handle->writable = borrowed_view.readonly ? 0 : 1;
    if (region) {
        (void)kain_py_region_track_view(region, handle);
    }
    KAIN_set_destructor(handle, kain_py_buffer_view_destructor);
    return (long long)(intptr_t)handle;
}

long long py_buffer_view(long long target) {
    KainPythonGilScope scope = kain_py_gil_enter();
    long long result = 0;
    if (!scope.active) {
        return 0;
    }
    result = kain_py_buffer_view_from_target_active(target, NULL);
    kain_py_gil_exit(&scope);
    return result;
}

long long py_buffer_view_byte_length(long long target) {
    KainPythonBufferViewHandle* handle = kain_py_as_buffer_view_handle(target);
    return handle ? (long long)handle->view.len : 0;
}

long long py_buffer_view_element_count(long long target) {
    KainPythonBufferViewHandle* handle = kain_py_as_buffer_view_handle(target);
    return handle ? handle->item_count : 0;
}

long long py_buffer_view_element_size(long long target) {
    KainPythonBufferViewHandle* handle = kain_py_as_buffer_view_handle(target);
    return handle ? handle->item_size : 0;
}

long long py_buffer_view_c_contiguous(long long target) {
    KainPythonBufferViewHandle* handle = kain_py_as_buffer_view_handle(target);
    return (handle && handle->c_contiguous) ? 1 : 0;
}

long long py_buffer_view_writable(long long target) {
    KainPythonBufferViewHandle* handle = kain_py_as_buffer_view_handle(target);
    return (handle && handle->writable) ? 1 : 0;
}

void py_buffer_view_release(long long target) {
    KainPythonBufferViewHandle* handle = kain_py_as_buffer_view_handle(target);
    if (!handle) {
        return;
    }
    if (handle->region_owner && handle->region_owner->active) {
        handle->region_owner->views_released += 1u;
    }
    kain_py_buffer_view_close_active(handle);
    rc_release(handle);
}

long long kain_shared_buffer_from_py(long long target) {
    KainPythonGilScope scope = kain_py_gil_enter();
    PyObject* object;
    PyObject* export_target;
    PyObject* bytes_obj = NULL;
    Py_buffer borrowed_view;
    unsigned char* byte_values = NULL;
    long long byte_length = 0;
    long long* shape_values = NULL;
    long long shape_len = 0;
    long long shape_handle = 0;
    long long strides_handle = 0;
    long long labels = 0;
    long long handle = 0;
    long long item_size = 1;
    long long zero_copy_owner = 0;
    char backend[32];
    char dtype[24];
    const char* element_type;
    if (!scope.active) {
        return 0;
    }
    object = kain_py_resolve_target(target);
    if (!object) {
        if (kain_py_trace_enabled()) {
            fprintf(stderr, "[kain-py] shared_buffer: resolve_target failed\n");
        }
        kain_py_gil_exit(&scope);
        return 0;
    }
    export_target = kain_py_export_buffer_target(object, backend, sizeof(backend));
    if (!export_target) {
        if (kain_py_trace_enabled()) {
            fprintf(stderr, "[kain-py] shared_buffer: export_target failed\n");
        }
        g_kain_python_api.Py_DecRef(object);
        kain_py_gil_exit(&scope);
        return 0;
    }
    dtype[0] = '\0';
    memset(&borrowed_view, 0, sizeof(borrowed_view));
    kain_py_copy_dtype_name(export_target, dtype, sizeof(dtype));
    item_size = kain_py_attr_int(export_target, "itemsize", 1);
    zero_copy_owner = kain_py_borrowed_buffer_owner_create(export_target, &borrowed_view);
    if (zero_copy_owner) {
        if (borrowed_view.itemsize > 0) {
            item_size = (long long)borrowed_view.itemsize;
        }
        if (!dtype[0]) {
            const char* view_dtype = kain_py_dtype_from_buffer_format(borrowed_view.format);
            if (view_dtype) {
                strncpy_s(dtype, sizeof(dtype), view_dtype, _TRUNCATE);
            }
        }
        if (kain_py_buffer_shape_handles_from_view(
                &borrowed_view,
                item_size > 0 ? item_size : 1,
                &shape_handle,
                &strides_handle
            )) {
            labels = kain_py_build_shared_labels("python", backend[0] ? backend : "buffer");
            element_type = kain_py_storage_from_dtype(dtype);
            handle = kain_shared_buffer_create_borrowed(
                (const unsigned char*)borrowed_view.buf,
                (long long)borrowed_view.len,
                element_type,
                item_size > 0 ? item_size : 1,
                shape_handle,
                strides_handle,
                dtype[0] ? dtype : NULL,
                "application/octet-stream",
                "python",
                backend[0] ? backend : NULL,
                "shared",
                labels,
                zero_copy_owner
            );
        }
    }
    if (!handle) {
        if (!shape_handle || !strides_handle) {
            if (!kain_py_read_shape(export_target, &shape_values, &shape_len) || shape_len <= 0) {
                long long inferred[1];
                long long nbytes = kain_py_attr_int(export_target, "nbytes", 0);
                inferred[0] = (item_size > 0 && nbytes > 0) ? (nbytes / item_size) : nbytes;
                shape_len = 1;
                if (!shape_handle) {
                    shape_handle = kain_py_array_handle_from_values(inferred, 1);
                }
                if (!strides_handle) {
                    strides_handle = kain_py_compact_strides_handle(inferred, 1);
                }
            } else {
                if (!shape_handle) {
                    shape_handle = kain_py_array_handle_from_values(shape_values, shape_len);
                }
                if (!strides_handle) {
                    strides_handle = kain_py_compact_strides_handle(shape_values, shape_len);
                }
            }
        }
        bytes_obj = kain_py_call_method0_owned(export_target, "tobytes");
        if (!labels) {
            labels = kain_py_build_shared_labels("python", backend[0] ? backend : "buffer");
        }
        if (kain_py_trace_enabled()) {
            fprintf(
                stderr,
                "[kain-py] shared_buffer: backend=%s dtype=%s item_size=%lld shape_len=%lld shape_handle=%p strides_handle=%p zero_copy_owner=%p bytes_obj=%p\n",
                backend[0] ? backend : "<none>",
                dtype[0] ? dtype : "<none>",
                item_size,
                shape_len,
                (void*)(intptr_t)shape_handle,
                (void*)(intptr_t)strides_handle,
                (void*)(intptr_t)zero_copy_owner,
                (void*)bytes_obj
            );
        }
        if (bytes_obj && kain_py_extract_byte_sequence(bytes_obj, &byte_values, &byte_length) && shape_handle) {
            element_type = kain_py_storage_from_dtype(dtype);
            handle = kain_shared_buffer_create_owned(
                byte_values,
                byte_length,
                element_type,
                item_size > 0 ? item_size : 1,
                shape_handle,
                strides_handle,
                dtype[0] ? dtype : NULL,
                "application/octet-stream",
                "python",
                backend[0] ? backend : NULL,
                "owned",
                labels
            );
            if (kain_py_trace_enabled()) {
                fprintf(
                    stderr,
                    "[kain-py] shared_buffer: extracted_bytes=%lld labels=%p handle=%p element_type=%s\n",
                    byte_length,
                    (void*)(intptr_t)labels,
                    (void*)(intptr_t)handle,
                    element_type ? element_type : "<null>"
                );
            }
        } else if (kain_py_trace_enabled()) {
            fprintf(
                stderr,
                "[kain-py] shared_buffer: extraction gate failed bytes_obj=%p shape_handle=%p\n",
                (void*)bytes_obj,
                (void*)(intptr_t)shape_handle
            );
        }
    } else if (kain_py_trace_enabled()) {
        fprintf(
            stderr,
            "[kain-py] shared_buffer: zero-copy adopted backend=%s dtype=%s bytes=%lld owner=%p handle=%p\n",
            backend[0] ? backend : "<none>",
            dtype[0] ? dtype : "<none>",
            (long long)borrowed_view.len,
            (void*)(intptr_t)zero_copy_owner,
            (void*)(intptr_t)handle
        );
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
    free(shape_values);
    free(byte_values);
    if (bytes_obj) {
        g_kain_python_api.Py_DecRef(bytes_obj);
    }
    if (zero_copy_owner) {
        kain_interop_zero_copy_owner_release(zero_copy_owner);
    }
    g_kain_python_api.Py_DecRef(export_target);
    g_kain_python_api.Py_DecRef(object);
    kain_py_gil_exit(&scope);
    return handle;
}
