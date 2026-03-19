/*
 * KAIN Native Runtime Reflection Implementation
 *
 * Implements reflection payload loading, schema validation, and metadata
 * lookup for the native runtime. Provides APIs for loading compiler-emitted
 * reflection payloads and querying type schemas and item metadata.
 */

#include "kain_runtime_reflection.h"
#include "kain_runtime_diagnostics.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JSON parsing - minimal implementation for reflection payloads */
#define JSON_MAX_DEPTH 32
#define JSON_MAX_TOKENS 4096

typedef enum {
    JSON_TOKEN_OBJECT_START,
    JSON_TOKEN_OBJECT_END,
    JSON_TOKEN_ARRAY_START,
    JSON_TOKEN_ARRAY_END,
    JSON_TOKEN_STRING,
    JSON_TOKEN_NUMBER,
    JSON_TOKEN_TRUE,
    JSON_TOKEN_FALSE,
    JSON_TOKEN_NULL,
    JSON_TOKEN_COLON,
    JSON_TOKEN_COMMA,
    JSON_TOKEN_EOF,
} JsonTokenType;

typedef struct {
    JsonTokenType type;
    const char* start;
    size_t length;
} JsonToken;

typedef struct {
    const char* json;
    size_t pos;
    size_t length;
    JsonToken tokens[JSON_MAX_TOKENS];
    int token_count;
    int current_token;
} JsonParser;

/* Reflection Payload Structure */
struct KainReflectionPayload {
    unsigned int schema_major;
    unsigned int schema_minor;
    
    KainTypeSchema* types;
    int type_count;
    int type_capacity;
    
    KainItemMetadata* items;
    int item_count;
    int item_capacity;
    
    char* json_source;
};

/* Forward declarations */
static int json_parse(JsonParser* parser, const char* json);
static int parse_reflection_payload_json(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag);
static void json_skip_ws(JsonParser* parser);
static int json_match_literal(JsonParser* parser, const char* literal);
static int json_match_char(JsonParser* parser, char expected);
static int json_skip_string(JsonParser* parser);
static int json_parse_string(JsonParser* parser, char* out, size_t out_size);
static int json_parse_u64(JsonParser* parser, unsigned long long* out);
static int json_skip_value(JsonParser* parser);
static int json_skip_object(JsonParser* parser);
static int json_skip_array(JsonParser* parser);
static int json_parse_top_level(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag);
static int json_parse_type_array(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag);
static int json_parse_item_array(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag);
static int json_parse_named_object_array(JsonParser* parser, KainDiagnostic* diag, const char* array_name);
static int parse_type_object(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag);
static int parse_item_object(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag);
static KainTypeKind reflection_type_kind_from_string(const char* kind);
static KainItemKind reflection_item_kind_from_string(const char* kind);
static int reflection_ensure_type_capacity(KainReflectionPayload* payload, int min_capacity);
static int reflection_ensure_item_capacity(KainReflectionPayload* payload, int min_capacity);
static char* reflection_strdup(const char* text);
static void reflection_copy_text(char* out, size_t out_size, const char* text);
static FILE* reflection_open_file(const char* path, const char* mode);
static char* reflection_get_env_value(const char* env_name);
static void reflection_set_diag(
    KainDiagnostic* diag,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail
);

int kain_reflection_load_from_json(
    const char* json,
    KainReflectionPayload** payload,
    KainDiagnostic* diag
) {
    if (!json || !payload) {
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Invalid arguments to kain_reflection_load_from_json",
                NULL,
                NULL
            );
        }
        return -1;
    }

    *payload = (KainReflectionPayload*)calloc(1, sizeof(KainReflectionPayload));
    if (!*payload) {
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                "Failed to allocate reflection payload",
                NULL,
                NULL
            );
        }
        return -1;
    }

    (*payload)->json_source = reflection_strdup(json);
    if (!(*payload)->json_source) {
        free(*payload);
        *payload = NULL;
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                "Failed to copy JSON source",
                NULL,
                NULL
            );
        }
        return -1;
    }

    JsonParser parser = {0};
    if (json_parse(&parser, json) != 0) {
        free((*payload)->json_source);
        free(*payload);
        *payload = NULL;
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Failed to parse reflection JSON",
                NULL,
                NULL
            );
        }
        return -1;
    }

    if (parse_reflection_payload_json(&parser, *payload, diag) != 0) {
        kain_reflection_free(*payload);
        *payload = NULL;
        return -1;
    }

    return 0;
}

int kain_reflection_load_from_path(
    const char* path,
    KainReflectionPayload** payload,
    KainDiagnostic* diag
) {
    if (!path || !payload) {
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Invalid arguments to kain_reflection_load_from_path",
                NULL,
                NULL
            );
        }
        return -1;
    }

    FILE* file = reflection_open_file(path, "rb");
    if (!file) {
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_NOT_FOUND,
                "Failed to open reflection payload file",
                NULL,
                path
            );
        }
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
        fclose(file);
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Invalid reflection payload file size",
                NULL,
                path
            );
        }
        return -1;
    }

    char* json = (char*)malloc(file_size + 1);
    if (!json) {
        fclose(file);
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
                "Failed to allocate buffer for reflection payload",
                NULL,
                path
            );
        }
        return -1;
    }

    size_t read_size = fread(json, 1, file_size, file);
    fclose(file);

    if (read_size != (size_t)file_size) {
        free(json);
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Failed to read reflection payload file",
                NULL,
                path
            );
        }
        return -1;
    }

    json[file_size] = '\0';

    int result = kain_reflection_load_from_json(json, payload, diag);
    free(json);
    return result;
}

int kain_reflection_load_from_env(
    const char* env_name,
    KainReflectionPayload** payload,
    KainDiagnostic* diag
) {
    if (!env_name || !payload) {
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Invalid arguments to kain_reflection_load_from_env",
                NULL,
                NULL
            );
        }
        return -1;
    }

    char* env_value = reflection_get_env_value(env_name);
    const char* path = env_value;
    if (!path) {
        if (diag) {
            kain_diagnostic_init(diag);
            kain_diagnostic_create(
                diag,
                KAIN_DIAG_SUBSYSTEM_REFLECTION,
                KAIN_DIAG_SEVERITY_WARNING,
                KAIN_DIAG_CODE_REFLECTION_NOT_FOUND,
                "Reflection payload environment variable not set",
                env_name,
                NULL
            );
        }
        return -1;
    }

    {
        int result = kain_reflection_load_from_path(path, payload, diag);
        free(env_value);
        return result;
    }
}

void kain_reflection_free(KainReflectionPayload* payload) {
    if (!payload) {
        return;
    }

    if (payload->types) {
        free(payload->types);
    }

    if (payload->items) {
        free(payload->items);
    }

    if (payload->json_source) {
        free(payload->json_source);
    }

    free(payload);
}

void kain_reflection_get_schema_version(
    const KainReflectionPayload* payload,
    unsigned int* major,
    unsigned int* minor
) {
    if (!payload || !major || !minor) {
        if (major) *major = 0;
        if (minor) *minor = 0;
        return;
    }

    *major = payload->schema_major;
    *minor = payload->schema_minor;
}

int kain_reflection_check_schema_compatibility(
    const KainReflectionPayload* payload
) {
    if (!payload) {
        return 0;
    }

    /* Check major version compatibility */
    if (payload->schema_major != KAIN_REFLECTION_SCHEMA_VERSION_MAJOR) {
        return 0;
    }

    /* Minor version differences are compatible */
    return 1;
}

const KainTypeSchema* kain_reflection_lookup_type_by_id(
    const KainReflectionPayload* payload,
    unsigned long long type_id
) {
    if (!payload || !payload->types) {
        return NULL;
    }

    for (int i = 0; i < payload->type_count; i++) {
        if (payload->types[i].type_id == type_id) {
            return &payload->types[i];
        }
    }

    return NULL;
}

const KainTypeSchema* kain_reflection_lookup_type_by_name(
    const KainReflectionPayload* payload,
    const char* name
) {
    if (!payload || !payload->types || !name) {
        return NULL;
    }

    for (int i = 0; i < payload->type_count; i++) {
        if (strcmp(payload->types[i].name, name) == 0) {
            return &payload->types[i];
        }
    }

    return NULL;
}

const KainItemMetadata* kain_reflection_lookup_item_by_id(
    const KainReflectionPayload* payload,
    unsigned long long item_id
) {
    if (!payload || !payload->items) {
        return NULL;
    }

    for (int i = 0; i < payload->item_count; i++) {
        if (payload->items[i].item_id == item_id) {
            return &payload->items[i];
        }
    }

    return NULL;
}

const KainItemMetadata* kain_reflection_lookup_item_by_name(
    const KainReflectionPayload* payload,
    const char* name
) {
    if (!payload || !payload->items || !name) {
        return NULL;
    }

    for (int i = 0; i < payload->item_count; i++) {
        if (strcmp(payload->items[i].name, name) == 0) {
            return &payload->items[i];
        }
    }

    return NULL;
}

int kain_reflection_get_type_count(const KainReflectionPayload* payload) {
    return payload ? payload->type_count : 0;
}

int kain_reflection_get_item_count(const KainReflectionPayload* payload) {
    return payload ? payload->item_count : 0;
}

int kain_reflection_get_items_by_kind(
    const KainReflectionPayload* payload,
    KainItemKind kind,
    const KainItemMetadata** items,
    int max_items
) {
    if (!payload || !payload->items) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < payload->item_count && count < max_items; i++) {
        if (payload->items[i].kind == kind) {
            if (items) {
                items[count] = &payload->items[i];
            }
            count++;
        }
    }

    return count;
}

int kain_reflection_format_type_schema(
    const KainTypeSchema* schema,
    char* out,
    size_t out_size
) {
    if (!schema || !out || out_size == 0) {
        return 0;
    }

    return snprintf(out, out_size,
        "Type{id=%llu, kind=%d, name=\"%s\", size=%zu, align=%zu, fields=%d}",
        schema->type_id,
        schema->kind,
        schema->name,
        schema->size_bytes,
        schema->align_bytes,
        schema->field_count
    );
}

int kain_reflection_format_item_metadata(
    const KainItemMetadata* metadata,
    char* out,
    size_t out_size
) {
    if (!metadata || !out || out_size == 0) {
        return 0;
    }

    return snprintf(out, out_size,
        "Item{id=%llu, kind=%d, name=\"%s\", module=\"%s\", type_id=%llu}",
        metadata->item_id,
        metadata->kind,
        metadata->name,
        metadata->module_path,
        metadata->type_id
    );
}

void kain_reflection_print_summary(const KainReflectionPayload* payload) {
    if (!payload) {
        printf("Reflection payload: NULL\n");
        return;
    }

    printf("Reflection Payload Summary:\n");
    printf("  Schema version: %u.%u\n", payload->schema_major, payload->schema_minor);
    printf("  Types: %d\n", payload->type_count);
    printf("  Items: %d\n", payload->item_count);

    if (payload->type_count > 0) {
        printf("\n  Types:\n");
        for (int i = 0; i < payload->type_count && i < 10; i++) {
            printf("    - %s (id=%llu, kind=%d)\n",
                payload->types[i].name,
                payload->types[i].type_id,
                payload->types[i].kind
            );
        }
        if (payload->type_count > 10) {
            printf("    ... and %d more\n", payload->type_count - 10);
        }
    }

    if (payload->item_count > 0) {
        printf("\n  Items:\n");
        for (int i = 0; i < payload->item_count && i < 10; i++) {
            printf("    - %s (id=%llu, kind=%d)\n",
                payload->items[i].name,
                payload->items[i].item_id,
                payload->items[i].kind
            );
        }
        if (payload->item_count > 10) {
            printf("    ... and %d more\n", payload->item_count - 10);
        }
    }
}

/* Minimal JSON parser implementation */
static int json_parse(JsonParser* parser, const char* json) {
    parser->json = json;
    parser->length = strlen(json);
    parser->pos = 0;
    parser->token_count = 0;
    parser->current_token = 0;

    if (!json) {
        return -1;
    }

    json_skip_ws(parser);
    if (parser->length >= 3 &&
        (unsigned char)parser->json[0] == 0xEF &&
        (unsigned char)parser->json[1] == 0xBB &&
        (unsigned char)parser->json[2] == 0xBF) {
        parser->pos = 3;
        json_skip_ws(parser);
    }

    if (parser->pos >= parser->length || parser->json[parser->pos] != '{') {
        return -1;
    }

    return 0;
}

static int parse_reflection_payload_json(
    JsonParser* parser,
    KainReflectionPayload* payload,
    KainDiagnostic* diag
) {
    if (!parser || !payload) {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
            "Invalid reflection parser state",
            NULL
        );
        return -1;
    }

    payload->schema_major = 0;
    payload->schema_minor = 0;
    payload->type_count = 0;
    payload->type_capacity = 0;
    payload->types = NULL;
    payload->item_count = 0;
    payload->item_capacity = 0;
    payload->items = NULL;

    if (json_parse_top_level(parser, payload, diag) != 0) {
        return -1;
    }

    if (payload->schema_minor == 0) {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
            "Reflection payload is missing schema_version",
            NULL
        );
        return -1;
    }

    payload->schema_major = KAIN_REFLECTION_SCHEMA_VERSION_MAJOR;
    return 0;
}

static void json_skip_ws(JsonParser* parser) {
    if (!parser || !parser->json) {
        return;
    }

    while (parser->pos < parser->length) {
        char ch = parser->json[parser->pos];
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
            break;
        }
        parser->pos++;
    }
}

static int json_match_literal(JsonParser* parser, const char* literal) {
    size_t length;

    if (!parser || !literal) {
        return 0;
    }

    json_skip_ws(parser);
    length = strlen(literal);
    if (parser->pos + length > parser->length) {
        return 0;
    }
    if (strncmp(parser->json + parser->pos, literal, length) != 0) {
        return 0;
    }

    parser->pos += length;
    return 1;
}

static int json_match_char(JsonParser* parser, char expected) {
    json_skip_ws(parser);
    if (!parser || parser->pos >= parser->length || parser->json[parser->pos] != expected) {
        return 0;
    }
    parser->pos++;
    return 1;
}

static int json_skip_string(JsonParser* parser) {
    char scratch[4];
    return json_parse_string(parser, scratch, sizeof(scratch));
}

static int json_parse_string(JsonParser* parser, char* out, size_t out_size) {
    size_t out_pos = 0;

    if (!parser || parser->pos >= parser->length || parser->json[parser->pos] != '"') {
        json_skip_ws(parser);
        if (!parser || parser->pos >= parser->length || parser->json[parser->pos] != '"') {
            return 0;
        }
    }

    parser->pos++;
    if (out && out_size > 0) {
        out[0] = '\0';
    }

    while (parser->pos < parser->length) {
        char ch = parser->json[parser->pos++];

        if (ch == '"') {
            if (out && out_size > 0) {
                out[out_pos < out_size ? out_pos : out_size - 1] = '\0';
            }
            return 1;
        }

        if (ch == '\\') {
            if (parser->pos >= parser->length) {
                return 0;
            }

            ch = parser->json[parser->pos++];
            switch (ch) {
                case '"': break;
                case '\\': break;
                case '/': break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                case 'u':
                    if (parser->pos + 4 > parser->length) {
                        return 0;
                    }
                    parser->pos += 4;
                    ch = '?';
                    break;
                default:
                    return 0;
            }
        }

        if (out && out_size > 0 && out_pos + 1 < out_size) {
            out[out_pos++] = ch;
        } else if (out && out_size > 0) {
            out_pos++;
        }
    }

    return 0;
}

static int json_parse_u64(JsonParser* parser, unsigned long long* out) {
    char* end = NULL;
    unsigned long long value;

    if (!parser || !out) {
        return 0;
    }

    json_skip_ws(parser);
    if (parser->pos >= parser->length) {
        return 0;
    }

    errno = 0;
    value = strtoull(parser->json + parser->pos, &end, 10);
    if (end == parser->json + parser->pos || errno != 0) {
        return 0;
    }

    parser->pos = (size_t)(end - parser->json);
    *out = value;
    return 1;
}

static int json_skip_value(JsonParser* parser) {
    json_skip_ws(parser);
    if (!parser || parser->pos >= parser->length) {
        return 0;
    }

    switch (parser->json[parser->pos]) {
        case '{':
            return json_skip_object(parser);
        case '[':
            return json_skip_array(parser);
        case '"':
            return json_skip_string(parser);
        case 't':
            return json_match_literal(parser, "true");
        case 'f':
            return json_match_literal(parser, "false");
        case 'n':
            return json_match_literal(parser, "null");
        default:
            return json_parse_u64(parser, &(unsigned long long){0});
    }
}

static int json_skip_object(JsonParser* parser) {
    if (!json_match_char(parser, '{')) {
        return 0;
    }

    json_skip_ws(parser);
    if (json_match_char(parser, '}')) {
        return 1;
    }

    while (parser->pos < parser->length) {
        json_skip_ws(parser);
        if (!json_skip_string(parser)) {
            return 0;
        }
        if (!json_match_char(parser, ':')) {
            return 0;
        }
        if (!json_skip_value(parser)) {
            return 0;
        }
        json_skip_ws(parser);
        if (json_match_char(parser, ',')) {
            continue;
        }
        return json_match_char(parser, '}');
    }

    return 0;
}

static int json_skip_array(JsonParser* parser) {
    if (!json_match_char(parser, '[')) {
        return 0;
    }

    json_skip_ws(parser);
    if (json_match_char(parser, ']')) {
        return 1;
    }

    while (parser->pos < parser->length) {
        if (!json_skip_value(parser)) {
            return 0;
        }
        json_skip_ws(parser);
        if (json_match_char(parser, ',')) {
            continue;
        }
        return json_match_char(parser, ']');
    }

    return 0;
}

static int reflection_ensure_type_capacity(KainReflectionPayload* payload, int min_capacity) {
    int new_capacity;
    KainTypeSchema* types;

    if (payload->type_capacity >= min_capacity) {
        return 0;
    }

    new_capacity = payload->type_capacity > 0 ? payload->type_capacity : 4;
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }

    types = (KainTypeSchema*)realloc(payload->types, (size_t)new_capacity * sizeof(KainTypeSchema));
    if (!types) {
        return -1;
    }

    payload->types = types;
    payload->type_capacity = new_capacity;
    return 0;
}

static int reflection_ensure_item_capacity(KainReflectionPayload* payload, int min_capacity) {
    int new_capacity;
    KainItemMetadata* items;

    if (payload->item_capacity >= min_capacity) {
        return 0;
    }

    new_capacity = payload->item_capacity > 0 ? payload->item_capacity : 4;
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }

    items = (KainItemMetadata*)realloc(payload->items, (size_t)new_capacity * sizeof(KainItemMetadata));
    if (!items) {
        return -1;
    }

    payload->items = items;
    payload->item_capacity = new_capacity;
    return 0;
}

static KainTypeKind reflection_type_kind_from_string(const char* kind) {
    if (!kind) {
        return KAIN_TYPE_KIND_UNKNOWN;
    }
    if (strcmp(kind, "primitive") == 0) return KAIN_TYPE_KIND_PRIMITIVE;
    if (strcmp(kind, "struct") == 0) return KAIN_TYPE_KIND_STRUCT;
    if (strcmp(kind, "enum") == 0) return KAIN_TYPE_KIND_ENUM;
    if (strcmp(kind, "array") == 0) return KAIN_TYPE_KIND_ARRAY;
    if (strcmp(kind, "pointer") == 0) return KAIN_TYPE_KIND_POINTER;
    if (strcmp(kind, "function") == 0) return KAIN_TYPE_KIND_FUNCTION;
    if (strcmp(kind, "actor") == 0) return KAIN_TYPE_KIND_ACTOR;
    if (strcmp(kind, "message") == 0) return KAIN_TYPE_KIND_MESSAGE;
    return KAIN_TYPE_KIND_UNKNOWN;
}

static KainItemKind reflection_item_kind_from_string(const char* kind) {
    if (!kind) {
        return KAIN_ITEM_KIND_UNKNOWN;
    }
    if (strcmp(kind, "function") == 0) return KAIN_ITEM_KIND_FUNCTION;
    if (strcmp(kind, "struct") == 0) return KAIN_ITEM_KIND_STRUCT;
    if (strcmp(kind, "enum") == 0) return KAIN_ITEM_KIND_ENUM;
    if (strcmp(kind, "actor") == 0) return KAIN_ITEM_KIND_ACTOR;
    if (strcmp(kind, "component") == 0) return KAIN_ITEM_KIND_COMPONENT;
    if (strcmp(kind, "message") == 0) return KAIN_ITEM_KIND_MESSAGE;
    if (strcmp(kind, "service") == 0) return KAIN_ITEM_KIND_SERVICE;
    if (strcmp(kind, "module") == 0) return KAIN_ITEM_KIND_MODULE;
    return KAIN_ITEM_KIND_UNKNOWN;
}

static int json_parse_named_object_array(JsonParser* parser, KainDiagnostic* diag, const char* array_name) {
    char key[KAIN_REFLECTION_NAME_MAX];

    if (!json_match_char(parser, '[')) {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
            "Expected reflection array",
            array_name
        );
        return -1;
    }

    json_skip_ws(parser);
    if (json_match_char(parser, ']')) {
        return 0;
    }

    while (parser->pos < parser->length) {
        json_skip_ws(parser);
        if (!json_match_char(parser, '{')) {
            reflection_set_diag(
                diag,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Expected reflection object",
                array_name
            );
            return -1;
        }

        int saw_name = 0;
        int saw_item_id = 0;
        while (parser->pos < parser->length) {
            json_skip_ws(parser);
            char field_name[KAIN_REFLECTION_NAME_MAX];
            if (!json_parse_string(parser, field_name, sizeof(field_name))) {
                reflection_set_diag(
                    diag,
                    KAIN_DIAG_SEVERITY_ERROR,
                    KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                    "Failed to parse reflection object field name",
                    array_name
                );
                return -1;
            }
            if (!json_match_char(parser, ':')) {
                reflection_set_diag(
                    diag,
                    KAIN_DIAG_SEVERITY_ERROR,
                    KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                    "Failed to parse reflection object field separator",
                    array_name
                );
                return -1;
            }
            if (strcmp(field_name, "name") == 0) {
                if (!json_parse_string(parser, key, sizeof(key))) {
                    reflection_set_diag(
                        diag,
                        KAIN_DIAG_SEVERITY_ERROR,
                        KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                        "Failed to parse reflection object name",
                        array_name
                    );
                    return -1;
                }
                saw_name = 1;
            } else if (strcmp(field_name, "item_id") == 0) {
                unsigned long long dummy = 0;
                if (!json_parse_u64(parser, &dummy)) {
                    reflection_set_diag(
                        diag,
                        KAIN_DIAG_SEVERITY_ERROR,
                        KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                        "Failed to parse reflection object item_id",
                        array_name
                    );
                    return -1;
                }
                saw_item_id = 1;
            } else {
                if (!json_skip_value(parser)) {
                    reflection_set_diag(
                        diag,
                        KAIN_DIAG_SEVERITY_ERROR,
                        KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                        "Failed to skip reflection object value",
                        array_name
                    );
                    return -1;
                }
            }

            json_skip_ws(parser);
            if (json_match_char(parser, ',')) {
                continue;
            }
            if (json_match_char(parser, '}')) {
                break;
            }
            reflection_set_diag(
                diag,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Malformed reflection object",
                array_name
            );
            return -1;
        }

        if (!saw_name || !saw_item_id) {
            reflection_set_diag(
                diag,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
                "Reflection object missing required metadata",
                array_name
            );
            return -1;
        }

        json_skip_ws(parser);
        if (json_match_char(parser, ',')) {
            continue;
        }
        if (json_match_char(parser, ']')) {
            return 0;
        }
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
            "Malformed reflection array",
            array_name
        );
        return -1;
    }

    return 0;
}

static int parse_type_object(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag) {
    char field_name[KAIN_REFLECTION_NAME_MAX];
    char name[KAIN_REFLECTION_NAME_MAX] = {0};
    char kind[KAIN_REFLECTION_NAME_MAX] = {0};
    unsigned long long type_id = 0;
    unsigned long long size_hint = 0;
    int has_type_id = 0;
    int has_size_hint = 0;
    int field_count = 0;
#define REFLECTION_TYPE_FAIL(message) do { \
    reflection_set_diag( \
        diag, \
        KAIN_DIAG_SEVERITY_ERROR, \
        KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED, \
        message, \
        name[0] ? name : NULL \
    ); \
    return -1; \
} while (0)

    if (!json_match_char(parser, '{')) {
        REFLECTION_TYPE_FAIL("Expected reflection type object");
    }

    while (parser->pos < parser->length) {
        json_skip_ws(parser);
        if (!json_parse_string(parser, field_name, sizeof(field_name))) {
            REFLECTION_TYPE_FAIL("Failed to parse reflection type field name");
        }
        if (!json_match_char(parser, ':')) {
            REFLECTION_TYPE_FAIL("Failed to parse reflection type field separator");
        }

        if (strcmp(field_name, "type_id") == 0) {
            if (!json_parse_u64(parser, &type_id)) {
                REFLECTION_TYPE_FAIL("Failed to parse reflection type type_id");
            }
            has_type_id = 1;
        } else if (strcmp(field_name, "name") == 0) {
            if (!json_parse_string(parser, name, sizeof(name))) {
                REFLECTION_TYPE_FAIL("Failed to parse reflection type name");
            }
        } else if (strcmp(field_name, "kind") == 0) {
            if (!json_parse_string(parser, kind, sizeof(kind))) {
                REFLECTION_TYPE_FAIL("Failed to parse reflection type kind");
            }
        } else if (strcmp(field_name, "size_hint") == 0) {
            if (json_match_literal(parser, "null")) {
                has_size_hint = 0;
            } else if (json_parse_u64(parser, &size_hint)) {
                has_size_hint = 1;
            } else {
                REFLECTION_TYPE_FAIL("Failed to parse reflection type size_hint");
            }
        } else if (strcmp(field_name, "fields") == 0) {
            if (!json_match_char(parser, '[')) {
                REFLECTION_TYPE_FAIL("Failed to parse reflection type fields array");
            }
            json_skip_ws(parser);
            if (json_match_char(parser, ']')) {
                field_count = 0;
            } else {
                while (parser->pos < parser->length) {
                    if (!json_skip_value(parser)) {
                        REFLECTION_TYPE_FAIL("Failed to parse reflection type field value");
                    }
                    field_count++;
                    json_skip_ws(parser);
                    if (json_match_char(parser, ',')) {
                        continue;
                    }
                    if (json_match_char(parser, ']')) {
                        break;
                    }
                    REFLECTION_TYPE_FAIL("Malformed reflection type fields array");
                }
            }
        } else {
            if (!json_skip_value(parser)) {
                REFLECTION_TYPE_FAIL("Failed to skip reflection type value");
            }
        }

        json_skip_ws(parser);
        if (json_match_char(parser, ',')) {
            continue;
        }
        if (json_match_char(parser, '}')) {
            break;
        }
        REFLECTION_TYPE_FAIL("Malformed reflection type object");
    }

    if (!has_type_id || name[0] == '\0') {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
            "Reflection type is missing required fields",
            name[0] ? name : NULL
        );
        return -1;
    }

    if (reflection_ensure_type_capacity(payload, payload->type_count + 1) != 0) {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Failed to grow reflection type table",
            NULL
        );
        return -1;
    }

    KainTypeSchema* schema = &payload->types[payload->type_count++];
    ZeroMemory(schema, sizeof(*schema));
    schema->type_id = type_id;
    schema->kind = reflection_type_kind_from_string(kind);
    reflection_copy_text(schema->name, sizeof(schema->name), name);
    schema->size_bytes = has_size_hint ? (size_t)size_hint : 0u;
    schema->align_bytes = 0u;
    schema->field_count = field_count;
    schema->fields = NULL;
#undef REFLECTION_TYPE_FAIL
    return 0;
}

static int parse_item_object(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag) {
    char field_name[KAIN_REFLECTION_NAME_MAX];
    char name[KAIN_REFLECTION_NAME_MAX] = {0};
    char kind[KAIN_REFLECTION_NAME_MAX] = {0};
    char module_path[KAIN_REFLECTION_PATH_MAX] = {0};
    unsigned long long item_id = 0;
    unsigned long long type_id = 0;
    int has_item_id = 0;
    int has_type_id = 0;
#define REFLECTION_ITEM_FAIL(message) do { \
    reflection_set_diag( \
        diag, \
        KAIN_DIAG_SEVERITY_ERROR, \
        KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED, \
        message, \
        name[0] ? name : NULL \
    ); \
    return -1; \
} while (0)

    if (!json_match_char(parser, '{')) {
        REFLECTION_ITEM_FAIL("Expected reflection item object");
    }

    while (parser->pos < parser->length) {
        json_skip_ws(parser);
        if (!json_parse_string(parser, field_name, sizeof(field_name))) {
            REFLECTION_ITEM_FAIL("Failed to parse reflection item field name");
        }
        if (!json_match_char(parser, ':')) {
            REFLECTION_ITEM_FAIL("Failed to parse reflection item field separator");
        }

        if (strcmp(field_name, "item_id") == 0) {
            if (!json_parse_u64(parser, &item_id)) {
                REFLECTION_ITEM_FAIL("Failed to parse reflection item item_id");
            }
            has_item_id = 1;
        } else if (strcmp(field_name, "name") == 0) {
            if (!json_parse_string(parser, name, sizeof(name))) {
                REFLECTION_ITEM_FAIL("Failed to parse reflection item name");
            }
        } else if (strcmp(field_name, "kind") == 0) {
            if (!json_parse_string(parser, kind, sizeof(kind))) {
                REFLECTION_ITEM_FAIL("Failed to parse reflection item kind");
            }
        } else if (strcmp(field_name, "module_path") == 0) {
            if (!json_parse_string(parser, module_path, sizeof(module_path))) {
                REFLECTION_ITEM_FAIL("Failed to parse reflection item module_path");
            }
        } else if (strcmp(field_name, "type_id") == 0) {
            if (json_match_literal(parser, "null")) {
                has_type_id = 0;
            } else if (json_parse_u64(parser, &type_id)) {
                has_type_id = 1;
            } else {
                REFLECTION_ITEM_FAIL("Failed to parse reflection item type_id");
            }
        } else {
            if (!json_skip_value(parser)) {
                REFLECTION_ITEM_FAIL("Failed to skip reflection item value");
            }
        }

        json_skip_ws(parser);
        if (json_match_char(parser, ',')) {
            continue;
        }
        if (json_match_char(parser, '}')) {
            break;
        }
        REFLECTION_ITEM_FAIL("Malformed reflection item object");
    }

    if (!has_item_id || name[0] == '\0') {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
            "Reflection item is missing required fields",
            name[0] ? name : NULL
        );
        return -1;
    }

    if (reflection_ensure_item_capacity(payload, payload->item_count + 1) != 0) {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_MEMORY_ALLOC_FAILED,
            "Failed to grow reflection item table",
            NULL
        );
        return -1;
    }

    KainItemMetadata* item = &payload->items[payload->item_count++];
    ZeroMemory(item, sizeof(*item));
    item->item_id = item_id;
    item->kind = reflection_item_kind_from_string(kind);
    reflection_copy_text(item->name, sizeof(item->name), name);
    reflection_copy_text(item->module_path, sizeof(item->module_path), module_path);
    if (has_type_id) {
        item->type_id = type_id;
    }
#undef REFLECTION_ITEM_FAIL
    return 0;
}

static int json_parse_type_array(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag) {
    if (!json_match_char(parser, '[')) {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
            "Expected types array",
            NULL
        );
        return -1;
    }

    json_skip_ws(parser);
    if (json_match_char(parser, ']')) {
        return 0;
    }

    while (parser->pos < parser->length) {
        if (parse_type_object(parser, payload, diag) != 0) {
            return -1;
        }
        json_skip_ws(parser);
        if (json_match_char(parser, ',')) {
            continue;
        }
        if (json_match_char(parser, ']')) {
            return 0;
        }
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
            "Malformed types array",
            NULL
        );
        return -1;
    }

    return 0;
}

static int json_parse_item_array(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag) {
    if (!json_match_char(parser, '[')) {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
            "Expected items array",
            NULL
        );
        return -1;
    }

    json_skip_ws(parser);
    if (json_match_char(parser, ']')) {
        return 0;
    }

    while (parser->pos < parser->length) {
        if (parse_item_object(parser, payload, diag) != 0) {
            return -1;
        }
        json_skip_ws(parser);
        if (json_match_char(parser, ',')) {
            continue;
        }
        if (json_match_char(parser, ']')) {
            return 0;
        }
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
            "Malformed items array",
            NULL
        );
        return -1;
    }

    return 0;
}

static int json_parse_top_level(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag) {
    char key[KAIN_REFLECTION_NAME_MAX];
    unsigned long long schema_version = 0;
    int saw_schema_version = 0;

    if (!json_match_char(parser, '{')) {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
            "Expected top-level reflection object",
            NULL
        );
        return -1;
    }

    while (parser->pos < parser->length) {
        if (json_match_char(parser, '}')) {
            break;
        }

        if (!json_parse_string(parser, key, sizeof(key))) {
            reflection_set_diag(
                diag,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Failed to parse reflection payload key",
                NULL
            );
            return -1;
        }
        if (!json_match_char(parser, ':')) {
            reflection_set_diag(
                diag,
                KAIN_DIAG_SEVERITY_ERROR,
                KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                "Failed to parse reflection payload separator",
                key
            );
            return -1;
        }

        if (strcmp(key, "schema_version") == 0) {
            if (!json_parse_u64(parser, &schema_version)) {
                reflection_set_diag(
                    diag,
                    KAIN_DIAG_SEVERITY_ERROR,
                    KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
                    "Reflection payload schema_version is invalid",
                    NULL
                );
                return -1;
            }
            saw_schema_version = 1;
        } else if (strcmp(key, "types") == 0) {
            if (json_parse_type_array(parser, payload, diag) != 0) {
                return -1;
            }
        } else if (strcmp(key, "items") == 0) {
            if (json_parse_item_array(parser, payload, diag) != 0) {
                return -1;
            }
        } else if (strcmp(key, "actors") == 0 || strcmp(key, "components") == 0 || strcmp(key, "messages") == 0) {
            if (json_parse_named_object_array(parser, diag, key) != 0) {
                return -1;
            }
        } else {
            if (!json_skip_value(parser)) {
                reflection_set_diag(
                    diag,
                    KAIN_DIAG_SEVERITY_ERROR,
                    KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
                    "Failed to skip unknown reflection payload field",
                    key
                );
                return -1;
            }
        }

        json_skip_ws(parser);
        if (json_match_char(parser, ',')) {
            continue;
        }
        if (json_match_char(parser, '}')) {
            break;
        }
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_PARSE_FAILED,
            "Malformed top-level reflection payload",
            key
        );
        return -1;
    }

    if (!saw_schema_version) {
        reflection_set_diag(
            diag,
            KAIN_DIAG_SEVERITY_ERROR,
            KAIN_DIAG_CODE_REFLECTION_INVALID_SCHEMA,
            "Reflection payload is missing schema_version",
            NULL
        );
        return -1;
    }

    payload->schema_major = KAIN_REFLECTION_SCHEMA_VERSION_MAJOR;
    payload->schema_minor = (unsigned int)schema_version;
    return 0;
}

static char* reflection_strdup(const char* text) {
    size_t length;
    char* copy;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char*)malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void reflection_copy_text(char* out, size_t out_size, const char* text) {
    size_t length;

    if (!out || out_size == 0) {
        return;
    }

    if (!text) {
        out[0] = '\0';
        return;
    }

    length = strlen(text);
    if (length >= out_size) {
        length = out_size - 1;
    }

    memcpy(out, text, length);
    out[length] = '\0';
}

static FILE* reflection_open_file(const char* path, const char* mode) {
#ifdef _WIN32
    FILE* file = NULL;
    if (fopen_s(&file, path, mode) != 0) {
        return NULL;
    }
    return file;
#else
    return fopen(path, mode);
#endif
}

static char* reflection_get_env_value(const char* env_name) {
#ifdef _WIN32
    size_t length = 0;
    char* value = NULL;
    if (!env_name) {
        return NULL;
    }
    if (_dupenv_s(&value, &length, env_name) != 0) {
        return NULL;
    }
    return value;
#else
    return getenv(env_name);
#endif
}

static void reflection_set_diag(
    KainDiagnostic* diag,
    KainDiagSeverity severity,
    int code,
    const char* message,
    const char* detail
) {
    if (!diag) {
        return;
    }

    kain_diagnostic_create(
        diag,
        KAIN_DIAG_SUBSYSTEM_REFLECTION,
        severity,
        code,
        message,
        detail,
        "runtime/native/src/core/kain_runtime_reflection.c"
    );
}
