#ifndef KAIN_RUNTIME_REFLECTION_H
#define KAIN_RUNTIME_REFLECTION_H

#include "kain_runtime_base.h"
#include "kain_runtime_diagnostics.h"
#include "kain_runtime_scene.h"
#include <stddef.h>

/*
 * KAIN Native Runtime Reflection ABI
 *
 * This header defines the canonical reflection and metadata runtime ABI for
 * the KAIN native runtime. It provides declarations for loading reflection
 * payloads, querying type schemas, and accessing runtime-significant item
 * metadata.
 *
 * Reflection Features:
 * - Reflection payload loading and validation
 * - Type schema lookup and traversal
 * - Actor/component/message metadata access
 * - Service binding metadata
 * - Runtime item identity resolution
 * - Schema version compatibility checking
 */

/* Reflection Schema Version */
#define KAIN_REFLECTION_SCHEMA_VERSION_MAJOR 0
#define KAIN_REFLECTION_SCHEMA_VERSION_MINOR 1

/* Item Kind */
typedef enum {
    KAIN_ITEM_KIND_UNKNOWN = 0,
    KAIN_ITEM_KIND_FUNCTION,
    KAIN_ITEM_KIND_STRUCT,
    KAIN_ITEM_KIND_ENUM,
    KAIN_ITEM_KIND_ACTOR,
    KAIN_ITEM_KIND_COMPONENT,
    KAIN_ITEM_KIND_MESSAGE,
    KAIN_ITEM_KIND_SERVICE,
    KAIN_ITEM_KIND_MODULE,
} KainItemKind;

/* Type Kind */
typedef enum {
    KAIN_TYPE_KIND_UNKNOWN = 0,
    KAIN_TYPE_KIND_PRIMITIVE,
    KAIN_TYPE_KIND_STRUCT,
    KAIN_TYPE_KIND_ENUM,
    KAIN_TYPE_KIND_ARRAY,
    KAIN_TYPE_KIND_POINTER,
    KAIN_TYPE_KIND_FUNCTION,
    KAIN_TYPE_KIND_ACTOR,
    KAIN_TYPE_KIND_MESSAGE,
} KainTypeKind;

/* String Buffer Sizes */
#define KAIN_REFLECTION_NAME_MAX        128
#define KAIN_REFLECTION_PATH_MAX        512
#define KAIN_REFLECTION_SIGNATURE_MAX   256

/*
 * Reflection Payload
 *
 * Represents a loaded reflection payload. Contains type schemas, item
 * metadata, and runtime-significant information emitted by the compiler.
 * Opaque structure - implementation details in runtime core.
 */
typedef struct KainReflectionPayload KainReflectionPayload;

/*
 * Type Schema
 *
 * Describes a type's structure, layout, and metadata.
 */
typedef struct {
    unsigned long long type_id;
    KainTypeKind kind;
    char name[KAIN_REFLECTION_NAME_MAX];
    size_t size_bytes;
    size_t align_bytes;
    int field_count;
    void* fields;  /* Opaque pointer to field array */
} KainTypeSchema;

/*
 * Item Metadata
 *
 * Describes a runtime-significant item (actor, component, message, service).
 */
typedef struct {
    unsigned long long item_id;
    KainItemKind kind;
    char name[KAIN_REFLECTION_NAME_MAX];
    char module_path[KAIN_REFLECTION_PATH_MAX];
    char signature[KAIN_REFLECTION_SIGNATURE_MAX];
    unsigned long long type_id;
} KainItemMetadata;

typedef enum {
    KAIN_RUNTIME_REFLECTION_SCOPE_UNKNOWN = 0,
    KAIN_RUNTIME_REFLECTION_SCOPE_SCENE,
    KAIN_RUNTIME_REFLECTION_SCOPE_RESOURCE,
    KAIN_RUNTIME_REFLECTION_SCOPE_BINDING,
    KAIN_RUNTIME_REFLECTION_SCOPE_DEVICE,
    KAIN_RUNTIME_REFLECTION_SCOPE_BUNDLE,
} KainRuntimeReflectionScope;

typedef enum {
    KAIN_RUNTIME_REFLECTION_SELECTOR_NONE = 0,
    KAIN_RUNTIME_REFLECTION_SELECTOR_PRIMARY,
    KAIN_RUNTIME_REFLECTION_SELECTOR_HANDLE,
    KAIN_RUNTIME_REFLECTION_SELECTOR_NAME,
    KAIN_RUNTIME_REFLECTION_SELECTOR_TYPE_ID,
    KAIN_RUNTIME_REFLECTION_SELECTOR_ITEM_ID,
} KainRuntimeReflectionSelectorKind;

typedef struct {
    KainRuntimeReflectionScope scope;
    KainRuntimeReflectionSelectorKind selector_kind;
    KainSceneResourceKind subject_kind;
    KainSceneHandle scene_handle;
    KainSceneHandle subject_handle;
    unsigned long long type_id;
    unsigned long long item_id;
    char subject_name[KAIN_REFLECTION_NAME_MAX];
} KainRuntimeReflectionQuery;

typedef struct {
    int resolved;
    KainRuntimeReflectionScope scope;
    KainSceneResourceKind subject_kind;
    KainSceneHandle scene_handle;
    KainSceneHandle subject_handle;
    unsigned long long type_id;
    unsigned long long item_id;
    char subject_name[KAIN_REFLECTION_NAME_MAX];
    char source_path[KAIN_REFLECTION_PATH_MAX];
    char summary[KAIN_REFLECTION_SIGNATURE_MAX];
} KainRuntimeReflectionRecord;

/*
 * Load Reflection Payload from JSON
 *
 * Parses and loads a reflection payload from JSON string. Returns 0 on
 * success, non-zero on error. Populates diagnostic on error.
 */
int kain_reflection_load_from_json(
    const char* json,
    KainReflectionPayload** payload,
    KainDiagnostic* diag
);

/*
 * Load Reflection Payload from File
 *
 * Loads a reflection payload from a file path. Returns 0 on success,
 * non-zero on error. Populates diagnostic on error.
 */
int kain_reflection_load_from_path(
    const char* path,
    KainReflectionPayload** payload,
    KainDiagnostic* diag
);

/*
 * Load Reflection Payload from Environment Variable
 *
 * Loads a reflection payload from a path specified in an environment
 * variable. Returns 0 on success, non-zero on error.
 */
int kain_reflection_load_from_env(
    const char* env_name,
    KainReflectionPayload** payload,
    KainDiagnostic* diag
);

/*
 * Free Reflection Payload
 *
 * Releases resources associated with a reflection payload.
 */
void kain_reflection_free(KainReflectionPayload* payload);

/*
 * Get Schema Version
 *
 * Returns the schema version of the loaded reflection payload.
 */
void kain_reflection_get_schema_version(
    const KainReflectionPayload* payload,
    unsigned int* major,
    unsigned int* minor
);

/*
 * Check Schema Compatibility
 *
 * Checks if the reflection payload schema is compatible with the runtime.
 * Returns 1 if compatible, 0 if incompatible.
 */
int kain_reflection_check_schema_compatibility(
    const KainReflectionPayload* payload
);

/*
 * Lookup Type by ID
 *
 * Finds a type schema by its ID. Returns NULL if not found.
 */
const KainTypeSchema* kain_reflection_lookup_type_by_id(
    const KainReflectionPayload* payload,
    unsigned long long type_id
);

/*
 * Lookup Type by Name
 *
 * Finds a type schema by its name. Returns NULL if not found.
 */
const KainTypeSchema* kain_reflection_lookup_type_by_name(
    const KainReflectionPayload* payload,
    const char* name
);

/*
 * Lookup Item by ID
 *
 * Finds an item metadata entry by its ID. Returns NULL if not found.
 */
const KainItemMetadata* kain_reflection_lookup_item_by_id(
    const KainReflectionPayload* payload,
    unsigned long long item_id
);

/*
 * Lookup Item by Name
 *
 * Finds an item metadata entry by its name. Returns NULL if not found.
 */
const KainItemMetadata* kain_reflection_lookup_item_by_name(
    const KainReflectionPayload* payload,
    const char* name
);

/*
 * Get Type Count
 *
 * Returns the number of types in the reflection payload.
 */
int kain_reflection_get_type_count(const KainReflectionPayload* payload);

/*
 * Get Item Count
 *
 * Returns the number of items in the reflection payload.
 */
int kain_reflection_get_item_count(const KainReflectionPayload* payload);

/*
 * Get Items by Kind
 *
 * Retrieves all items of a specific kind. Returns the number of items found.
 * If items array is NULL, returns the count without populating.
 */
int kain_reflection_get_items_by_kind(
    const KainReflectionPayload* payload,
    KainItemKind kind,
    const KainItemMetadata** items,
    int max_items
);

/*
 * Format Type Schema
 *
 * Formats a type schema as a human-readable string.
 * Returns number of characters written (excluding null terminator).
 */
int kain_reflection_format_type_schema(
    const KainTypeSchema* schema,
    char* out,
    size_t out_size
);

/*
 * Format Item Metadata
 *
 * Formats item metadata as a human-readable string.
 * Returns number of characters written (excluding null terminator).
 */
int kain_reflection_format_item_metadata(
    const KainItemMetadata* metadata,
    char* out,
    size_t out_size
);
void kain_runtime_reflection_query_init(KainRuntimeReflectionQuery* query);
void kain_runtime_reflection_record_init(KainRuntimeReflectionRecord* record);
int kain_runtime_reflection_query_matches_item(
    const KainRuntimeReflectionQuery* query,
    const KainItemMetadata* metadata
);
void kain_runtime_reflection_record_from_item(
    const KainRuntimeReflectionQuery* query,
    const KainItemMetadata* metadata,
    KainRuntimeReflectionRecord* record
);
int kain_runtime_reflection_format_record(
    const KainRuntimeReflectionRecord* record,
    char* out,
    size_t out_size
);

/*
 * Print Reflection Summary
 *
 * Prints a summary of the reflection payload to stdout for diagnostics.
 */
void kain_reflection_print_summary(const KainReflectionPayload* payload);

#endif /* KAIN_RUNTIME_REFLECTION_H */
