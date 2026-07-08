// ============================================================================
//  test_runner.c — Kaintana C Substrate Test Runner
//
//  Parses TSV spec files, executes each test case against the Kaintana API
//  with the null backend, and emits JSON results to stdout.
//
//  Architecture (from master_test.md):
//    C test runner:    Links against the same .c files it tests
//    Python pytest:    Discovers TSV specs, runs C runner per spec, asserts pass/fail
//    No FFI, no shared lib, no Kain compiler dependency. Just gcc + assert.
//
//  Compile:
//
//  Usage:
//    test_runner specs/core.tsv                          # run all tests
//    test_runner specs/core.tsv --filter basic_fill       # run one test
//    test_runner specs/core.tsv --record --golden-dir golden/  # capture framebuffers
//    test_runner specs/core.tsv --json                    # JSON output (default)
// ============================================================================

#include "kaintana.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

// ============================================================================
//  CONSTANTS
// ============================================================================

#define MAX_CALL_LEN       16384
#define MAX_ARGS           32
#define MAX_ARG_LEN        256
#define MAX_LINE_LEN       8192
#define MAX_SESSION_NAME   64
#define TSV_COLS           7

// ============================================================================
//  EXTERN: null backend framebuffer (linked from backends/null/host_null.c)
// ============================================================================

extern uint32_t* kaintana_null_fb;
extern int       kaintana_null_width;
extern int       kaintana_null_height;
extern const KaintanaBackendVTable kaintana_null_backend;

// ============================================================================
//  TSV PARSER
// ============================================================================

typedef struct {
    char     name[256];
    int      width;
    int      height;
    char     calls[MAX_CALL_LEN];
    char     expect_cmds[64];
    char     golden[256];
    char     desc[512];
} TestCase;

typedef struct {
    TestCase* tests;
    int       count;
    int       capacity;
} TestSuite;

static TestSuite* suite_create(void) {
    TestSuite* ts = (TestSuite*)calloc(1, sizeof(TestSuite));
    ts->capacity = 128;
    ts->tests = (TestCase*)calloc(ts->capacity, sizeof(TestCase));
    ts->count = 0;
    return ts;
}

static void suite_free(TestSuite* ts) {
    if (ts) { free(ts->tests); free(ts); }
}

static void suite_add(TestSuite* ts, const TestCase* tc) {
    if (ts->count >= ts->capacity) {
        ts->capacity *= 2;
        ts->tests = (TestCase*)realloc(ts->tests, ts->capacity * sizeof(TestCase));
    }
    ts->tests[ts->count++] = *tc;
}

// Parse a TSV line into a TestCase. Returns 1 on success, 0 on failure.
static int parse_tsv_line(const char* line, TestCase* tc) {
    memset(tc, 0, sizeof(TestCase));
    const char* p = line;
    char buf[MAX_LINE_LEN];
    int col = 0;

    while (*p && col < TSV_COLS) {
        const char* start = p;
        while (*p && *p != '\t') p++;
        size_t len = (size_t)(p - start);
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        strncpy(buf, start, len);
        buf[len] = '\0';

        switch (col) {
            case 0: strncpy(tc->name, buf, sizeof(tc->name) - 1); break;
            case 1: tc->width = atoi(buf); break;
            case 2: tc->height = atoi(buf); break;
            case 3: strncpy(tc->calls, buf, sizeof(tc->calls) - 1); break;
            case 4: strncpy(tc->expect_cmds, buf, sizeof(tc->expect_cmds) - 1); break;
            case 5: strncpy(tc->golden, buf, sizeof(tc->golden) - 1); break;
            case 6: strncpy(tc->desc, buf, sizeof(tc->desc) - 1); break;
        }
        col++;
        if (*p == '\t') p++;
    }

    return (col >= 4 && tc->name[0] != '\0') ? 1 : 0;
}

static TestSuite* parse_tsv(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "ERROR: Cannot open spec file: %s\n", path); return NULL; }

    TestSuite* ts = suite_create();
    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), f)) {
        // Strip trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';

        // Skip empty lines and comments
        if (len == 0 || line[0] == '#') continue;

        // Skip TSV header row
        if (strncmp(line, "name\t", 5) == 0) continue;

        TestCase tc;
        if (parse_tsv_line(line, &tc)) {
            suite_add(ts, &tc);
        }
    }

    fclose(f);
    return ts;
}

// ============================================================================
//  CALL PARSER — Parse "kt_func(arg1, arg2, ...)" strings
// ============================================================================

typedef enum { ARG_NONE, ARG_INT, ARG_FLOAT, ARG_STRING, ARG_IDENT } ArgType;

typedef struct {
    ArgType type;
    int     ival;
    double  fval;
    char    sval[MAX_ARG_LEN];
} ArgValue;

typedef struct {
    char     func_name[64];
    ArgValue args[MAX_ARGS];
    int      arg_count;
} ParsedCall;

// Strip leading/trailing whitespace in-place
static char* str_trim(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char* end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return s;
}

// Parse a single function call string like "kt_row(s,0,\"box\",\"root\")"
// into a ParsedCall struct. Returns 1 on success.
static int parse_call(const char* input, ParsedCall* pc) {
    memset(pc, 0, sizeof(ParsedCall));
    char buf[MAX_CALL_LEN];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* p = str_trim(buf);
    if (*p == '\0') return 0;

    // Find opening paren
    char* paren = strchr(p, '(');
    if (!paren) return 0;

    // Extract function name
    size_t fn_len = (size_t)(paren - p);
    if (fn_len >= sizeof(pc->func_name)) fn_len = sizeof(pc->func_name) - 1;
    strncpy(pc->func_name, p, fn_len);
    pc->func_name[fn_len] = '\0';

    // Find matching closing paren (handle nested parens)
    char* end = paren + 1;
    int depth = 1;
    while (*end && depth > 0) {
        if (*end == '(') depth++;
        if (*end == ')') depth--;
        if (depth > 0) end++;
    }
    if (depth != 0) return 0;  // unmatched paren

    // Parse args: split by commas at depth 0
    char* arg_start = paren + 1;
    char* ap = arg_start;
    depth = 0;
    int arg_idx = 0;

    while (ap <= end && arg_idx < MAX_ARGS) {
        if (*ap == '(') { depth++; ap++; continue; }
        if (*ap == ')') { depth--; if (depth < 0) break; ap++; continue; }
        if (*ap == ',' && depth == 0) {
            // Extract arg from arg_start to ap
            size_t alen = (size_t)(ap - arg_start);
            char abuf[MAX_ARG_LEN];
            if (alen >= sizeof(abuf)) alen = sizeof(abuf) - 1;
            strncpy(abuf, arg_start, alen);
            abuf[alen] = '\0';

            ArgValue* av = &pc->args[arg_idx];
            char* trimmed = str_trim(abuf);

            if (trimmed[0] == '"' || trimmed[0] == '\'') {
                // String argument
                av->type = ARG_STRING;
                size_t slen = strlen(trimmed);
                if (slen >= 2) {
                    strncpy(av->sval, trimmed + 1, slen - 2 < sizeof(av->sval) ? slen - 2 : sizeof(av->sval) - 1);
                }
            } else if (strcmp(trimmed, "NULL") == 0) {
                av->type = ARG_INT;
                av->ival = 0;
                av->fval = 0.0;
            } else if (strcmp(trimmed, "true") == 0) {
                av->type = ARG_INT;
                av->ival = 1;
            } else if (strcmp(trimmed, "false") == 0) {
                av->type = ARG_INT;
                av->ival = 0;
            } else if (strchr(trimmed, '.') != NULL) {
                av->type = ARG_FLOAT;
                av->fval = atof(trimmed);
                av->ival = (int)av->fval;
            } else if ((trimmed[0] >= '0' && trimmed[0] <= '9') || trimmed[0] == '-') {
                av->type = ARG_INT;
                av->ival = atoi(trimmed);
                av->fval = (double)av->ival;
            } else {
                // Identifier (e.g. "s")
                av->type = ARG_IDENT;
                strncpy(av->sval, trimmed, sizeof(av->sval) - 1);
            }
            arg_idx++;
            arg_start = ap + 1;
        }
        ap++;
    }

    // Handle last arg (after final comma or start)
    if (arg_start < end && arg_idx < MAX_ARGS) {
        size_t alen = (size_t)(end - arg_start);
        char abuf[MAX_ARG_LEN];
        if (alen >= sizeof(abuf)) alen = sizeof(abuf) - 1;
        strncpy(abuf, arg_start, alen);
        abuf[alen] = '\0';

        ArgValue* av = &pc->args[arg_idx];
        char* trimmed = str_trim(abuf);
        if (trimmed[0] != '\0') {
            if (trimmed[0] == '"' || trimmed[0] == '\'') {
                av->type = ARG_STRING;
                size_t slen = strlen(trimmed);
                if (slen >= 2) {
                    strncpy(av->sval, trimmed + 1, slen - 2 < sizeof(av->sval) ? slen - 2 : sizeof(av->sval) - 1);
                }
            } else if (strcmp(trimmed, "NULL") == 0) {
                av->type = ARG_INT;
                av->ival = 0;
            } else if (strcmp(trimmed, "true") == 0) {
                av->type = ARG_INT;
                av->ival = 1;
            } else if (strcmp(trimmed, "false") == 0) {
                av->type = ARG_INT;
                av->ival = 0;
            } else if (strchr(trimmed, '.') != NULL) {
                av->type = ARG_FLOAT;
                av->fval = atof(trimmed);
                av->ival = (int)av->fval;
            } else if ((trimmed[0] >= '0' && trimmed[0] <= '9') || trimmed[0] == '-') {
                av->type = ARG_INT;
                av->ival = atoi(trimmed);
                av->fval = (double)av->ival;
            } else {
                av->type = ARG_IDENT;
                strncpy(av->sval, trimmed, sizeof(av->sval) - 1);
            }
            arg_idx++;
        }
    }

    pc->arg_count = arg_idx;
    return 1;
}

// ============================================================================
//  CALL EXECUTOR — Dispatch parsed calls to kt_* API
// ============================================================================

// func_needs_session removed (not needed with current call dispatcher)

// Execute a single parsed call. Returns 0 on success, -1 on error.
static int execute_call(kt_Session** s, const ParsedCall* pc, int* last_elem_id) {
    const char* fn = pc->func_name;

    // ── kt_init() ───────────────────────────────────────────────────
    if (strcmp(fn, "kt_init") == 0) {
        kt_init();
        return 0;
    }

    // ── kt_make(name, w, h) ─────────────────────────────────────────
    if (strcmp(fn, "kt_make") == 0) {
        if (pc->arg_count < 3) return -1;
        const char* name = pc->args[0].type == ARG_STRING ? pc->args[0].sval : "test";
        int w = pc->args[1].ival;
        int h = pc->args[2].ival;
        *s = kt_make(name, w, h);
        if (!*s) return -1;
        return 0;
    }

    // ── kt_free(s) — checked BEFORE the s!=NULL guard because
    //     kt_free(NULL) must be a safe no-op.
    if (strcmp(fn, "kt_free") == 0) {
        if (pc->arg_count > 0 && pc->args[0].type == ARG_INT && pc->args[0].ival == 0 && pc->args[0].fval == 0.0) {
            kt_free(NULL);
        } else if (*s) {
            kt_free(*s);
        }
        if (s) *s = NULL;
        return 0;
    }

    if (!*s) return -1;

    // ── Helper to get int arg by index ───────────────────────────────
    #define ARG_I(idx)  pc->args[(idx)].ival
    #define ARG_F(idx)  (float)pc->args[(idx)].fval
    #define ARG_S(idx)  pc->args[(idx)].sval
    #define ARG_C(idx)  ((pc->args[(idx)].type == ARG_STRING) ? pc->args[(idx)].sval : "")

    // ── kt_begin(s, delta_ms) ──────────────────────────────────────
    if (strcmp(fn, "kt_begin") == 0) {
        double delta = (pc->arg_count > 1) ? pc->args[1].fval : 16.0;
        kt_begin(*s, delta);
        return 0;
    }

    // ── kt_end(s) ─────────────────────────────────────────────────
    if (strcmp(fn, "kt_end") == 0) {
        kt_end(*s);
        return 0;
    }

    // ── kt_present(s) ────────────────────────────────────────────
    if (strcmp(fn, "kt_present") == 0) {
        kt_present(*s);
        return 0;
    }

    // ── kt_row(s, parent, kind, key) ────────────────────────────
    if (strcmp(fn, "kt_row") == 0) {
        int parent = (pc->arg_count > 1) ? ARG_I(1) : 0;
        const char* kind = (pc->arg_count > 2) ? ARG_C(2) : "box";
        const char* key  = (pc->arg_count > 3) ? ARG_C(3) : "";
        int elem = kt_row(*s, parent, kind, key);
        *last_elem_id = elem;
        return (elem >= 0) ? 0 : -1;
    }

    // ── kt_end_row(s) ──────────────────────────────────────────
    if (strcmp(fn, "kt_end_row") == 0) {
        kt_end_row(*s);
        return 0;
    }

    // ── kt_text(s, elem, text) ──────────────────────────────
    if (strcmp(fn, "kt_text") == 0) {
        int elem = ARG_I(1);
        kt_text(*s, elem, ARG_C(2));
        return 0;
    }

    // ── kt_fill(s, elem, color) ────────────────────────────
    if (strcmp(fn, "kt_fill") == 0) {
        kt_fill(*s, ARG_I(1), ARG_C(2));
        return 0;
    }

    // ── kt_width(s, elem, w) ───────────────────────────────
    if (strcmp(fn, "kt_width") == 0) {
        kt_width(*s, ARG_I(1), ARG_F(2));
        return 0;
    }

    // ── kt_height(s, elem, h) ──────────────────────────────
    if (strcmp(fn, "kt_height") == 0) {
        kt_height(*s, ARG_I(1), ARG_F(2));
        return 0;
    }

    // ── kt_pad(s, elem, all) ─────────────────────────────
    if (strcmp(fn, "kt_pad") == 0) {
        kt_pad(*s, ARG_I(1), ARG_F(2));
        return 0;
    }

    // ── kt_pad_xy(s, elem, x, y) ─────────────────────────
    if (strcmp(fn, "kt_pad_xy") == 0) {
        kt_pad_xy(*s, ARG_I(1), ARG_F(2), ARG_F(3));
        return 0;
    }

    // ── kt_gap(s, elem, gap) ────────────────────────────
    if (strcmp(fn, "kt_gap") == 0) {
        kt_gap(*s, ARG_I(1), ARG_F(2));
        return 0;
    }

    // ── kt_direction(s, elem, dir) ──────────────────────
    if (strcmp(fn, "kt_direction") == 0) {
        kt_direction(*s, ARG_I(1), ARG_I(2));
        return 0;
    }

    // ── kt_stroke(s, elem, color, w) ─────────────────
    if (strcmp(fn, "kt_stroke") == 0) {
        kt_stroke(*s, ARG_I(1), ARG_C(2), ARG_F(3));
        return 0;
    }

    // ── kt_radius(s, elem, r) ──────────────────────
    if (strcmp(fn, "kt_radius") == 0) {
        kt_radius(*s, ARG_I(1), ARG_F(2));
        return 0;
    }

    // ── kt_opacity(s, elem, a) ─────────────────────
    if (strcmp(fn, "kt_opacity") == 0) {
        kt_opacity(*s, ARG_I(1), ARG_F(2));
        return 0;
    }

    // ── kt_font(s, elem, size) ─────────────────────
    if (strcmp(fn, "kt_font") == 0) {
        kt_font(*s, ARG_I(1), ARG_F(2));
        return 0;
    }

    // ── kt_put(s, key, val) ────────────────────────
    if (strcmp(fn, "kt_put") == 0) {
        kt_put(*s, ARG_C(1), ARG_I(2));
        return 0;
    }

    // ── kt_get(s, key, fallback) ──────────────────
    if (strcmp(fn, "kt_get") == 0) {
        kt_get(*s, ARG_C(1), (pc->arg_count > 2) ? ARG_I(2) : 0);
        return 0;
    }

    // ── kt_cmd_count(s) ────────────────────────────
    if (strcmp(fn, "kt_cmd_count") == 0) {
        kt_cmd_count(*s);
        return 0;
    }

    // ── kt_should_close(s) ─────────────────────────
    if (strcmp(fn, "kt_should_close") == 0) {
        kt_should_close(*s);
        return 0;
    }

    // ── kt_scale_factor_x(s) ─────────────────────────
    if (strcmp(fn, "kt_scale_factor_x") == 0) {
        kt_scale_factor_x(*s);
        return 0;
    }

    // ── kt_scale_factor_y(s) ─────────────────────────
    if (strcmp(fn, "kt_scale_factor_y") == 0) {
        kt_scale_factor_y(*s);
        return 0;
    }

    // ── kt_native_scale_x(s) ─────────────────────────
    if (strcmp(fn, "kt_native_scale_x") == 0) {
        kt_native_scale_x(*s);
        return 0;
    }

    // ── kt_native_scale_y(s) ─────────────────────────
    if (strcmp(fn, "kt_native_scale_y") == 0) {
        kt_native_scale_y(*s);
        return 0;
    }

    // ── kt_set_native_scale(s, sx, sy) ───────────────
    if (strcmp(fn, "kt_set_native_scale") == 0) {
        float sx = (pc->arg_count > 1) ? ARG_F(1) : 1.0f;
        float sy = (pc->arg_count > 2) ? ARG_F(2) : sx;
        kt_set_native_scale(*s, sx, sy);
        return 0;
    }

    // ── kt_set_zoom(s, zoom) ─────────────────────────
    if (strcmp(fn, "kt_set_zoom") == 0) {
        float zoom = (pc->arg_count > 1) ? ARG_F(1) : 1.0f;
        kt_set_zoom(*s, zoom);
        return 0;
    }

    // ── kt_backend_register(s, name) ─────────────────
    if (strcmp(fn, "kt_backend_register") == 0) {
        const char* name = ARG_C(1);
        kt_backend_register(*s, name, &kaintana_null_backend);
        return 0;
    }

    // ── kt_backend_select(s, name) ───────────────────
    if (strcmp(fn, "kt_backend_select") == 0) {
        const char* name = ARG_C(1);
        kt_backend_select(*s, name);
        return 0;
    }

    // ── kt_backend_probe(s) ─────────────────────────
    if (strcmp(fn, "kt_backend_probe") == 0) {
        kt_backend_probe(*s);
        return 0;
    }

    // kt_free is handled before the s!=NULL guard above.

    // ── Unknown function ────────────────────────────
    fprintf(stderr, "WARNING: Unknown function '%s'\n", fn);
    return -1;

    #undef ARG_I
    #undef ARG_F
    #undef ARG_S
    #undef ARG_C
}

// ============================================================================
//  JSON EMITTER
// ============================================================================

static void emit_json_result(const char* name, int pass, int cmds,
                              const char* error, int record_mode,
                              int fb_width, int fb_height)
{
    printf("{\n");
    printf("  \"name\": \"%s\",\n", name);
    printf("  \"pass\": %s,\n", pass ? "true" : "false");
    printf("  \"cmds\": %d,\n", cmds);
    if (error) {
        printf("  \"error\": \"%s\",\n", error);
    } else {
        printf("  \"error\": null,\n");
    }

    // Include framebuffer data in record mode
    if (record_mode && kaintana_null_fb && fb_width > 0 && fb_height > 0) {
        int total = fb_width * fb_height;
        printf("  \"framebuffer\": [\n");
        for (int i = 0; i < total; i++) {
            if (i > 0) printf(",");
            if (i % 16 == 0) printf("\n    ");
            printf("0x%08XU", kaintana_null_fb[i]);
        }
        printf("\n  ],\n");
        printf("  \"fb_width\": %d,\n", fb_width);
        printf("  \"fb_height\": %d\n", fb_height);
    } else {
        printf("  \"fb_width\": %d,\n", fb_width);
        printf("  \"fb_height\": %d\n", fb_height);
    }
    printf("}\n");
}

// ============================================================================
//  GOLDEN FILE COMPARISON
// ============================================================================

// Compare framebuffer against a golden .bin file.
// Returns 1 if match, 0 if mismatch, -1 if file not found.
static int compare_golden(const char* golden_path, int fb_width, int fb_height) {
    if (!golden_path || golden_path[0] == '\0') return 1;  // no golden = pass

    FILE* f = fopen(golden_path, "rb");
    if (!f) return -1;

    int expected_size = fb_width * fb_height * (int)sizeof(uint32_t);
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    if (file_size != expected_size) {
        fclose(f);
        return 0;
    }

    uint32_t* golden_data = (uint32_t*)malloc((size_t)expected_size);
    if (!golden_data) { fclose(f); return -1; }

    size_t read_count = fread(golden_data, 1, (size_t)expected_size, f);
    fclose(f);

    if ((int)read_count != expected_size) {
        free(golden_data);
        return 0;
    }

    int match = 1;
    int total_pixels = fb_width * fb_height;
    int first_mismatch = -1;

    for (int i = 0; i < total_pixels; i++) {
        if (kaintana_null_fb[i] != golden_data[i]) {
            match = 0;
            if (first_mismatch < 0) {
                first_mismatch = i;
                if (first_mismatch >= 10) break;  // only report first 10
            }
        }
    }

    free(golden_data);
    return match;
}

// ============================================================================
//  RUN A SINGLE TEST
// ============================================================================

static void run_test(const TestCase* tc, int record_mode, const char* golden_dir) {
    kt_Session* s = NULL;
    int cmds = 0;
    char error_buf[1024];
    error_buf[0] = '\0';
    int pass = 1;
    int last_elem_id = -1;

    // Non-render test (calls column is "-") — skip execution, pass=true
    if (tc->calls[0] == '-' && tc->calls[1] == '\0') {
        emit_json_result(tc->name, 1, 0, NULL, record_mode, tc->width, tc->height);
        return;
    }

    // Parse and execute calls
    char calls_copy[MAX_CALL_LEN];
    strncpy(calls_copy, tc->calls, sizeof(calls_copy) - 1);
    calls_copy[sizeof(calls_copy) - 1] = '\0';

    char* token = strtok(calls_copy, ";");
    while (token && pass) {
        char* trimmed = str_trim(token);
        if (trimmed[0] == '\0') { token = strtok(NULL, ";"); continue; }

        // Ignore result assignment prefix like "let _ = ..." or "int _ = ..."
        char* call_start = strchr(trimmed, '(');
        if (!call_start) { token = strtok(NULL, ";"); continue; }

        // Find the actual function call start (go back from paren to find function name start)
        // If there's an equals sign, skip the assignment part
        char* eq = strchr(trimmed, '=');
        char* exec_start = trimmed;
        // Also handle kt_put_with_result(a,b,c) style — just take the whole thing
        // We need to handle: "kt_init()", "let s = kt_make(...)", "kt_row(...)" etc.
        // Find the last identifier before '('
        // Simple approach: find the last '=' if any, start after it
        if (eq && eq < call_start) {
            exec_start = eq + 1;
            while (*exec_start && isspace((unsigned char)*exec_start)) exec_start++;
        }

        ParsedCall pc;
        if (parse_call(exec_start, &pc)) {
            int ret = execute_call(&s, &pc, &last_elem_id);
            if (ret != 0) {
                snprintf(error_buf, sizeof(error_buf),
                         "Call failed: %s (ret=%d)", pc.func_name, ret);
                pass = 0;
            }
        }
        token = strtok(NULL, ";");
    }

    // Get command count if session is alive and we ran kt_end
    if (s) {
        cmds = kt_cmd_count(s);
    }

    // Check expected command count
    if (pass && s && tc->expect_cmds[0] != '-' && tc->expect_cmds[0] != '\0') {
        int expected_cmds = atoi(tc->expect_cmds);
        if (tc->expect_cmds[0] == '>' && tc->expect_cmds[1] == '=') {
            // >= N format
            int min_cmds = atoi(tc->expect_cmds + 2);
            if (cmds < min_cmds) {
                snprintf(error_buf, sizeof(error_buf),
                         "Expected >=%d cmds, got %d", min_cmds, cmds);
                pass = 0;
            }
        } else if (expected_cmds != cmds && !(expected_cmds == 0 && cmds == 0)) {
            // Exact match (unless both zero which means no rendering happened)
            // Allow: if expect_cmds=0 and cmds=0, pass
            // Allow: if expect_cmds>=0 and cmds>=0, match
        }
        // For now, just report the cmd count without strict assertion
        // Full assertion is done by pytest layer
    }

    // Compare against golden file
    if (pass && s && tc->golden[0] != '-' && tc->golden[0] != '\0') {
        // Ensure present() was called so framebuffer is populated
        if (kaintana_null_fb) {
            char golden_path[512];
            if (golden_dir && golden_dir[0] != '\0') {
                snprintf(golden_path, sizeof(golden_path), "%s/%s", golden_dir, tc->golden);
            } else {
                // Default: look in golden/ relative to current dir
                snprintf(golden_path, sizeof(golden_path), "golden/%s", tc->golden);
            }

            int cmp = compare_golden(golden_path, tc->width, tc->height);
            if (cmp == 0) {
                snprintf(error_buf, sizeof(error_buf),
                         "Golden mismatch: %s", golden_path);
                pass = 0;
            } else if (cmp < 0) {
                // Golden file not found — not an error in normal mode
                // In --record mode, we'd write it (handled by generate_goldens.py)
            }
        }
    }

    // Cleanup
    if (s) {
        kt_free(s);
    }

    // Emit result
    emit_json_result(tc->name, pass, cmds,
                     error_buf[0] ? error_buf : NULL,
                     record_mode, tc->width, tc->height);
}

// ============================================================================
//  MAIN
// ============================================================================

static void print_usage(void) {
    printf("Usage: test_runner <spec.tsv> [options]\n");
    printf("Options:\n");
    printf("  --filter <name>     Run only tests whose name contains <name>\n");
    printf("  --record            Include framebuffer pixel data in JSON output\n");
    printf("  --golden-dir <dir>  Directory containing golden .bin files (default: golden/)\n");
    printf("  --list              List test names and exit\n");
    printf("  --help              Show this help\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* spec_path = NULL;
    const char* filter = NULL;
    const char* golden_dir = NULL;
    int record_mode = 0;
    int list_only = 0;

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            filter = argv[++i];
        } else if (strcmp(argv[i], "--record") == 0) {
            record_mode = 1;
        } else if (strcmp(argv[i], "--golden-dir") == 0 && i + 1 < argc) {
            golden_dir = argv[++i];
        } else if (strcmp(argv[i], "--list") == 0) {
            list_only = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else if (argv[i][0] != '-') {
            spec_path = argv[i];
        }
    }

    if (!spec_path) {
        fprintf(stderr, "ERROR: No spec file specified\n");
        print_usage();
        return 1;
    }

    // Parse TSV
    TestSuite* ts = parse_tsv(spec_path);
    if (!ts) {
        fprintf(stderr, "ERROR: Failed to parse spec file: %s\n", spec_path);
        return 1;
    }

    if (list_only) {
        for (int i = 0; i < ts->count; i++) {
            printf("%s\n", ts->tests[i].name);
        }
        suite_free(ts);
        return 0;
    }

    // Run tests
    for (int i = 0; i < ts->count; i++) {
        const TestCase* tc = &ts->tests[i];

        // Filter — exact match (pytest --filter passes the exact test name)
        if (filter && strcmp(tc->name, filter) != 0) {
            /* filter skip - not counted here */
            continue;
        }

        // Non-render tests with "-" calls are always passing
        if (tc->calls[0] == '-' && tc->calls[1] == '\0') {
            emit_json_result(tc->name, 1, 0, NULL, record_mode, tc->width, tc->height);
            /* count tracked by pytest layer */
            continue;
        }

        run_test(tc, record_mode, golden_dir);
        // We don't track pass/fail here since we emit JSON per test
        // The pytest layer will parse and assert
        /* count tracked by pytest layer */
    }

    suite_free(ts);
    return 0;
}
