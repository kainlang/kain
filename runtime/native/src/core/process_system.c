#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/process_system.h"
#include "../../include/attrition.h"
#include "../../include/base.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>

#ifdef _WIN32
#include <wchar.h>
#else
#include <strings.h>
#endif

void* kain_alloc_rc(size_t size, long long type_tag);

typedef enum KainNativeProcessStdioMode {
    ABI_PROCESS_STDIO_INHERIT = 0,
    ABI_PROCESS_STDIO_PIPE = 1,
    ABI_PROCESS_STDIO_NULL = 2
} KainNativeProcessStdioMode;

typedef struct KainNativeProcessEnvironmentEntry {
    int in_use;
    char key[ABI_PROCESS_MAX_KEY];
    char value[ABI_PROCESS_MAX_VALUE];
} KainNativeProcessEnvironmentEntry;

typedef struct KainNativeProcessSpec {
    int in_use;
    int64_t id;
    int64_t inherit_environment;
    char executable[ABI_PROCESS_MAX_PATH];
    char current_working_directory[ABI_PROCESS_MAX_PATH];
    char arguments[ABI_PROCESS_MAX_ARGUMENTS][ABI_PROCESS_MAX_VALUE];
    int64_t argument_count;
    KainNativeProcessEnvironmentEntry environment[ABI_PROCESS_MAX_ENVIRONMENT_ENTRIES];
    int64_t environment_count;
    KainNativeProcessStdioMode stdin_mode;
    KainNativeProcessStdioMode stdout_mode;
    KainNativeProcessStdioMode stderr_mode;
} KainNativeProcessSpec;

typedef struct KainNativeProcessCapture {
    unsigned char* bytes;
    size_t length;
    size_t capacity;
} KainNativeProcessCapture;

typedef struct KainNativeProcessHandle {
    int in_use;
    int is_pty;
    int exited;
    int64_t id;
    int64_t exit_code;
    int64_t operating_system_process_id;
    KainNativeProcessCapture stdout_capture;
    KainNativeProcessCapture stderr_capture;
    KainNativeProcessCapture pty_capture;
#ifdef _WIN32
    HANDLE process_handle;
    HANDLE thread_handle;
    HANDLE stdin_write_handle;
    HANDLE stdout_read_handle;
    HANDLE stderr_read_handle;
    HANDLE pty_console_handle;
    HANDLE pty_input_write_handle;
    HANDLE pty_output_read_handle;
#endif
} KainNativeProcessHandle;

static KainNativeProcessSpec g_specs[ABI_PROCESS_MAX_SPECS];
static KainNativeProcessHandle g_processes[ABI_PROCESS_MAX_PROCESSES];
static uint64_t g_spec_occupancy_bits = 0u;
static uint64_t g_process_occupancy_bits = 0u;
#define ABI_PROCESS_SLOT_WORD_BITS 64u
#define ABI_PROCESS_SPEC_VALID_MASK UINT64_MAX
#define ABI_PROCESS_PROCESS_VALID_MASK UINT64_MAX
#define ABI_PROCESS_SPEC_INDEX_CAPACITY 128u
#define ABI_PROCESS_SPEC_INDEX_MASK (ABI_PROCESS_SPEC_INDEX_CAPACITY - 1u)
#define ABI_PROCESS_PROCESS_INDEX_CAPACITY 128u
#define ABI_PROCESS_PROCESS_INDEX_MASK (ABI_PROCESS_PROCESS_INDEX_CAPACITY - 1u)
#if (ABI_PROCESS_SPEC_INDEX_CAPACITY & ABI_PROCESS_SPEC_INDEX_MASK) != 0
#error "ABI_PROCESS_SPEC_INDEX_CAPACITY must be a power of two for masked probing."
#endif
#if (ABI_PROCESS_PROCESS_INDEX_CAPACITY & ABI_PROCESS_PROCESS_INDEX_MASK) != 0
#error "ABI_PROCESS_PROCESS_INDEX_CAPACITY must be a power of two for masked probing."
#endif
static uint32_t g_spec_index[ABI_PROCESS_SPEC_INDEX_CAPACITY];
static uint32_t g_process_index[ABI_PROCESS_PROCESS_INDEX_CAPACITY];
static int64_t g_next_spec_id = 1;
static int64_t g_next_process_id = 1;
static int64_t g_last_status = ABI_PROCESS_OK;
static char g_last_error_kind[ABI_PROCESS_MAX_KEY] = "ok";
static char g_last_error_message[ABI_PROCESS_MAX_ERROR_TEXT] = "";
static const char g_empty_string[] = "";
static const char g_stdio_inherit[] = "inherit";
static const char g_stdio_pipe[] = "pipe";
static const char g_stdio_null[] = "null";
static atomic_uint_least64_t g_attrition_process_live_count = 0;
static atomic_uint_least64_t g_attrition_process_peak_count = 0;
static atomic_uint_least64_t g_attrition_process_spawn_count = 0;
static atomic_uint_least64_t g_attrition_process_exit_count = 0;
static atomic_uint_least64_t g_attrition_process_stale_reject_count = 0;

static void abi_process_attrition_update_peak(
    atomic_uint_least64_t* peak_counter,
    uint64_t candidate
) {
    uint64_t current_peak = atomic_load_explicit(peak_counter, memory_order_relaxed);
    while (candidate > current_peak &&
           !atomic_compare_exchange_weak_explicit(
               peak_counter,
               &current_peak,
               candidate,
               memory_order_relaxed,
               memory_order_relaxed)) {
    }
}

static uint64_t abi_process_popcount_u64(uint64_t value) {
    value = value - ((value >> 1u) & UINT64_C(0x5555555555555555));
    value = (value & UINT64_C(0x3333333333333333)) + ((value >> 2u) & UINT64_C(0x3333333333333333));
    value = (value + (value >> 4u)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
    return (value * UINT64_C(0x0101010101010101)) >> 56u;
}

#ifdef _WIN32
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

typedef HRESULT (WINAPI *KainCreatePseudoConsoleFn)(
    COORD size,
    HANDLE input_read_side,
    HANDLE output_write_side,
    DWORD flags,
    HANDLE* pseudo_console
);
typedef void (WINAPI *KainClosePseudoConsoleFn)(HANDLE pseudo_console);
typedef HRESULT (WINAPI *KainResizePseudoConsoleFn)(HANDLE pseudo_console, COORD size);
#endif

static void abi_process_copy(char* destination, size_t destination_capacity, const char* source) {
    if (destination == 0 || destination_capacity == 0u) {
        return;
    }
    if (source == 0) {
        destination[0] = '\0';
        return;
    }
    snprintf(destination, destination_capacity, "%s", source);
}

static int abi_process_text_empty(const char* text) {
    return text == 0 || text[0] == '\0';
}

static int abi_process_text_equal_ci(const char* left, const char* right) {
    if (left == 0 || right == 0) {
        return 0;
    }
#ifdef _WIN32
    return _stricmp(left, right) == 0;
#else
    return strcasecmp(left, right) == 0;
#endif
}

static const char* abi_process_string(const char* source) {
    return string_new((char*)(source ? source : g_empty_string));
}

static const char* abi_process_string_from_bytes(const unsigned char* bytes, size_t length) {
    char* output;
    size_t allocation_size;
    if (bytes == 0 || length == 0u) {
        return string_new("");
    }
    if (length > (SIZE_MAX - 1u)) {
        return string_new("");
    }
    allocation_size = length + 1u;
    output = (char*)kain_alloc_rc(allocation_size, 1);
    if (output == 0) {
        return string_new("");
    }
    memcpy(output, bytes, length);
    output[length] = '\0';
    kain_rc_set_string_length(output, kain_bounded_text_length((const char*)bytes, length));
    return output;
}

static int64_t abi_process_ok(void) {
    g_last_status = ABI_PROCESS_OK;
    abi_process_copy(g_last_error_kind, sizeof(g_last_error_kind), "ok");
    g_last_error_message[0] = '\0';
    return ABI_PROCESS_OK;
}

static int64_t abi_process_fail(int64_t status, const char* kind, const char* message) {
    g_last_status = status;
    abi_process_copy(g_last_error_kind, sizeof(g_last_error_kind), kind ? kind : "error");
    abi_process_copy(
        g_last_error_message,
        sizeof(g_last_error_message),
        message ? message : ""
    );
    if (status == ABI_PROCESS_INVALID_PROCESS || status == ABI_PROCESS_INVALID_SPEC) {
        atomic_fetch_add_explicit(&g_attrition_process_stale_reject_count, 1u, memory_order_relaxed);
        kain_attrition_note_process_stale_reject(0u, status);
    }
    return status;
}

static int abi_process_size_add_overflow(size_t left, size_t right, size_t* out_value) {
    if (out_value == 0) {
        return 1;
    }
    if (left > (SIZE_MAX - right)) {
        return 1;
    }
    *out_value = left + right;
    return 0;
}

static int abi_process_size_mul_overflow(size_t left, size_t right, size_t* out_value) {
    if (out_value == 0) {
        return 1;
    }
    if (left != 0u && right > (SIZE_MAX / left)) {
        return 1;
    }
    *out_value = left * right;
    return 0;
}

static int abi_process_mode_from_text(
    const char* mode_text,
    KainNativeProcessStdioMode* out_mode
) {
    if (out_mode == 0) {
        return 0;
    }
    if (abi_process_text_empty(mode_text) || abi_process_text_equal_ci(mode_text, g_stdio_inherit)) {
        *out_mode = ABI_PROCESS_STDIO_INHERIT;
        return 1;
    }
    if (abi_process_text_equal_ci(mode_text, g_stdio_pipe)) {
        *out_mode = ABI_PROCESS_STDIO_PIPE;
        return 1;
    }
    if (abi_process_text_equal_ci(mode_text, g_stdio_null)) {
        *out_mode = ABI_PROCESS_STDIO_NULL;
        return 1;
    }
    return 0;
}

/*
 * Proofs:
 * - runtime/native/src/core/z3/proofs-experimental/process-handle-index-probe-bounds.smt2
 * - runtime/native/src/core/z3/proofs-experimental/actor-table-debruijn-hash-distinct.smt2
 *
 * The solver owns the handle-registry math: masked probes must stay in bounds,
 * and the one-hot low-bit decoder is shared with the already-proved actor
 * occupancy path.
 */
static uint64_t abi_process_mix_id(uint64_t id) {
    uint64_t x = id;
    x ^= x >> 30u;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27u;
    x *= UINT64_C(0x94d049bb133111eb);
    x ^= x >> 31u;
    return x;
}

static uint64_t abi_process_isolate_low_bit_u64(uint64_t value) {
    return value & (0u - value);
}

static unsigned int abi_process_low_bit_index_u64(uint64_t one_hot) {
    static const unsigned char debruijn_index[64] = {
        0, 1, 48, 2, 57, 49, 28, 3,
        61, 58, 50, 42, 38, 29, 17, 4,
        62, 55, 59, 36, 53, 51, 43, 22,
        45, 39, 33, 30, 24, 18, 12, 5,
        63, 47, 56, 27, 60, 41, 37, 16,
        54, 35, 52, 21, 44, 32, 23, 11,
        46, 26, 40, 15, 34, 20, 31, 10,
        25, 14, 19, 9, 13, 8, 7, 6
    };
    return debruijn_index[(one_hot * UINT64_C(0x03f79d71b4cb0a89)) >> 58u];
}

static uint32_t abi_process_index_start_slot(uint64_t id, uint32_t mask) {
    return (uint32_t)(abi_process_mix_id(id) & mask);
}

static int abi_process_index_insert(
    uint32_t* index_table,
    uint32_t index_capacity,
    uint32_t index_mask,
    uint64_t id,
    uint32_t slot
) {
    uint32_t start_index = abi_process_index_start_slot(id, index_mask);
    uint32_t encoded_slot = slot + 1u;
    uint32_t probe;
    for (probe = 0u; probe < index_capacity; ++probe) {
        uint32_t candidate_index = (start_index + probe) & index_mask;
        uint32_t candidate = index_table[candidate_index];
        if (candidate == 0u || candidate == encoded_slot) {
            index_table[candidate_index] = encoded_slot;
            return 1;
        }
    }
    return 0;
}

static int abi_process_find_free_spec_slot(uint32_t* out_slot) {
    if (out_slot == 0 || g_spec_occupancy_bits == UINT64_MAX) {
        return 0;
    }
    *out_slot = (uint32_t)abi_process_low_bit_index_u64(
        abi_process_isolate_low_bit_u64(~g_spec_occupancy_bits)
    );
    return 1;
}

static int abi_process_find_free_process_slot(uint32_t* out_slot) {
    if (out_slot == 0 || g_process_occupancy_bits == UINT64_MAX) {
        return 0;
    }
    *out_slot = (uint32_t)abi_process_low_bit_index_u64(
        abi_process_isolate_low_bit_u64(~g_process_occupancy_bits)
    );
    return 1;
}

static void abi_process_rebuild_spec_index(void) {
    uint32_t slot;
    memset(g_spec_index, 0, sizeof(g_spec_index));
    for (slot = 0u; slot < ABI_PROCESS_MAX_SPECS; ++slot) {
        if (g_specs[slot].in_use) {
            (void)abi_process_index_insert(
                g_spec_index,
                ABI_PROCESS_SPEC_INDEX_CAPACITY,
                ABI_PROCESS_SPEC_INDEX_MASK,
                (uint64_t)g_specs[slot].id,
                slot
            );
        }
    }
}

static void abi_process_rebuild_process_index(void) {
    uint32_t slot;
    memset(g_process_index, 0, sizeof(g_process_index));
    for (slot = 0u; slot < ABI_PROCESS_MAX_PROCESSES; ++slot) {
        if (g_processes[slot].in_use) {
            (void)abi_process_index_insert(
                g_process_index,
                ABI_PROCESS_PROCESS_INDEX_CAPACITY,
                ABI_PROCESS_PROCESS_INDEX_MASK,
                (uint64_t)g_processes[slot].id,
                slot
            );
        }
    }
}

static KainNativeProcessSpec* abi_process_spec_lookup(int64_t spec_id) {
    uint32_t start_index;
    uint32_t probe;
    if (spec_id <= 0) {
        return 0;
    }
    start_index = abi_process_index_start_slot((uint64_t)spec_id, ABI_PROCESS_SPEC_INDEX_MASK);
    for (probe = 0u; probe < ABI_PROCESS_SPEC_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_PROCESS_SPEC_INDEX_MASK;
        uint32_t encoded_slot = g_spec_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_PROCESS_MAX_SPECS &&
            g_specs[slot].in_use &&
            g_specs[slot].id == spec_id) {
            return &g_specs[slot];
        }
    }
    return 0;
}

static KainNativeProcessHandle* abi_process_lookup(int64_t process_id) {
    uint32_t start_index;
    uint32_t probe;
    if (process_id <= 0) {
        return 0;
    }
    start_index = abi_process_index_start_slot((uint64_t)process_id, ABI_PROCESS_PROCESS_INDEX_MASK);
    for (probe = 0u; probe < ABI_PROCESS_PROCESS_INDEX_CAPACITY; ++probe) {
        uint32_t candidate_index = (start_index + probe) & ABI_PROCESS_PROCESS_INDEX_MASK;
        uint32_t encoded_slot = g_process_index[candidate_index];
        uint32_t slot;
        if (encoded_slot == 0u) {
            return 0;
        }
        slot = encoded_slot - 1u;
        if (slot < ABI_PROCESS_MAX_PROCESSES &&
            g_processes[slot].in_use &&
            g_processes[slot].id == process_id) {
            return &g_processes[slot];
        }
    }
    return 0;
}

static int abi_process_capture_reserve(KainNativeProcessCapture* capture, size_t required) {
    unsigned char* resized;
    size_t next_capacity;
    if (capture == 0) {
        return 0;
    }
    if (required <= capture->capacity) {
        return 1;
    }
    if (required > ABI_PROCESS_MAX_CAPTURE_BYTES) {
        return 0;
    }
    next_capacity = capture->capacity == 0u ? 1024u : capture->capacity;
    while (next_capacity < required) {
        if (next_capacity > (ABI_PROCESS_MAX_CAPTURE_BYTES / 2u)) {
            next_capacity = required;
            break;
        }
        next_capacity *= 2u;
    }
    if (next_capacity > ABI_PROCESS_MAX_CAPTURE_BYTES) {
        next_capacity = ABI_PROCESS_MAX_CAPTURE_BYTES;
    }
    if (next_capacity < required) {
        return 0;
    }
    resized = (unsigned char*)realloc(capture->bytes, next_capacity);
    if (resized == 0) {
        return 0;
    }
    capture->bytes = resized;
    capture->capacity = next_capacity;
    return 1;
}

static int abi_process_capture_append(
    KainNativeProcessCapture* capture,
    const unsigned char* bytes,
    size_t byte_length
) {
    size_t remaining_capacity;
    size_t required_length;
    size_t to_copy;
    if (capture == 0 || bytes == 0 || byte_length == 0u) {
        return 1;
    }
    if (capture->length >= ABI_PROCESS_MAX_CAPTURE_BYTES) {
        return 1;
    }
    remaining_capacity = ABI_PROCESS_MAX_CAPTURE_BYTES - capture->length;
    to_copy = byte_length < remaining_capacity ? byte_length : remaining_capacity;
    /* Proof: runtime/native/src/core/z3/proofs/native-process-capture-append-required-length-does-not-wrap-under-capture-limit.yaml */
    if (abi_process_size_add_overflow(capture->length, to_copy, &required_length) ||
        !abi_process_capture_reserve(capture, required_length)) {
        return 0;
    }
    memcpy(capture->bytes + capture->length, bytes, to_copy);
    capture->length += to_copy;
    return 1;
}

static void abi_process_capture_free(KainNativeProcessCapture* capture) {
    if (capture == 0) {
        return;
    }
    free(capture->bytes);
    capture->bytes = 0;
    capture->length = 0u;
    capture->capacity = 0u;
}

static int abi_process_hex_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

static int abi_process_decode_hex(
    const char* bytes_hex,
    unsigned char** out_bytes,
    size_t* out_length
) {
    size_t hex_length;
    size_t index;
    unsigned char* decoded;
    if (out_bytes == 0 || out_length == 0) {
        return 0;
    }
    *out_bytes = 0;
    *out_length = 0u;
    if (bytes_hex == 0 || bytes_hex[0] == '\0') {
        return 1;
    }
    hex_length = strlen(bytes_hex);
    if ((hex_length % 2u) != 0u) {
        return 0;
    }
    decoded = (unsigned char*)malloc(hex_length / 2u);
    if (decoded == 0) {
        return 0;
    }
    for (index = 0u; index < hex_length / 2u; index++) {
        int high = abi_process_hex_value(bytes_hex[index * 2u]);
        int low = abi_process_hex_value(bytes_hex[index * 2u + 1u]);
        if (high < 0 || low < 0) {
            free(decoded);
            return 0;
        }
        decoded[index] = (unsigned char)((high << 4) | low);
    }
    *out_bytes = decoded;
    *out_length = hex_length / 2u;
    return 1;
}

static const char* abi_process_encode_hex(const unsigned char* bytes, size_t byte_length) {
    static const char alphabet[] = "0123456789abcdef";
    size_t allocation_size;
    char* encoded;
    size_t index;
    if (bytes == 0 || byte_length == 0u) {
        return string_new("");
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-process-hex-encoding-allocation-does-not-wrap-under-capture-limit.yaml */
    if (abi_process_size_mul_overflow(byte_length, 2u, &allocation_size) ||
        abi_process_size_add_overflow(allocation_size, 1u, &allocation_size)) {
        return string_new("");
    }
    encoded = (char*)kain_alloc_rc(allocation_size, 1);
    if (encoded == 0) {
        return string_new("");
    }
    for (index = 0u; index < byte_length; index++) {
        encoded[index * 2u] = alphabet[(bytes[index] >> 4) & 0x0f];
        encoded[index * 2u + 1u] = alphabet[bytes[index] & 0x0f];
    }
    encoded[byte_length * 2u] = '\0';
    return encoded;
}

static void abi_process_release_handle(KainNativeProcessHandle* process, int terminate_running_process) {
    uint32_t slot;
    int64_t process_id;
    if (process == 0 || !process->in_use) {
        return;
    }
    slot = (uint32_t)(process - g_processes);
    process_id = process->id;
#ifdef _WIN32
    if (terminate_running_process && process->process_handle != 0 && !process->exited) {
        TerminateProcess(process->process_handle, 1u);
        WaitForSingleObject(process->process_handle, 250u);
    }
    if (process->stdin_write_handle != 0) {
        CloseHandle(process->stdin_write_handle);
        process->stdin_write_handle = 0;
    }
    if (process->stdout_read_handle != 0) {
        CloseHandle(process->stdout_read_handle);
        process->stdout_read_handle = 0;
    }
    if (process->stderr_read_handle != 0) {
        CloseHandle(process->stderr_read_handle);
        process->stderr_read_handle = 0;
    }
    if (process->pty_input_write_handle != 0) {
        CloseHandle(process->pty_input_write_handle);
        process->pty_input_write_handle = 0;
    }
    if (process->pty_output_read_handle != 0) {
        CloseHandle(process->pty_output_read_handle);
        process->pty_output_read_handle = 0;
    }
    if (process->pty_console_handle != 0) {
        HMODULE kernel_module = GetModuleHandleW(L"kernel32.dll");
        KainClosePseudoConsoleFn close_pseudo_console = 0;
        if (kernel_module != 0) {
            close_pseudo_console = (KainClosePseudoConsoleFn)GetProcAddress(kernel_module, "ClosePseudoConsole");
        }
        if (close_pseudo_console != 0) {
            close_pseudo_console(process->pty_console_handle);
        } else {
            CloseHandle(process->pty_console_handle);
        }
        process->pty_console_handle = 0;
    }
    if (process->thread_handle != 0) {
        CloseHandle(process->thread_handle);
        process->thread_handle = 0;
    }
    if (process->process_handle != 0) {
        CloseHandle(process->process_handle);
        process->process_handle = 0;
    }
#else
    (void)terminate_running_process;
#endif
    g_process_occupancy_bits &= ~(UINT64_C(1) << slot);
    abi_process_capture_free(&process->stdout_capture);
    abi_process_capture_free(&process->stderr_capture);
    abi_process_capture_free(&process->pty_capture);
    memset(process, 0, sizeof(*process));
    abi_process_rebuild_process_index();
    atomic_fetch_sub_explicit(&g_attrition_process_live_count, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&g_attrition_process_exit_count, 1u, memory_order_relaxed);
    kain_attrition_note_process_exit((uint64_t)process_id);
}

#ifdef _WIN32
typedef struct KainNativeProcessWideBuffer {
    wchar_t* text;
    size_t length;
    size_t capacity;
} KainNativeProcessWideBuffer;

typedef struct KainNativeProcessUtf8Buffer {
    char* text;
    size_t length;
    size_t capacity;
} KainNativeProcessUtf8Buffer;

typedef struct KainNativeProcessWideEntryList {
    wchar_t** entries;
    size_t count;
    size_t capacity;
} KainNativeProcessWideEntryList;

static wchar_t* abi_process_utf8_to_wide(const char* utf8_text) {
    int required_length;
    size_t allocation_size;
    wchar_t* wide_text;
    if (utf8_text == 0) {
        wide_text = (wchar_t*)malloc(sizeof(wchar_t));
        if (wide_text != 0) {
            wide_text[0] = L'\0';
        }
        return wide_text;
    }
    required_length = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, 0, 0);
    if (required_length <= 0) {
        return 0;
    }
    if (abi_process_size_mul_overflow((size_t)required_length, sizeof(wchar_t), &allocation_size)) {
        return 0;
    }
    wide_text = (wchar_t*)malloc(allocation_size);
    if (wide_text == 0) {
        return 0;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, wide_text, required_length) <= 0) {
        free(wide_text);
        return 0;
    }
    return wide_text;
}

static int abi_process_wide_buffer_reserve(
    KainNativeProcessWideBuffer* buffer,
    size_t required
) {
    size_t allocation_size;
    wchar_t* resized;
    size_t next_capacity;
    if (buffer == 0) {
        return 0;
    }
    if (required <= buffer->capacity) {
        return 1;
    }
    next_capacity = buffer->capacity == 0u ? 64u : buffer->capacity;
    while (next_capacity < required) {
        if (next_capacity > (SIZE_MAX / 2u)) {
            next_capacity = required;
            break;
        }
        next_capacity *= 2u;
    }
    if (abi_process_size_mul_overflow(next_capacity, sizeof(wchar_t), &allocation_size)) {
        return 0;
    }
    resized = (wchar_t*)realloc(buffer->text, allocation_size);
    if (resized == 0) {
        return 0;
    }
    buffer->text = resized;
    buffer->capacity = next_capacity;
    return 1;
}

static int abi_process_wide_buffer_append_char(
    KainNativeProcessWideBuffer* buffer,
    wchar_t character
) {
    size_t required;
    if (abi_process_size_add_overflow(buffer->length, 2u, &required) ||
        !abi_process_wide_buffer_reserve(buffer, required)) {
        return 0;
    }
    buffer->text[buffer->length] = character;
    buffer->length += 1u;
    buffer->text[buffer->length] = L'\0';
    return 1;
}

static int abi_process_wide_buffer_append_text(
    KainNativeProcessWideBuffer* buffer,
    const wchar_t* text
) {
    size_t required;
    size_t text_length;
    if (buffer == 0 || text == 0) {
        return 0;
    }
    text_length = wcslen(text);
    /* Proof: runtime/native/src/core/z3/proofs/native-process-wide-buffer-append-text-required-length-does-not-wrap.yaml */
    if (abi_process_size_add_overflow(buffer->length, text_length, &required) ||
        abi_process_size_add_overflow(required, 1u, &required) ||
        !abi_process_wide_buffer_reserve(buffer, required)) {
        return 0;
    }
    memcpy(buffer->text + buffer->length, text, text_length * sizeof(wchar_t));
    buffer->length += text_length;
    buffer->text[buffer->length] = L'\0';
    return 1;
}

static void abi_process_wide_buffer_free(KainNativeProcessWideBuffer* buffer) {
    if (buffer == 0) {
        return;
    }
    free(buffer->text);
    buffer->text = 0;
    buffer->length = 0u;
    buffer->capacity = 0u;
}

static int abi_process_utf8_buffer_reserve(
    KainNativeProcessUtf8Buffer* buffer,
    size_t required
) {
    char* resized;
    size_t next_capacity;
    if (buffer == 0) {
        return 0;
    }
    if (required <= buffer->capacity) {
        return 1;
    }
    next_capacity = buffer->capacity == 0u ? 64u : buffer->capacity;
    while (next_capacity < required) {
        if (next_capacity > (SIZE_MAX / 2u)) {
            next_capacity = required;
            break;
        }
        next_capacity *= 2u;
    }
    resized = (char*)realloc(buffer->text, next_capacity);
    if (resized == 0) {
        return 0;
    }
    buffer->text = resized;
    buffer->capacity = next_capacity;
    return 1;
}

static int abi_process_utf8_buffer_append_char(
    KainNativeProcessUtf8Buffer* buffer,
    char character
) {
    size_t required;
    if (abi_process_size_add_overflow(buffer->length, 2u, &required) ||
        !abi_process_utf8_buffer_reserve(buffer, required)) {
        return 0;
    }
    buffer->text[buffer->length] = character;
    buffer->length += 1u;
    buffer->text[buffer->length] = '\0';
    return 1;
}

static int abi_process_utf8_buffer_append_chars(
    KainNativeProcessUtf8Buffer* buffer,
    char character,
    size_t count
) {
    size_t index;
    for (index = 0u; index < count; index++) {
        if (!abi_process_utf8_buffer_append_char(buffer, character)) {
            return 0;
        }
    }
    return 1;
}

static int abi_process_utf8_buffer_append_text(
    KainNativeProcessUtf8Buffer* buffer,
    const char* text
) {
    size_t required;
    size_t text_length;
    if (buffer == 0 || text == 0) {
        return 0;
    }
    text_length = strlen(text);
    /* Proof: runtime/native/src/core/z3/proofs/native-process-utf8-buffer-append-text-required-length-does-not-wrap.yaml */
    if (abi_process_size_add_overflow(buffer->length, text_length, &required) ||
        abi_process_size_add_overflow(required, 1u, &required) ||
        !abi_process_utf8_buffer_reserve(buffer, required)) {
        return 0;
    }
    memcpy(buffer->text + buffer->length, text, text_length);
    buffer->length += text_length;
    buffer->text[buffer->length] = '\0';
    return 1;
}

static void abi_process_utf8_buffer_free(KainNativeProcessUtf8Buffer* buffer) {
    if (buffer == 0) {
        return;
    }
    free(buffer->text);
    buffer->text = 0;
    buffer->length = 0u;
    buffer->capacity = 0u;
}

static int abi_process_utf8_buffer_append_quoted_argument(
    KainNativeProcessUtf8Buffer* buffer,
    const char* argument
) {
    size_t index;
    size_t backslash_count = 0u;
    int needs_quotes = 0;
    if (buffer == 0 || argument == 0) {
        return 0;
    }
    if (argument[0] == '\0') {
        needs_quotes = 1;
    } else {
        for (index = 0u; argument[index] != '\0'; index++) {
            char character = argument[index];
            if (character == ' ' || character == '\t' || character == '\n' || character == '"') {
                needs_quotes = 1;
                break;
            }
        }
    }
    if (!needs_quotes) {
        return abi_process_utf8_buffer_append_text(buffer, argument);
    }
    if (!abi_process_utf8_buffer_append_char(buffer, '"')) {
        return 0;
    }
    for (index = 0u; argument[index] != '\0'; index++) {
        char character = argument[index];
        if (character == '\\') {
            backslash_count += 1u;
            continue;
        }
        if (character == '"') {
            if (!abi_process_utf8_buffer_append_chars(buffer, '\\', backslash_count * 2u + 1u)) {
                return 0;
            }
            if (!abi_process_utf8_buffer_append_char(buffer, '"')) {
                return 0;
            }
            backslash_count = 0u;
            continue;
        }
        if (backslash_count > 0u) {
            if (!abi_process_utf8_buffer_append_chars(buffer, '\\', backslash_count)) {
                return 0;
            }
            backslash_count = 0u;
        }
        if (!abi_process_utf8_buffer_append_char(buffer, character)) {
            return 0;
        }
    }
    if (backslash_count > 0u) {
        if (!abi_process_utf8_buffer_append_chars(buffer, '\\', backslash_count * 2u)) {
            return 0;
        }
    }
    return abi_process_utf8_buffer_append_char(buffer, '"');
}

static wchar_t* abi_process_build_command_line_w(const KainNativeProcessSpec* spec) {
    KainNativeProcessUtf8Buffer utf8_command_line = {0};
    wchar_t* wide_command_line = 0;
    int64_t index;
    if (spec == 0) {
        return 0;
    }
    if (!abi_process_utf8_buffer_append_quoted_argument(&utf8_command_line, spec->executable)) {
        goto cleanup;
    }
    for (index = 0; index < spec->argument_count; index++) {
        if (!abi_process_utf8_buffer_append_char(&utf8_command_line, ' ')) {
            goto cleanup;
        }
        if (!abi_process_utf8_buffer_append_quoted_argument(
                &utf8_command_line,
                spec->arguments[index]
            )) {
            goto cleanup;
        }
    }
    wide_command_line = abi_process_utf8_to_wide(
        utf8_command_line.text ? utf8_command_line.text : ""
    );
cleanup:
    abi_process_utf8_buffer_free(&utf8_command_line);
    return wide_command_line;
}

static wchar_t* abi_process_wide_duplicate(const wchar_t* text) {
    size_t length;
    size_t allocation_size;
    wchar_t* copy;
    if (text == 0) {
        return 0;
    }
    length = wcslen(text);
    if (abi_process_size_add_overflow(length, 1u, &allocation_size) ||
        abi_process_size_mul_overflow(allocation_size, sizeof(wchar_t), &allocation_size)) {
        return 0;
    }
    copy = (wchar_t*)malloc(allocation_size);
    if (copy == 0) {
        return 0;
    }
    memcpy(copy, text, (length + 1u) * sizeof(wchar_t));
    return copy;
}

static int abi_process_wide_entry_list_push(
    KainNativeProcessWideEntryList* list,
    wchar_t* entry
) {
    size_t allocation_size;
    wchar_t** resized;
    size_t next_capacity;
    if (list == 0 || entry == 0) {
        free(entry);
        return 0;
    }
    if (list->count == list->capacity) {
        next_capacity = list->capacity == 0u ? 16u : list->capacity;
        if (next_capacity > (SIZE_MAX / 2u)) {
            free(entry);
            return 0;
        }
        next_capacity *= 2u;
        if (abi_process_size_mul_overflow(next_capacity, sizeof(wchar_t*), &allocation_size)) {
            free(entry);
            return 0;
        }
        resized = (wchar_t**)realloc(list->entries, allocation_size);
        if (resized == 0) {
            free(entry);
            return 0;
        }
        list->entries = resized;
        list->capacity = next_capacity;
    }
    list->entries[list->count] = entry;
    list->count += 1u;
    return 1;
}

static void abi_process_wide_entry_list_free(KainNativeProcessWideEntryList* list) {
    size_t index;
    if (list == 0) {
        return;
    }
    for (index = 0u; index < list->count; index++) {
        free(list->entries[index]);
    }
    free(list->entries);
    list->entries = 0;
    list->count = 0u;
    list->capacity = 0u;
}

static size_t abi_process_wide_entry_key_length(const wchar_t* entry) {
    size_t index = 0u;
    if (entry == 0) {
        return 0u;
    }
    while (entry[index] != L'\0' && entry[index] != L'=') {
        index += 1u;
    }
    return index;
}

static int abi_process_wide_entry_key_equals(
    const wchar_t* entry,
    const wchar_t* key
) {
    size_t entry_key_length;
    size_t key_length;
    if (entry == 0 || key == 0) {
        return 0;
    }
    entry_key_length = abi_process_wide_entry_key_length(entry);
    key_length = wcslen(key);
    if (entry_key_length != key_length) {
        return 0;
    }
    return _wcsnicmp(entry, key, key_length) == 0;
}

static wchar_t* abi_process_build_env_entry_w(
    const char* key_utf8,
    const char* value_utf8
) {
    wchar_t* key_wide = 0;
    wchar_t* value_wide = 0;
    KainNativeProcessWideBuffer buffer = {0};
    wchar_t* entry = 0;
    key_wide = abi_process_utf8_to_wide(key_utf8);
    value_wide = abi_process_utf8_to_wide(value_utf8 ? value_utf8 : "");
    if (key_wide == 0 || value_wide == 0) {
        goto cleanup;
    }
    if (!abi_process_wide_buffer_append_text(&buffer, key_wide)) {
        goto cleanup;
    }
    if (!abi_process_wide_buffer_append_char(&buffer, L'=')) {
        goto cleanup;
    }
    if (!abi_process_wide_buffer_append_text(&buffer, value_wide)) {
        goto cleanup;
    }
    entry = buffer.text;
    buffer.text = 0;
cleanup:
    free(key_wide);
    free(value_wide);
    abi_process_wide_buffer_free(&buffer);
    return entry;
}

static wchar_t* abi_process_build_environment_block_w(const KainNativeProcessSpec* spec) {
    KainNativeProcessWideEntryList entries = {0};
    LPWCH environment_block = 0;
    const wchar_t* cursor;
    size_t allocation_size = 0u;
    size_t total_length = 1u;
    size_t index;
    wchar_t* flattened = 0;
    if (spec == 0) {
        return 0;
    }
    if (spec->inherit_environment) {
        environment_block = GetEnvironmentStringsW();
        if (environment_block == 0) {
            return 0;
        }
        cursor = environment_block;
        while (*cursor != L'\0') {
            wchar_t* copy = abi_process_wide_duplicate(cursor);
            if (copy == 0 || !abi_process_wide_entry_list_push(&entries, copy)) {
                FreeEnvironmentStringsW(environment_block);
                abi_process_wide_entry_list_free(&entries);
                return 0;
            }
            cursor += wcslen(cursor) + 1u;
        }
        FreeEnvironmentStringsW(environment_block);
    }

    for (index = 0u; index < (size_t)spec->environment_count; index++) {
        const KainNativeProcessEnvironmentEntry* override = &spec->environment[index];
        wchar_t* key_wide = 0;
        wchar_t* replacement = 0;
        size_t entry_index;
        if (!override->in_use) {
            continue;
        }
        key_wide = abi_process_utf8_to_wide(override->key);
        replacement = abi_process_build_env_entry_w(override->key, override->value);
        if (key_wide == 0 || replacement == 0) {
            free(key_wide);
            free(replacement);
            abi_process_wide_entry_list_free(&entries);
            return 0;
        }
        for (entry_index = 0u; entry_index < entries.count; entry_index++) {
            if (abi_process_wide_entry_key_equals(entries.entries[entry_index], key_wide)) {
                free(entries.entries[entry_index]);
                entries.entries[entry_index] = replacement;
                replacement = 0;
                break;
            }
        }
        if (replacement != 0 && !abi_process_wide_entry_list_push(&entries, replacement)) {
            free(key_wide);
            free(replacement);
            abi_process_wide_entry_list_free(&entries);
            return 0;
        }
        free(key_wide);
    }

    for (index = 0u; index < entries.count; index++) {
        size_t entry_length = 0u;
        if (abi_process_size_add_overflow(wcslen(entries.entries[index]), 1u, &entry_length) ||
            abi_process_size_add_overflow(total_length, entry_length, &total_length)) {
            abi_process_wide_entry_list_free(&entries);
            return 0;
        }
    }
    if (abi_process_size_mul_overflow(total_length, sizeof(wchar_t), &allocation_size)) {
        abi_process_wide_entry_list_free(&entries);
        return 0;
    }
    flattened = (wchar_t*)malloc(allocation_size);
    if (flattened == 0) {
        abi_process_wide_entry_list_free(&entries);
        return 0;
    }
    {
        wchar_t* output_cursor = flattened;
        for (index = 0u; index < entries.count; index++) {
            size_t entry_length = wcslen(entries.entries[index]);
            memcpy(output_cursor, entries.entries[index], entry_length * sizeof(wchar_t));
            output_cursor += entry_length;
            *output_cursor = L'\0';
            output_cursor += 1;
        }
        *output_cursor = L'\0';
    }
    abi_process_wide_entry_list_free(&entries);
    return flattened;
}

static int abi_process_create_pipe_pair(
    HANDLE* child_handle,
    HANDLE* parent_handle,
    int parent_writes
) {
    SECURITY_ATTRIBUTES security_attributes;
    HANDLE read_handle = 0;
    HANDLE write_handle = 0;
    memset(&security_attributes, 0, sizeof(security_attributes));
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.bInheritHandle = TRUE;
    if (!CreatePipe(&read_handle, &write_handle, &security_attributes, 0u)) {
        return 0;
    }
    if (parent_writes) {
        if (!SetHandleInformation(write_handle, HANDLE_FLAG_INHERIT, 0u)) {
            CloseHandle(read_handle);
            CloseHandle(write_handle);
            return 0;
        }
        *child_handle = read_handle;
        *parent_handle = write_handle;
        return 1;
    }
    if (!SetHandleInformation(read_handle, HANDLE_FLAG_INHERIT, 0u)) {
        CloseHandle(read_handle);
        CloseHandle(write_handle);
        return 0;
    }
    *child_handle = write_handle;
    *parent_handle = read_handle;
    return 1;
}

typedef struct KainNativeProcessWindowsCache {
    INIT_ONCE init_once;
    HANDLE null_read_template;
    HANDLE null_write_template;
    wchar_t* command_shell_path;
} KainNativeProcessWindowsCache;

static KainNativeProcessWindowsCache g_process_windows_cache = {
    .init_once = INIT_ONCE_STATIC_INIT
};

static HANDLE abi_process_open_null_device_raw(DWORD access_mask) {
    return CreateFileW(
        L"NUL",
        access_mask,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        0,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        0
    );
}

static BOOL CALLBACK abi_process_windows_cache_init(
    PINIT_ONCE init_once,
    PVOID parameter,
    PVOID* context
) {
    wchar_t command_shell_path_buffer[32768];
    const size_t command_shell_path_capacity =
        sizeof(command_shell_path_buffer) / sizeof(command_shell_path_buffer[0]);
    DWORD command_shell_length = 0u;
    HANDLE null_read_template = abi_process_open_null_device_raw(GENERIC_READ);
    HANDLE null_write_template = abi_process_open_null_device_raw(GENERIC_WRITE);
    wchar_t* command_shell_path = 0;
    (void)init_once;
    (void)parameter;
    (void)context;

    command_shell_length = GetEnvironmentVariableW(
        L"ComSpec",
        command_shell_path_buffer,
        (DWORD)command_shell_path_capacity
    );
    if (command_shell_length > 0u &&
        command_shell_length < (DWORD)command_shell_path_capacity) {
        command_shell_path = abi_process_wide_duplicate(command_shell_path_buffer);
    }
    if (command_shell_path == 0) {
        command_shell_length = GetSystemDirectoryW(
            command_shell_path_buffer,
            (UINT)command_shell_path_capacity
        );
        if (command_shell_length > 0u &&
            ((size_t)command_shell_length + 9u) <= command_shell_path_capacity) {
            memcpy(
                command_shell_path_buffer + command_shell_length,
                L"\\cmd.exe",
                9u * sizeof(wchar_t)
            );
            command_shell_path = abi_process_wide_duplicate(command_shell_path_buffer);
        }
    }

    g_process_windows_cache.null_read_template = null_read_template;
    g_process_windows_cache.null_write_template = null_write_template;
    g_process_windows_cache.command_shell_path = command_shell_path;
    return TRUE;
}

static void abi_process_windows_cache_ensure_initialized(void) {
    InitOnceExecuteOnce(
        &g_process_windows_cache.init_once,
        abi_process_windows_cache_init,
        0,
        0
    );
}

static HANDLE abi_process_duplicate_inheritable_handle(HANDLE source_handle) {
    HANDLE duplicated_handle = 0;
    if (source_handle == 0) {
        return 0;
    }
    if (!DuplicateHandle(
            GetCurrentProcess(),
            source_handle,
            GetCurrentProcess(),
            &duplicated_handle,
            0,
            TRUE,
            DUPLICATE_SAME_ACCESS
        )) {
        return 0;
    }
    return duplicated_handle;
}

static HANDLE abi_process_open_null_device(DWORD access_mask) {
    HANDLE duplicated_handle = 0;
    abi_process_windows_cache_ensure_initialized();
    if (access_mask == GENERIC_READ) {
        duplicated_handle = abi_process_duplicate_inheritable_handle(
            g_process_windows_cache.null_read_template
        );
    } else if (access_mask == GENERIC_WRITE) {
        duplicated_handle = abi_process_duplicate_inheritable_handle(
            g_process_windows_cache.null_write_template
        );
    }
    if (duplicated_handle != 0) {
        return duplicated_handle;
    }
    return abi_process_open_null_device_raw(access_mask);
}

static int abi_process_text_has_path_separator(const char* text) {
    size_t index;
    if (text == 0) {
        return 0;
    }
    for (index = 0u; text[index] != '\0'; index++) {
        if (text[index] == '\\' || text[index] == '/') {
            return 1;
        }
    }
    return 0;
}

static int abi_process_is_cmd_shell_alias(const char* executable) {
    if (abi_process_text_empty(executable) || abi_process_text_has_path_separator(executable)) {
        return 0;
    }
    return abi_process_text_equal_ci(executable, "cmd") ||
           abi_process_text_equal_ci(executable, "cmd.exe");
}

static wchar_t* abi_process_resolve_application_name_w(const char* executable) {
    abi_process_windows_cache_ensure_initialized();
    if (!abi_process_is_cmd_shell_alias(executable) ||
        g_process_windows_cache.command_shell_path == 0) {
        return 0;
    }
    return abi_process_wide_duplicate(g_process_windows_cache.command_shell_path);
}

static int abi_process_refresh_exit_state(KainNativeProcessHandle* process) {
    DWORD exit_code;
    if (process == 0 || !process->in_use || process->process_handle == 0) {
        return 0;
    }
    if (process->exited) {
        return 1;
    }
    if (!GetExitCodeProcess(process->process_handle, &exit_code)) {
        return 0;
    }
    if (exit_code == STILL_ACTIVE) {
        return 0;
    }
    process->exited = 1;
    process->exit_code = (int64_t)exit_code;
    return 1;
}

static int abi_process_drain_stream_handle(
    HANDLE stream_handle,
    KainNativeProcessCapture* capture,
    unsigned char** out_chunk,
    size_t* out_chunk_length
) {
    unsigned char* chunk = 0;
    size_t chunk_length = 0u;
    size_t chunk_capacity = 0u;
    for (;;) {
        DWORD available = 0u;
        DWORD bytes_read = 0u;
        unsigned char buffer[4096];
        if (stream_handle == 0) {
            break;
        }
        if (!PeekNamedPipe(stream_handle, 0, 0u, 0, &available, 0)) {
            DWORD last_error = GetLastError();
            if (last_error == ERROR_BROKEN_PIPE || last_error == ERROR_HANDLE_EOF) {
                break;
            }
            free(chunk);
            return 0;
        }
        if (available == 0u) {
            break;
        }
        if (!ReadFile(
                stream_handle,
                buffer,
                available < sizeof(buffer) ? available : (DWORD)sizeof(buffer),
                &bytes_read,
                0
            )) {
            DWORD last_error = GetLastError();
            if (last_error == ERROR_BROKEN_PIPE || last_error == ERROR_HANDLE_EOF) {
                break;
            }
            free(chunk);
            return 0;
        }
        if (bytes_read == 0u) {
            break;
        }
        if (!abi_process_capture_append(capture, buffer, bytes_read)) {
            free(chunk);
            return 0;
        }
        if (chunk_length + bytes_read > chunk_capacity) {
            size_t next_capacity = chunk_capacity == 0u ? 4096u : chunk_capacity;
            while (next_capacity < chunk_length + bytes_read) {
                next_capacity *= 2u;
            }
            chunk = (unsigned char*)realloc(chunk, next_capacity);
            if (chunk == 0) {
                return 0;
            }
            chunk_capacity = next_capacity;
        }
        memcpy(chunk + chunk_length, buffer, bytes_read);
        chunk_length += bytes_read;
    }
    if (out_chunk != 0) {
        *out_chunk = chunk;
    } else {
        free(chunk);
    }
    if (out_chunk_length != 0) {
        *out_chunk_length = chunk_length;
    }
    return 1;
}

static int abi_process_pump_output(
    KainNativeProcessHandle* process,
    unsigned char** out_primary_chunk,
    size_t* out_primary_chunk_length,
    unsigned char** out_secondary_chunk,
    size_t* out_secondary_chunk_length
) {
    if (process == 0 || !process->in_use) {
        return 0;
    }
    if (process->is_pty) {
        if (!abi_process_drain_stream_handle(
                process->pty_output_read_handle,
                &process->pty_capture,
                out_primary_chunk,
                out_primary_chunk_length
            )) {
            return 0;
        }
        if (out_secondary_chunk != 0) {
            *out_secondary_chunk = 0;
        }
        if (out_secondary_chunk_length != 0) {
            *out_secondary_chunk_length = 0u;
        }
        return 1;
    }
    if (!abi_process_drain_stream_handle(
            process->stdout_read_handle,
            &process->stdout_capture,
            out_primary_chunk,
            out_primary_chunk_length
        )) {
        return 0;
    }
    if (!abi_process_drain_stream_handle(
            process->stderr_read_handle,
            &process->stderr_capture,
            out_secondary_chunk,
            out_secondary_chunk_length
        )) {
        free(out_primary_chunk != 0 ? *out_primary_chunk : 0);
        if (out_primary_chunk != 0) {
            *out_primary_chunk = 0;
        }
        if (out_primary_chunk_length != 0) {
            *out_primary_chunk_length = 0u;
        }
        return 0;
    }
    return 1;
}

static int abi_process_wait_internal(
    KainNativeProcessHandle* process,
    int64_t timeout_ms,
    int* out_exited
) {
    DWORD wait_timeout = INFINITE;
    DWORD wait_result;
    if (process == 0 || !process->in_use || process->process_handle == 0) {
        return 0;
    }
    if (timeout_ms >= 0) {
        wait_timeout = (DWORD)timeout_ms;
    }
    wait_result = WaitForSingleObject(process->process_handle, wait_timeout);
    if (wait_result == WAIT_OBJECT_0) {
        abi_process_refresh_exit_state(process);
        if (out_exited != 0) {
            *out_exited = 1;
        }
        return 1;
    }
    if (wait_result == WAIT_TIMEOUT) {
        if (out_exited != 0) {
            *out_exited = 0;
        }
        return 1;
    }
    return 0;
}

static void abi_process_flush_exited_output(KainNativeProcessHandle* process) {
    int attempt;
    if (process == 0 || !process->in_use) {
        return;
    }
    if (!process->is_pty) {
        return;
    }
    for (attempt = 0; attempt < 8; attempt++) {
        unsigned char* primary_chunk = 0;
        unsigned char* secondary_chunk = 0;
        size_t primary_length = 0u;
        size_t secondary_length = 0u;
        int had_data = 0;
        if (!abi_process_pump_output(
                process,
                &primary_chunk,
                &primary_length,
                &secondary_chunk,
                &secondary_length
            )) {
            free(primary_chunk);
            free(secondary_chunk);
            return;
        }
        had_data = primary_length > 0u || secondary_length > 0u;
        free(primary_chunk);
        free(secondary_chunk);
        if (!had_data && attempt >= 2) {
            break;
        }
        Sleep(15u);
    }
}

static int abi_process_spec_append_direct_arg(
    KainNativeProcessSpec* spec,
    const char* argument
) {
    if (spec == 0 || argument == 0) {
        return 0;
    }
    if (spec->argument_count >= ABI_PROCESS_MAX_ARGUMENTS) {
        return 0;
    }
    abi_process_copy(
        spec->arguments[spec->argument_count],
        sizeof(spec->arguments[spec->argument_count]),
        argument
    );
    spec->argument_count += 1;
    return 1;
}

static void abi_process_close_os_resources(KainNativeProcessHandle* process, int terminate_running_process) {
#ifdef _WIN32
    if (process == 0) {
        return;
    }
    if (terminate_running_process && process->process_handle != 0 && !process->exited) {
        TerminateProcess(process->process_handle, 1u);
        WaitForSingleObject(process->process_handle, 250u);
    }
    if (process->stdin_write_handle != 0) {
        CloseHandle(process->stdin_write_handle);
        process->stdin_write_handle = 0;
    }
    if (process->stdout_read_handle != 0) {
        CloseHandle(process->stdout_read_handle);
        process->stdout_read_handle = 0;
    }
    if (process->stderr_read_handle != 0) {
        CloseHandle(process->stderr_read_handle);
        process->stderr_read_handle = 0;
    }
    if (process->pty_input_write_handle != 0) {
        CloseHandle(process->pty_input_write_handle);
        process->pty_input_write_handle = 0;
    }
    if (process->pty_output_read_handle != 0) {
        CloseHandle(process->pty_output_read_handle);
        process->pty_output_read_handle = 0;
    }
    if (process->pty_console_handle != 0) {
        HMODULE kernel_module = GetModuleHandleW(L"kernel32.dll");
        KainClosePseudoConsoleFn close_pseudo_console = 0;
        if (kernel_module != 0) {
            close_pseudo_console = (KainClosePseudoConsoleFn)GetProcAddress(kernel_module, "ClosePseudoConsole");
        }
        if (close_pseudo_console != 0) {
            close_pseudo_console(process->pty_console_handle);
        } else {
            CloseHandle(process->pty_console_handle);
        }
        process->pty_console_handle = 0;
    }
    if (process->thread_handle != 0) {
        CloseHandle(process->thread_handle);
        process->thread_handle = 0;
    }
    if (process->process_handle != 0) {
        CloseHandle(process->process_handle);
        process->process_handle = 0;
    }
#else
    (void)process;
    (void)terminate_running_process;
#endif
}

static int64_t abi_process_write_handle_bytes(
    HANDLE write_handle,
    const unsigned char* bytes,
    size_t byte_length
) {
    DWORD total_written = 0u;
    while (total_written < byte_length) {
        DWORD bytes_written = 0u;
        DWORD request = (DWORD)((byte_length - total_written) > 32768u ? 32768u : (byte_length - total_written));
        if (!WriteFile(write_handle, bytes + total_written, request, &bytes_written, 0)) {
            return -1;
        }
        if (bytes_written == 0u) {
            break;
        }
        total_written += bytes_written;
    }
    return (int64_t)total_written;
}

static int64_t abi_process_launch_standard_process(
    const KainNativeProcessSpec* spec,
    KainNativeProcessHandle* process
) {
    HANDLE child_stdin = 0;
    HANDLE child_stdout = 0;
    HANDLE child_stderr = 0;
    HANDLE parent_stdin = 0;
    HANDLE parent_stdout = 0;
    HANDLE parent_stderr = 0;
    HANDLE null_stdin = 0;
    HANDLE null_stdout = 0;
    HANDLE null_stderr = 0;
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_information;
    wchar_t* application_name = 0;
    wchar_t* command_line = 0;
    wchar_t* cwd_wide = 0;
    wchar_t* environment_block = 0;
    DWORD creation_flags = CREATE_UNICODE_ENVIRONMENT;
    memset(&startup_info, 0, sizeof(startup_info));
    memset(&process_information, 0, sizeof(process_information));

    application_name = abi_process_resolve_application_name_w(spec->executable);
    command_line = abi_process_build_command_line_w(spec);
    if (command_line == 0) {
        free(application_name);
        return abi_process_fail(
            ABI_PROCESS_SPAWN_FAILED,
            "command-line",
            "failed to build child process command line"
        );
    }
    if (!abi_process_text_empty(spec->current_working_directory)) {
        cwd_wide = abi_process_utf8_to_wide(spec->current_working_directory);
        if (cwd_wide == 0) {
            free(application_name);
            free(command_line);
            return abi_process_fail(
                ABI_PROCESS_SPAWN_FAILED,
                "cwd",
                "failed to convert child current working directory"
            );
        }
    }
    if (!spec->inherit_environment || spec->environment_count > 0) {
        environment_block = abi_process_build_environment_block_w(spec);
        if (environment_block == 0) {
            free(application_name);
            free(command_line);
            free(cwd_wide);
            return abi_process_fail(
                ABI_PROCESS_SPAWN_FAILED,
                "environment",
                "failed to build child environment block"
            );
        }
    }

    if (spec->stdin_mode == ABI_PROCESS_STDIO_PIPE) {
        if (!abi_process_create_pipe_pair(&child_stdin, &parent_stdin, 1)) {
            goto pipe_error;
        }
    } else if (spec->stdin_mode == ABI_PROCESS_STDIO_NULL) {
        null_stdin = abi_process_open_null_device(GENERIC_READ);
        child_stdin = null_stdin;
    } else {
        child_stdin = GetStdHandle(STD_INPUT_HANDLE);
    }

    if (spec->stdout_mode == ABI_PROCESS_STDIO_PIPE) {
        if (!abi_process_create_pipe_pair(&child_stdout, &parent_stdout, 0)) {
            goto pipe_error;
        }
    } else if (spec->stdout_mode == ABI_PROCESS_STDIO_NULL) {
        null_stdout = abi_process_open_null_device(GENERIC_WRITE);
        child_stdout = null_stdout;
    } else {
        child_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    if (spec->stderr_mode == ABI_PROCESS_STDIO_PIPE) {
        if (!abi_process_create_pipe_pair(&child_stderr, &parent_stderr, 0)) {
            goto pipe_error;
        }
    } else if (spec->stderr_mode == ABI_PROCESS_STDIO_NULL) {
        null_stderr = abi_process_open_null_device(GENERIC_WRITE);
        child_stderr = null_stderr;
    } else {
        child_stderr = GetStdHandle(STD_ERROR_HANDLE);
    }

    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = child_stdin;
    startup_info.hStdOutput = child_stdout;
    startup_info.hStdError = child_stderr;

    if (!CreateProcessW(
            application_name,
            command_line,
            0,
            0,
            TRUE,
            creation_flags,
            environment_block,
            cwd_wide,
            &startup_info,
            &process_information
        )) {
        goto spawn_error;
    }

    if (parent_stdin != 0) {
        process->stdin_write_handle = parent_stdin;
    }
    if (parent_stdout != 0) {
        process->stdout_read_handle = parent_stdout;
    }
    if (parent_stderr != 0) {
        process->stderr_read_handle = parent_stderr;
    }
    process->process_handle = process_information.hProcess;
    process->thread_handle = process_information.hThread;
    process->operating_system_process_id = (int64_t)process_information.dwProcessId;

    if (child_stdin != 0 && child_stdin != GetStdHandle(STD_INPUT_HANDLE)) {
        CloseHandle(child_stdin);
        if (child_stdin == null_stdin) {
            null_stdin = 0;
        }
    }
    if (child_stdout != 0 && child_stdout != GetStdHandle(STD_OUTPUT_HANDLE)) {
        CloseHandle(child_stdout);
        if (child_stdout == null_stdout) {
            null_stdout = 0;
        }
    }
    if (child_stderr != 0 && child_stderr != GetStdHandle(STD_ERROR_HANDLE)) {
        CloseHandle(child_stderr);
        if (child_stderr == null_stderr) {
            null_stderr = 0;
        }
    }
    free(application_name);
    free(command_line);
    free(cwd_wide);
    free(environment_block);
    return abi_process_ok();

pipe_error:
    abi_process_fail(
        ABI_PROCESS_IO_ERROR,
        "pipe",
        "failed to create child stdio pipe"
    );
    goto cleanup_failure;
spawn_error:
    abi_process_fail(
        ABI_PROCESS_SPAWN_FAILED,
        "spawn",
        "CreateProcessW failed for child process"
    );
cleanup_failure:
    if (parent_stdin != 0) {
        CloseHandle(parent_stdin);
    }
    if (parent_stdout != 0) {
        CloseHandle(parent_stdout);
    }
    if (parent_stderr != 0) {
        CloseHandle(parent_stderr);
    }
    if (child_stdin != 0 && child_stdin != GetStdHandle(STD_INPUT_HANDLE)) {
        CloseHandle(child_stdin);
        if (child_stdin == null_stdin) {
            null_stdin = 0;
        }
    }
    if (child_stdout != 0 && child_stdout != GetStdHandle(STD_OUTPUT_HANDLE)) {
        CloseHandle(child_stdout);
        if (child_stdout == null_stdout) {
            null_stdout = 0;
        }
    }
    if (child_stderr != 0 && child_stderr != GetStdHandle(STD_ERROR_HANDLE)) {
        CloseHandle(child_stderr);
        if (child_stderr == null_stderr) {
            null_stderr = 0;
        }
    }
    if (null_stdin != 0) {
        CloseHandle(null_stdin);
    }
    if (null_stdout != 0) {
        CloseHandle(null_stdout);
    }
    if (null_stderr != 0) {
        CloseHandle(null_stderr);
    }
    free(application_name);
    free(command_line);
    free(cwd_wide);
    free(environment_block);
    return g_last_status;
}

static int64_t abi_process_launch_pty_process(
    const KainNativeProcessSpec* spec,
    KainNativeProcessHandle* process,
    int64_t columns,
    int64_t rows
) {
    HMODULE kernel_module = GetModuleHandleW(L"kernel32.dll");
    KainCreatePseudoConsoleFn create_pseudo_console = 0;
    KainClosePseudoConsoleFn close_pseudo_console = 0;
    KainResizePseudoConsoleFn resize_pseudo_console = 0;
    HANDLE console_input_read = 0;
    HANDLE console_output_write = 0;
    HANDLE parent_input_write = 0;
    HANDLE parent_output_read = 0;
    SIZE_T attribute_list_bytes = 0u;
    STARTUPINFOEXW startup_info_ex;
    PROCESS_INFORMATION process_information;
    wchar_t* command_line = 0;
    wchar_t* cwd_wide = 0;
    wchar_t* environment_block = 0;
    HANDLE pseudo_console = 0;
    HRESULT console_result;
    memset(&startup_info_ex, 0, sizeof(startup_info_ex));
    memset(&process_information, 0, sizeof(process_information));

    if (kernel_module != 0) {
        create_pseudo_console = (KainCreatePseudoConsoleFn)GetProcAddress(kernel_module, "CreatePseudoConsole");
        close_pseudo_console = (KainClosePseudoConsoleFn)GetProcAddress(kernel_module, "ClosePseudoConsole");
        resize_pseudo_console = (KainResizePseudoConsoleFn)GetProcAddress(kernel_module, "ResizePseudoConsole");
    }
    if (create_pseudo_console == 0 || close_pseudo_console == 0 || resize_pseudo_console == 0) {
        return abi_process_fail(
            ABI_PROCESS_PTY_UNAVAILABLE,
            "pty",
            "Windows ConPTY entry points are not available on this host"
        );
    }

    command_line = abi_process_build_command_line_w(spec);
    if (command_line == 0) {
        return abi_process_fail(
            ABI_PROCESS_SPAWN_FAILED,
            "command-line",
            "failed to build PTY command line"
        );
    }
    if (!abi_process_text_empty(spec->current_working_directory)) {
        cwd_wide = abi_process_utf8_to_wide(spec->current_working_directory);
        if (cwd_wide == 0) {
            free(command_line);
            return abi_process_fail(
                ABI_PROCESS_SPAWN_FAILED,
                "cwd",
                "failed to convert PTY current working directory"
            );
        }
    }
    if (!spec->inherit_environment || spec->environment_count > 0) {
        environment_block = abi_process_build_environment_block_w(spec);
        if (environment_block == 0) {
            free(command_line);
            free(cwd_wide);
            return abi_process_fail(
                ABI_PROCESS_SPAWN_FAILED,
                "environment",
                "failed to build PTY environment block"
            );
        }
    }

    if (!abi_process_create_pipe_pair(&console_input_read, &parent_input_write, 1)) {
        goto pty_io_error;
    }
    if (!abi_process_create_pipe_pair(&console_output_write, &parent_output_read, 0)) {
        goto pty_io_error;
    }

    console_result = create_pseudo_console(
        (COORD){(SHORT)columns, (SHORT)rows},
        console_input_read,
        console_output_write,
        0u,
        &pseudo_console
    );
    if (FAILED(console_result) || pseudo_console == 0) {
        abi_process_fail(
            ABI_PROCESS_PTY_UNAVAILABLE,
            "pty",
            "CreatePseudoConsole failed"
        );
        goto pty_cleanup_error;
    }

    startup_info_ex.StartupInfo.cb = sizeof(startup_info_ex);
    InitializeProcThreadAttributeList(0, 1u, 0u, &attribute_list_bytes);
    startup_info_ex.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(attribute_list_bytes);
    if (startup_info_ex.lpAttributeList == 0) {
        abi_process_fail(
            ABI_PROCESS_SPAWN_FAILED,
            "pty",
            "failed to allocate PTY attribute list"
        );
        goto pty_cleanup_error;
    }
    if (!InitializeProcThreadAttributeList(
            startup_info_ex.lpAttributeList,
            1u,
            0u,
            &attribute_list_bytes
        )) {
        abi_process_fail(
            ABI_PROCESS_SPAWN_FAILED,
            "pty",
            "failed to initialize PTY attribute list"
        );
        goto pty_cleanup_error;
    }
    if (!UpdateProcThreadAttribute(
            startup_info_ex.lpAttributeList,
            0u,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            pseudo_console,
            sizeof(pseudo_console),
            0,
            0
        )) {
        abi_process_fail(
            ABI_PROCESS_SPAWN_FAILED,
            "pty",
            "failed to bind pseudo console attribute"
        );
        goto pty_cleanup_error;
    }

    startup_info_ex.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup_info_ex.StartupInfo.hStdInput = console_input_read;
    startup_info_ex.StartupInfo.hStdOutput = console_output_write;
    startup_info_ex.StartupInfo.hStdError = console_output_write;

    if (!CreateProcessW(
            0,
            command_line,
            0,
            0,
            TRUE,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            environment_block,
            cwd_wide,
            &startup_info_ex.StartupInfo,
            &process_information
        )) {
        abi_process_fail(
            ABI_PROCESS_SPAWN_FAILED,
            "spawn",
            "CreateProcessW failed for PTY child process"
        );
        goto pty_cleanup_error;
    }

    CloseHandle(console_input_read);
    CloseHandle(console_output_write);
    DeleteProcThreadAttributeList(startup_info_ex.lpAttributeList);
    free(startup_info_ex.lpAttributeList);
    startup_info_ex.lpAttributeList = 0;
    process->is_pty = 1;
    process->pty_console_handle = pseudo_console;
    process->pty_input_write_handle = parent_input_write;
    process->pty_output_read_handle = parent_output_read;
    process->process_handle = process_information.hProcess;
    process->thread_handle = process_information.hThread;
    process->operating_system_process_id = (int64_t)process_information.dwProcessId;
    free(command_line);
    free(cwd_wide);
    free(environment_block);
    return abi_process_ok();

pty_io_error:
    abi_process_fail(
        ABI_PROCESS_IO_ERROR,
        "pty",
        "failed to create PTY transport pipes"
    );
pty_cleanup_error:
    if (console_input_read != 0) {
        CloseHandle(console_input_read);
    }
    if (console_output_write != 0) {
        CloseHandle(console_output_write);
    }
    if (parent_input_write != 0) {
        CloseHandle(parent_input_write);
    }
    if (parent_output_read != 0) {
        CloseHandle(parent_output_read);
    }
    if (startup_info_ex.lpAttributeList != 0) {
        DeleteProcThreadAttributeList(startup_info_ex.lpAttributeList);
        free(startup_info_ex.lpAttributeList);
    }
    if (pseudo_console != 0 && close_pseudo_console != 0) {
        close_pseudo_console(pseudo_console);
    }
    free(command_line);
    free(cwd_wide);
    free(environment_block);
    return g_last_status;
}
#endif

int64_t abi_process_reset(void) {
    int index;
    for (index = 0; index < ABI_PROCESS_MAX_PROCESSES; index++) {
        if (g_processes[index].in_use) {
            abi_process_release_handle(&g_processes[index], 1);
        }
    }
    memset(g_specs, 0, sizeof(g_specs));
    memset(g_processes, 0, sizeof(g_processes));
    memset(g_spec_index, 0, sizeof(g_spec_index));
    memset(g_process_index, 0, sizeof(g_process_index));
    g_spec_occupancy_bits = 0u;
    g_process_occupancy_bits = 0u;
    g_next_spec_id = 1;
    g_next_process_id = 1;
    return abi_process_ok();
}

void kain_attrition_process_counters_reset(void) {
    atomic_store_explicit(&g_attrition_process_live_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_process_peak_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_process_spawn_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_process_exit_count, 0u, memory_order_relaxed);
    atomic_store_explicit(&g_attrition_process_stale_reject_count, 0u, memory_order_relaxed);
}

void kain_attrition_process_fill_snapshot(KainAttritionSnapshot* snapshot) {
    size_t slot;
    uint64_t process_pipe_handle_live_count = 0u;
    uint64_t process_os_handle_live_count = 0u;
    uint64_t process_pty_live_count = 0u;
    uint64_t process_capture_live_bytes = 0u;
    if (snapshot == NULL) {
        return;
    }
    snapshot->process_live_count = atomic_load_explicit(&g_attrition_process_live_count, memory_order_relaxed);
    snapshot->process_peak_count = atomic_load_explicit(&g_attrition_process_peak_count, memory_order_relaxed);
    snapshot->process_spawn_count = atomic_load_explicit(&g_attrition_process_spawn_count, memory_order_relaxed);
    snapshot->process_exit_count = atomic_load_explicit(&g_attrition_process_exit_count, memory_order_relaxed);
    snapshot->process_stale_reject_count = atomic_load_explicit(
        &g_attrition_process_stale_reject_count,
        memory_order_relaxed);
    snapshot->process_spec_live_count = abi_process_popcount_u64(g_spec_occupancy_bits);
    snapshot->process_spec_occupancy_bits = g_spec_occupancy_bits;
    snapshot->process_occupancy_bits = g_process_occupancy_bits;
    for (slot = 0u; slot < ABI_PROCESS_MAX_PROCESSES; ++slot) {
        KainNativeProcessHandle* process = &g_processes[slot];
        if (!process->in_use) {
            continue;
        }
        process_capture_live_bytes += (uint64_t)process->stdout_capture.length;
        process_capture_live_bytes += (uint64_t)process->stderr_capture.length;
        process_capture_live_bytes += (uint64_t)process->pty_capture.length;
        if (process->is_pty) {
            process_pty_live_count += 1u;
        }
#ifdef _WIN32
        if (process->process_handle != 0) {
            process_os_handle_live_count += 1u;
        }
        if (process->thread_handle != 0) {
            process_os_handle_live_count += 1u;
        }
        if (process->stdin_write_handle != 0) {
            process_pipe_handle_live_count += 1u;
            process_os_handle_live_count += 1u;
        }
        if (process->stdout_read_handle != 0) {
            process_pipe_handle_live_count += 1u;
            process_os_handle_live_count += 1u;
        }
        if (process->stderr_read_handle != 0) {
            process_pipe_handle_live_count += 1u;
            process_os_handle_live_count += 1u;
        }
        if (process->pty_console_handle != 0) {
            process_os_handle_live_count += 1u;
        }
        if (process->pty_input_write_handle != 0) {
            process_pipe_handle_live_count += 1u;
            process_os_handle_live_count += 1u;
        }
        if (process->pty_output_read_handle != 0) {
            process_pipe_handle_live_count += 1u;
            process_os_handle_live_count += 1u;
        }
#endif
    }
    snapshot->process_pipe_handle_live_count = process_pipe_handle_live_count;
    snapshot->process_os_handle_live_count = process_os_handle_live_count;
    snapshot->process_pty_live_count = process_pty_live_count;
    snapshot->process_capture_live_bytes = process_capture_live_bytes;
}

int64_t abi_process_platform_available(void) {
#ifdef _WIN32
    return 1;
#else
    return 0;
#endif
}

int64_t abi_process_spec_create(const char* executable) {
    KainNativeProcessSpec* spec = 0;
    uint32_t slot;
    uint64_t bit;
    if (abi_process_text_empty(executable)) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_ARGUMENT,
            "invalid-argument",
            "process executable cannot be empty"
        );
    }
    if (!abi_process_find_free_spec_slot(&slot)) {
        return abi_process_fail(
            ABI_PROCESS_CAPACITY_EXCEEDED,
            "capacity",
            "process specification registry is full"
        );
    }
    spec = &g_specs[slot];
    memset(spec, 0, sizeof(*spec));
    spec->in_use = 1;
    spec->id = g_next_spec_id++;
    spec->inherit_environment = 1;
    spec->stdin_mode = ABI_PROCESS_STDIO_INHERIT;
    spec->stdout_mode = ABI_PROCESS_STDIO_INHERIT;
    spec->stderr_mode = ABI_PROCESS_STDIO_INHERIT;
    abi_process_copy(spec->executable, sizeof(spec->executable), executable);
    bit = UINT64_C(1) << slot;
    g_spec_occupancy_bits |= bit;
    if (!abi_process_index_insert(
            g_spec_index,
            ABI_PROCESS_SPEC_INDEX_CAPACITY,
            ABI_PROCESS_SPEC_INDEX_MASK,
            (uint64_t)spec->id,
            slot)) {
        g_spec_occupancy_bits &= ~bit;
        memset(spec, 0, sizeof(*spec));
        return abi_process_fail(
            ABI_PROCESS_CAPACITY_EXCEEDED,
            "capacity",
            "process specification registry is full"
        );
    }
    abi_process_ok();
    return spec->id;
}

int64_t abi_process_spec_destroy(int64_t spec_id) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    uint32_t slot;
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "process specification id is not active"
        );
    }
    slot = (uint32_t)(spec - g_specs);
    g_spec_occupancy_bits &= ~(UINT64_C(1) << slot);
    memset(spec, 0, sizeof(*spec));
    abi_process_rebuild_spec_index();
    return abi_process_ok();
}

int64_t abi_process_spec_count(void) {
    uint64_t value = g_spec_occupancy_bits;
    value = value - ((value >> 1u) & UINT64_C(0x5555555555555555));
    value = (value & UINT64_C(0x3333333333333333)) + ((value >> 2u) & UINT64_C(0x3333333333333333));
    value = (value + (value >> 4u)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
    return (int64_t)((value * UINT64_C(0x0101010101010101)) >> 56u);
}

int64_t abi_process_spec_add_arg(int64_t spec_id, const char* argument) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot add argument to a missing process specification"
        );
    }
    if (argument == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_ARGUMENT,
            "invalid-argument",
            "process argument cannot be null"
        );
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-process-argument-count-stays-within-capacity.yaml */
    if (spec->argument_count >= ABI_PROCESS_MAX_ARGUMENTS) {
        return abi_process_fail(
            ABI_PROCESS_CAPACITY_EXCEEDED,
            "capacity",
            "process specification argument capacity exceeded"
        );
    }
    abi_process_copy(
        spec->arguments[spec->argument_count],
        sizeof(spec->arguments[spec->argument_count]),
        argument
    );
    spec->argument_count += 1;
    return abi_process_ok();
}

int64_t abi_process_spec_set_cwd(int64_t spec_id, const char* cwd_path) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot set cwd on a missing process specification"
        );
    }
    abi_process_copy(
        spec->current_working_directory,
        sizeof(spec->current_working_directory),
        cwd_path ? cwd_path : ""
    );
    return abi_process_ok();
}

int64_t abi_process_spec_set_env(int64_t spec_id, const char* key, const char* value) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    int64_t index;
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot set environment on a missing process specification"
        );
    }
    if (abi_process_text_empty(key)) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_ARGUMENT,
            "invalid-argument",
            "process environment key cannot be empty"
        );
    }
    for (index = 0; index < spec->environment_count; index++) {
        if (spec->environment[index].in_use
            && abi_process_text_equal_ci(spec->environment[index].key, key)) {
            abi_process_copy(spec->environment[index].value, sizeof(spec->environment[index].value), value ? value : "");
            return abi_process_ok();
        }
    }
    /* Proof: runtime/native/src/core/z3/proofs/native-process-environment-count-stays-within-capacity.yaml */
    if (spec->environment_count >= ABI_PROCESS_MAX_ENVIRONMENT_ENTRIES) {
        return abi_process_fail(
            ABI_PROCESS_CAPACITY_EXCEEDED,
            "capacity",
            "process environment override capacity exceeded"
        );
    }
    spec->environment[spec->environment_count].in_use = 1;
    abi_process_copy(
        spec->environment[spec->environment_count].key,
        sizeof(spec->environment[spec->environment_count].key),
        key
    );
    abi_process_copy(
        spec->environment[spec->environment_count].value,
        sizeof(spec->environment[spec->environment_count].value),
        value ? value : ""
    );
    spec->environment_count += 1;
    return abi_process_ok();
}

int64_t abi_process_spec_set_inherit_environment(int64_t spec_id, int64_t enabled) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot set inherit_environment on a missing process specification"
        );
    }
    spec->inherit_environment = enabled != 0;
    return abi_process_ok();
}

static int64_t abi_process_spec_set_mode(
    int64_t spec_id,
    const char* mode_text,
    KainNativeProcessStdioMode* target_mode
) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    KainNativeProcessStdioMode mode;
    (void)spec;
    if (spec == 0 || target_mode == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot set stdio mode on a missing process specification"
        );
    }
    if (!abi_process_mode_from_text(mode_text, &mode)) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_ARGUMENT,
            "invalid-mode",
            "stdio mode must be inherit, pipe, or null"
        );
    }
    *target_mode = mode;
    return abi_process_ok();
}

int64_t abi_process_spec_set_stdin_mode(int64_t spec_id, const char* mode) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot set stdin mode on a missing process specification"
        );
    }
    return abi_process_spec_set_mode(spec_id, mode, &spec->stdin_mode);
}

int64_t abi_process_spec_set_stdout_mode(int64_t spec_id, const char* mode) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot set stdout mode on a missing process specification"
        );
    }
    return abi_process_spec_set_mode(spec_id, mode, &spec->stdout_mode);
}

int64_t abi_process_spec_set_stderr_mode(int64_t spec_id, const char* mode) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot set stderr mode on a missing process specification"
        );
    }
    return abi_process_spec_set_mode(spec_id, mode, &spec->stderr_mode);
}

static KainNativeProcessHandle* abi_process_allocate_slot(void) {
    uint32_t slot;
    uint64_t bit;
    if (!abi_process_find_free_process_slot(&slot)) {
        return 0;
    }
    memset(&g_processes[slot], 0, sizeof(g_processes[slot]));
    g_processes[slot].in_use = 1;
    g_processes[slot].id = g_next_process_id++;
    bit = UINT64_C(1) << slot;
    g_process_occupancy_bits |= bit;
    if (!abi_process_index_insert(
            g_process_index,
            ABI_PROCESS_PROCESS_INDEX_CAPACITY,
            ABI_PROCESS_PROCESS_INDEX_MASK,
            (uint64_t)g_processes[slot].id,
            slot)) {
        g_process_occupancy_bits &= ~bit;
        memset(&g_processes[slot], 0, sizeof(g_processes[slot]));
        return 0;
    }
    {
        uint64_t live_count = atomic_fetch_add_explicit(
                                  &g_attrition_process_live_count,
                                  1u,
                                  memory_order_relaxed) + 1u;
        atomic_fetch_add_explicit(&g_attrition_process_spawn_count, 1u, memory_order_relaxed);
        abi_process_attrition_update_peak(&g_attrition_process_peak_count, live_count);
    }
    kain_attrition_note_process_spawn((uint64_t)g_processes[slot].id);
    return &g_processes[slot];
}

int64_t abi_process_spawn(int64_t spec_id) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    KainNativeProcessHandle* process;
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot spawn a missing process specification"
        );
    }
    process = abi_process_allocate_slot();
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_CAPACITY_EXCEEDED,
            "capacity",
            "process handle registry is full"
        );
    }
#ifdef _WIN32
    if (abi_process_launch_standard_process(spec, process) != ABI_PROCESS_OK) {
        abi_process_release_handle(process, 0);
        return g_last_status;
    }
    return process->id;
#else
    abi_process_release_handle(process, 0);
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process spawning is not implemented for this host yet"
    );
#endif
}

int64_t abi_process_spawn_pty(int64_t spec_id, int64_t columns, int64_t rows) {
    KainNativeProcessSpec* spec = abi_process_spec_lookup(spec_id);
    KainNativeProcessHandle* process;
    if (spec == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_SPEC,
            "invalid-spec",
            "cannot spawn a PTY from a missing process specification"
        );
    }
    if (columns <= 0 || rows <= 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_ARGUMENT,
            "invalid-argument",
            "PTY size must use positive column and row counts"
        );
    }
    process = abi_process_allocate_slot();
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_CAPACITY_EXCEEDED,
            "capacity",
            "process handle registry is full"
        );
    }
#ifdef _WIN32
    if (abi_process_launch_pty_process(spec, process, columns, rows) != ABI_PROCESS_OK) {
        abi_process_release_handle(process, 0);
        return g_last_status;
    }
    return process->id;
#else
    abi_process_release_handle(process, 0);
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native PTY spawning is not implemented for this host yet"
    );
#endif
}

int64_t abi_process_count(void) {
    uint64_t value = g_process_occupancy_bits;
    value = value - ((value >> 1u) & UINT64_C(0x5555555555555555));
    value = (value & UINT64_C(0x3333333333333333)) + ((value >> 2u) & UINT64_C(0x3333333333333333));
    value = (value + (value >> 4u)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
    return (int64_t)((value * UINT64_C(0x0101010101010101)) >> 56u);
}

int64_t abi_process_close(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot close a missing process handle"
        );
    }
    abi_process_release_handle(process, 0);
    return abi_process_ok();
}

int64_t abi_process_poll(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    unsigned char* primary_chunk = 0;
    unsigned char* secondary_chunk = 0;
    size_t primary_length = 0u;
    size_t secondary_length = 0u;
    int exited = 0;
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot poll a missing process handle"
        );
    }
#ifdef _WIN32
    if (!abi_process_wait_internal(process, 0, &exited)) {
        return abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "wait",
            "failed to poll child process state"
        );
    }
    if (!abi_process_pump_output(
            process,
            &primary_chunk,
            &primary_length,
            &secondary_chunk,
            &secondary_length
        )) {
        return abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "read",
            "failed to drain child process output"
        );
    }
    free(primary_chunk);
    free(secondary_chunk);
    abi_process_ok();
    return exited ? 1 : 0;
#else
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process polling is not implemented for this host yet"
    );
#endif
}

int64_t abi_process_wait(int64_t process_id, int64_t timeout_ms) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    unsigned char* primary_chunk = 0;
    unsigned char* secondary_chunk = 0;
    size_t primary_length = 0u;
    size_t secondary_length = 0u;
    int exited = 0;
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot wait on a missing process handle"
        );
    }
#ifdef _WIN32
    if (!abi_process_wait_internal(process, timeout_ms, &exited)) {
        return abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "wait",
            "failed to wait on child process"
        );
    }
    if (exited) {
        abi_process_flush_exited_output(process);
    }
    if (!abi_process_pump_output(
            process,
            &primary_chunk,
            &primary_length,
            &secondary_chunk,
            &secondary_length
        )) {
        return abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "read",
            "failed to drain child process output while waiting"
        );
    }
    free(primary_chunk);
    free(secondary_chunk);
    abi_process_ok();
    return exited ? 1 : 0;
#else
    (void)timeout_ms;
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process waiting is not implemented for this host yet"
    );
#endif
}

int64_t abi_process_is_running(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot query a missing process handle"
        );
    }
#ifdef _WIN32
    abi_process_refresh_exit_state(process);
    abi_process_ok();
    return process->exited ? 0 : 1;
#else
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process status is not implemented for this host yet"
    );
#endif
}

int64_t abi_process_exit_code(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot query exit code for a missing process handle"
        );
    }
#ifdef _WIN32
    abi_process_refresh_exit_state(process);
    if (!process->exited) {
        return abi_process_fail(
            ABI_PROCESS_STILL_RUNNING,
            "still-running",
            "child process has not exited yet"
        );
    }
    abi_process_ok();
    return process->exit_code;
#else
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process exit codes are not implemented for this host yet"
    );
#endif
}

int64_t abi_process_os_pid(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot query pid for a missing process handle"
        );
    }
    abi_process_ok();
    return process->operating_system_process_id;
}

int64_t abi_process_terminate(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot terminate a missing process handle"
        );
    }
#ifdef _WIN32
    if (process->process_handle == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "process handle is missing an operating system child handle"
        );
    }
    if (!TerminateProcess(process->process_handle, 1u)) {
        return abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "terminate",
            "TerminateProcess failed"
        );
    }
    WaitForSingleObject(process->process_handle, 250u);
    abi_process_refresh_exit_state(process);
    return abi_process_ok();
#else
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process termination is not implemented for this host yet"
    );
#endif
}

int64_t abi_process_kill(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot kill a missing process handle"
        );
    }
#ifdef _WIN32
    if (process->process_handle == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "process handle is missing an operating system child handle"
        );
    }
    if (!TerminateProcess(process->process_handle, 137u)) {
        return abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "kill",
            "TerminateProcess failed"
        );
    }
    WaitForSingleObject(process->process_handle, 250u);
    abi_process_refresh_exit_state(process);
    return abi_process_ok();
#else
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process killing is not implemented for this host yet"
    );
#endif
}

static int64_t abi_process_write_plain_text(
    int64_t process_id,
    const char* text,
    int use_pty_channel
) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot write to a missing process handle"
        );
    }
#ifdef _WIN32
    HANDLE target_handle = use_pty_channel ? process->pty_input_write_handle : process->stdin_write_handle;
    int64_t written;
    if (target_handle == 0) {
        return abi_process_fail(
            ABI_PROCESS_PIPE_NOT_AVAILABLE,
            "missing-pipe",
            use_pty_channel ? "PTY input pipe is not available" : "stdin pipe is not available"
        );
    }
    written = abi_process_write_handle_bytes(
        target_handle,
        (const unsigned char*)(text ? text : ""),
        text ? strlen(text) : 0u
    );
    if (written < 0) {
        return abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "write",
            "failed to write child process input"
        );
    }
    abi_process_ok();
    return written;
#else
    (void)text;
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process input is not implemented for this host yet"
    );
#endif
}

static int64_t abi_process_write_hex(
    int64_t process_id,
    const char* bytes_hex,
    int use_pty_channel
) {
    unsigned char* decoded = 0;
    size_t decoded_length = 0u;
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot write bytes to a missing process handle"
        );
    }
    if (!abi_process_decode_hex(bytes_hex, &decoded, &decoded_length)) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_ARGUMENT,
            "invalid-hex",
            "hex byte payload must contain complete hexadecimal bytes"
        );
    }
#ifdef _WIN32
    HANDLE target_handle = use_pty_channel ? process->pty_input_write_handle : process->stdin_write_handle;
    int64_t written;
    if (target_handle == 0) {
        free(decoded);
        return abi_process_fail(
            ABI_PROCESS_PIPE_NOT_AVAILABLE,
            "missing-pipe",
            use_pty_channel ? "PTY input pipe is not available" : "stdin pipe is not available"
        );
    }
    written = abi_process_write_handle_bytes(target_handle, decoded, decoded_length);
    free(decoded);
    if (written < 0) {
        return abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "write",
            "failed to write child process input bytes"
        );
    }
    abi_process_ok();
    return written;
#else
    free(decoded);
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process byte input is not implemented for this host yet"
    );
#endif
}

int64_t abi_process_stdin_write_text(int64_t process_id, const char* text) {
    return abi_process_write_plain_text(process_id, text, 0);
}

int64_t abi_process_stdin_write_hex(int64_t process_id, const char* bytes_hex) {
    return abi_process_write_hex(process_id, bytes_hex, 0);
}

int64_t abi_process_stdin_close(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot close stdin on a missing process handle"
        );
    }
#ifdef _WIN32
    if (process->stdin_write_handle == 0) {
        return abi_process_fail(
            ABI_PROCESS_PIPE_NOT_AVAILABLE,
            "missing-pipe",
            "stdin pipe is not available"
        );
    }
    CloseHandle(process->stdin_write_handle);
    process->stdin_write_handle = 0;
    return abi_process_ok();
#else
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process stdin closing is not implemented for this host yet"
    );
#endif
}

static const char* abi_process_read_stream_text(
    int64_t process_id,
    int stream_kind
) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    unsigned char* primary_chunk = 0;
    unsigned char* secondary_chunk = 0;
    size_t primary_length = 0u;
    size_t secondary_length = 0u;
    const char* output;
    if (process == 0) {
        abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot read output from a missing process handle"
        );
        return string_new("");
    }
#ifdef _WIN32
    if (!abi_process_pump_output(
            process,
            &primary_chunk,
            &primary_length,
            &secondary_chunk,
            &secondary_length
        )) {
        abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "read",
            "failed to read child process output"
        );
        return string_new("");
    }
    if (stream_kind == 0) {
        output = process->is_pty
            ? abi_process_string_from_bytes(primary_chunk, primary_length)
            : abi_process_string_from_bytes(primary_chunk, primary_length);
    } else {
        output = process->is_pty
            ? string_new("")
            : abi_process_string_from_bytes(secondary_chunk, secondary_length);
    }
    free(primary_chunk);
    free(secondary_chunk);
    abi_process_ok();
    return output;
#else
    (void)stream_kind;
    abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process output is not implemented for this host yet"
    );
    return string_new("");
#endif
}

static const char* abi_process_read_stream_hex(
    int64_t process_id,
    int stream_kind
) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    unsigned char* primary_chunk = 0;
    unsigned char* secondary_chunk = 0;
    size_t primary_length = 0u;
    size_t secondary_length = 0u;
    const char* output;
    if (process == 0) {
        abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot read output bytes from a missing process handle"
        );
        return string_new("");
    }
#ifdef _WIN32
    if (!abi_process_pump_output(
            process,
            &primary_chunk,
            &primary_length,
            &secondary_chunk,
            &secondary_length
        )) {
        abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "read",
            "failed to read child process output bytes"
        );
        return string_new("");
    }
    if (stream_kind == 0) {
        output = abi_process_encode_hex(primary_chunk, primary_length);
    } else {
        output = process->is_pty ? string_new("") : abi_process_encode_hex(secondary_chunk, secondary_length);
    }
    free(primary_chunk);
    free(secondary_chunk);
    abi_process_ok();
    return output;
#else
    (void)stream_kind;
    abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process output bytes are not implemented for this host yet"
    );
    return string_new("");
#endif
}

static const char* abi_process_capture_text_internal(
    int64_t process_id,
    int capture_kind
) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    unsigned char* primary_chunk = 0;
    unsigned char* secondary_chunk = 0;
    size_t primary_length = 0u;
    size_t secondary_length = 0u;
    const KainNativeProcessCapture* capture = 0;
    if (process == 0) {
        abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot query output capture for a missing process handle"
        );
        return string_new("");
    }
#ifdef _WIN32
    if (!abi_process_pump_output(
            process,
            &primary_chunk,
            &primary_length,
            &secondary_chunk,
            &secondary_length
        )) {
        abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "read",
            "failed to refresh output capture"
        );
        return string_new("");
    }
    free(primary_chunk);
    free(secondary_chunk);
    if (capture_kind == 0) {
        capture = process->is_pty ? &process->pty_capture : &process->stdout_capture;
    } else {
        capture = &process->stderr_capture;
    }
    abi_process_ok();
    return abi_process_string_from_bytes(capture->bytes, capture->length);
#else
    (void)capture_kind;
    abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process output capture is not implemented for this host yet"
    );
    return string_new("");
#endif
}

static const char* abi_process_capture_hex_internal(
    int64_t process_id,
    int capture_kind
) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    unsigned char* primary_chunk = 0;
    unsigned char* secondary_chunk = 0;
    size_t primary_length = 0u;
    size_t secondary_length = 0u;
    const KainNativeProcessCapture* capture = 0;
    if (process == 0) {
        abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot query output hex capture for a missing process handle"
        );
        return string_new("");
    }
#ifdef _WIN32
    if (!abi_process_pump_output(
            process,
            &primary_chunk,
            &primary_length,
            &secondary_chunk,
            &secondary_length
        )) {
        abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "read",
            "failed to refresh output hex capture"
        );
        return string_new("");
    }
    free(primary_chunk);
    free(secondary_chunk);
    if (capture_kind == 0) {
        capture = process->is_pty ? &process->pty_capture : &process->stdout_capture;
    } else {
        capture = &process->stderr_capture;
    }
    abi_process_ok();
    return abi_process_encode_hex(capture->bytes, capture->length);
#else
    (void)capture_kind;
    abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "native process output hex capture is not implemented for this host yet"
    );
    return string_new("");
#endif
}

const char* abi_process_stdout_read_text(int64_t process_id) {
    return abi_process_read_stream_text(process_id, 0);
}

const char* abi_process_stdout_read_hex(int64_t process_id) {
    return abi_process_read_stream_hex(process_id, 0);
}

const char* abi_process_stderr_read_text(int64_t process_id) {
    return abi_process_read_stream_text(process_id, 1);
}

const char* abi_process_stderr_read_hex(int64_t process_id) {
    return abi_process_read_stream_hex(process_id, 1);
}

const char* abi_process_stdout_capture_text(int64_t process_id) {
    return abi_process_capture_text_internal(process_id, 0);
}

const char* abi_process_stdout_capture_hex(int64_t process_id) {
    return abi_process_capture_hex_internal(process_id, 0);
}

const char* abi_process_stderr_capture_text(int64_t process_id) {
    return abi_process_capture_text_internal(process_id, 1);
}

const char* abi_process_stderr_capture_hex(int64_t process_id) {
    return abi_process_capture_hex_internal(process_id, 1);
}

int64_t abi_process_pty_write_text(int64_t process_id, const char* text) {
    return abi_process_write_plain_text(process_id, text, 1);
}

int64_t abi_process_pty_write_hex(int64_t process_id, const char* bytes_hex) {
    return abi_process_write_hex(process_id, bytes_hex, 1);
}

int64_t abi_process_pty_resize(int64_t process_id, int64_t columns, int64_t rows) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot resize a missing PTY process handle"
        );
    }
    if (!process->is_pty) {
        return abi_process_fail(
            ABI_PROCESS_PTY_UNAVAILABLE,
            "pty",
            "process handle does not own a PTY session"
        );
    }
    if (columns <= 0 || rows <= 0) {
        return abi_process_fail(
            ABI_PROCESS_INVALID_ARGUMENT,
            "invalid-argument",
            "PTY resize requires positive column and row counts"
        );
    }
#ifdef _WIN32
    {
        HMODULE kernel_module = GetModuleHandleW(L"kernel32.dll");
        KainResizePseudoConsoleFn resize_pseudo_console = 0;
        if (kernel_module != 0) {
            resize_pseudo_console = (KainResizePseudoConsoleFn)GetProcAddress(
                kernel_module,
                "ResizePseudoConsole"
            );
        }
        if (resize_pseudo_console == 0 || process->pty_console_handle == 0) {
            return abi_process_fail(
                ABI_PROCESS_PTY_UNAVAILABLE,
                "pty",
                "ResizePseudoConsole is not available on this host"
            );
        }
        if (FAILED(resize_pseudo_console(
                process->pty_console_handle,
                (COORD){(SHORT)columns, (SHORT)rows}
            ))) {
            return abi_process_fail(
                ABI_PROCESS_IO_ERROR,
                "pty",
                "ResizePseudoConsole failed"
            );
        }
    }
    return abi_process_ok();
#else
    return abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "PTY resize is not implemented for this host yet"
    );
#endif
}

const char* abi_process_pty_read_text(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot read PTY output from a missing process handle"
        );
        return string_new("");
    }
    if (!process->is_pty) {
        abi_process_fail(
            ABI_PROCESS_PTY_UNAVAILABLE,
            "pty",
            "process handle does not own a PTY session"
        );
        return string_new("");
    }
    return abi_process_read_stream_text(process_id, 0);
}

const char* abi_process_pty_read_hex(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot read PTY output bytes from a missing process handle"
        );
        return string_new("");
    }
    if (!process->is_pty) {
        abi_process_fail(
            ABI_PROCESS_PTY_UNAVAILABLE,
            "pty",
            "process handle does not own a PTY session"
        );
        return string_new("");
    }
    return abi_process_read_stream_hex(process_id, 0);
}

const char* abi_process_pty_capture_text(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot query PTY capture from a missing process handle"
        );
        return string_new("");
    }
    if (!process->is_pty) {
        abi_process_fail(
            ABI_PROCESS_PTY_UNAVAILABLE,
            "pty",
            "process handle does not own a PTY session"
        );
        return string_new("");
    }
    return abi_process_capture_text_internal(process_id, 0);
}

const char* abi_process_pty_capture_hex(int64_t process_id) {
    KainNativeProcessHandle* process = abi_process_lookup(process_id);
    if (process == 0) {
        abi_process_fail(
            ABI_PROCESS_INVALID_PROCESS,
            "invalid-process",
            "cannot query PTY hex capture from a missing process handle"
        );
        return string_new("");
    }
    if (!process->is_pty) {
        abi_process_fail(
            ABI_PROCESS_PTY_UNAVAILABLE,
            "pty",
            "process handle does not own a PTY session"
        );
        return string_new("");
    }
    return abi_process_capture_hex_internal(process_id, 0);
}

const char* abi_process_output_text(
    const char* executable,
    const char* arg0,
    const char* arg1,
    const char* arg2,
    int64_t timeout_ms
) {
    KainNativeProcessSpec spec;
    KainNativeProcessHandle process;
    unsigned char* stdout_chunk = 0;
    unsigned char* stderr_chunk = 0;
    size_t stdout_chunk_length = 0u;
    size_t stderr_chunk_length = 0u;
    const char* output = 0;
    int exited = 0;
    int attrition_live = 0;

    if (abi_process_text_empty(executable)) {
        abi_process_fail(
            ABI_PROCESS_INVALID_ARGUMENT,
            "invalid-argument",
            "process output requires an executable"
        );
        return string_new("");
    }

    memset(&spec, 0, sizeof(spec));
    memset(&process, 0, sizeof(process));
    spec.in_use = 1;
    spec.inherit_environment = 1;
    spec.stdin_mode = ABI_PROCESS_STDIO_NULL;
    spec.stdout_mode = ABI_PROCESS_STDIO_PIPE;
    /* `process_output_text(...)` only surfaces stdout, so keep stderr off the
     * pipe path and let the child write it straight to NUL. */
    spec.stderr_mode = ABI_PROCESS_STDIO_NULL;
    abi_process_copy(spec.executable, sizeof(spec.executable), executable);
    if (!abi_process_spec_append_direct_arg(&spec, arg0) ||
        !abi_process_spec_append_direct_arg(&spec, arg1) ||
        !abi_process_spec_append_direct_arg(&spec, arg2)) {
        abi_process_fail(
            ABI_PROCESS_INVALID_ARGUMENT,
            "invalid-argument",
            "process output currently requires three direct arguments"
        );
        return string_new("");
    }

    process.in_use = 1;
    process.id = g_next_process_id++;
    {
        uint64_t live_count = atomic_fetch_add_explicit(
                                  &g_attrition_process_live_count,
                                  1u,
                                  memory_order_relaxed) + 1u;
        atomic_fetch_add_explicit(&g_attrition_process_spawn_count, 1u, memory_order_relaxed);
        abi_process_attrition_update_peak(&g_attrition_process_peak_count, live_count);
        attrition_live = 1;
    }
    kain_attrition_note_process_spawn((uint64_t)process.id);

#ifdef _WIN32
    if (abi_process_launch_standard_process(&spec, &process) != ABI_PROCESS_OK) {
        output = string_new("");
        goto cleanup;
    }
    if (!abi_process_wait_internal(&process, timeout_ms, &exited)) {
        abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "wait",
            "failed to wait for process output"
        );
        output = string_new("");
        goto cleanup;
    }
    if (!exited) {
        abi_process_fail(
            ABI_PROCESS_TIMEOUT,
            "timeout",
            "process output timed out"
        );
        output = string_new("");
        goto cleanup;
    }
    if (!abi_process_pump_output(
            &process,
            &stdout_chunk,
            &stdout_chunk_length,
            &stderr_chunk,
            &stderr_chunk_length
        )) {
        abi_process_fail(
            ABI_PROCESS_IO_ERROR,
            "read",
            "failed to drain process output"
        );
        output = string_new("");
        goto cleanup;
    }
    free(stdout_chunk);
    free(stderr_chunk);
    stdout_chunk = 0;
    stderr_chunk = 0;
    abi_process_refresh_exit_state(&process);
    if (!process.exited) {
        abi_process_fail(
            ABI_PROCESS_STILL_RUNNING,
            "still-running",
            "process output child did not report an exit code"
        );
        output = string_new("");
        goto cleanup;
    }
    if (process.exit_code != 0) {
        abi_process_fail(
            ABI_PROCESS_SPAWN_FAILED,
            "exit-code",
            "process output child exited with a nonzero status"
        );
        output = string_new("");
        goto cleanup;
    }
    output = abi_process_string_from_bytes(
        process.stdout_capture.bytes,
        process.stdout_capture.length
    );
    abi_process_ok();
#else
    (void)timeout_ms;
    (void)stdout_chunk_length;
    (void)stderr_chunk_length;
    abi_process_fail(
        ABI_PROCESS_UNSUPPORTED_PLATFORM,
        "unsupported-platform",
        "process output is not implemented for this host yet"
    );
    output = string_new("");
#endif

cleanup:
    free(stdout_chunk);
    free(stderr_chunk);
    abi_process_close_os_resources(&process, 0);
    abi_process_capture_free(&process.stdout_capture);
    abi_process_capture_free(&process.stderr_capture);
    abi_process_capture_free(&process.pty_capture);
    if (attrition_live) {
        atomic_fetch_sub_explicit(&g_attrition_process_live_count, 1u, memory_order_relaxed);
        atomic_fetch_add_explicit(&g_attrition_process_exit_count, 1u, memory_order_relaxed);
        kain_attrition_note_process_exit((uint64_t)process.id);
    }
    return output ? output : string_new("");
}

int64_t abi_process_last_status(void) {
    return g_last_status;
}

const char* abi_process_last_error_kind(void) {
    return abi_process_string(g_last_error_kind);
}

const char* abi_process_last_error_message(void) {
    return abi_process_string(g_last_error_message);
}

const KainNativeProcessFunctionTable g_kain_native_process_function_table = {
    abi_process_reset,
    abi_process_platform_available,
    abi_process_spec_create,
    abi_process_spec_destroy,
    abi_process_spec_count,
    abi_process_spec_add_arg,
    abi_process_spec_set_cwd,
    abi_process_spec_set_env,
    abi_process_spec_set_inherit_environment,
    abi_process_spec_set_stdin_mode,
    abi_process_spec_set_stdout_mode,
    abi_process_spec_set_stderr_mode,
    abi_process_spawn,
    abi_process_spawn_pty,
    abi_process_count,
    abi_process_close,
    abi_process_poll,
    abi_process_wait,
    abi_process_is_running,
    abi_process_exit_code,
    abi_process_os_pid,
    abi_process_terminate,
    abi_process_kill,
    abi_process_stdin_write_text,
    abi_process_stdin_write_hex,
    abi_process_stdin_close,
    abi_process_pty_write_text,
    abi_process_pty_write_hex,
    abi_process_pty_resize,
    abi_process_stdout_read_text,
    abi_process_stdout_read_hex,
    abi_process_stderr_read_text,
    abi_process_stderr_read_hex,
    abi_process_stdout_capture_text,
    abi_process_stdout_capture_hex,
    abi_process_stderr_capture_text,
    abi_process_stderr_capture_hex,
    abi_process_pty_read_text,
    abi_process_pty_read_hex,
    abi_process_pty_capture_text,
    abi_process_pty_capture_hex,
    abi_process_output_text,
    abi_process_last_status,
    abi_process_last_error_kind,
    abi_process_last_error_message
};
