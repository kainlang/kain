static KainPythonRegionHandle* kain_py_as_region_handle(long long value) {
    value = kain_py_unbox_tagged_handle(value, KAIN_RC_TYPE_PY_REGION);
    return kain_py_type_tag_matches((void*)(intptr_t)value, KAIN_RC_TYPE_PY_REGION)
        ? (KainPythonRegionHandle*)(intptr_t)value
        : NULL;
}

static void kain_py_region_import_entry_clear(KainPythonRegionImportCacheEntry* entry) {
    if (!entry) {
        return;
    }
    if (entry->module) {
        g_kain_python_api.Py_DecRef(entry->module);
        entry->module = NULL;
    }
    if (entry->module_name) {
        free(entry->module_name);
        entry->module_name = NULL;
    }
}

static void kain_py_region_attr_entry_clear(KainPythonRegionAttrCacheEntry* entry) {
    if (!entry) {
        return;
    }
    if (entry->owner) {
        g_kain_python_api.Py_DecRef(entry->owner);
        entry->owner = NULL;
    }
    if (entry->value) {
        g_kain_python_api.Py_DecRef(entry->value);
        entry->value = NULL;
    }
    if (entry->attr_name) {
        free(entry->attr_name);
        entry->attr_name = NULL;
    }
}

static void kain_py_buffer_view_unregister_region(KainPythonBufferViewHandle* handle) {
    KainPythonRegionHandle* region;
    size_t slot;
    size_t last_slot;
    if (!handle || !handle->region_owner) {
        return;
    }
    region = handle->region_owner;
    slot = handle->region_slot;
    if (!region->open_views || region->open_view_count == 0u) {
        handle->region_owner = NULL;
        handle->region_slot = 0u;
        return;
    }
    if (slot >= region->open_view_count || region->open_views[slot] != handle) {
        size_t index;
        slot = region->open_view_count;
        for (index = 0u; index < region->open_view_count; ++index) {
            if (region->open_views[index] == handle) {
                slot = index;
                break;
            }
        }
        if (slot == region->open_view_count) {
            handle->region_owner = NULL;
            handle->region_slot = 0u;
            return;
        }
    }
    last_slot = region->open_view_count - 1u;
    if (slot != last_slot) {
        KainPythonBufferViewHandle* moved = region->open_views[last_slot];
        region->open_views[slot] = moved;
        if (moved) {
            moved->region_slot = slot;
        }
    }
    region->open_views[last_slot] = NULL;
    region->open_view_count -= 1u;
    handle->region_owner = NULL;
    handle->region_slot = 0u;
}

static void kain_py_buffer_view_close_active(KainPythonBufferViewHandle* handle) {
    if (!handle) {
        return;
    }
    kain_py_buffer_view_unregister_region(handle);
    if (handle->view.obj && g_kain_python_api.PyBuffer_Release) {
        g_kain_python_api.PyBuffer_Release(&handle->view);
        memset(&handle->view, 0, sizeof(handle->view));
    }
}

static int kain_py_region_track_view(KainPythonRegionHandle* region, KainPythonBufferViewHandle* handle) {
    KainPythonBufferViewHandle** grown;
    size_t new_capacity;
    if (!region || !handle || !region->active) {
        return 0;
    }
    if (region->open_view_count == region->open_view_capacity) {
        new_capacity = region->open_view_capacity == 0u ? 16u : region->open_view_capacity * 2u;
        grown = (KainPythonBufferViewHandle**)realloc(
            region->open_views,
            new_capacity * sizeof(KainPythonBufferViewHandle*)
        );
        if (!grown) {
            return 0;
        }
        memset(
            grown + region->open_view_capacity,
            0,
            (new_capacity - region->open_view_capacity) * sizeof(KainPythonBufferViewHandle*)
        );
        region->open_views = grown;
        region->open_view_capacity = new_capacity;
    }
    region->open_views[region->open_view_count] = handle;
    handle->region_owner = region;
    handle->region_slot = region->open_view_count;
    region->open_view_count += 1u;
    region->views_opened += 1u;
    return 1;
}

static long long kain_py_region_close(KainPythonRegionHandle* region) {
    long long auto_released = 0;
    if (!region || !region->active) {
        return 0;
    }
    while (region->open_view_count > 0u) {
        KainPythonBufferViewHandle* handle = region->open_views[region->open_view_count - 1u];
        region->open_views[region->open_view_count - 1u] = NULL;
        region->open_view_count -= 1u;
        if (!handle) {
            continue;
        }
        handle->region_owner = NULL;
        handle->region_slot = 0u;
        if (handle->view.obj && g_kain_python_api.PyBuffer_Release) {
            g_kain_python_api.PyBuffer_Release(&handle->view);
            memset(&handle->view, 0, sizeof(handle->view));
            auto_released += 1;
            region->views_released += 1u;
        }
    }
    while (region->import_count > 0u) {
        region->import_count -= 1u;
        kain_py_region_import_entry_clear(&region->imports[region->import_count]);
    }
    while (region->attr_count > 0u) {
        region->attr_count -= 1u;
        kain_py_region_attr_entry_clear(&region->attrs[region->attr_count]);
    }
    if (region->scope.active) {
        g_kain_python_api.PyGILState_Release(region->scope.state);
        region->scope.active = 0;
    }
    region->active = 0;
    return auto_released;
}

static void kain_py_region_destructor(void* payload) {
    KainPythonRegionHandle* region = (KainPythonRegionHandle*)payload;
    if (!region) {
        return;
    }
    (void)kain_py_region_close(region);
    if (region->open_views) {
        free(region->open_views);
        region->open_views = NULL;
    }
    region->open_view_capacity = 0u;
}

static PyObject* kain_py_region_cached_import(
    KainPythonRegionHandle* region,
    const char* module_name,
    const char* importer_file
) {
    size_t index;
    PyObject* module;
    if (!region || !region->active || !module_name || !module_name[0]) {
        return NULL;
    }
    for (index = 0u; index < region->import_count; ++index) {
        KainPythonRegionImportCacheEntry* entry = &region->imports[index];
        if (entry->module && entry->module_name && strcmp(entry->module_name, module_name) == 0) {
            region->import_cache_hits += 1u;
            g_kain_python_api.Py_IncRef(entry->module);
            return entry->module;
        }
    }
    region->import_cache_misses += 1u;
    kain_py_prepare_import_context(importer_file);
    module = g_kain_python_api.PyImport_ImportModule(module_name);
    if (!module) {
        kain_py_clear_error();
        return NULL;
    }
    if (region->import_count < KAIN_PY_REGION_IMPORT_CACHE) {
        char* owned_name = kain_py_dup_cstr(module_name);
        if (owned_name) {
            KainPythonRegionImportCacheEntry* entry = &region->imports[region->import_count];
            entry->module_name = owned_name;
            entry->module = module;
            region->import_count += 1u;
            g_kain_python_api.Py_IncRef(module);
        }
    }
    return module;
}

static PyObject* kain_py_region_cached_attr(
    KainPythonRegionHandle* region,
    PyObject* owner,
    const char* attr_name
) {
    size_t index;
    PyObject* value;
    if (!region || !region->active || !owner || !attr_name || !attr_name[0]) {
        return NULL;
    }
    for (index = 0u; index < region->attr_count; ++index) {
        KainPythonRegionAttrCacheEntry* entry = &region->attrs[index];
        if (entry->value && entry->owner == owner && entry->attr_name && strcmp(entry->attr_name, attr_name) == 0) {
            region->attr_cache_hits += 1u;
            g_kain_python_api.Py_IncRef(entry->value);
            return entry->value;
        }
    }
    region->attr_cache_misses += 1u;
    value = g_kain_python_api.PyObject_GetAttrString(owner, attr_name);
    if (!value) {
        kain_py_clear_error();
        return NULL;
    }
    if (region->attr_count < KAIN_PY_REGION_ATTR_CACHE) {
        char* owned_name = kain_py_dup_cstr(attr_name);
        if (owned_name) {
            KainPythonRegionAttrCacheEntry* entry = &region->attrs[region->attr_count];
            g_kain_python_api.Py_IncRef(owner);
            entry->owner = owner;
            entry->attr_name = owned_name;
            entry->value = value;
            region->attr_count += 1u;
            g_kain_python_api.Py_IncRef(value);
        }
    }
    return value;
}

long long py_region_begin(void) {
    KainPythonRegionHandle* region =
        (KainPythonRegionHandle*)kain_alloc_rc(sizeof(KainPythonRegionHandle), KAIN_RC_TYPE_PY_REGION);
    if (!region) {
        return 0;
    }
    memset(region, 0, sizeof(*region));
    region->scope = kain_py_gil_enter();
    if (!region->scope.active) {
        rc_release(region);
        return 0;
    }
    region->active = 1;
    KAIN_set_destructor(region, kain_py_region_destructor);
    return (long long)(intptr_t)region;
}

long long py_region_end(long long region_value) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    long long auto_released = 0;
    if (!region) {
        return 0;
    }
    auto_released = kain_py_region_close(region);
    rc_release(region);
    return auto_released;
}

long long py_region_import(long long region_value, char* module_name) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    if (!region || !region->active) {
        return 0;
    }
    return kain_py_import_internal_active(module_name, NULL, region);
}

long long py_region_getattr_raw(long long region_value, long long target, char* name) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    if (!region || !region->active || !name) {
        return 0;
    }
    return kain_py_getattr_internal_active(target, name, region);
}

long long py_region_call_args(long long region_value, long long target, long long args, long long kwargs) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    if (!region || !region->active) {
        return 0;
    }
    return kain_py_call_internal_active(target, NULL, args, kwargs, 0, region);
}

long long py_region_call_attr_args(long long region_value, long long target, char* attr, long long args, long long kwargs) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    if (!region || !region->active || !attr) {
        return 0;
    }
    return kain_py_call_internal_active(target, attr, args, kwargs, 0, region);
}

long long py_region_call_raw_args(long long region_value, long long target, long long args) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    if (!region || !region->active) {
        return 0;
    }
    return kain_py_call_internal_active(target, NULL, args, 4LL, 1, region);
}

long long py_region_call_raw_attr(long long region_value, long long target, char* attr, long long args) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    if (!region || !region->active || !attr) {
        return 0;
    }
    return kain_py_call_internal_active(target, attr, args, 4LL, 1, region);
}

long long py_region_buffer_view(long long region_value, long long target) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    if (!region || !region->active) {
        return 0;
    }
    return kain_py_buffer_view_from_target_active(target, region);
}

long long py_region_import_cache_hits(long long region_value) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    return region ? (long long)region->import_cache_hits : 0;
}

long long py_region_import_cache_misses(long long region_value) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    return region ? (long long)region->import_cache_misses : 0;
}

long long py_region_attr_cache_hits(long long region_value) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    return region ? (long long)region->attr_cache_hits : 0;
}

long long py_region_attr_cache_misses(long long region_value) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    return region ? (long long)region->attr_cache_misses : 0;
}

long long py_region_views_opened(long long region_value) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    return region ? (long long)region->views_opened : 0;
}

long long py_region_views_released(long long region_value) {
    KainPythonRegionHandle* region = kain_py_as_region_handle(region_value);
    return region ? (long long)region->views_released : 0;
}
