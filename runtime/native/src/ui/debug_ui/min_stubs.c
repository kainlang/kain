// Minimal stubs for linking Kain LLVM IR against runtime C sources directly.
// Unlike stubs.c, this does NOT duplicate symbols provided by the runtime C files
// (string_new in core.c, kain_clampd in core.c, kain_component_surface_resolve in component_surface.c).
#include <stddef.h>
#include <stdlib.h>

int kain_env_flag(const char* name, int fallback) { (void)name; return fallback; }
int kain_env_int(const char* name, int fallback) { (void)name; return fallback; }
double kain_env_double(const char* name, double fallback) { (void)name; return fallback; }
char* kain_env_dup(const char* name) { (void)name; return NULL; }
void kain_env_free(char* value) { (void)value; }
