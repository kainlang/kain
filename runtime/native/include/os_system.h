#ifndef OS_SYSTEM_H
#define OS_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t abi_os_setenv(const char* key, const char* value);
int64_t abi_os_unsetenv(const char* key);
int64_t abi_os_chdir(const char* path);
int64_t abi_os_getppid(void);
const char* abi_os_getlogin(void);
int64_t abi_os_getuid(void);
int64_t abi_os_getgid(void);
int64_t abi_os_symlink(const char* src, const char* dst);
const char* abi_os_readlink(const char* path);
const char* abi_os_urandom(int64_t byte_count);
int64_t abi_os_terminal_columns(void);
int64_t abi_os_terminal_rows(void);
int64_t abi_os_last_status(void);
const char* abi_os_last_error_kind(void);
const char* abi_os_last_error_message(void);

#ifdef __cplusplus
}
#endif

#endif /* OS_SYSTEM_H */
