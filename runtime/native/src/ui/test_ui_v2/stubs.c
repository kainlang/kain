// Stubs for standalone test_ui_v2 linking.
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "component_surface.h"

char* string_new(char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* copy = (char*)malloc(len + 1);
    if (copy) memcpy(copy, src, len + 1);
    return copy;
}

double kain_clampd(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

int kain_env_flag(const char* name, int fallback) { (void)name; return fallback; }
int kain_env_int(const char* name, int fallback) { (void)name; return fallback; }
double kain_env_double(const char* name, double fallback) { (void)name; return fallback; }
char* kain_env_dup(const char* name) { (void)name; return NULL; }
void kain_env_free(char* value) { (void)value; }
