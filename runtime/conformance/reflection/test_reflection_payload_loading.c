/*
 * Reflection conformance smoke: happy-path payload loading.
 *
 * Validates that the native runtime can load compiler-shaped reflection payloads
 * from JSON, file paths, and environment variables, and then query the emitted
 * type/item tables.
 */

#include "../../native/include/kain_runtime_reflection.h"
#include "../../native/include/kain_runtime_diagnostics.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <stdlib.h>
#endif

static const char* FIXTURE_ENV = "KAIN_REFLECTION_FIXTURE_PATH";

static void build_fixture_path(char* out, size_t out_size) {
    const char* source = __FILE__;
    const char* last_slash = strrchr(source, '/');
#ifdef _WIN32
    const char* last_backslash = strrchr(source, '\\');
    if (!last_slash || (last_backslash && last_backslash > last_slash)) {
        last_slash = last_backslash;
    }
#endif

    assert(out != NULL);
    assert(out_size > 0);

    if (!last_slash) {
        snprintf(out, out_size, "fixtures/native_reflection_payload.json");
        return;
    }

    snprintf(
        out,
        out_size,
        "%.*s/fixtures/native_reflection_payload.json",
        (int)(last_slash - source),
        source
    );
}

static char* read_file(const char* path) {
    FILE* file = NULL;
    long size;
    char* buffer;

    if (!path) {
        return NULL;
    }

#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) {
        file = NULL;
    }
#else
    file = fopen(path, "rb");
#endif

    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

static void set_fixture_env(const char* value) {
#ifdef _WIN32
    _putenv_s(FIXTURE_ENV, value);
#else
    setenv(FIXTURE_ENV, value, 1);
#endif
}

static void dump_diag(const KainDiagnostic* diag) {
    char buffer[512];

    if (!diag) {
        fprintf(stderr, "  diagnostic: <null>\n");
        return;
    }

    buffer[0] = '\0';
    kain_diagnostic_format(diag, buffer, sizeof(buffer));
    fprintf(stderr, "  diagnostic: %s\n", buffer);
}

static void expect_lookup_and_format(const KainReflectionPayload* payload) {
    const KainTypeSchema* point_type = kain_reflection_lookup_type_by_name(payload, "Point");
    const KainItemMetadata* point_item = kain_reflection_lookup_item_by_name(payload, "Point");
    const KainTypeSchema* point_type_by_id = kain_reflection_lookup_type_by_id(payload, 1);
    const KainItemMetadata* point_item_by_id = kain_reflection_lookup_item_by_id(payload, 1);
    const KainItemMetadata* struct_items[4] = {0};
    char buffer[512];

    assert(point_type != NULL);
    assert(point_item != NULL);
    assert(point_type_by_id != NULL);
    assert(point_item_by_id != NULL);
    assert(kain_reflection_get_items_by_kind(payload, KAIN_ITEM_KIND_STRUCT, struct_items, 4) == 1);

    assert(strcmp(point_type->name, "Point") == 0);
    assert(point_type->type_id == 1);
    assert(point_type->kind == KAIN_TYPE_KIND_STRUCT);
    assert(strcmp(point_item->name, "Point") == 0);
    assert(point_item->item_id == 1);
    assert(point_item->kind == KAIN_ITEM_KIND_STRUCT);

    assert(kain_reflection_format_type_schema(point_type, buffer, sizeof(buffer)) > 0);
    assert(strstr(buffer, "Point") != NULL);
    assert(strstr(buffer, "fields=0") != NULL);

    assert(kain_reflection_format_item_metadata(point_item, buffer, sizeof(buffer)) > 0);
    assert(strstr(buffer, "Point") != NULL);
    assert(strstr(buffer, "module=\"app::geometry\"") != NULL);
}

int main(void) {
    KainReflectionPayload* payload = NULL;
    KainDiagnostic diag;
    unsigned int major = 0;
    unsigned int minor = 0;
    char fixture_path[512];
    char* json;
    int result = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    build_fixture_path(fixture_path, sizeof(fixture_path));
    json = read_file(fixture_path);

    printf("TEST: reflection payload load from JSON\n");
    assert(json != NULL);

    kain_diagnostic_init(&diag);
    result = kain_reflection_load_from_json(json, &payload, &diag);
    if (result != 0) {
        dump_diag(&diag);
    }
    assert(result == 0);
    assert(payload != NULL);
    kain_reflection_get_schema_version(payload, &major, &minor);
    assert(major == KAIN_REFLECTION_SCHEMA_VERSION_MAJOR);
    assert(minor == KAIN_REFLECTION_SCHEMA_VERSION_MINOR);
    assert(kain_reflection_check_schema_compatibility(payload) == 1);
    assert(kain_reflection_get_type_count(payload) == 1);
    assert(kain_reflection_get_item_count(payload) == 1);
    expect_lookup_and_format(payload);
    kain_reflection_free(payload);
    free(json);

    printf("TEST: reflection payload load from file path\n");
    kain_diagnostic_init(&diag);
    result = kain_reflection_load_from_path(fixture_path, &payload, &diag);
    if (result != 0) {
        dump_diag(&diag);
    }
    assert(result == 0);
    assert(payload != NULL);
    assert(kain_reflection_get_type_count(payload) == 1);
    assert(kain_reflection_get_item_count(payload) == 1);
    expect_lookup_and_format(payload);
    kain_reflection_free(payload);

    printf("TEST: reflection payload load from env\n");
    set_fixture_env(fixture_path);
    kain_diagnostic_init(&diag);
    result = kain_reflection_load_from_env(FIXTURE_ENV, &payload, &diag);
    if (result != 0) {
        dump_diag(&diag);
    }
    assert(result == 0);
    assert(payload != NULL);
    assert(kain_reflection_get_type_count(payload) == 1);
    assert(kain_reflection_get_item_count(payload) == 1);
    expect_lookup_and_format(payload);
    kain_reflection_free(payload);

    printf("PASS: reflection payload loading\n");
    return 0;
}
