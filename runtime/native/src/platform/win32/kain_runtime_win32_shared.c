#include "../../../include/kain_runtime_win32.h"

#ifdef _WIN32
static const KainViewportProfile g_kain_viewport_profiles[] = {
    {
        "default_viewport",
        "Default Viewport",
        "",
        {0.03f, 0.04f, 0.06f, 1.0f},
        {0.04f, 0.05f, 0.07f, 1.0f},
        {0.20f, 0.22f, 0.26f, 1.0f},
        {0.86f, 0.90f, 1.00f, 1.0f},
        {10.0f, 18.0f, -10.0f, 1.0f},
        {0.24f, 0.68f, 1.00f},
        {1.00f, 0.76f, 0.36f},
        1440,
        900,
        8.8,
        1.9,
        6.4,
        18.4,
        0.0021,
        1.76,
        0.018,
        180
    }
};

static const size_t g_kain_viewport_profile_count =
    sizeof(g_kain_viewport_profiles) / sizeof(g_kain_viewport_profiles[0]);

static int kain_viewport_profile_matches_alias(const char* alias_list, const char* id) {
    const char* cursor = alias_list;
    size_t id_length;

    if (!alias_list || !alias_list[0] || !id || !id[0]) {
        return 0;
    }

    id_length = strlen(id);
    while (*cursor) {
        const char* token_start;
        const char* token_end;
        size_t token_length;

        while (*cursor == ' ' || *cursor == ',' || *cursor == ';' || *cursor == '|') {
            ++cursor;
        }
        if (!*cursor) {
            break;
        }

        token_start = cursor;
        token_end = cursor;
        while (*token_end && *token_end != ',' && *token_end != ';' && *token_end != '|') {
            ++token_end;
        }
        while (token_end > token_start && token_end[-1] == ' ') {
            --token_end;
        }

        token_length = (size_t)(token_end - token_start);
        if (token_length == id_length && _strnicmp(token_start, id, id_length) == 0) {
            return 1;
        }

        cursor = token_end;
        if (*cursor) {
            ++cursor;
        }
    }

    return 0;
}

char* kain_env_dup(const char* name) {
    char* value = NULL;
    if (!name || !name[0]) return NULL;
#ifdef _WIN32
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || !value || !value[0]) {
        free(value);
        return NULL;
    }
    return value;
#else
    const char* source = getenv(name);
    size_t length;
    if (!source || !source[0]) return NULL;
    length = strlen(source);
    value = (char*)malloc(length + 1);
    if (!value) return NULL;
    memcpy(value, source, length + 1);
    return value;
#endif
}

void kain_env_free(char* value) {
    if (value) {
        free(value);
    }
}

int kain_env_set_string(const char* name, const char* value) {
    if (!name || !name[0]) {
        return 0;
    }
#ifdef _WIN32
    return SetEnvironmentVariableA(name, value ? value : "") ? 1 : 0;
#else
    return setenv(name, value ? value : "", 1) == 0 ? 1 : 0;
#endif
}

int kain_env_set_int(const char* name, long long value) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%lld", value);
    return kain_env_set_string(name, buffer);
}

int kain_env_set_double(const char* name, double value) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.6f", value);
    return kain_env_set_string(name, buffer);
}

int kain_env_set_flag(const char* name, int value) {
    return kain_env_set_string(name, value ? "1" : "0");
}

int kain_win32_get_executable_path(char* out_path, size_t out_cap) {
    DWORD length;
    if (!out_path || out_cap == 0) {
        return 0;
    }
    out_path[0] = '\0';
    length = GetModuleFileNameA(NULL, out_path, (DWORD)out_cap);
    if (length == 0 || length >= (DWORD)out_cap) {
        out_path[0] = '\0';
        return 0;
    }
    return 1;
}

int kain_win32_get_executable_sidecar_path(const char* suffix, char* out_path, size_t out_cap) {
    char* last_dot;
    char* last_backslash;
    char* last_slash;
    char* last_sep;
    size_t base_len;
    size_t suffix_len;

    if (!kain_win32_get_executable_path(out_path, out_cap)) {
        return 0;
    }
    if (!suffix || !suffix[0]) {
        return 1;
    }

    last_backslash = strrchr(out_path, '\\');
    last_slash = strrchr(out_path, '/');
    last_sep = last_backslash;
    if (!last_sep || (last_slash && last_slash > last_sep)) {
        last_sep = last_slash;
    }

    last_dot = strrchr(out_path, '.');
    if (last_dot && (!last_sep || last_dot > last_sep)) {
        *last_dot = '\0';
    }

    base_len = strlen(out_path);
    suffix_len = strlen(suffix);
    if (base_len + suffix_len + 1 > out_cap) {
        out_path[0] = '\0';
        return 0;
    }

    memcpy(out_path + base_len, suffix, suffix_len + 1);
    return 1;
}

int kain_env_flag(const char* name, int fallback) {
    char* value = kain_env_dup(name);
    int result = fallback;
    if (!value || !value[0]) return fallback;
    if (_stricmp(value, "1") == 0 || _stricmp(value, "true") == 0 || _stricmp(value, "yes") == 0 || _stricmp(value, "on") == 0) {
        result = 1;
    } else if (_stricmp(value, "0") == 0 || _stricmp(value, "false") == 0 || _stricmp(value, "no") == 0 || _stricmp(value, "off") == 0) {
        result = 0;
    }
    kain_env_free(value);
    return result;
}

int kain_env_int(const char* name, int fallback) {
    char* value = kain_env_dup(name);
    int result = fallback;
    if (!value || !value[0]) return fallback;
    result = atoi(value);
    kain_env_free(value);
    return result;
}

double kain_env_double(const char* name, double fallback) {
    char* value = kain_env_dup(name);
    double result = fallback;
    if (!value || !value[0]) return fallback;
    result = atof(value);
    kain_env_free(value);
    return result;
}

void native_config_string(char* key, char* value) {
    kain_env_set_string(key, value);
}

void native_config_int(char* key, long long value) {
    kain_env_set_int(key, value);
}

void native_config_float(char* key, double value) {
    kain_env_set_double(key, value);
}

void native_config_flag(char* key, long long enabled) {
    kain_env_set_flag(key, enabled != 0);
}

const KainViewportProfile* kain_find_viewport_profile(const char* id) {
    size_t index;
    if (!id || !id[0]) {
        return &g_kain_viewport_profiles[0];
    }
    for (index = 0; index < g_kain_viewport_profile_count; ++index) {
        if (_stricmp(g_kain_viewport_profiles[index].id, id) == 0) {
            return &g_kain_viewport_profiles[index];
        }
    }
    for (index = 0; index < g_kain_viewport_profile_count; ++index) {
        if (kain_viewport_profile_matches_alias(g_kain_viewport_profiles[index].scene_aliases, id)) {
            return &g_kain_viewport_profiles[index];
        }
    }
    return &g_kain_viewport_profiles[0];
}

KainVec3 kain_vec3_make(double x, double y, double z) {
    KainVec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

KainVec3 kain_vec3_add(KainVec3 a, KainVec3 b) {
    return kain_vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

KainVec3 kain_vec3_sub(KainVec3 a, KainVec3 b) {
    return kain_vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

KainVec3 kain_vec3_scale(KainVec3 v, double scale) {
    return kain_vec3_make(v.x * scale, v.y * scale, v.z * scale);
}

double kain_vec3_dot(KainVec3 a, KainVec3 b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

KainVec3 kain_vec3_cross(KainVec3 a, KainVec3 b) {
    return kain_vec3_make(
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x)
    );
}

KainVec3 kain_vec3_normalize(KainVec3 v) {
    double length = sqrt(kain_vec3_dot(v, v));
    if (length <= 0.000001) {
        return kain_vec3_make(0.0, 1.0, 0.0);
    }
    return kain_vec3_scale(v, 1.0 / length);
}
#endif
