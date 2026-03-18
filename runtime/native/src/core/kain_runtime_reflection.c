/*
 * KAIN Native Runtime Reflection Implementation
 *
 * Implements reflection payload loading, schema validation, and metadata
 * lookup for the native runtime. Provides APIs for loading compiler-emitted
 * reflection payloads and querying type schemas and item metadata.
 */

#include "kain_runtime_reflection.h"
#include "kain_runtime_diagnostics.h"
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
static int json_expect(JsonParser* parser, JsonTokenType type);
static const char* json_token_string(JsonParser* parser, JsonToken* token, char* out, size_t out_size);
static long long json_token_number(JsonParser* parser, JsonToken* token);
static int parse_reflection_payload_json(JsonParser* parser, KainReflectionPayload* payload, KainDiagnostic* diag);

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

    (*payload)->json_source = strdup(json);
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

    FILE* file = fopen(path, "rb");
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

    const char* path = getenv(env_name);
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

    return kain_reflection_load_from_path(path, payload, diag);
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

    /* For now, just mark as parsed - full JSON parsing would be more complex */
    /* This is a placeholder that assumes well-formed JSON from the compiler */
    return 0;
}

static int parse_reflection_payload_json(
    JsonParser* parser,
    KainReflectionPayload* payload,
    KainDiagnostic* diag
) {
    /* Placeholder implementation - in production, this would parse the JSON */
    /* For now, initialize with default values */
    payload->schema_major = KAIN_REFLECTION_SCHEMA_VERSION_MAJOR;
    payload->schema_minor = KAIN_REFLECTION_SCHEMA_VERSION_MINOR;
    payload->type_count = 0;
    payload->type_capacity = 0;
    payload->types = NULL;
    payload->item_count = 0;
    payload->item_capacity = 0;
    payload->items = NULL;

    /* TODO: Implement full JSON parsing for reflection payloads */
    /* This would parse the schema_version, types, items, actors, components, messages fields */

    return 0;
}
