#ifndef ABI_PROCESS_SYSTEM_H
#define ABI_PROCESS_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ABI_PROCESS_MAX_SPECS 64
#define ABI_PROCESS_MAX_PROCESSES 64
#define ABI_PROCESS_MAX_ARGUMENTS 64
#define ABI_PROCESS_MAX_ENVIRONMENT_ENTRIES 64
#define ABI_PROCESS_MAX_KEY 128
#define ABI_PROCESS_MAX_PATH 1024
#define ABI_PROCESS_MAX_VALUE 2048
#define ABI_PROCESS_MAX_ERROR_TEXT 512
#define ABI_PROCESS_MAX_CAPTURE_BYTES 1048576

typedef enum KainNativeProcessStatus {
    ABI_PROCESS_OK = 0,
    ABI_PROCESS_INVALID_SPEC = -1,
    ABI_PROCESS_INVALID_PROCESS = -2,
    ABI_PROCESS_INVALID_ARGUMENT = -3,
    ABI_PROCESS_CAPACITY_EXCEEDED = -4,
    ABI_PROCESS_SPAWN_FAILED = -5,
    ABI_PROCESS_IO_ERROR = -6,
    ABI_PROCESS_TIMEOUT = -7,
    ABI_PROCESS_STILL_RUNNING = -8,
    ABI_PROCESS_UNSUPPORTED_PLATFORM = -9,
    ABI_PROCESS_PTY_UNAVAILABLE = -10,
    ABI_PROCESS_PIPE_NOT_AVAILABLE = -11
} KainNativeProcessStatus;

typedef struct KainNativeProcessFunctionTable {
    int64_t (*reset)(void);
    int64_t (*platform_available)(void);
    int64_t (*spec_create)(const char* executable);
    int64_t (*spec_destroy)(int64_t spec_id);
    int64_t (*spec_count)(void);
    int64_t (*spec_add_arg)(int64_t spec_id, const char* argument);
    int64_t (*spec_set_cwd)(int64_t spec_id, const char* cwd_path);
    int64_t (*spec_set_env)(int64_t spec_id, const char* key, const char* value);
    int64_t (*spec_set_inherit_environment)(int64_t spec_id, int64_t enabled);
    int64_t (*spec_set_stdin_mode)(int64_t spec_id, const char* mode);
    int64_t (*spec_set_stdout_mode)(int64_t spec_id, const char* mode);
    int64_t (*spec_set_stderr_mode)(int64_t spec_id, const char* mode);
    int64_t (*spawn)(int64_t spec_id);
    int64_t (*spawn_pty)(int64_t spec_id, int64_t columns, int64_t rows);
    int64_t (*count)(void);
    int64_t (*close)(int64_t process_id);
    int64_t (*poll)(int64_t process_id);
    int64_t (*wait)(int64_t process_id, int64_t timeout_ms);
    int64_t (*is_running)(int64_t process_id);
    int64_t (*exit_code)(int64_t process_id);
    int64_t (*os_pid)(int64_t process_id);
    int64_t (*terminate)(int64_t process_id);
    int64_t (*kill)(int64_t process_id);
    int64_t (*stdin_write_text)(int64_t process_id, const char* text);
    int64_t (*stdin_write_hex)(int64_t process_id, const char* bytes_hex);
    int64_t (*stdin_close)(int64_t process_id);
    int64_t (*pty_write_text)(int64_t process_id, const char* text);
    int64_t (*pty_write_hex)(int64_t process_id, const char* bytes_hex);
    int64_t (*pty_resize)(int64_t process_id, int64_t columns, int64_t rows);
    const char* (*stdout_read_text)(int64_t process_id);
    const char* (*stdout_read_hex)(int64_t process_id);
    const char* (*stderr_read_text)(int64_t process_id);
    const char* (*stderr_read_hex)(int64_t process_id);
    const char* (*stdout_capture_text)(int64_t process_id);
    const char* (*stdout_capture_hex)(int64_t process_id);
    const char* (*stderr_capture_text)(int64_t process_id);
    const char* (*stderr_capture_hex)(int64_t process_id);
    const char* (*pty_read_text)(int64_t process_id);
    const char* (*pty_read_hex)(int64_t process_id);
    const char* (*pty_capture_text)(int64_t process_id);
    const char* (*pty_capture_hex)(int64_t process_id);
    int64_t (*last_status)(void);
    const char* (*last_error_kind)(void);
    const char* (*last_error_message)(void);
} KainNativeProcessFunctionTable;

extern const KainNativeProcessFunctionTable g_kain_native_process_function_table;

int64_t abi_process_reset(void);
int64_t abi_process_platform_available(void);

int64_t abi_process_spec_create(const char* executable);
int64_t abi_process_spec_destroy(int64_t spec_id);
int64_t abi_process_spec_count(void);
int64_t abi_process_spec_add_arg(int64_t spec_id, const char* argument);
int64_t abi_process_spec_set_cwd(int64_t spec_id, const char* cwd_path);
int64_t abi_process_spec_set_env(int64_t spec_id, const char* key, const char* value);
int64_t abi_process_spec_set_inherit_environment(int64_t spec_id, int64_t enabled);
int64_t abi_process_spec_set_stdin_mode(int64_t spec_id, const char* mode);
int64_t abi_process_spec_set_stdout_mode(int64_t spec_id, const char* mode);
int64_t abi_process_spec_set_stderr_mode(int64_t spec_id, const char* mode);

int64_t abi_process_spawn(int64_t spec_id);
int64_t abi_process_spawn_pty(int64_t spec_id, int64_t columns, int64_t rows);
int64_t abi_process_count(void);
int64_t abi_process_close(int64_t process_id);
int64_t abi_process_poll(int64_t process_id);
int64_t abi_process_wait(int64_t process_id, int64_t timeout_ms);
int64_t abi_process_is_running(int64_t process_id);
int64_t abi_process_exit_code(int64_t process_id);
int64_t abi_process_os_pid(int64_t process_id);
int64_t abi_process_terminate(int64_t process_id);
int64_t abi_process_kill(int64_t process_id);

int64_t abi_process_stdin_write_text(int64_t process_id, const char* text);
int64_t abi_process_stdin_write_hex(int64_t process_id, const char* bytes_hex);
int64_t abi_process_stdin_close(int64_t process_id);
const char* abi_process_stdout_read_text(int64_t process_id);
const char* abi_process_stdout_read_hex(int64_t process_id);
const char* abi_process_stderr_read_text(int64_t process_id);
const char* abi_process_stderr_read_hex(int64_t process_id);
const char* abi_process_stdout_capture_text(int64_t process_id);
const char* abi_process_stdout_capture_hex(int64_t process_id);
const char* abi_process_stderr_capture_text(int64_t process_id);
const char* abi_process_stderr_capture_hex(int64_t process_id);

int64_t abi_process_pty_write_text(int64_t process_id, const char* text);
int64_t abi_process_pty_write_hex(int64_t process_id, const char* bytes_hex);
int64_t abi_process_pty_resize(int64_t process_id, int64_t columns, int64_t rows);
const char* abi_process_pty_read_text(int64_t process_id);
const char* abi_process_pty_read_hex(int64_t process_id);
const char* abi_process_pty_capture_text(int64_t process_id);
const char* abi_process_pty_capture_hex(int64_t process_id);

int64_t abi_process_last_status(void);
const char* abi_process_last_error_kind(void);
const char* abi_process_last_error_message(void);

#ifdef __cplusplus
}
#endif

#endif /* ABI_PROCESS_SYSTEM_H */
