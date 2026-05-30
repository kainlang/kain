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
