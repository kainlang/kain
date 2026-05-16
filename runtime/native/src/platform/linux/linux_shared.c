#include "../../../include/win32.h"
#if defined(__linux__)
#include <unistd.h>

char* kain_env_dup(const char* name) {
    char* value = NULL;
    size_t length = 0u;

    if (!name || !name[0]) {
        return NULL;
    }
    if (_dupenv_s(&value, &length, name) != 0 || !value || !value[0]) {
        free(value);
        return NULL;
    }
    return value;
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
    return setenv(name, value ? value : "", 1) == 0 ? 1 : 0;
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
    ssize_t length;

    if (!out_path || out_cap == 0u) {
        return 0;
    }

    out_path[0] = '\0';
    length = readlink("/proc/self/exe", out_path, out_cap - 1u);
    if (length < 0 || (size_t)length >= out_cap) {
        out_path[0] = '\0';
        return 0;
    }

    out_path[length] = '\0';
    return 1;
}

int kain_win32_get_executable_sidecar_path(const char* suffix, char* out_path, size_t out_cap) {
    char* last_dot;
    char* last_slash;
    size_t base_length;
    size_t suffix_length;

    if (!kain_win32_get_executable_path(out_path, out_cap)) {
        return 0;
    }
    if (!suffix || !suffix[0]) {
        return 1;
    }

    last_slash = strrchr(out_path, '/');
    last_dot = strrchr(out_path, '.');
    if (last_dot && (!last_slash || last_dot > last_slash)) {
        *last_dot = '\0';
    }

    base_length = strlen(out_path);
    suffix_length = strlen(suffix);
    if (base_length + suffix_length + 1u > out_cap) {
        out_path[0] = '\0';
        return 0;
    }

    memcpy(out_path + base_length, suffix, suffix_length + 1u);
    return 1;
}

int kain_env_flag(const char* name, int fallback) {
    char* value = kain_env_dup(name);
    int result = fallback;

    if (!value || !value[0]) {
        kain_env_free(value);
        return fallback;
    }

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

    if (!value || !value[0]) {
        kain_env_free(value);
        return fallback;
    }

    result = atoi(value);
    kain_env_free(value);
    return result;
}

double kain_env_double(const char* name, double fallback) {
    char* value = kain_env_dup(name);
    double result = fallback;

    if (!value || !value[0]) {
        kain_env_free(value);
        return fallback;
    }

    result = atof(value);
    kain_env_free(value);
    return result;
}

KainVec3 kain_vec3_make(double x, double y, double z) {
    KainVec3 value;
    value.x = x;
    value.y = y;
    value.z = z;
    return value;
}

KainVec3 kain_vec3_add(KainVec3 a, KainVec3 b) {
    return kain_vec3_make(a.x + b.x, a.y + b.y, a.z + b.z);
}

KainVec3 kain_vec3_sub(KainVec3 a, KainVec3 b) {
    return kain_vec3_make(a.x - b.x, a.y - b.y, a.z - b.z);
}

KainVec3 kain_vec3_scale(KainVec3 value, double scale) {
    return kain_vec3_make(value.x * scale, value.y * scale, value.z * scale);
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

KainVec3 kain_vec3_normalize(KainVec3 value) {
    double length = sqrt(kain_vec3_dot(value, value));
    if (length <= 0.000001) {
        return kain_vec3_make(0.0, 1.0, 0.0);
    }
    return kain_vec3_scale(value, 1.0 / length);
}
#endif
