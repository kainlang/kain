#!/usr/bin/env python3
"""Generate a Linux-appropriate config.h from the cmake template."""
import re
import sys

template_path = sys.argv[1] if len(sys.argv) > 1 else "include/core/config/config.h.cmake"
output_path = sys.argv[2] if len(sys.argv) > 2 else "include/core/config/config.h"

with open(template_path, "r") as f:
    content = f.read()

# Linux-specific defines
linux_defines = {
    "HAVE_SYSEXITS_H", "HAVE_UNISTD_H", "HAVE_INTTYPES_H",
    "HAVE_SYS_STAT_H", "HAVE_SYS_TYPES_H", "HAVE_SYS_MMAN_H",
    "HAVE_SYS_PARAM_H", "HAVE_SYS_RESOURCE_H", "HAVE_SYS_TIME_H",
    "HAVE_SYS_WAIT_H", "HAVE_TERMIOS_H", "HAVE_SIGNAL_H",
    "HAVE_MALLOC_H", "HAVE_PTHREAD_H", "HAVE_LIBPTHREAD",
    "HAVE_PTHREAD_GETNAME_NP", "HAVE_PTHREAD_SETNAME_NP",
    "HAVE_BACKTRACE", "HAVE_DLOPEN", "HAVE_DEREGISTER_FRAME",
    "HAVE_REGISTER_FRAME", "HAVE___THREAD",
    "LLVM_ON_UNIX", "LLVM_ENABLE_THREADS",
    "LLVM_ENABLE_ZLIB", "HAVE_DLFCN_H",
    "HAVE_POSIX_SPAWN", "HAVE_SYS_PRCTL_H",
}

def replace_cmakedefine(m):
    full = m.group(0)
    var_match = re.search(r'#cmakedefine\s+([A-Za-z0-9_]+)', full)
    if var_match:
        var = var_match.group(1)
        if var in linux_defines:
            return f"#define {var} 1"
        else:
            return f"/* #undef {var} */"
    return full

content = re.sub(
    r'^#cmakedefine\s+[A-Za-z0-9_]+(\s+.*)?$',
    replace_cmakedefine,
    content,
    flags=re.MULTILINE
)

def replace_cmakedefine01(m):
    full = m.group(0)
    var_match = re.search(r'#cmakedefine01\s+([A-Za-z0-9_]+)', full)
    if var_match:
        var = var_match.group(1)
        if var in linux_defines:
            return f"#define {var} 1"
        else:
            return f"#define {var} 0"
    return full

content = re.sub(
    r'^#cmakedefine01\s+[A-Za-z0-9_]+(\s+.*)?$',
    replace_cmakedefine01,
    content,
    flags=re.MULTILINE
)

# Handle LLVM_ON_WIN32 specially (remove it)
content = content.replace('#define LLVM_ON_WIN32', '/* #undef LLVM_ON_WIN32 */')

with open(output_path, "w") as f:
    f.write(content)

print(f"Generated Linux config.h at {output_path}")
