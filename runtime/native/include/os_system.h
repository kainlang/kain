#ifndef OS_SYSTEM_H
#define OS_SYSTEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── Legacy OS primitives ───────────────────────────────────────────────

int64_t abi_os_setenv(const char* key, const char* value);
const char* abi_os_getenv(const char* key);
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

// ─── Raw syscall escape hatch (x86_64 Linux primary; arm64 close behind) ───

int64_t abi_os_syscall0(int64_t sysno);
int64_t abi_os_syscall1(int64_t sysno, int64_t arg1);
int64_t abi_os_syscall2(int64_t sysno, int64_t arg1, int64_t arg2);
int64_t abi_os_syscall3(int64_t sysno, int64_t arg1, int64_t arg2, int64_t arg3);
int64_t abi_os_syscall4(int64_t sysno, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4);
int64_t abi_os_syscall5(int64_t sysno, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4, int64_t arg5);
int64_t abi_os_syscall6(int64_t sysno, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4, int64_t arg5, int64_t arg6);

// ─── mmap / munmap / madvise — typed file-backed + anonymous mappings ───

// Protection flags (mirrors PROT_* but platform-fused)
#define KAIN_MMAP_PROT_NONE  0
#define KAIN_MMAP_PROT_READ  1
#define KAIN_MMAP_PROT_WRITE 2
#define KAIN_MMAP_PROT_RW    3
#define KAIN_MMAP_PROT_EXEC  4
#define KAIN_MMAP_PROT_RX    5
#define KAIN_MMAP_PROT_RWX   7

// Map flags — anonymous vs file-backed, shared vs private
#define KAIN_MMAP_SHARED  1
#define KAIN_MMAP_PRIVATE 2
#define KAIN_MMAP_FIXED   16
#define KAIN_MMAP_HUGETLB 64

// madvise hints
#define KAIN_MADV_NORMAL     0
#define KAIN_MADV_RANDOM     1
#define KAIN_MADV_SEQUENTIAL 2
#define KAIN_MADV_WILLNEED   3
#define KAIN_MADV_DONTNEED   4
#define KAIN_MADV_FREE       8
#define KAIN_MADV_HUGEPAGE   14

int64_t abi_os_mmap_anon(int64_t byte_count, int64_t prot, int64_t flags);
int64_t abi_os_mmap_file(int64_t byte_count, int64_t prot, int64_t flags, int64_t fd, int64_t offset);
int64_t abi_os_munmap(int64_t addr, int64_t byte_count);
int64_t abi_os_mprotect(int64_t addr, int64_t byte_count, int64_t prot);
int64_t abi_os_madvise(int64_t addr, int64_t byte_count, int64_t advice);
int64_t abi_os_msync(int64_t addr, int64_t byte_count, int64_t flags);
int64_t abi_os_mlock(int64_t addr, int64_t byte_count);
int64_t abi_os_munlock(int64_t addr, int64_t byte_count);

// ─── Process primitives — fork / execve / waitpid ──────────────────────

int64_t abi_os_fork(void);
int64_t abi_os_execve(const char* path, const char* const* argv, const char* const* envp);
int64_t abi_os_waitpid(int64_t pid, int64_t options);

// waitpid constants
#define KAIN_WNOHANG    1
#define KAIN_WUNTRACED  2
#define KAIN_WCONTINUED 8

// ─── io_uring — kernel-side async I/O submission ──────────────────────

int64_t abi_os_io_uring_setup(int64_t entries);
int64_t abi_os_io_uring_enter(int64_t ring_fd, int64_t to_submit, int64_t min_complete, int64_t flags);

// io_uring constants
#define KAIN_IORING_SETUP_IOPOLL    1
#define KAIN_IORING_SETUP_SQPOLL    2
#define KAIN_IORING_SETUP_SQ_AFF    4
#define KAIN_IORING_ENTER_GETEVENTS 1

#ifdef __cplusplus
}
#endif

#endif /* OS_SYSTEM_H */
