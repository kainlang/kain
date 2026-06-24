// Stubs for standalone linking — only env helpers not provided by runtime C files.
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "component_surface.h"

int kain_env_flag(const char* name, int fallback) { (void)name; return fallback; }
int kain_env_int(const char* name, int fallback) { (void)name; return fallback; }
double kain_env_double(const char* name, double fallback) { (void)name; return fallback; }
char* kain_env_dup(const char* name) { (void)name; return NULL; }
void kain_env_free(char* value) { (void)value; }
