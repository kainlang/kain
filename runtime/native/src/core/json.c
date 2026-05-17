#include "../../include/json.h"
#include "../../include/base.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>

#define KAIN_JSON_ANY_TAG_MASK 7LL
#define KAIN_JSON_ANY_TAG_INT 1LL
#define KAIN_JSON_ANY_TAG_BOOL 2LL
#define KAIN_JSON_ANY_TAG_STRING 3LL
#define KAIN_JSON_ANY_TAG_NULL 4LL

typedef enum KainJsonKind {
    KAIN_JSON_NULL = 0,
    KAIN_JSON_BOOL = 1,
    KAIN_JSON_INT = 2,
    KAIN_JSON_STRING = 3,
    KAIN_JSON_OBJECT = 4,
    KAIN_JSON_ARRAY = 5
} KainJsonKind;

typedef struct KainJsonValue KainJsonValue;

typedef struct KainJsonEntry {
    char* key;
    KainJsonValue* value;
} KainJsonEntry;

struct KainJsonValue {
    KainJsonKind kind;
    int bool_value;
    int64_t int_value;
    char* string_value;
    KainJsonEntry* fields;
    int64_t field_count;
    int64_t field_capacity;
    KainJsonValue** items;
    int64_t item_count;
    int64_t item_capacity;
};

typedef struct KainJsonParser {
    const char* cursor;
} KainJsonParser;

typedef struct KainJsonRegistryNode {
    KainJsonValue* value;
    struct KainJsonRegistryNode* next;
} KainJsonRegistryNode;

static KainJsonRegistryNode* g_json_registry;

void* kain_alloc_rc(size_t size, long long type_tag);

static void json_register_value(KainJsonValue* value) {
    KainJsonRegistryNode* node;
    if (!value) {
        return;
    }
    node = (KainJsonRegistryNode*)malloc(sizeof(KainJsonRegistryNode));
    if (!node) {
        return;
    }
    node->value = value;
    node->next = g_json_registry;
    g_json_registry = node;
}

static KainJsonValue* json_registered_handle(int64_t handle) {
    KainJsonValue* candidate;
    KainJsonRegistryNode* node;
    if ((handle & KAIN_JSON_ANY_TAG_MASK) != 0) {
        return NULL;
    }
    candidate = (KainJsonValue*)(intptr_t)handle;
    for (node = g_json_registry; node; node = node->next) {
        if (node->value == candidate) {
            return candidate;
        }
    }
    return NULL;
}

static char* json_dup_range(const char* text, size_t length) {
    char* out = (char*)malloc(length + 1u);
    if (!out) {
        return NULL;
    }
    if (length > 0u) {
        memcpy(out, text, length);
    }
    out[length] = '\0';
    return out;
}

static char* json_dup_cstr(const char* text) {
    if (!text) {
        return json_dup_range("", 0u);
    }
    return json_dup_range(text, strlen(text));
}

static char* json_dup_kain_string(const char* text) {
    return json_dup_cstr(text);
}

static KainJsonValue* json_value_new(KainJsonKind kind) {
    KainJsonValue* value = (KainJsonValue*)calloc(1u, sizeof(KainJsonValue));
    if (!value) {
        return NULL;
    }
    value->kind = kind;
    json_register_value(value);
    return value;
}

static KainJsonValue* json_value_int(int64_t value) {
    KainJsonValue* json = json_value_new(KAIN_JSON_INT);
    if (json) {
        json->int_value = value;
    }
    return json;
}

static KainJsonValue* json_value_bool(int value) {
    KainJsonValue* json = json_value_new(KAIN_JSON_BOOL);
    if (json) {
        json->bool_value = value != 0;
    }
    return json;
}

static KainJsonValue* json_value_string_copy(const char* text) {
    KainJsonValue* json = json_value_new(KAIN_JSON_STRING);
    if (!json) {
        return NULL;
    }
    json->string_value = json_dup_kain_string(text);
    if (!json->string_value) {
        free(json);
        return NULL;
    }
    return json;
}

static KainJsonValue* json_value_string_owned(char* text) {
    KainJsonValue* json = json_value_new(KAIN_JSON_STRING);
    if (!json) {
        free(text);
        return NULL;
    }
    json->string_value = text ? text : json_dup_range("", 0u);
    return json;
}

static int64_t json_handle_from_value(KainJsonValue* value) {
    return (int64_t)(intptr_t)value;
}

static KainJsonValue* json_value_from_handle(int64_t handle) {
    if ((handle & KAIN_JSON_ANY_TAG_MASK) != 0) {
        return NULL;
    }
    return (KainJsonValue*)(intptr_t)handle;
}

static KainJsonValue* json_clone_value(const KainJsonValue* value);

static KainJsonValue* json_value_from_any(int64_t any) {
    int64_t tag = any & KAIN_JSON_ANY_TAG_MASK;
    if (tag == 0) {
        KainJsonValue* existing = json_value_from_handle(any);
        return existing ? json_clone_value(existing) : json_value_new(KAIN_JSON_NULL);
    }
    if (tag == KAIN_JSON_ANY_TAG_INT) {
        int64_t payload = any >> 3;
        KainJsonValue* existing = json_registered_handle(payload);
        return existing ? json_clone_value(existing) : json_value_int(payload);
    }
    if (tag == KAIN_JSON_ANY_TAG_BOOL) {
        return json_value_bool((any >> 3) != 0);
    }
    if (tag == KAIN_JSON_ANY_TAG_STRING) {
        const char* text = (const char*)(intptr_t)(any & ~KAIN_JSON_ANY_TAG_MASK);
        return json_value_string_copy(text);
    }
    return json_value_new(KAIN_JSON_NULL);
}

static KainJsonValue* json_clone_value(const KainJsonValue* value) {
    KainJsonValue* out;
    int64_t i;
    if (!value) {
        return json_value_new(KAIN_JSON_NULL);
    }
    out = json_value_new(value->kind);
    if (!out) {
        return NULL;
    }
    out->bool_value = value->bool_value;
    out->int_value = value->int_value;
    if (value->kind == KAIN_JSON_STRING) {
        out->string_value = json_dup_cstr(value->string_value);
    } else if (value->kind == KAIN_JSON_OBJECT && value->field_count > 0) {
        out->fields = (KainJsonEntry*)calloc((size_t)value->field_count, sizeof(KainJsonEntry));
        if (!out->fields) {
            free(out);
            return NULL;
        }
        out->field_capacity = value->field_count;
        out->field_count = value->field_count;
        for (i = 0; i < value->field_count; ++i) {
            out->fields[i].key = json_dup_cstr(value->fields[i].key);
            out->fields[i].value = json_clone_value(value->fields[i].value);
        }
    } else if (value->kind == KAIN_JSON_ARRAY && value->item_count > 0) {
        out->items = (KainJsonValue**)calloc((size_t)value->item_count, sizeof(KainJsonValue*));
        if (!out->items) {
            free(out);
            return NULL;
        }
        out->item_capacity = value->item_count;
        out->item_count = value->item_count;
        for (i = 0; i < value->item_count; ++i) {
            out->items[i] = json_clone_value(value->items[i]);
        }
    }
    return out;
}

static int json_object_reserve(KainJsonValue* object, int64_t needed) {
    int64_t capacity;
    KainJsonEntry* fields;
    if (!object || object->kind != KAIN_JSON_OBJECT) {
        return 0;
    }
    if (needed <= object->field_capacity) {
        return 1;
    }
    capacity = object->field_capacity > 0 ? object->field_capacity * 2 : 8;
    while (capacity < needed) {
        capacity *= 2;
    }
    fields = (KainJsonEntry*)realloc(object->fields, (size_t)capacity * sizeof(KainJsonEntry));
    if (!fields) {
        return 0;
    }
    memset(fields + object->field_capacity, 0, (size_t)(capacity - object->field_capacity) * sizeof(KainJsonEntry));
    object->fields = fields;
    object->field_capacity = capacity;
    return 1;
}

static int json_array_reserve(KainJsonValue* array, int64_t needed) {
    int64_t capacity;
    KainJsonValue** items;
    if (!array || array->kind != KAIN_JSON_ARRAY) {
        return 0;
    }
    if (needed <= array->item_capacity) {
        return 1;
    }
    capacity = array->item_capacity > 0 ? array->item_capacity * 2 : 8;
    while (capacity < needed) {
        capacity *= 2;
    }
    items = (KainJsonValue**)realloc(array->items, (size_t)capacity * sizeof(KainJsonValue*));
    if (!items) {
        return 0;
    }
    memset(items + array->item_capacity, 0, (size_t)(capacity - array->item_capacity) * sizeof(KainJsonValue*));
    array->items = items;
    array->item_capacity = capacity;
    return 1;
}

static KainJsonValue* json_object_get_value(KainJsonValue* object, const char* key) {
    int64_t i;
    if (!object || object->kind != KAIN_JSON_OBJECT || !key) {
        return NULL;
    }
    for (i = 0; i < object->field_count; ++i) {
        if (object->fields[i].key && strcmp(object->fields[i].key, key) == 0) {
            return object->fields[i].value;
        }
    }
    return NULL;
}

static void json_object_set_value(KainJsonValue* object, const char* key, KainJsonValue* value) {
    int64_t i;
    if (!object || object->kind != KAIN_JSON_OBJECT || !key || !value) {
        return;
    }
    for (i = 0; i < object->field_count; ++i) {
        if (object->fields[i].key && strcmp(object->fields[i].key, key) == 0) {
            object->fields[i].value = value;
            return;
        }
    }
    if (!json_object_reserve(object, object->field_count + 1)) {
        return;
    }
    object->fields[object->field_count].key = json_dup_kain_string(key);
    object->fields[object->field_count].value = value;
    object->field_count++;
}

typedef struct JsonBuffer {
    char* data;
    size_t len;
    size_t cap;
} JsonBuffer;

static int json_buffer_reserve(JsonBuffer* buffer, size_t extra) {
    size_t needed;
    size_t capacity;
    char* data;
    if (!buffer) {
        return 0;
    }
    needed = buffer->len + extra + 1u;
    if (needed <= buffer->cap) {
        return 1;
    }
    capacity = buffer->cap ? buffer->cap * 2u : 128u;
    while (capacity < needed) {
        capacity *= 2u;
    }
    data = (char*)realloc(buffer->data, capacity);
    if (!data) {
        return 0;
    }
    buffer->data = data;
    buffer->cap = capacity;
    return 1;
}

static void json_buffer_append_n(JsonBuffer* buffer, const char* text, size_t length) {
    if (!json_buffer_reserve(buffer, length)) {
        return;
    }
    if (length > 0u) {
        memcpy(buffer->data + buffer->len, text, length);
    }
    buffer->len += length;
    buffer->data[buffer->len] = '\0';
}

static void json_buffer_append(JsonBuffer* buffer, const char* text) {
    json_buffer_append_n(buffer, text, text ? strlen(text) : 0u);
}

static void json_buffer_append_char(JsonBuffer* buffer, char ch) {
    json_buffer_append_n(buffer, &ch, 1u);
}

static void json_write_escaped(JsonBuffer* buffer, const char* text) {
    const unsigned char* p = (const unsigned char*)(text ? text : "");
    json_buffer_append_char(buffer, '"');
    while (*p) {
        char escape[7];
        switch (*p) {
            case '"': json_buffer_append(buffer, "\\\""); break;
            case '\\': json_buffer_append(buffer, "\\\\"); break;
            case '\b': json_buffer_append(buffer, "\\b"); break;
            case '\f': json_buffer_append(buffer, "\\f"); break;
            case '\n': json_buffer_append(buffer, "\\n"); break;
            case '\r': json_buffer_append(buffer, "\\r"); break;
            case '\t': json_buffer_append(buffer, "\\t"); break;
            default:
                if (*p < 32u) {
                    snprintf(escape, sizeof(escape), "\\u%04x", (unsigned int)*p);
                    json_buffer_append(buffer, escape);
                } else {
                    json_buffer_append_char(buffer, (char)*p);
                }
                break;
        }
        ++p;
    }
    json_buffer_append_char(buffer, '"');
}

static void json_write_value(JsonBuffer* buffer, const KainJsonValue* value) {
    int64_t i;
    char number[64];
    if (!value) {
        json_buffer_append(buffer, "null");
        return;
    }
    switch (value->kind) {
        case KAIN_JSON_BOOL:
            json_buffer_append(buffer, value->bool_value ? "true" : "false");
            break;
        case KAIN_JSON_INT:
            snprintf(number, sizeof(number), "%lld", (long long)value->int_value);
            json_buffer_append(buffer, number);
            break;
        case KAIN_JSON_STRING:
            json_write_escaped(buffer, value->string_value);
            break;
        case KAIN_JSON_OBJECT:
            json_buffer_append_char(buffer, '{');
            for (i = 0; i < value->field_count; ++i) {
                if (i > 0) {
                    json_buffer_append_char(buffer, ',');
                }
                json_write_escaped(buffer, value->fields[i].key);
                json_buffer_append_char(buffer, ':');
                json_write_value(buffer, value->fields[i].value);
            }
            json_buffer_append_char(buffer, '}');
            break;
        case KAIN_JSON_ARRAY:
            json_buffer_append_char(buffer, '[');
            for (i = 0; i < value->item_count; ++i) {
                if (i > 0) {
                    json_buffer_append_char(buffer, ',');
                }
                json_write_value(buffer, value->items[i]);
            }
            json_buffer_append_char(buffer, ']');
            break;
        case KAIN_JSON_NULL:
        default:
            json_buffer_append(buffer, "null");
            break;
    }
}

static void json_skip_ws(KainJsonParser* parser) {
    while (parser->cursor && isspace((unsigned char)*parser->cursor)) {
        parser->cursor++;
    }
}

static int json_consume(KainJsonParser* parser, char ch) {
    json_skip_ws(parser);
    if (*parser->cursor != ch) {
        return 0;
    }
    parser->cursor++;
    return 1;
}

static char* json_parse_string_raw(KainJsonParser* parser) {
    JsonBuffer buffer = {0};
    if (!json_consume(parser, '"')) {
        return NULL;
    }
    while (*parser->cursor && *parser->cursor != '"') {
        unsigned char ch = (unsigned char)*parser->cursor++;
        if (ch == '\\') {
            ch = (unsigned char)*parser->cursor++;
            switch (ch) {
                case '"': json_buffer_append_char(&buffer, '"'); break;
                case '\\': json_buffer_append_char(&buffer, '\\'); break;
                case '/': json_buffer_append_char(&buffer, '/'); break;
                case 'b': json_buffer_append_char(&buffer, '\b'); break;
                case 'f': json_buffer_append_char(&buffer, '\f'); break;
                case 'n': json_buffer_append_char(&buffer, '\n'); break;
                case 'r': json_buffer_append_char(&buffer, '\r'); break;
                case 't': json_buffer_append_char(&buffer, '\t'); break;
                case 'u':
                    if (isxdigit((unsigned char)parser->cursor[0]) &&
                        isxdigit((unsigned char)parser->cursor[1]) &&
                        isxdigit((unsigned char)parser->cursor[2]) &&
                        isxdigit((unsigned char)parser->cursor[3])) {
                        json_buffer_append_char(&buffer, '?');
                        parser->cursor += 4;
                    }
                    break;
                default:
                    json_buffer_append_char(&buffer, (char)ch);
                    break;
            }
        } else {
            json_buffer_append_char(&buffer, (char)ch);
        }
    }
    if (*parser->cursor == '"') {
        parser->cursor++;
    }
    if (!buffer.data) {
        return json_dup_range("", 0u);
    }
    return buffer.data;
}

static KainJsonValue* json_parse_value_inner(KainJsonParser* parser);

static KainJsonValue* json_parse_object(KainJsonParser* parser) {
    KainJsonValue* object = json_value_new(KAIN_JSON_OBJECT);
    if (!object || !json_consume(parser, '{')) {
        return object;
    }
    json_skip_ws(parser);
    if (*parser->cursor == '}') {
        parser->cursor++;
        return object;
    }
    while (*parser->cursor) {
        char* key;
        KainJsonValue* value;
        json_skip_ws(parser);
        key = json_parse_string_raw(parser);
        json_consume(parser, ':');
        value = json_parse_value_inner(parser);
        json_object_set_value(object, key ? key : "", value ? value : json_value_new(KAIN_JSON_NULL));
        free(key);
        json_skip_ws(parser);
        if (*parser->cursor == '}') {
            parser->cursor++;
            break;
        }
        if (!json_consume(parser, ',')) {
            break;
        }
    }
    return object;
}

static KainJsonValue* json_parse_array(KainJsonParser* parser) {
    KainJsonValue* array = json_value_new(KAIN_JSON_ARRAY);
    if (!array || !json_consume(parser, '[')) {
        return array;
    }
    json_skip_ws(parser);
    if (*parser->cursor == ']') {
        parser->cursor++;
        return array;
    }
    while (*parser->cursor) {
        KainJsonValue* value = json_parse_value_inner(parser);
        if (json_array_reserve(array, array->item_count + 1)) {
            array->items[array->item_count++] = value ? value : json_value_new(KAIN_JSON_NULL);
        }
        json_skip_ws(parser);
        if (*parser->cursor == ']') {
            parser->cursor++;
            break;
        }
        if (!json_consume(parser, ',')) {
            break;
        }
    }
    return array;
}

static KainJsonValue* json_parse_number(KainJsonParser* parser) {
    char* end = NULL;
    long long value;
    json_skip_ws(parser);
    value = strtoll(parser->cursor, &end, 10);
    if (end == parser->cursor) {
        return json_value_new(KAIN_JSON_NULL);
    }
    parser->cursor = end;
    return json_value_int((int64_t)value);
}

static KainJsonValue* json_parse_value_inner(KainJsonParser* parser) {
    char* text;
    json_skip_ws(parser);
    if (*parser->cursor == '{') {
        return json_parse_object(parser);
    }
    if (*parser->cursor == '[') {
        return json_parse_array(parser);
    }
    if (*parser->cursor == '"') {
        text = json_parse_string_raw(parser);
        return json_value_string_owned(text);
    }
    if (strncmp(parser->cursor, "true", 4u) == 0) {
        parser->cursor += 4;
        return json_value_bool(1);
    }
    if (strncmp(parser->cursor, "false", 5u) == 0) {
        parser->cursor += 5;
        return json_value_bool(0);
    }
    if (strncmp(parser->cursor, "null", 4u) == 0) {
        parser->cursor += 4;
        return json_value_new(KAIN_JSON_NULL);
    }
    return json_parse_number(parser);
}

int64_t json_parse(const char* text) {
    KainJsonParser parser;
    parser.cursor = text ? text : "";
    return json_handle_from_value(json_parse_value_inner(&parser));
}

char* json_string(int64_t value) {
    JsonBuffer buffer = {0};
    KainJsonValue* json;
    char* out;
    if ((value & KAIN_JSON_ANY_TAG_MASK) != 0) {
        json = json_value_from_any(value);
    } else {
        json = json_value_from_handle(value);
    }
    json_write_value(&buffer, json);
    if (!buffer.data) {
        return string_new("null");
    }
    out = string_new(buffer.data);
    free(buffer.data);
    return out;
}

int64_t json_object_new(void) {
    return json_handle_from_value(json_value_new(KAIN_JSON_OBJECT));
}

void json_object_set(int64_t object, const char* key, int64_t value) {
    json_object_set_value(json_value_from_handle(object), key, json_value_from_any(value));
}

bool json_has(int64_t object, const char* key) {
    return json_object_get_value(json_value_from_handle(object), key) != NULL;
}

int64_t json_get(int64_t object, const char* key) {
    KainJsonValue* value = json_object_get_value(json_value_from_handle(object), key);
    if (!value) {
        return KAIN_JSON_ANY_TAG_NULL;
    }
    if (value->kind == KAIN_JSON_INT) {
        return (value->int_value << 3) | KAIN_JSON_ANY_TAG_INT;
    }
    if (value->kind == KAIN_JSON_BOOL) {
        return ((int64_t)(value->bool_value != 0) << 3) | KAIN_JSON_ANY_TAG_BOOL;
    }
    if (value->kind == KAIN_JSON_STRING) {
        return ((int64_t)(intptr_t)value->string_value) | KAIN_JSON_ANY_TAG_STRING;
    }
    return json_handle_from_value(value);
}

char* json_get_string(int64_t object, const char* key) {
    KainJsonValue* value = json_object_get_value(json_value_from_handle(object), key);
    if (!value) {
        return string_new("");
    }
    if (value->kind == KAIN_JSON_STRING) {
        return string_new(value->string_value ? value->string_value : "");
    }
    return json_string(json_get(object, key));
}

int64_t json_get_int(int64_t object, const char* key) {
    KainJsonValue* value = json_object_get_value(json_value_from_handle(object), key);
    if (!value) {
        return 0;
    }
    if (value->kind == KAIN_JSON_INT) {
        return value->int_value;
    }
    if (value->kind == KAIN_JSON_BOOL) {
        return value->bool_value != 0;
    }
    return 0;
}

bool json_get_bool(int64_t object, const char* key) {
    KainJsonValue* value = json_object_get_value(json_value_from_handle(object), key);
    if (!value) {
        return false;
    }
    if (value->kind == KAIN_JSON_BOOL) {
        return value->bool_value != 0;
    }
    if (value->kind == KAIN_JSON_INT) {
        return value->int_value != 0;
    }
    return false;
}

int64_t json_array_new(void) {
    return json_handle_from_value(json_value_new(KAIN_JSON_ARRAY));
}

void json_array_push(int64_t array, int64_t value) {
    KainJsonValue* json_array = json_value_from_handle(array);
    if (!json_array || json_array->kind != KAIN_JSON_ARRAY) {
        return;
    }
    if (!json_array_reserve(json_array, json_array->item_count + 1)) {
        return;
    }
    json_array->items[json_array->item_count++] = json_value_from_any(value);
}

int64_t json_array_len(int64_t array) {
    KainJsonValue* json_array = json_value_from_handle(array);
    if (!json_array || json_array->kind != KAIN_JSON_ARRAY) {
        return 0;
    }
    return json_array->item_count;
}

int64_t json_array_get(int64_t array, int64_t index) {
    KainJsonValue* json_array = json_value_from_handle(array);
    KainJsonValue* value;
    if (!json_array || json_array->kind != KAIN_JSON_ARRAY || index < 0 || index >= json_array->item_count) {
        return KAIN_JSON_ANY_TAG_NULL;
    }
    value = json_array->items[index];
    if (!value) {
        return KAIN_JSON_ANY_TAG_NULL;
    }
    if (value->kind == KAIN_JSON_INT) {
        return (value->int_value << 3) | KAIN_JSON_ANY_TAG_INT;
    }
    if (value->kind == KAIN_JSON_BOOL) {
        return ((int64_t)(value->bool_value != 0) << 3) | KAIN_JSON_ANY_TAG_BOOL;
    }
    if (value->kind == KAIN_JSON_STRING) {
        return ((int64_t)(intptr_t)value->string_value) | KAIN_JSON_ANY_TAG_STRING;
    }
    return json_handle_from_value(value);
}
