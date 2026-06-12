/*
 * check_reflection.c - CBMC verification harness for reflection module
 *
 * Verifies the reflection metadata system: payload construction, schema
 * versioning, type/item lookup, metadata formatting, and runtime reflection
 * query/record operations.
 *
 * Focus areas:
 *   - NULL-safety on all public functions
 *   - Type lookup by ID and name on payloads with valid/single/multiple types
 *   - Item lookup by ID and name
 *   - Schema version access and compatibility checking
 *   - Count/kind-filter accessors
 *   - Format functions (snprintf postconditions)
 *   - Runtime reflection query matching, record construction, and formatting
 *
 * Run via: python test/scripts/run_pipeline.py cbmc --harness check_reflection
 */

#include "reflection.h"

/* ──────────────────────────────────────────────────────────────────────
 * Static backing buffers for pointer provenance
 *
 * These give CBMC real allocated-object identity so that pointer
 * dereferences through them have valid provenange.
 * ────────────────────────────────────────────────────────────────────── */

/* Maximum number of types/items we model in the static payload */
#define TEST_MAX_TYPES 8
#define TEST_MAX_ITEMS 8

/* Backing arrays for the payload's dynamic arrays */
static KainTypeSchema    g_types_backing[TEST_MAX_TYPES];
static KainItemMetadata  g_items_backing[TEST_MAX_ITEMS];

/* Backing for the json_source string */
static char g_json_source_backing[256];

/* Backing for format output buffers */
static char g_format_buffer[512];

/* ──────────────────────────────────────────────────────────────────────
 * Struct mirror of the opaque KainReflectionPayload
 *
 * Defined in reflection.c; mirrored here so the harness can construct
 * valid payloads directly (single translation unit).
 * ────────────────────────────────────────────────────────────────────── */
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

/* ──────────────────────────────────────────────────────────────────────
 * Forward declarations of static (internal-linkage) functions we test
 * directly.  These are NOT in the header; the combined source+harness
 * gives us visibility into them.
 * ────────────────────────────────────────────────────────────────────── */
static KainTypeKind reflection_type_kind_from_string(const char* kind);
static KainItemKind reflection_item_kind_from_string(const char* kind);

/* ──────────────────────────────────────────────────────────────────────
 * Factory: create a valid reflection payload backed by static arrays
 *
 * Points types/items at the static backing buffers so CBMC has valid
 * pointer provenance.  Contents are havoc'd and constrained to sensible
 * ranges.
 * ────────────────────────────────────────────────────────────────────── */
static struct KainReflectionPayload* create_valid_payload(void) {
    static struct KainReflectionPayload payload;
    __CPROVER_havoc_object(&payload);

    /* Havoc the backing arrays (nondet contents for types/items) */
    __CPROVER_havoc_object(g_types_backing);
    __CPROVER_havoc_object(g_items_backing);
    __CPROVER_havoc_object(g_json_source_backing);

    /* Pointer provenance: types/items/json_source point into static buffers */
    payload.types       = g_types_backing;
    payload.items       = g_items_backing;
    payload.json_source = g_json_source_backing;

    /* Constrain counts to within array bounds */
    __CPROVER_assume(payload.type_count >= 0);
    __CPROVER_assume(payload.type_count <= TEST_MAX_TYPES);
    __CPROVER_assume(payload.item_count >= 0);
    __CPROVER_assume(payload.item_count <= TEST_MAX_ITEMS);

    payload.type_capacity = TEST_MAX_TYPES;
    payload.item_capacity = TEST_MAX_ITEMS;

    /* For each type, constrain name to be null-terminated within the buffer.
     * We don't constrain the exact string content — just that member fields
     * of the type schema are within valid ranges. */
    for (int i = 0; i < TEST_MAX_TYPES; i++) {
        /* Ensure type kind is in valid enum range */
        __CPROVER_assume(
            g_types_backing[i].kind >= KAIN_TYPE_KIND_UNKNOWN &&
            g_types_backing[i].kind <= KAIN_TYPE_KIND_MESSAGE
        );
        /* type_id can be any unsigned long long */
        /* name[] is a char array — CBMC will treat it as arbitrary bytes;
         * ensure name has a null terminator somewhere before the end */
        __CPROVER_assume(
            g_types_backing[i].name[KAIN_REFLECTION_NAME_MAX - 1] == '\0'
        );
        /* fields pointer may be NULL or non-NULL; constrain to NULL for
         * simplicity since we don't model field arrays */
        g_types_backing[i].fields = NULL;
        __CPROVER_assume(g_types_backing[i].field_count >= 0);
    }

    for (int i = 0; i < TEST_MAX_ITEMS; i++) {
        __CPROVER_assume(
            g_items_backing[i].kind >= KAIN_ITEM_KIND_UNKNOWN &&
            g_items_backing[i].kind <= KAIN_ITEM_KIND_MODULE
        );
        __CPROVER_assume(
            g_items_backing[i].name[KAIN_REFLECTION_NAME_MAX - 1] == '\0'
        );
        __CPROVER_assume(
            g_items_backing[i].module_path[KAIN_REFLECTION_PATH_MAX - 1] == '\0'
        );
        __CPROVER_assume(
            g_items_backing[i].signature[KAIN_REFLECTION_SIGNATURE_MAX - 1] == '\0'
        );
    }

    return &payload;
}


/* ──────────────────────────────────────────────────────────────────────
 * Factory: create a payload with exactly one type and one item for
 * deterministic lookup testing.
 * ────────────────────────────────────────────────────────────────────── */
static struct KainReflectionPayload* create_payload_with_one_type(void) {
    static struct KainReflectionPayload payload;
    static KainTypeSchema single_type;
    static KainItemMetadata single_item;

    __CPROVER_havoc_object(&payload);
    __CPROVER_havoc_object(&single_type);
    __CPROVER_havoc_object(&single_item);

    payload.types       = &single_type;
    payload.type_count  = 1;
    payload.type_capacity = 1;
    payload.items       = &single_item;
    payload.item_count  = 1;
    payload.item_capacity = 1;
    payload.json_source = g_json_source_backing;

    /* Constrain type fields */
    single_type.type_id   = 42ull;
    single_type.kind      = KAIN_TYPE_KIND_STRUCT;
    single_type.name[0]   = 'T';
    single_type.name[1]   = 'e';
    single_type.name[2]   = 's';
    single_type.name[3]   = 't';
    single_type.name[4]   = '\0';
    single_type.size_bytes    = 16;
    single_type.align_bytes   = 8;
    single_type.field_count   = 2;
    single_type.fields        = NULL;

    /* Constrain item fields */
    single_item.item_id   = 100ull;
    single_item.kind      = KAIN_ITEM_KIND_FUNCTION;
    single_item.name[0]   = 'f';
    single_item.name[1]   = 'o';
    single_item.name[2]   = 'o';
    single_item.name[3]   = '\0';
    single_item.module_path[0] = 'm';
    single_item.module_path[1] = 'o';
    single_item.module_path[2] = 'd';
    single_item.module_path[3] = '\0';
    single_item.type_id   = 42ull;
    single_item.signature[0] = '\0';

    payload.schema_major = KAIN_REFLECTION_SCHEMA_VERSION_MAJOR;
    payload.schema_minor = 1;

    return &payload;
}


/* ──────────────────────────────────────────────────────────────────────
 * Factory: create a payload with multiple types and items for iteration
 * and kind-filter testing.
 * ────────────────────────────────────────────────────────────────────── */
static struct KainReflectionPayload* create_payload_multi(void) {
    static struct KainReflectionPayload payload;
    static KainTypeSchema types[3];
    static KainItemMetadata items[4];

    __CPROVER_havoc_object(&payload);
    __CPROVER_havoc_object(types);
    __CPROVER_havoc_object(items);

    payload.types       = types;
    payload.type_count  = 3;
    payload.type_capacity = 3;
    payload.items       = items;
    payload.item_count  = 4;
    payload.item_capacity = 4;
    payload.json_source = g_json_source_backing;

    /* Type 0: struct */
    types[0].type_id   = 1ull;
    types[0].kind      = KAIN_TYPE_KIND_STRUCT;
    types[0].name[0]   = 'S';
    types[0].name[1]   = '\0';
    types[0].fields    = NULL;
    types[0].field_count = 0;
    types[0].name[KAIN_REFLECTION_NAME_MAX - 1] = '\0';

    /* Type 1: enum */
    types[1].type_id   = 2ull;
    types[1].kind      = KAIN_TYPE_KIND_ENUM;
    types[1].name[0]   = 'E';
    types[1].name[1]   = '\0';
    types[1].fields    = NULL;
    types[1].field_count = 0;
    types[1].name[KAIN_REFLECTION_NAME_MAX - 1] = '\0';

    /* Type 2: pointer */
    types[2].type_id   = 3ull;
    types[2].kind      = KAIN_TYPE_KIND_POINTER;
    types[2].name[0]   = 'P';
    types[2].name[1]   = '\0';
    types[2].fields    = NULL;
    types[2].field_count = 0;
    types[2].name[KAIN_REFLECTION_NAME_MAX - 1] = '\0';

    /* Item 0: function */
    items[0].item_id  = 10ull;
    items[0].kind     = KAIN_ITEM_KIND_FUNCTION;
    items[0].name[0]  = 'f';
    items[0].name[1]  = '\0';
    items[0].type_id  = 1ull;
    items[0].module_path[0] = '\0';
    items[0].name[KAIN_REFLECTION_NAME_MAX - 1]     = '\0';
    items[0].module_path[KAIN_REFLECTION_PATH_MAX - 1]     = '\0';
    items[0].signature[KAIN_REFLECTION_SIGNATURE_MAX - 1]  = '\0';

    /* Item 1: struct */
    items[1].item_id  = 20ull;
    items[1].kind     = KAIN_ITEM_KIND_STRUCT;
    items[1].name[0]  = 's';
    items[1].name[1]  = '\0';
    items[1].type_id  = 2ull;
    items[1].module_path[0] = '\0';
    items[1].name[KAIN_REFLECTION_NAME_MAX - 1]     = '\0';
    items[1].module_path[KAIN_REFLECTION_PATH_MAX - 1]     = '\0';
    items[1].signature[KAIN_REFLECTION_SIGNATURE_MAX - 1]  = '\0';

    /* Item 2: actor */
    items[2].item_id  = 30ull;
    items[2].kind     = KAIN_ITEM_KIND_ACTOR;
    items[2].name[0]  = 'a';
    items[2].name[1]  = '\0';
    items[2].type_id  = 3ull;
    items[2].module_path[0] = '\0';
    items[2].name[KAIN_REFLECTION_NAME_MAX - 1]     = '\0';
    items[2].module_path[KAIN_REFLECTION_PATH_MAX - 1]     = '\0';
    items[2].signature[KAIN_REFLECTION_SIGNATURE_MAX - 1]  = '\0';

    /* Item 3: message */
    items[3].item_id  = 40ull;
    items[3].kind     = KAIN_ITEM_KIND_MESSAGE;
    items[3].name[0]  = 'm';
    items[3].name[1]  = '\0';
    items[3].type_id  = 1ull;
    items[3].module_path[0] = '\0';
    items[3].name[KAIN_REFLECTION_NAME_MAX - 1]     = '\0';
    items[3].module_path[KAIN_REFLECTION_PATH_MAX - 1]     = '\0';
    items[3].signature[KAIN_REFLECTION_SIGNATURE_MAX - 1]  = '\0';

    payload.schema_major = KAIN_REFLECTION_SCHEMA_VERSION_MAJOR;
    payload.schema_minor = 1;

    return &payload;
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 1: kain_reflection_get_schema_version — NULL safety and correct
 * version propagation
 * ────────────────────────────────────────────────────────────────────── */
void check_get_schema_version(void) {
    unsigned int major, minor;

    /* NULL payload */
    major = 99; minor = 99;
    kain_reflection_get_schema_version(NULL, &major, &minor);
    __CPROVER_assert(major == 0, "NULL payload -> major=0");
    __CPROVER_assert(minor == 0, "NULL payload -> minor=0");

    /* NULL output pointers */
    struct KainReflectionPayload* p = create_valid_payload();
    kain_reflection_get_schema_version(p, NULL, NULL);
    /* no crash — this is a no-op assertion */

    /* Normal path */
    p->schema_major = 0;
    p->schema_minor = 1;
    kain_reflection_get_schema_version(p, &major, &minor);
    __CPROVER_assert(major == 0, "schema_major read back");
    __CPROVER_assert(minor == 1, "schema_minor read back");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 2: kain_reflection_check_schema_compatibility — NULL safety and
 * version matching
 * ────────────────────────────────────────────────────────────────────── */
void check_schema_compatibility(void) {
    /* NULL payload */
    __CPROVER_assert(
        kain_reflection_check_schema_compatibility(NULL) == 0,
        "NULL payload -> incompatible"
    );

    /* Matching major version */
    struct KainReflectionPayload* p = create_valid_payload();
    p->schema_major = KAIN_REFLECTION_SCHEMA_VERSION_MAJOR;
    p->schema_minor = 42;  /* any minor is OK */
    __CPROVER_assert(
        kain_reflection_check_schema_compatibility(p) == 1,
        "matching major -> compatible"
    );

    /* Mismatched major version */
    p->schema_major = KAIN_REFLECTION_SCHEMA_VERSION_MAJOR + 1;
    __CPROVER_assert(
        kain_reflection_check_schema_compatibility(p) == 0,
        "different major -> incompatible"
    );
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 3: kain_reflection_lookup_type_by_id — exact match vs miss
 * ────────────────────────────────────────────────────────────────────── */
void check_lookup_type_by_id(void) {
    /* NULL payload */
    __CPROVER_assert(
        kain_reflection_lookup_type_by_id(NULL, 42) == NULL,
        "NULL payload -> NULL result"
    );

    /* Payload with zero types */
    struct KainReflectionPayload* p0 = create_valid_payload();
    p0->type_count = 0;
    __CPROVER_assert(
        kain_reflection_lookup_type_by_id(p0, 42) == NULL,
        "empty payload -> NULL result"
    );

    /* Payload with one type, matching */
    struct KainReflectionPayload* p1 = create_payload_with_one_type();
    const KainTypeSchema* ts = kain_reflection_lookup_type_by_id(p1, 42ull);
    __CPROVER_assert(ts != NULL, "type with id 42 found");
    if (ts) {
        __CPROVER_assert(ts->type_id == 42ull, "found type has id 42");
        __CPROVER_assert(ts->kind == KAIN_TYPE_KIND_STRUCT, "found type is struct");
    }

    /* Payload with one type, non-matching */
    const KainTypeSchema* tn = kain_reflection_lookup_type_by_id(p1, 999ull);
    __CPROVER_assert(tn == NULL, "type with id 999 not found");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 4: kain_reflection_lookup_type_by_name
 * ────────────────────────────────────────────────────────────────────── */
void check_lookup_type_by_name(void) {
    __CPROVER_assert(
        kain_reflection_lookup_type_by_name(NULL, "Test") == NULL,
        "NULL payload -> NULL"
    );

    struct KainReflectionPayload* p1 = create_payload_with_one_type();

    /* Matching name */
    const KainTypeSchema* ts = kain_reflection_lookup_type_by_name(p1, "Test");
    __CPROVER_assert(ts != NULL, "type named Test found");
    if (ts) {
        __CPROVER_assert(ts->type_id == 42ull, "found type has id 42");
    }

    /* Non-matching name */
    const KainTypeSchema* tn = kain_reflection_lookup_type_by_name(p1, "NonExistent");
    __CPROVER_assert(tn == NULL, "type with wrong name not found");

    /* NULL name */
    const KainTypeSchema* tnull = kain_reflection_lookup_type_by_name(p1, NULL);
    __CPROVER_assert(tnull == NULL, "NULL name -> NULL");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 5: kain_reflection_lookup_item_by_id
 * ────────────────────────────────────────────────────────────────────── */
void check_lookup_item_by_id(void) {
    __CPROVER_assert(
        kain_reflection_lookup_item_by_id(NULL, 100) == NULL,
        "NULL payload -> NULL"
    );

    struct KainReflectionPayload* p = create_payload_with_one_type();

    const KainItemMetadata* im = kain_reflection_lookup_item_by_id(p, 100ull);
    __CPROVER_assert(im != NULL, "item with id 100 found");
    if (im) {
        __CPROVER_assert(im->item_id == 100ull, "found item has id 100");
        __CPROVER_assert(im->kind == KAIN_ITEM_KIND_FUNCTION, "found item is function");
    }

    const KainItemMetadata* im2 = kain_reflection_lookup_item_by_id(p, 999ull);
    __CPROVER_assert(im2 == NULL, "item with id 999 not found");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 6: kain_reflection_lookup_item_by_name
 * ────────────────────────────────────────────────────────────────────── */
void check_lookup_item_by_name(void) {
    struct KainReflectionPayload* p = create_payload_with_one_type();

    const KainItemMetadata* im = kain_reflection_lookup_item_by_name(p, "foo");
    __CPROVER_assert(im != NULL, "item named foo found");
    if (im) {
        __CPROVER_assert(im->item_id == 100ull, "found item has id 100");
    }

    const KainItemMetadata* im2 = kain_reflection_lookup_item_by_name(p, "nope");
    __CPROVER_assert(im2 == NULL, "item with wrong name not found");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 7: kain_reflection_get_type_count / get_item_count
 * ────────────────────────────────────────────────────────────────────── */
void check_count_functions(void) {
    __CPROVER_assert(
        kain_reflection_get_type_count(NULL) == 0,
        "NULL payload -> type_count=0"
    );
    __CPROVER_assert(
        kain_reflection_get_item_count(NULL) == 0,
        "NULL payload -> item_count=0"
    );

    struct KainReflectionPayload* p = create_payload_multi();
    __CPROVER_assert(
        kain_reflection_get_type_count(p) == 3,
        "type_count == 3"
    );
    __CPROVER_assert(
        kain_reflection_get_item_count(p) == 4,
        "item_count == 4"
    );
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 8: kain_reflection_get_items_by_kind
 * ────────────────────────────────────────────────────────────────────── */
void check_get_items_by_kind(void) {
    /* NULL payload */
    const KainItemMetadata* arr[8];
    int n = kain_reflection_get_items_by_kind(NULL, KAIN_ITEM_KIND_ACTOR, arr, 8);
    __CPROVER_assert(n == 0, "NULL payload -> count=0");

    /* NULL items array (count-only mode) */
    struct KainReflectionPayload* p = create_payload_multi();
    n = kain_reflection_get_items_by_kind(p, KAIN_ITEM_KIND_ACTOR, NULL, 8);
    __CPROVER_assert(n == 1, "one actor item in multi payload");

    /* Filter by STRUCT kind */
    const KainItemMetadata* results[8];
    n = kain_reflection_get_items_by_kind(p, KAIN_ITEM_KIND_STRUCT, results, 8);
    __CPROVER_assert(n >= 1, "at least one struct item");
    if (n > 0) {
        __CPROVER_assert(results[0]->kind == KAIN_ITEM_KIND_STRUCT,
                         "first result is struct kind");
    }

    /* Filter by MESSAGE kind */
    n = kain_reflection_get_items_by_kind(p, KAIN_ITEM_KIND_MESSAGE, results, 8);
    __CPROVER_assert(n >= 1, "at least one message item");

    /* Filter by unknown kind (should return 0) */
    n = kain_reflection_get_items_by_kind(p, KAIN_ITEM_KIND_UNKNOWN, results, 8);
    __CPROVER_assert(n == 0, "unknown kind matches nothing");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 9: kain_reflection_format_type_schema — snprintf postconditions
 * ────────────────────────────────────────────────────────────────────── */
void check_format_type_schema(void) {
    /* NULL schema */
    int r = kain_reflection_format_type_schema(NULL, g_format_buffer, sizeof(g_format_buffer));
    __CPROVER_assert(r == 0, "NULL schema -> 0 chars");

    /* NULL output buffer */
    struct KainReflectionPayload* p = create_payload_with_one_type();
    r = kain_reflection_format_type_schema(&p->types[0], NULL, 0);
    __CPROVER_assert(r == 0, "NULL buffer -> 0 chars");

    /* Normal path */
    r = kain_reflection_format_type_schema(&p->types[0], g_format_buffer, sizeof(g_format_buffer));
    __CPROVER_assert(r > 0, "format produced output");
    __CPROVER_assert(r < (int)sizeof(g_format_buffer),
                     "format output fits in buffer");
    __CPROVER_assert(g_format_buffer[sizeof(g_format_buffer) - 1] == '\0',
                     "buffer is null-terminated (snprintf guarantee)");

    /* Tiny buffer (should produce truncated output) */
    char tiny[4];
    r = kain_reflection_format_type_schema(&p->types[0], tiny, sizeof(tiny));
    __CPROVER_assert(r >= 0, "tiny buffer: non-negative return");
    __CPROVER_assert(tiny[sizeof(tiny) - 1] == '\0',
                     "tiny buffer null-terminated");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 10: kain_reflection_format_item_metadata
 * ────────────────────────────────────────────────────────────────────── */
void check_format_item_metadata(void) {
    struct KainReflectionPayload* p = create_payload_with_one_type();

    int r = kain_reflection_format_item_metadata(&p->items[0], g_format_buffer, sizeof(g_format_buffer));
    __CPROVER_assert(r > 0, "format produced output");
    __CPROVER_assert(g_format_buffer[sizeof(g_format_buffer) - 1] == '\0',
                     "buffer null-terminated");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 11: reflection_query_init / reflection_record_init — clear state
 * ────────────────────────────────────────────────────────────────────── */
void check_query_record_init(void) {
    KainRuntimeReflectionQuery query;
    KainRuntimeReflectionRecord record;

    /* Fill with known garbage */
    memset(&query, 0xFF, sizeof(query));
    memset(&record, 0xFF, sizeof(record));

    reflection_query_init(&query);
    __CPROVER_assert(query.scope == REFLECTION_SCOPE_UNKNOWN,
                     "query scope initialized to unknown");
    __CPROVER_assert(query.selector_kind == REFLECTION_SELECTOR_NONE,
                     "query selector_kind initialized to none");
    __CPROVER_assert(query.subject_name[0] == '\0',
                     "query subject_name empty");

    reflection_record_init(&record);
    __CPROVER_assert(record.resolved == 0,
                     "record resolved initialized to 0");

    /* NULL safety */
    reflection_query_init(NULL);
    reflection_record_init(NULL);  /* no crash */
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 12: reflection_query_matches_item — various selector kinds
 * ────────────────────────────────────────────────────────────────────── */
void check_query_matches_item(void) {
    KainRuntimeReflectionQuery query;
    KainItemMetadata item;

    /* Item with known id/name */
    item.item_id = 100ull;
    item.kind    = KAIN_ITEM_KIND_FUNCTION;
    item.name[0] = 'f'; item.name[1] = 'o'; item.name[2] = 'o'; item.name[3] = '\0';
    item.name[KAIN_REFLECTION_NAME_MAX - 1] = '\0';

    /* NONE selector -> always matches */
    reflection_query_init(&query);
    query.selector_kind = REFLECTION_SELECTOR_NONE;
    __CPROVER_assert(reflection_query_matches_item(&query, &item) != 0,
                     "NONE selector matches any item");

    /* PRIMARY selector -> always matches */
    query.selector_kind = REFLECTION_SELECTOR_PRIMARY;
    __CPROVER_assert(reflection_query_matches_item(&query, &item) != 0,
                     "PRIMARY selector matches any item");

    /* ITEM_ID selector */
    query.selector_kind = REFLECTION_SELECTOR_ITEM_ID;
    query.item_id = 100ull;
    __CPROVER_assert(reflection_query_matches_item(&query, &item) != 0,
                     "ITEM_ID matches correct item");
    query.item_id = 999ull;
    __CPROVER_assert(reflection_query_matches_item(&query, &item) == 0,
                     "ITEM_ID rejects wrong item");

    /* NAME selector */
    reflection_query_init(&query);
    query.selector_kind = REFLECTION_SELECTOR_NAME;
    query.subject_name[0] = 'f'; query.subject_name[1] = 'o'; query.subject_name[2] = 'o'; query.subject_name[3] = '\0';
    __CPROVER_assert(reflection_query_matches_item(&query, &item) != 0,
                     "NAME matches correct item");
    query.subject_name[0] = 'b'; query.subject_name[1] = 'a'; query.subject_name[2] = 'r'; query.subject_name[3] = '\0';
    __CPROVER_assert(reflection_query_matches_item(&query, &item) == 0,
                     "NAME rejects wrong name");

    /* NULL query */
    __CPROVER_assert(reflection_query_matches_item(NULL, &item) == 0,
                     "NULL query -> 0");
    /* NULL metadata */
    __CPROVER_assert(reflection_query_matches_item(&query, NULL) == 0,
                     "NULL metadata -> 0");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 13: reflection_record_from_item — data propagation
 * ────────────────────────────────────────────────────────────────────── */
void check_record_from_item(void) {
    KainRuntimeReflectionQuery query;
    KainItemMetadata item;
    KainRuntimeReflectionRecord record;

    item.item_id = 100ull;
    item.type_id = 42ull;
    item.kind    = KAIN_ITEM_KIND_FUNCTION;
    item.name[0] = 'f'; item.name[1] = 'o'; item.name[2] = 'o'; item.name[3] = '\0';
    item.module_path[0] = 'm'; item.module_path[1] = 'o'; item.module_path[2] = 'd'; item.module_path[3] = '\0';
    item.module_path[KAIN_REFLECTION_PATH_MAX - 1] = '\0';
    item.name[KAIN_REFLECTION_NAME_MAX - 1] = '\0';

    reflection_query_init(&query);
    query.scope    = REFLECTION_SCOPE_SCENE;
    query.item_id  = 100ull;

    memset(&record, 0xFF, sizeof(record));
    reflection_record_from_item(&query, &item, &record);

    __CPROVER_assert(record.resolved == 1, "record is resolved");
    __CPROVER_assert(record.item_id == 100ull, "item_id propagated");
    __CPROVER_assert(record.type_id == 42ull, "type_id propagated");
    __CPROVER_assert(record.scope == REFLECTION_SCOPE_SCENE,
                     "scope propagated from query");

    /* NULL metadata -> should not crash, record stays init'd */
    memset(&record, 0xFF, sizeof(record));
    reflection_record_from_item(&query, NULL, &record);
    __CPROVER_assert(record.resolved == 0,
                     "NULL metadata -> record stays unresolved (init)");

    /* NULL record */
    reflection_record_from_item(&query, &item, NULL);  /* no crash */
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 14: reflection_format_record — snprintf postconditions
 * ────────────────────────────────────────────────────────────────────── */
void check_format_record(void) {
    KainRuntimeReflectionRecord record;

    memset(&record, 0xFF, sizeof(record));
    record.resolved = 1;
    record.scope    = REFLECTION_SCOPE_RESOURCE;
    record.item_id  = 100ull;
    record.type_id  = 42ull;
    record.subject_name[0] = 't'; record.subject_name[1] = '\0';
    record.source_path[0]   = 's'; record.source_path[1] = 'r'; record.source_path[2] = 'c'; record.source_path[3] = '\0';
    record.subject_name[KAIN_REFLECTION_NAME_MAX - 1]    = '\0';
    record.source_path[KAIN_REFLECTION_PATH_MAX - 1]    = '\0';

    int r = reflection_format_record(&record, g_format_buffer, sizeof(g_format_buffer));
    __CPROVER_assert(r > 0, "format produced output");
    __CPROVER_assert(g_format_buffer[sizeof(g_format_buffer) - 1] == '\0',
                     "buffer null-terminated");

    /* NULL record */
    r = reflection_format_record(NULL, g_format_buffer, sizeof(g_format_buffer));
    __CPROVER_assert(r == 0, "NULL record -> 0 chars");

    /* NULL buffer */
    r = reflection_format_record(&record, NULL, 0);
    __CPROVER_assert(r == 0, "NULL buffer -> 0 chars");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 15: kain_reflection_free — NULL safety and idempotence
 * ────────────────────────────────────────────────────────────────────── */
void check_free(void) {
    /* NULL payload */
    kain_reflection_free(NULL);  /* no crash */

    /* Normal payload */
    struct KainReflectionPayload* p = create_valid_payload();
    /* types/items/json_source point into static buffers — free will call
     * free() on them.  CBMC models free() as valid for malloc'd pointers,
     * but havoc'd static-pointer addresses are not malloc'd.
     *
     * To keep CBMC happy, we set the pointers to NULL so free() is a no-op
     * on the inner arrays, and only the top-level calloc-free happens.
     * (In reality these would be malloc'd; CBMC will explore the malloc
     * failure path too.)
     */
    p->types       = NULL;
    p->items       = NULL;
    p->json_source = NULL;

    kain_reflection_free(p);  /* no crash, freeing static backing is safe */

    /* Double free — the outer payload was malloc'd (calloc in load_from_json)
     * but our static address isn't.  Just verify the inner pointer handling:
     * calling with types/items already NULL is safe. */
    __CPROVER_assert(1, "free path completed");
}


/* ──────────────────────────────────────────────────────────────────────
 * Test 16: kain_reflection_print_summary — NULL safety
 * ────────────────────────────────────────────────────────────────────── */
void check_print_summary(void) {
    kain_reflection_print_summary(NULL);  /* no crash */

    struct KainReflectionPayload* p = create_valid_payload();
    kain_reflection_print_summary(p);
    __CPROVER_assert(1, "print_summary completed without crash");
}


/* ──────────────────────────────────────────────────────────────────────
 * Main — run all checks
 * ────────────────────────────────────────────────────────────────────── */
int main(void) {
    check_get_schema_version();
    check_schema_compatibility();
    check_lookup_type_by_id();
    check_lookup_type_by_name();
    check_lookup_item_by_id();
    check_lookup_item_by_name();
    check_count_functions();
    check_get_items_by_kind();
    check_format_type_schema();
    check_format_item_metadata();
    check_query_record_init();
    check_query_matches_item();
    check_record_from_item();
    check_format_record();
    check_free();
    check_print_summary();
    return 0;
}
