#ifndef KAIN_NATIVE_JSON_H
#define KAIN_NATIVE_JSON_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KAIN_JSON_KIND_UNKNOWN (-1)
#define KAIN_JSON_KIND_NULL 0
#define KAIN_JSON_KIND_BOOL 1
#define KAIN_JSON_KIND_INT 2
#define KAIN_JSON_KIND_FLOAT 3
#define KAIN_JSON_KIND_STRING 4
#define KAIN_JSON_KIND_OBJECT 5
#define KAIN_JSON_KIND_ARRAY 6
#define KAIN_JSON_KIND_OPAQUE 7

int64_t json_parse(const char* text);
char* json_string(int64_t value);
int64_t json_get(int64_t object, const char* key);
char* json_get_string(int64_t object, const char* key);
int64_t json_get_int(int64_t object, const char* key);
double json_get_float(int64_t object, const char* key);
bool json_get_bool(int64_t object, const char* key);
bool json_has(int64_t object, const char* key);
int64_t json_object_new(void);
void json_object_set(int64_t object, const char* key, int64_t value);
int64_t json_array_new(void);
void json_array_push(int64_t array, int64_t value);
int64_t json_array_len(int64_t array);
int64_t json_array_get(int64_t array, int64_t index);
int64_t json_box_runtime_array(void* array_ptr, int64_t mode);
int64_t json_box_float(double value);
int json_any_kind(int64_t value);
int64_t json_any_to_int(int64_t value);
double json_any_to_float(int64_t value);
char* json_any_to_string(int64_t value);
void json_retain(int64_t value);
void json_release(int64_t value);

#ifdef __cplusplus
}
#endif

#endif /* KAIN_NATIVE_JSON_H */
