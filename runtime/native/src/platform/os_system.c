#define _CRT_RAND_S

#include "../../include/os_system.h"
#include "../../include/base.h"

#ifdef _WIN32
#include <io.h>
#include <tlhelp32.h>
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif
#else
#include <fcntl.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#endif

#define KAIN_OS_ERROR_KIND_MAX 64
#define KAIN_OS_ERROR_MESSAGE_MAX 512
#define KAIN_OS_PATH_BUFFER_CAP 4096

static int64_t g_kain_os_last_status = 0;
static char g_kain_os_last_error_kind[KAIN_OS_ERROR_KIND_MAX] = "ok";
static char g_kain_os_last_error_message[KAIN_OS_ERROR_MESSAGE_MAX] = "";

static void kain_os_copy_text(char* destination, size_t destination_capacity, const char* source) {
    size_t copy_length;

    if (destination == NULL || destination_capacity == 0u) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    copy_length = strlen(source);
    if (copy_length >= destination_capacity) {
        copy_length = destination_capacity - 1u;
    }

    memcpy(destination, source, copy_length);
    destination[copy_length] = '\0';
}

static void kain_os_set_ok(void) {
    g_kain_os_last_status = 0;
    kain_os_copy_text(g_kain_os_last_error_kind, sizeof(g_kain_os_last_error_kind), "ok");
    g_kain_os_last_error_message[0] = '\0';
}

static void kain_os_set_error(int64_t status, const char* kind, const char* message) {
    g_kain_os_last_status = status;
    kain_os_copy_text(g_kain_os_last_error_kind, sizeof(g_kain_os_last_error_kind), kind ? kind : "error");
    kain_os_copy_text(
        g_kain_os_last_error_message,
        sizeof(g_kain_os_last_error_message),
        message ? message : ""
    );
}

#ifdef _WIN32
static void kain_os_set_win32_error(const char* kind, DWORD code, const char* fallback_message) {
    char message[KAIN_OS_ERROR_MESSAGE_MAX];
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD length = FormatMessageA(
        flags,
        NULL,
        code,
        0,
        message,
        (DWORD)sizeof(message),
        NULL
    );

    if (length == 0u) {
        kain_os_set_error(-(int64_t)code, kind, fallback_message ? fallback_message : "win32 error");
        return;
    }

    while (length > 0u &&
           (message[length - 1u] == '\r' || message[length - 1u] == '\n' || message[length - 1u] == ' ')) {
        message[length - 1u] = '\0';
        length -= 1u;
    }
    kain_os_set_error(-(int64_t)code, kind, message);
}

static void kain_os_normalize_win32_path(char* path) {
    const char* unc_prefix = "\\\\?\\UNC\\";
    const char* device_prefix = "\\\\?\\";
    size_t unc_prefix_length = strlen(unc_prefix);
    size_t device_prefix_length = strlen(device_prefix);
    size_t length;

    if (path == NULL || path[0] == '\0') {
        return;
    }

    if (strncmp(path, unc_prefix, unc_prefix_length) == 0) {
        length = strlen(path + unc_prefix_length);
        memmove(path + 2, path + unc_prefix_length, length + 1u);
        path[0] = '\\';
        path[1] = '\\';
        return;
    }

    if (strncmp(path, device_prefix, device_prefix_length) == 0) {
        length = strlen(path + device_prefix_length);
        memmove(path, path + device_prefix_length, length + 1u);
    }
}

static int64_t kain_os_fill_random_bytes(uint8_t* bytes, size_t byte_count) {
    size_t index = 0u;

    while (index < byte_count) {
        unsigned int chunk = 0u;
        size_t lane = 0u;
        if (rand_s(&chunk) != 0) {
            kain_os_set_error(-1, "urandom", "rand_s failed");
            return -1;
        }
        while (lane < sizeof(chunk) && index < byte_count) {
            bytes[index] = (uint8_t)((chunk >> (lane * 8u)) & 0xffu);
            index += 1u;
            lane += 1u;
        }
    }

    return 0;
}
#else
static void kain_os_set_errno_error(const char* kind, int code, const char* fallback_message) {
    const char* text = strerror(code);
    kain_os_set_error(-(int64_t)code, kind, text ? text : fallback_message);
}

static int64_t kain_os_fill_random_bytes(uint8_t* bytes, size_t byte_count) {
    size_t offset = 0u;
    int fd;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        kain_os_set_errno_error("urandom", errno, "open(/dev/urandom) failed");
        return -1;
    }

    while (offset < byte_count) {
        ssize_t read_count = read(fd, bytes + offset, byte_count - offset);
        if (read_count < 0) {
            int code = errno;
            close(fd);
            kain_os_set_errno_error("urandom", code, "read(/dev/urandom) failed");
            return -1;
        }
        if (read_count == 0) {
            close(fd);
            kain_os_set_error(-1, "urandom", "read(/dev/urandom) returned EOF");
            return -1;
        }
        offset += (size_t)read_count;
    }

    close(fd);
    return 0;
}
#endif

static char* kain_os_hex_encode(const uint8_t* bytes, size_t byte_count) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    char* encoded;
    char* owned;
    size_t index;

    encoded = (char*)malloc((byte_count * 2u) + 1u);
    if (encoded == NULL) {
        kain_os_set_error(-1, "alloc", "hex buffer allocation failed");
        return string_new("");
    }

    for (index = 0u; index < byte_count; ++index) {
        encoded[index * 2u] = HEX_DIGITS[(bytes[index] >> 4) & 0x0fu];
        encoded[(index * 2u) + 1u] = HEX_DIGITS[bytes[index] & 0x0fu];
    }
    encoded[byte_count * 2u] = '\0';

    owned = string_new(encoded);
    free(encoded);
    return owned ? owned : string_new("");
}

int64_t abi_os_setenv(const char* key, const char* value) {
    if (key == NULL || key[0] == '\0') {
        kain_os_set_error(-1, "invalid_argument", "environment key cannot be empty");
        return -1;
    }

#ifdef _WIN32
    if (!SetEnvironmentVariableA(key, value ? value : "")) {
        kain_os_set_win32_error("setenv", GetLastError(), "SetEnvironmentVariableA failed");
        return -1;
    }
#else
    if (setenv(key, value ? value : "", 1) != 0) {
        kain_os_set_errno_error("setenv", errno, "setenv failed");
        return -1;
    }
#endif

    kain_os_set_ok();
    return 0;
}

int64_t abi_os_unsetenv(const char* key) {
    if (key == NULL || key[0] == '\0') {
        kain_os_set_error(-1, "invalid_argument", "environment key cannot be empty");
        return -1;
    }

#ifdef _WIN32
    if (!SetEnvironmentVariableA(key, NULL)) {
        kain_os_set_win32_error("unsetenv", GetLastError(), "SetEnvironmentVariableA failed");
        return -1;
    }
#else
    if (unsetenv(key) != 0) {
        kain_os_set_errno_error("unsetenv", errno, "unsetenv failed");
        return -1;
    }
#endif

    kain_os_set_ok();
    return 0;
}

int64_t abi_os_chdir(const char* path) {
    if (path == NULL || path[0] == '\0') {
        kain_os_set_error(-1, "invalid_argument", "working directory path cannot be empty");
        return -1;
    }

#ifdef _WIN32
    if (!SetCurrentDirectoryA(path)) {
        kain_os_set_win32_error("chdir", GetLastError(), "SetCurrentDirectoryA failed");
        return -1;
    }
#else
    if (chdir(path) != 0) {
        kain_os_set_errno_error("chdir", errno, "chdir failed");
        return -1;
    }
#endif

    kain_os_set_ok();
    return 0;
}

int64_t abi_os_getppid(void) {
#ifdef _WIN32
    HANDLE snapshot;
    PROCESSENTRY32 entry;
    DWORD current_pid = GetCurrentProcessId();

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        kain_os_set_win32_error("getppid", GetLastError(), "CreateToolhelp32Snapshot failed");
        return -1;
    }

    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (!Process32First(snapshot, &entry)) {
        DWORD code = GetLastError();
        CloseHandle(snapshot);
        kain_os_set_win32_error("getppid", code, "Process32First failed");
        return -1;
    }

    do {
        if (entry.th32ProcessID == current_pid) {
            CloseHandle(snapshot);
            kain_os_set_ok();
            return (int64_t)entry.th32ParentProcessID;
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);
    kain_os_set_error(-1, "getppid", "parent process was not found");
    return -1;
#else
    kain_os_set_ok();
    return (int64_t)getppid();
#endif
}

const char* abi_os_getlogin(void) {
#ifdef _WIN32
    char buffer[256];
    DWORD size = (DWORD)sizeof(buffer);

    if (!GetUserNameA(buffer, &size)) {
        kain_os_set_win32_error("getlogin", GetLastError(), "GetUserNameA failed");
        return string_new("");
    }

    kain_os_set_ok();
    return string_new(buffer);
#else
    char buffer[256];
    const char* fallback = NULL;

    if (getlogin_r(buffer, sizeof(buffer)) == 0 && buffer[0] != '\0') {
        kain_os_set_ok();
        return string_new(buffer);
    }

    fallback = getenv("LOGNAME");
    if (fallback == NULL || fallback[0] == '\0') {
        fallback = getenv("USER");
    }
    if (fallback != NULL && fallback[0] != '\0') {
        kain_os_set_ok();
        return string_new((char*)fallback);
    }

    kain_os_set_errno_error("getlogin", errno, "getlogin_r failed");
    return string_new("");
#endif
}

int64_t abi_os_getuid(void) {
#ifdef _WIN32
    kain_os_set_error(-1, "unsupported", "uid semantics are not available on win32");
    return -1;
#else
    kain_os_set_ok();
    return (int64_t)getuid();
#endif
}

int64_t abi_os_getgid(void) {
#ifdef _WIN32
    kain_os_set_error(-1, "unsupported", "gid semantics are not available on win32");
    return -1;
#else
    kain_os_set_ok();
    return (int64_t)getgid();
#endif
}

int64_t abi_os_symlink(const char* src, const char* dst) {
    if (src == NULL || src[0] == '\0' || dst == NULL || dst[0] == '\0') {
        kain_os_set_error(-1, "invalid_argument", "symlink source and destination cannot be empty");
        return -1;
    }

#ifdef _WIN32
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    DWORD attributes = GetFileAttributesA(src);

    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
        flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
    }
    if (!CreateSymbolicLinkA(dst, src, flags)) {
        kain_os_set_win32_error("symlink", GetLastError(), "CreateSymbolicLinkA failed");
        return -1;
    }
#else
    if (symlink(src, dst) != 0) {
        kain_os_set_errno_error("symlink", errno, "symlink failed");
        return -1;
    }
#endif

    kain_os_set_ok();
    return 0;
}

const char* abi_os_readlink(const char* path) {
    if (path == NULL || path[0] == '\0') {
        kain_os_set_error(-1, "invalid_argument", "readlink path cannot be empty");
        return string_new("");
    }

#ifdef _WIN32
    HANDLE handle;
    char buffer[KAIN_OS_PATH_BUFFER_CAP];
    DWORD length;

    handle = CreateFileA(
        path,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );
    if (handle == INVALID_HANDLE_VALUE) {
        kain_os_set_win32_error("readlink", GetLastError(), "CreateFileA failed");
        return string_new("");
    }

    length = GetFinalPathNameByHandleA(handle, buffer, (DWORD)sizeof(buffer), FILE_NAME_NORMALIZED);
    if (length == 0u || length >= (DWORD)sizeof(buffer)) {
        DWORD code = GetLastError();
        CloseHandle(handle);
        kain_os_set_win32_error("readlink", code, "GetFinalPathNameByHandleA failed");
        return string_new("");
    }
    CloseHandle(handle);

    kain_os_normalize_win32_path(buffer);
    kain_os_set_ok();
    return string_new(buffer);
#else
    char buffer[KAIN_OS_PATH_BUFFER_CAP];
    ssize_t length = readlink(path, buffer, sizeof(buffer) - 1u);
    if (length < 0) {
        kain_os_set_errno_error("readlink", errno, "readlink failed");
        return string_new("");
    }

    buffer[length] = '\0';
    kain_os_set_ok();
    return string_new(buffer);
#endif
}

const char* abi_os_urandom(int64_t byte_count) {
    uint8_t* bytes;
    char* encoded;

    if (byte_count < 0) {
        kain_os_set_error(-1, "invalid_argument", "random byte count cannot be negative");
        return string_new("");
    }
    if (byte_count == 0) {
        kain_os_set_ok();
        return string_new("");
    }

    bytes = (uint8_t*)malloc((size_t)byte_count);
    if (bytes == NULL) {
        kain_os_set_error(-1, "alloc", "random byte buffer allocation failed");
        return string_new("");
    }

    if (kain_os_fill_random_bytes(bytes, (size_t)byte_count) != 0) {
        free(bytes);
        return string_new("");
    }

    encoded = kain_os_hex_encode(bytes, (size_t)byte_count);
    free(bytes);
    if (encoded == NULL) {
        return string_new("");
    }

    kain_os_set_ok();
    return encoded;
}

int64_t abi_os_terminal_columns(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (handle == INVALID_HANDLE_VALUE || handle == NULL || !GetConsoleScreenBufferInfo(handle, &info)) {
        kain_os_set_win32_error("terminal", GetLastError(), "GetConsoleScreenBufferInfo failed");
        return -1;
    }

    kain_os_set_ok();
    return (int64_t)(info.srWindow.Right - info.srWindow.Left + 1);
#else
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_col == 0u) {
        kain_os_set_errno_error("terminal", errno, "TIOCGWINSZ failed");
        return -1;
    }

    kain_os_set_ok();
    return (int64_t)size.ws_col;
#endif
}

int64_t abi_os_terminal_rows(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

    if (handle == INVALID_HANDLE_VALUE || handle == NULL || !GetConsoleScreenBufferInfo(handle, &info)) {
        kain_os_set_win32_error("terminal", GetLastError(), "GetConsoleScreenBufferInfo failed");
        return -1;
    }

    kain_os_set_ok();
    return (int64_t)(info.srWindow.Bottom - info.srWindow.Top + 1);
#else
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 || size.ws_row == 0u) {
        kain_os_set_errno_error("terminal", errno, "TIOCGWINSZ failed");
        return -1;
    }

    kain_os_set_ok();
    return (int64_t)size.ws_row;
#endif
}

int64_t abi_os_last_status(void) {
    return g_kain_os_last_status;
}

const char* abi_os_last_error_kind(void) {
    return string_new(g_kain_os_last_error_kind);
}

const char* abi_os_last_error_message(void) {
    return string_new(g_kain_os_last_error_message);
}

// ============================================================================
//  RAW SYSCALL ESCAPE HATCH (x86_64 + aarch64 + Win32 stubs)
// ============================================================================
//  This is where Kain punches through libc and talks to the kernel directly.
//  Zig-style: arch-compiled, zero overhead, 3 instructions for a syscall0.

#if defined(__x86_64__) && !defined(_WIN32)
    #define KAIN_RAW_SYSCALL_IMPL(n) \
        int64_t abi_os_syscall##n(int64_t sysno KAIN_RAW_ARG_LIST_##n) \
        { \
            int64_t result; \
            register int64_t r10 __asm__("r10") = KAIN_RAW_ARG_##n(4); \
            register int64_t r8  __asm__("r8")  = KAIN_RAW_ARG_##n(5); \
            register int64_t r9  __asm__("r9")  = KAIN_RAW_ARG_##n(6); \
            KAIN_RAW_REG_CLOBBER_##n \
            __asm__ volatile( \
                "syscall" \
                : "=a"(result) \
                : "a"(sysno), "D"(KAIN_RAW_ARG_##n(1)), \
                  "S"(KAIN_RAW_ARG_##n(2)), "d"(KAIN_RAW_ARG_##n(3)), \
                  "r"(r10), "r"(r8), "r"(r9) \
                : "rcx", "r11", "memory" \
            ); \
            return result; \
        }

    #define KAIN_RAW_ARG_LIST_0
    #define KAIN_RAW_ARG_0(n) 0
    #define KAIN_RAW_REG_CLOBBER_0

    #define KAIN_RAW_ARG_LIST_1 , int64_t arg1
    #define KAIN_RAW_ARG_1(n) (n == 1 ? arg1 : 0)
    #define KAIN_RAW_REG_CLOBBER_1 (void)arg1;

    #define KAIN_RAW_ARG_LIST_2 , int64_t arg1, int64_t arg2
    #define KAIN_RAW_ARG_2(n) (n == 1 ? arg1 : (n == 2 ? arg2 : 0))
    #define KAIN_RAW_REG_CLOBBER_2 (void)arg1; (void)arg2;

    #define KAIN_RAW_ARG_LIST_3 , int64_t arg1, int64_t arg2, int64_t arg3
    #define KAIN_RAW_ARG_3(n) (n == 1 ? arg1 : (n == 2 ? arg2 : (n == 3 ? arg3 : 0)))
    #define KAIN_RAW_REG_CLOBBER_3 (void)arg1; (void)arg2; (void)arg3;

    #define KAIN_RAW_ARG_LIST_4 , int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4
    #define KAIN_RAW_ARG_4(n) \
        (n == 1 ? arg1 : (n == 2 ? arg2 : (n == 3 ? arg3 : (n == 4 ? arg4 : 0))))
    #define KAIN_RAW_REG_CLOBBER_4 (void)arg1; (void)arg2; (void)arg3; (void)arg4;

    #define KAIN_RAW_ARG_LIST_5 , int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4, int64_t arg5
    #define KAIN_RAW_ARG_5(n) \
        (n == 1 ? arg1 : (n == 2 ? arg2 : (n == 3 ? arg3 : (n == 4 ? arg4 : (n == 5 ? arg5 : 0)))))
    #define KAIN_RAW_REG_CLOBBER_5 \
        (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    #define KAIN_RAW_ARG_LIST_6 , int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4, int64_t arg5, int64_t arg6
    #define KAIN_RAW_ARG_6(n) \
        (n == 1 ? arg1 : (n == 2 ? arg2 : (n == 3 ? arg3 : (n == 4 ? arg4 : (n == 5 ? arg5 : (n == 6 ? arg6 : 0))))))
    #define KAIN_RAW_REG_CLOBBER_6 \
        (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; (void)arg6;

    KAIN_RAW_SYSCALL_IMPL(0)
    KAIN_RAW_SYSCALL_IMPL(1)
    KAIN_RAW_SYSCALL_IMPL(2)
    KAIN_RAW_SYSCALL_IMPL(3)
    KAIN_RAW_SYSCALL_IMPL(4)
    KAIN_RAW_SYSCALL_IMPL(5)
    KAIN_RAW_SYSCALL_IMPL(6)

    #undef KAIN_RAW_SYSCALL_IMPL
    #undef KAIN_RAW_ARG_LIST_0
    #undef KAIN_RAW_ARG_0
    #undef KAIN_RAW_REG_CLOBBER_0
    #undef KAIN_RAW_ARG_LIST_1
    #undef KAIN_RAW_ARG_1
    #undef KAIN_RAW_REG_CLOBBER_1
    #undef KAIN_RAW_ARG_LIST_2
    #undef KAIN_RAW_ARG_2
    #undef KAIN_RAW_REG_CLOBBER_2
    #undef KAIN_RAW_ARG_LIST_3
    #undef KAIN_RAW_ARG_3
    #undef KAIN_RAW_REG_CLOBBER_3
    #undef KAIN_RAW_ARG_LIST_4
    #undef KAIN_RAW_ARG_4
    #undef KAIN_RAW_REG_CLOBBER_4
    #undef KAIN_RAW_ARG_LIST_5
    #undef KAIN_RAW_ARG_5
    #undef KAIN_RAW_REG_CLOBBER_5
    #undef KAIN_RAW_ARG_LIST_6
    #undef KAIN_RAW_ARG_6
    #undef KAIN_RAW_REG_CLOBBER_6

#elif defined(__aarch64__) && !defined(_WIN32)
    // AArch64 syscall convention: x8 = sysno, x0-x5 = args 1-6, x0 = return
    // For syscallN: x0 = arg1 (returns through same register), x1..x5 = arg2..arg6

    // syscall0: no args, x0 dummy = 0
    int64_t abi_os_syscall0(int64_t sysno) {
        register int64_t x8 __asm__("x8") = sysno;
        register int64_t x0 __asm__("x0") = 0;
        __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory");
        return x0;
    }

    #define KAIN_AARCH64_SYSCALL(n, ...) \
        int64_t abi_os_syscall##n(int64_t sysno __VA_OPT__(,) __VA_ARGS__) { \
            register int64_t x8 __asm__("x8") = sysno; \
            KAIN_AARCH64_REGS_##n(__VA_ARGS__) \
            __asm__ volatile( \
                "svc #0" \
                : "+r"(x0) \
                : "r"(x8) KAIN_AARCH64_INPUTS_##n \
                : "memory" \
            ); \
            return x0; \
        }

    #define KAIN_AARCH64_REGS_1(a1) \
        register int64_t x0 __asm__("x0") = a1; (void)a1;
    #define KAIN_AARCH64_INPUTS_1

    #define KAIN_AARCH64_REGS_2(a1, a2) \
        register int64_t x0 __asm__("x0") = a1; \
        register int64_t x1 __asm__("x1") = a2; \
        (void)a1; (void)a2;
    #define KAIN_AARCH64_INPUTS_2 , "r"(x1)

    #define KAIN_AARCH64_REGS_3(a1, a2, a3) \
        register int64_t x0 __asm__("x0") = a1; \
        register int64_t x1 __asm__("x1") = a2; \
        register int64_t x2 __asm__("x2") = a3; \
        (void)a1; (void)a2; (void)a3;
    #define KAIN_AARCH64_INPUTS_3 , "r"(x1), "r"(x2)

    #define KAIN_AARCH64_REGS_4(a1, a2, a3, a4) \
        register int64_t x0 __asm__("x0") = a1; \
        register int64_t x1 __asm__("x1") = a2; \
        register int64_t x2 __asm__("x2") = a3; \
        register int64_t x3 __asm__("x3") = a4; \
        (void)a1; (void)a2; (void)a3; (void)a4;
    #define KAIN_AARCH64_INPUTS_4 , "r"(x1), "r"(x2), "r"(x3)

    #define KAIN_AARCH64_REGS_5(a1, a2, a3, a4, a5) \
        register int64_t x0 __asm__("x0") = a1; \
        register int64_t x1 __asm__("x1") = a2; \
        register int64_t x2 __asm__("x2") = a3; \
        register int64_t x3 __asm__("x3") = a4; \
        register int64_t x4 __asm__("x4") = a5; \
        (void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
    #define KAIN_AARCH64_INPUTS_5 , "r"(x1), "r"(x2), "r"(x3), "r"(x4)

    #define KAIN_AARCH64_REGS_6(a1, a2, a3, a4, a5, a6) \
        register int64_t x0 __asm__("x0") = a1; \
        register int64_t x1 __asm__("x1") = a2; \
        register int64_t x2 __asm__("x2") = a3; \
        register int64_t x3 __asm__("x3") = a4; \
        register int64_t x4 __asm__("x4") = a5; \
        register int64_t x5 __asm__("x5") = a6; \
        (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    #define KAIN_AARCH64_INPUTS_6 , "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)

    KAIN_AARCH64_SYSCALL(1, int64_t arg1)
    KAIN_AARCH64_SYSCALL(2, int64_t arg1, int64_t arg2)
    KAIN_AARCH64_SYSCALL(3, int64_t arg1, int64_t arg2, int64_t arg3)
    KAIN_AARCH64_SYSCALL(4, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4)
    KAIN_AARCH64_SYSCALL(5, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4, int64_t arg5)
    KAIN_AARCH64_SYSCALL(6, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4, int64_t arg5, int64_t arg6)

#else
    // Windows / unknown arch — stub via errno path (or Nt* for real users)
    int64_t abi_os_syscall0(int64_t sysno) { (void)sysno; kain_os_set_error(-1, "unsupported", "raw syscall unavailable on this platform"); return -1; }
    int64_t abi_os_syscall1(int64_t sysno, int64_t a1) { (void)sysno; (void)a1; kain_os_set_error(-1, "unsupported", "raw syscall unavailable on this platform"); return -1; }
    int64_t abi_os_syscall2(int64_t sysno, int64_t a1, int64_t a2) { (void)sysno; (void)a1; (void)a2; kain_os_set_error(-1, "unsupported", "raw syscall unavailable on this platform"); return -1; }
    int64_t abi_os_syscall3(int64_t sysno, int64_t a1, int64_t a2, int64_t a3) { (void)sysno; (void)a1; (void)a2; (void)a3; kain_os_set_error(-1, "unsupported", "raw syscall unavailable on this platform"); return -1; }
    int64_t abi_os_syscall4(int64_t sysno, int64_t a1, int64_t a2, int64_t a3, int64_t a4) { (void)sysno; (void)a1; (void)a2; (void)a3; (void)a4; kain_os_set_error(-1, "unsupported", "raw syscall unavailable on this platform"); return -1; }
    int64_t abi_os_syscall5(int64_t sysno, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5) { (void)sysno; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; kain_os_set_error(-1, "unsupported", "raw syscall unavailable on this platform"); return -1; }
    int64_t abi_os_syscall6(int64_t sysno, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6) { (void)sysno; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; kain_os_set_error(-1, "unsupported", "raw syscall unavailable on this platform"); return -1; }
#endif

// ============================================================================
//  MMAP / MUNMAP / MADVISE — Typed memory mappings
// ============================================================================

static int kain_os_mmap_native_prot(int64_t prot_flags) {
    int native = 0;
#if !defined(_WIN32)
    if (prot_flags & KAIN_MMAP_PROT_READ)  native |= PROT_READ;
    if (prot_flags & KAIN_MMAP_PROT_WRITE) native |= PROT_WRITE;
    if (prot_flags & KAIN_MMAP_PROT_EXEC)  native |= PROT_EXEC;
#else
    if (prot_flags & KAIN_MMAP_PROT_READ)  native |= 0x02;
    if (prot_flags & KAIN_MMAP_PROT_WRITE) native |= 0x04;
    if (prot_flags & KAIN_MMAP_PROT_EXEC)  native |= 0x10;
#endif
    return native;
}

int64_t abi_os_mmap_anon(int64_t byte_count, int64_t prot, int64_t flags) {
    if (byte_count <= 0) {
        kain_os_set_error(-1, "invalid_argument", "mmap byte_count must be positive");
        return -1;
    }
#if !defined(_WIN32)
    {
        int native_prot  = kain_os_mmap_native_prot(prot);
        int native_flags = MAP_ANONYMOUS | MAP_PRIVATE;
        if (flags & KAIN_MMAP_SHARED) native_flags = MAP_ANONYMOUS | MAP_SHARED;
        if (flags & KAIN_MMAP_FIXED)  native_flags |= MAP_FIXED;
        if (flags & KAIN_MMAP_HUGETLB) native_flags |= MAP_HUGETLB;
        void* addr = mmap(NULL, (size_t)byte_count, native_prot, native_flags, -1, 0);
        if (addr == MAP_FAILED) {
            kain_os_set_errno_error("mmap_anon", errno, "mmap failed");
            return -1;
        }
        kain_os_set_ok();
        return (int64_t)(intptr_t)addr;
    }
#else
    {
        DWORD win_prot = PAGE_READWRITE;
        if (prot == KAIN_MMAP_PROT_READ)  win_prot = PAGE_READONLY;
        if (prot & KAIN_MMAP_PROT_EXEC)   win_prot = PAGE_EXECUTE_READWRITE;
        void* addr = VirtualAlloc(NULL, (SIZE_T)byte_count, MEM_RESERVE | MEM_COMMIT, win_prot);
        if (!addr) {
            kain_os_set_win32_error("mmap_anon", GetLastError(), "VirtualAlloc failed");
            return -1;
        }
        kain_os_set_ok();
        return (int64_t)(intptr_t)addr;
    }
#endif
}

int64_t abi_os_mmap_file(int64_t byte_count, int64_t prot, int64_t flags, int64_t fd, int64_t offset) {
    if (byte_count <= 0 || fd < 0) {
        kain_os_set_error(-1, "invalid_argument", "mmap_file requires positive byte_count and valid fd");
        return -1;
    }
#if !defined(_WIN32)
    {
        int native_prot  = kain_os_mmap_native_prot(prot);
        int native_flags = (flags & KAIN_MMAP_SHARED) ? MAP_SHARED : MAP_PRIVATE;
        if (flags & KAIN_MMAP_FIXED) native_flags |= MAP_FIXED;
        void* addr = mmap(NULL, (size_t)byte_count, native_prot, native_flags, (int)fd, (off_t)offset);
        if (addr == MAP_FAILED) {
            kain_os_set_errno_error("mmap_file", errno, "mmap failed");
            return -1;
        }
        kain_os_set_ok();
        return (int64_t)(intptr_t)addr;
    }
#else
    {
        HANDLE mapping = CreateFileMappingA((HANDLE)(intptr_t)fd, NULL,
            (prot & KAIN_MMAP_PROT_WRITE) ? PAGE_READWRITE : PAGE_READONLY,
            0, 0, NULL);
        if (!mapping) {
            kain_os_set_win32_error("mmap_file", GetLastError(), "CreateFileMappingA failed");
            return -1;
        }
        void* addr = MapViewOfFile(mapping, FILE_MAP_READ, 0, (DWORD)offset, (SIZE_T)byte_count);
        CloseHandle(mapping);
        if (!addr) {
            kain_os_set_win32_error("mmap_file", GetLastError(), "MapViewOfFile failed");
            return -1;
        }
        kain_os_set_ok();
        return (int64_t)(intptr_t)addr;
    }
#endif
}

int64_t abi_os_munmap(int64_t addr, int64_t byte_count) {
    if (addr <= 0 || byte_count <= 0) {
        kain_os_set_error(-1, "invalid_argument", "munmap requires valid address and positive byte_count");
        return -1;
    }
#if !defined(_WIN32)
    if (munmap((void*)(intptr_t)addr, (size_t)byte_count) != 0) {
        kain_os_set_errno_error("munmap", errno, "munmap failed");
        return -1;
    }
    kain_os_set_ok();
    return 0;
#else
    if (!VirtualFree((void*)(intptr_t)addr, 0, MEM_RELEASE)) {
        kain_os_set_win32_error("munmap", GetLastError(), "VirtualFree(MEM_RELEASE) failed");
        return -1;
    }
    kain_os_set_ok();
    return 0;
#endif
}

int64_t abi_os_mprotect(int64_t addr, int64_t byte_count, int64_t prot) {
    if (addr <= 0 || byte_count <= 0) {
        kain_os_set_error(-1, "invalid_argument", "mprotect requires valid address and positive byte_count");
        return -1;
    }
#if !defined(_WIN32)
    if (mprotect((void*)(intptr_t)addr, (size_t)byte_count, kain_os_mmap_native_prot(prot)) != 0) {
        kain_os_set_errno_error("mprotect", errno, "mprotect failed");
        return -1;
    }
    kain_os_set_ok();
    return 0;
#else
    {
        DWORD old;
        DWORD win_prot = PAGE_READWRITE;
        if (prot == KAIN_MMAP_PROT_READ)  win_prot = PAGE_READONLY;
        if (prot & KAIN_MMAP_PROT_EXEC)   win_prot = PAGE_EXECUTE_READWRITE;
        if (!VirtualProtect((void*)(intptr_t)addr, (SIZE_T)byte_count, win_prot, &old)) {
            kain_os_set_win32_error("mprotect", GetLastError(), "VirtualProtect failed");
            return -1;
        }
        kain_os_set_ok();
        return 0;
    }
#endif
}

int64_t abi_os_madvise(int64_t addr, int64_t byte_count, int64_t advice) {
    if (addr <= 0 || byte_count <= 0) {
        kain_os_set_error(-1, "invalid_argument", "madvise requires valid address and positive byte_count");
        return -1;
    }
#if !defined(_WIN32)
    {
        static const int NATIVE_ADVICE[] = {
            MADV_NORMAL, MADV_RANDOM, MADV_SEQUENTIAL,
            MADV_WILLNEED, MADV_DONTNEED, 0, 0, 0,
            MADV_FREE, 0, 0, 0, 0, 0, MADV_HUGEPAGE
        };
        int native = (advice >= 0 && (size_t)advice < sizeof(NATIVE_ADVICE)/sizeof(NATIVE_ADVICE[0]))
            ? NATIVE_ADVICE[advice] : MADV_NORMAL;
        if (native == 0 && advice != KAIN_MADV_NORMAL) {
            kain_os_set_ok();
            return 0; // silent no-op for unsupported advice
        }
        if (madvise((void*)(intptr_t)addr, (size_t)byte_count, native) != 0) {
            kain_os_set_errno_error("madvise", errno, "madvise failed");
            return -1;
        }
        kain_os_set_ok();
        return 0;
    }
#else
    (void)addr; (void)byte_count; (void)advice;
    kain_os_set_ok();
    return 0;
#endif
}

int64_t abi_os_msync(int64_t addr, int64_t byte_count, int64_t flags) {
    if (addr <= 0 || byte_count <= 0) {
        kain_os_set_error(-1, "invalid_argument", "msync requires valid address and positive byte_count");
        return -1;
    }
#if !defined(_WIN32)
    if (msync((void*)(intptr_t)addr, (size_t)byte_count, (int)flags) != 0) {
        kain_os_set_errno_error("msync", errno, "msync failed");
        return -1;
    }
    kain_os_set_ok();
    return 0;
#else
    if (!FlushViewOfFile((void*)(intptr_t)addr, (SIZE_T)byte_count)) {
        kain_os_set_win32_error("msync", GetLastError(), "FlushViewOfFile failed");
        return -1;
    }
    kain_os_set_ok();
    return 0;
#endif
}

int64_t abi_os_mlock(int64_t addr, int64_t byte_count) {
    if (addr <= 0 || byte_count <= 0) {
        kain_os_set_error(-1, "invalid_argument", "mlock requires valid address and positive byte_count");
        return -1;
    }
#if !defined(_WIN32)
    if (mlock((void*)(intptr_t)addr, (size_t)byte_count) != 0) {
        kain_os_set_errno_error("mlock", errno, "mlock failed");
        return -1;
    }
    kain_os_set_ok();
    return 0;
#else
    if (!VirtualLock((void*)(intptr_t)addr, (SIZE_T)byte_count)) {
        kain_os_set_win32_error("mlock", GetLastError(), "VirtualLock failed");
        return -1;
    }
    kain_os_set_ok();
    return 0;
#endif
}

int64_t abi_os_munlock(int64_t addr, int64_t byte_count) {
    if (addr <= 0 || byte_count <= 0) {
        kain_os_set_error(-1, "invalid_argument", "munlock requires valid address and positive byte_count");
        return -1;
    }
#if !defined(_WIN32)
    if (munlock((void*)(intptr_t)addr, (size_t)byte_count) != 0) {
        kain_os_set_errno_error("munlock", errno, "munlock failed");
        return -1;
    }
    kain_os_set_ok();
    return 0;
#else
    if (!VirtualUnlock((void*)(intptr_t)addr, (SIZE_T)byte_count)) {
        kain_os_set_win32_error("munlock", GetLastError(), "VirtualUnlock failed");
        return -1;
    }
    kain_os_set_ok();
    return 0;
#endif
}

// ============================================================================
//  PROCESS PRIMITIVES — fork / execve / waitpid
// ============================================================================

int64_t abi_os_fork(void) {
#if !defined(_WIN32)
    pid_t pid = fork();
    if (pid < 0) {
        kain_os_set_errno_error("fork", errno, "fork failed");
        return -1;
    }
    kain_os_set_ok();
    return (int64_t)pid;
#else
    kain_os_set_error(-1, "unsupported", "fork is not available on Windows");
    return -1;
#endif
}

int64_t abi_os_execve(const char* path, const char* const* argv, const char* const* envp) {
    if (path == NULL || path[0] == '\0') {
        kain_os_set_error(-1, "invalid_argument", "execve path cannot be empty");
        return -1;
    }
#if !defined(_WIN32)
    execve(path, (char* const*)argv, (char* const*)envp);
    // execve only returns on error
    kain_os_set_errno_error("execve", errno, "execve failed");
    return -1;
#else
    (void)argv; (void)envp;
    // Use CreateProcess via abi_os_execve_windows helper — execve doesn't
    // exist on Windows; the caller should use process_system spawn instead.
    kain_os_set_error(-1, "unsupported", "execve semantics differ on Windows; use spawn");
    return -1;
#endif
}

int64_t abi_os_waitpid(int64_t pid, int64_t options) {
#if !defined(_WIN32)
    int status = 0;
    int native_options = 0;
    if (options & KAIN_WNOHANG)    native_options |= WNOHANG;
    if (options & KAIN_WUNTRACED)  native_options |= WUNTRACED;
    if (options & KAIN_WCONTINUED) native_options |= WCONTINUED;
    pid_t result = waitpid((pid_t)pid, &status, native_options);
    if (result < 0) {
        kain_os_set_errno_error("waitpid", errno, "waitpid failed");
        return -1;
    }
    // Encode exit status in upper 32 bits for Kain to decode:
    //    bits 0-31: raw status
    //    bits 32-63: PID that changed state
    kain_os_set_ok();
    return ((int64_t)(uint32_t)status) | ((int64_t)result << 32);
#else
    (void)pid; (void)options;
    kain_os_set_error(-1, "unsupported", "waitpid is not available on Windows");
    return -1;
#endif
}

// ============================================================================
//  IO_URING — Kernel-side async I/O submission
// ============================================================================

int64_t abi_os_io_uring_setup(int64_t entries) {
    if (entries <= 0 || entries > 32768) {
        kain_os_set_error(-1, "invalid_argument", "io_uring entries must be 1..32768");
        return -1;
    }
#if defined(__linux__)
    {
        // SYS_io_uring_setup = 425 on x86_64, 425 on aarch64 (>= 5.1)
        long result = (long)abi_os_syscall2(425, entries, 0);
        if (result < 0) {
            kain_os_set_error(result, "io_uring", "io_uring_setup syscall failed");
            return result;
        }
        kain_os_set_ok();
        return (int64_t)result;
    }
#else
    (void)entries;
    kain_os_set_error(-1, "unsupported", "io_uring requires Linux >= 5.1");
    return -1;
#endif
}

int64_t abi_os_io_uring_enter(int64_t ring_fd, int64_t to_submit, int64_t min_complete, int64_t flags) {
    if (ring_fd < 0) {
        kain_os_set_error(-1, "invalid_argument", "io_uring_enter requires valid ring_fd");
        return -1;
    }
#if defined(__linux__)
    {
        // SYS_io_uring_enter = 426 on x86_64 and aarch64
        long result = (long)abi_os_syscall4(426, ring_fd, to_submit, min_complete, flags);
        if (result < 0) {
            kain_os_set_error(result, "io_uring", "io_uring_enter syscall failed");
            return result;
        }
        kain_os_set_ok();
        return (int64_t)result;
    }
#else
    (void)ring_fd; (void)to_submit; (void)min_complete; (void)flags;
    kain_os_set_error(-1, "unsupported", "io_uring requires Linux >= 5.1");
    return -1;
#endif
}
