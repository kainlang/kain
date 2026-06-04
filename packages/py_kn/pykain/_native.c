#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define PYKAIN_MODULUS 1000000007LL

static PyObject* pykain_native_error;

static int dict_set_string(PyObject* dict, const char* key, const char* value) {
    PyObject* py_value = PyUnicode_FromString(value ? value : "");
    if (!py_value) {
        return -1;
    }
    int status = PyDict_SetItemString(dict, key, py_value);
    Py_DECREF(py_value);
    return status;
}

static int dict_set_int64(PyObject* dict, const char* key, long long value) {
    PyObject* py_value = PyLong_FromLongLong(value);
    if (!py_value) {
        return -1;
    }
    int status = PyDict_SetItemString(dict, key, py_value);
    Py_DECREF(py_value);
    return status;
}

static int dict_set_bool(PyObject* dict, const char* key, int value) {
    PyObject* py_value = value ? Py_True : Py_False;
    return PyDict_SetItemString(dict, key, py_value);
}

static const char* buffer_format_to_type(const char* format, Py_ssize_t itemsize) {
    if (!format || format[0] == '\0') {
        return itemsize == 1 ? "uint8" : "bytes";
    }
    if (strcmp(format, "B") == 0) return "uint8";
    if (strcmp(format, "b") == 0) return "int8";
    if (strcmp(format, "H") == 0) return "uint16";
    if (strcmp(format, "h") == 0) return "int16";
    if (strcmp(format, "I") == 0 || strcmp(format, "L") == 0) return "uint32";
    if (strcmp(format, "i") == 0 || strcmp(format, "l") == 0) return "int32";
    if (strcmp(format, "Q") == 0) return "uint64";
    if (strcmp(format, "q") == 0) return "int64";
    if (strcmp(format, "f") == 0) return "float32";
    if (strcmp(format, "d") == 0) return "float64";
    if (strcmp(format, "?") == 0) return "bool";
    return format;
}

static PyObject* object_backend(PyObject* obj) {
    PyObject* cls = PyObject_GetAttrString(obj, "__class__");
    if (!cls) {
        PyErr_Clear();
        return PyUnicode_FromString("python");
    }

    PyObject* module = PyObject_GetAttrString(cls, "__module__");
    Py_DECREF(cls);
    if (!module) {
        PyErr_Clear();
        return PyUnicode_FromString("python");
    }

    const char* module_name = PyUnicode_AsUTF8(module);
    PyObject* backend = NULL;
    if (module_name && strncmp(module_name, "numpy", 5) == 0) {
        backend = PyUnicode_FromString("numpy");
    } else if (module_name && strncmp(module_name, "torch", 5) == 0) {
        backend = PyUnicode_FromString("torch");
    } else if (module_name && strncmp(module_name, "PIL", 3) == 0) {
        backend = PyUnicode_FromString("pillow");
    } else if (module_name && strncmp(module_name, "cv2", 3) == 0) {
        backend = PyUnicode_FromString("opencv");
    } else {
        backend = PyUnicode_FromString("python");
    }
    Py_DECREF(module);
    return backend;
}

static PyObject* object_type_name(PyObject* obj) {
    PyObject* cls = PyObject_GetAttrString(obj, "__class__");
    if (!cls) {
        PyErr_Clear();
        return PyUnicode_FromString("unknown");
    }
    PyObject* name = PyObject_GetAttrString(cls, "__name__");
    Py_DECREF(cls);
    if (!name) {
        PyErr_Clear();
        return PyUnicode_FromString("unknown");
    }
    return name;
}

static PyObject* list_from_shape(Py_buffer* view) {
    PyObject* list = PyList_New(0);
    if (!list) {
        return NULL;
    }

    if (view->ndim > 0 && view->shape) {
        for (int i = 0; i < view->ndim; i++) {
            PyObject* item = PyLong_FromSsize_t(view->shape[i]);
            if (!item || PyList_Append(list, item) < 0) {
                Py_XDECREF(item);
                Py_DECREF(list);
                return NULL;
            }
            Py_DECREF(item);
        }
    } else {
        Py_ssize_t count = view->itemsize > 0 ? view->len / view->itemsize : view->len;
        PyObject* item = PyLong_FromSsize_t(count);
        if (!item || PyList_Append(list, item) < 0) {
            Py_XDECREF(item);
            Py_DECREF(list);
            return NULL;
        }
        Py_DECREF(item);
    }
    return list;
}

static PyObject* list_from_strides(Py_buffer* view) {
    PyObject* list = PyList_New(0);
    if (!list) {
        return NULL;
    }

    if (view->ndim > 0 && view->strides) {
        for (int i = 0; i < view->ndim; i++) {
            PyObject* item = PyLong_FromSsize_t(view->strides[i]);
            if (!item || PyList_Append(list, item) < 0) {
                Py_XDECREF(item);
                Py_DECREF(list);
                return NULL;
            }
            Py_DECREF(item);
        }
    } else {
        PyObject* item = PyLong_FromSsize_t(view->itemsize > 0 ? view->itemsize : 1);
        if (!item || PyList_Append(list, item) < 0) {
            Py_XDECREF(item);
            Py_DECREF(list);
            return NULL;
        }
        Py_DECREF(item);
    }
    return list;
}

static long long element_count_from_shape(Py_buffer* view) {
    if (view->ndim <= 0 || !view->shape) {
        return view->itemsize > 0 ? (long long)(view->len / view->itemsize) : (long long)view->len;
    }

    long long count = 1;
    for (int i = 0; i < view->ndim; i++) {
        count *= (long long)view->shape[i];
    }
    return count;
}

static int fill_buffer_descriptor(PyObject* dict, PyObject* obj, Py_buffer* view, const char* kind) {
    PyObject* backend = object_backend(obj);
    PyObject* type_name = object_type_name(obj);
    PyObject* shape = list_from_shape(view);
    PyObject* strides = list_from_strides(view);
    PyObject* pointer = PyLong_FromVoidPtr(view->buf);
    if (!backend || !type_name || !shape || !strides || !pointer) {
        Py_XDECREF(backend);
        Py_XDECREF(type_name);
        Py_XDECREF(shape);
        Py_XDECREF(strides);
        Py_XDECREF(pointer);
        return -1;
    }

    const char* format = view->format ? view->format : "B";
    const char* element_type = buffer_format_to_type(format, view->itemsize);
    int contiguous = PyBuffer_IsContiguous(view, 'C');

    if (dict_set_bool(dict, "valid", 1) < 0 ||
        dict_set_string(dict, "kind", kind ? kind : "buffer") < 0 ||
        PyDict_SetItemString(dict, "backend", backend) < 0 ||
        PyDict_SetItemString(dict, "python_type", type_name) < 0 ||
        dict_set_string(dict, "source_runtime", "python") < 0 ||
        dict_set_string(dict, "ownership", "python-borrowed") < 0 ||
        dict_set_string(dict, "contract", "kain.shared.buffer") < 0 ||
        dict_set_int64(dict, "contract_version", 1) < 0 ||
        dict_set_string(dict, "element_type", element_type) < 0 ||
        dict_set_string(dict, "dtype", element_type) < 0 ||
        dict_set_string(dict, "format", format) < 0 ||
        dict_set_int64(dict, "element_size", (long long)view->itemsize) < 0 ||
        dict_set_int64(dict, "byte_length", (long long)view->len) < 0 ||
        dict_set_int64(dict, "element_count", element_count_from_shape(view)) < 0 ||
        PyDict_SetItemString(dict, "shape", shape) < 0 ||
        PyDict_SetItemString(dict, "strides", strides) < 0 ||
        dict_set_bool(dict, "is_contiguous", contiguous) < 0 ||
        dict_set_bool(dict, "is_writeable", view->readonly == 0) < 0 ||
        dict_set_bool(dict, "readonly", view->readonly != 0) < 0 ||
        dict_set_bool(dict, "pointer_available", view->buf != NULL) < 0 ||
        PyDict_SetItemString(dict, "pointer", pointer) < 0) {
        Py_DECREF(backend);
        Py_DECREF(type_name);
        Py_DECREF(shape);
        Py_DECREF(strides);
        Py_DECREF(pointer);
        return -1;
    }

    Py_DECREF(backend);
    Py_DECREF(type_name);
    Py_DECREF(shape);
    Py_DECREF(strides);
    Py_DECREF(pointer);
    return 0;
}

static PyObject* list_from_python_shape(PyObject* shape_obj) {
    PyObject* iterator = PyObject_GetIter(shape_obj);
    if (!iterator) {
        PyErr_Clear();
        return PyList_New(0);
    }
    PyObject* list = PyList_New(0);
    if (!list) {
        Py_DECREF(iterator);
        return NULL;
    }
    PyObject* item;
    while ((item = PyIter_Next(iterator))) {
        PyObject* value = PyNumber_Long(item);
        Py_DECREF(item);
        if (!value || PyList_Append(list, value) < 0) {
            Py_XDECREF(value);
            Py_DECREF(list);
            Py_DECREF(iterator);
            return NULL;
        }
        Py_DECREF(value);
    }
    Py_DECREF(iterator);
    return list;
}

static int fill_torch_descriptor(PyObject* dict, PyObject* obj, const char* kind) {
    PyObject* backend = object_backend(obj);
    PyObject* type_name = object_type_name(obj);
    PyObject* shape_obj = PyObject_GetAttrString(obj, "shape");
    PyObject* shape = shape_obj ? list_from_python_shape(shape_obj) : PyList_New(0);
    PyObject* dtype = PyObject_GetAttrString(obj, "dtype");
    PyObject* device = PyObject_GetAttrString(obj, "device");
    PyObject* element_size = PyObject_CallMethod(obj, "element_size", NULL);
    PyObject* numel = PyObject_CallMethod(obj, "numel", NULL);
    PyObject* is_contiguous = PyObject_CallMethod(obj, "is_contiguous", NULL);

    Py_XDECREF(shape_obj);
    if (!backend || !type_name || !shape || !dtype || !element_size || !numel) {
        Py_XDECREF(backend);
        Py_XDECREF(type_name);
        Py_XDECREF(shape);
        Py_XDECREF(dtype);
        Py_XDECREF(device);
        Py_XDECREF(element_size);
        Py_XDECREF(numel);
        Py_XDECREF(is_contiguous);
        PyErr_Clear();
        return -1;
    }

    long long itemsize = PyLong_AsLongLong(element_size);
    long long count = PyLong_AsLongLong(numel);
    PyObject* dtype_text = PyObject_Str(dtype);
    PyObject* device_text = device ? PyObject_Str(device) : PyUnicode_FromString("cpu");
    const char* dtype_c = dtype_text ? PyUnicode_AsUTF8(dtype_text) : "torch.unknown";
    const char* dot = dtype_c ? strrchr(dtype_c, '.') : NULL;
    const char* element_type = dot ? dot + 1 : dtype_c;
    int contiguous = is_contiguous ? PyObject_IsTrue(is_contiguous) : 0;

    if (dict_set_bool(dict, "valid", 1) < 0 ||
        dict_set_string(dict, "kind", kind ? kind : "tensor") < 0 ||
        PyDict_SetItemString(dict, "backend", backend) < 0 ||
        PyDict_SetItemString(dict, "python_type", type_name) < 0 ||
        dict_set_string(dict, "source_runtime", "python") < 0 ||
        dict_set_string(dict, "ownership", "python-host-object") < 0 ||
        dict_set_string(dict, "contract", "kain.shared.tensor") < 0 ||
        dict_set_int64(dict, "contract_version", 1) < 0 ||
        dict_set_string(dict, "element_type", element_type ? element_type : "unknown") < 0 ||
        dict_set_string(dict, "dtype", element_type ? element_type : "unknown") < 0 ||
        dict_set_int64(dict, "element_size", itemsize) < 0 ||
        dict_set_int64(dict, "byte_length", itemsize * count) < 0 ||
        dict_set_int64(dict, "element_count", count) < 0 ||
        PyDict_SetItemString(dict, "shape", shape) < 0 ||
        dict_set_bool(dict, "is_contiguous", contiguous) < 0 ||
        dict_set_bool(dict, "is_writeable", 1) < 0 ||
        dict_set_bool(dict, "pointer_available", 0) < 0) {
        Py_XDECREF(backend);
        Py_XDECREF(type_name);
        Py_XDECREF(shape);
        Py_XDECREF(dtype);
        Py_XDECREF(device);
        Py_XDECREF(element_size);
        Py_XDECREF(numel);
        Py_XDECREF(is_contiguous);
        Py_XDECREF(dtype_text);
        Py_XDECREF(device_text);
        return -1;
    }

    if (device_text) {
        PyDict_SetItemString(dict, "device", device_text);
    }

    Py_DECREF(backend);
    Py_DECREF(type_name);
    Py_DECREF(shape);
    Py_DECREF(dtype);
    Py_XDECREF(device);
    Py_DECREF(element_size);
    Py_DECREF(numel);
    Py_XDECREF(is_contiguous);
    Py_XDECREF(dtype_text);
    Py_XDECREF(device_text);
    return 0;
}

static PyObject* pykain_native_inspect(PyObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* obj = NULL;
    const char* kind = "auto";
    static char* kwlist[] = {"obj", "kind", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|s", kwlist, &obj, &kind)) {
        return NULL;
    }

    PyObject* dict = PyDict_New();
    if (!dict) {
        return NULL;
    }

    Py_buffer view;
    if (PyObject_GetBuffer(obj, &view, PyBUF_FULL_RO) == 0) {
        if (fill_buffer_descriptor(dict, obj, &view, kind) < 0) {
            PyBuffer_Release(&view);
            Py_DECREF(dict);
            return NULL;
        }
        PyBuffer_Release(&view);
        return dict;
    }
    PyErr_Clear();

    PyObject* backend = object_backend(obj);
    const char* backend_c = backend ? PyUnicode_AsUTF8(backend) : "";
    int is_torch_backend = backend_c && strcmp(backend_c, "torch") == 0;
    Py_XDECREF(backend);
    if (is_torch_backend) {
        if (fill_torch_descriptor(dict, obj, kind) == 0) {
            return dict;
        }
    }
    PyErr_Clear();

    PyObject* type_name = object_type_name(obj);
    PyObject* backend2 = object_backend(obj);
    if (!type_name || !backend2) {
        Py_XDECREF(type_name);
        Py_XDECREF(backend2);
        Py_DECREF(dict);
        return NULL;
    }

    Py_ssize_t seq_len = PySequence_Check(obj) ? PySequence_Length(obj) : -1;
    dict_set_bool(dict, "valid", seq_len >= 0);
    dict_set_string(dict, "kind", kind ? kind : "object");
    PyDict_SetItemString(dict, "backend", backend2);
    PyDict_SetItemString(dict, "python_type", type_name);
    dict_set_string(dict, "source_runtime", "python");
    dict_set_string(dict, "ownership", "python-host-object");
    dict_set_string(dict, "contract", "pykain.host.object");
    dict_set_bool(dict, "pointer_available", 0);
    if (seq_len >= 0) {
        dict_set_int64(dict, "element_count", (long long)seq_len);
    } else {
        dict_set_string(dict, "error", "object does not expose the buffer protocol");
    }

    Py_DECREF(type_name);
    Py_DECREF(backend2);
    return dict;
}

static int acquire_read_buffer(PyObject* obj, Py_buffer* view) {
    if (PyObject_GetBuffer(obj, view, PyBUF_CONTIG_RO) == 0) {
        return 0;
    }
    PyErr_Clear();
    if (PyObject_GetBuffer(obj, view, PyBUF_SIMPLE) == 0) {
        return 0;
    }
    PyErr_Clear();
    return -1;
}

static PyObject* pykain_native_signature(PyObject* self, PyObject* args) {
    PyObject* obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &obj)) {
        return NULL;
    }

    Py_buffer view;
    if (acquire_read_buffer(obj, &view) == 0) {
        unsigned char* bytes = (unsigned char*)view.buf;
        long long acc = 0;
        for (Py_ssize_t i = 0; i < view.len; i++) {
            acc = (acc + (long long)bytes[i]) % PYKAIN_MODULUS;
        }
        PyBuffer_Release(&view);
        return PyLong_FromLongLong(acc);
    }

    PyObject* seq = PySequence_Fast(obj, "object is not bytes-like or a sequence");
    if (!seq) {
        return NULL;
    }

    Py_ssize_t len = PySequence_Fast_GET_SIZE(seq);
    PyObject** items = PySequence_Fast_ITEMS(seq);
    long long acc = 0;
    for (Py_ssize_t i = 0; i < len; i++) {
        long long value = PyLong_AsLongLong(items[i]);
        if (PyErr_Occurred()) {
            Py_DECREF(seq);
            return NULL;
        }
        acc = (acc + (value & 255LL)) % PYKAIN_MODULUS;
    }
    Py_DECREF(seq);
    return PyLong_FromLongLong(acc);
}

static PyObject* pykain_native_as_bytes(PyObject* self, PyObject* args) {
    PyObject* obj = NULL;
    if (!PyArg_ParseTuple(args, "O", &obj)) {
        return NULL;
    }

    Py_buffer view;
    if (acquire_read_buffer(obj, &view) == 0) {
        PyObject* result = PyBytes_FromStringAndSize((const char*)view.buf, view.len);
        PyBuffer_Release(&view);
        return result;
    }

    PyObject* bytes_type = (PyObject*)&PyBytes_Type;
    return PyObject_CallFunctionObjArgs(bytes_type, obj, NULL);
}

static PyObject* pykain_native_version(PyObject* self, PyObject* args) {
    return PyUnicode_FromString("0.1.0");
}

static PyMethodDef PykainNativeMethods[] = {
    {"native_version", pykain_native_version, METH_NOARGS, "Return pykain native extension version."},
    {"inspect", (PyCFunction)pykain_native_inspect, METH_VARARGS | METH_KEYWORDS, "Inspect a Python object into a Kain-ready descriptor."},
    {"signature", pykain_native_signature, METH_VARARGS, "Compute a byte-level signature for a Python object."},
    {"as_bytes", pykain_native_as_bytes, METH_VARARGS, "Copy a Python object into bytes using the buffer protocol."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pykain_native_module = {
    PyModuleDef_HEAD_INIT,
    "pykain._native",
    "C acceleration layer for pykain.",
    -1,
    PykainNativeMethods
};

PyMODINIT_FUNC PyInit__native(void) {
    PyObject* module = PyModule_Create(&pykain_native_module);
    if (!module) {
        return NULL;
    }
    pykain_native_error = PyErr_NewException("pykain._native.NativeError", NULL, NULL);
    if (!pykain_native_error) {
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(pykain_native_error);
    PyModule_AddObject(module, "NativeError", pykain_native_error);
    return module;
}
