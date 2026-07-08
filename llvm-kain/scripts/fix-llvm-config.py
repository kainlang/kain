#!/usr/bin/env python3
"""Generate proper llvm-config.h from the cmake template.

Fixes over the original:
1. Process #cmakedefine01 BEFORE #cmakedefine (avoid regex prefix collision)
2. Substitute ${VAR} patterns the same as @VAR@ — BEFORE cmakedefine processing
3. Handle #cmakedefine VAR value — emit #define VAR value when defined
4. Treat any non-empty, non-'0' value as "true" for cmakedefine
5. Look up bare variable names by also checking @var@ in the dict
6. Process all .cmake templates generically
"""
import re
import sys
import os

def is_cmake_true(val):
    """Return True if val is considered 'true' in CMake sense."""
    if val == '' or val is None:
        return False
    if val.lower() in ('0', 'off', 'no', 'false', 'ignore', 'notfound', ''):
        return False
    return True


def lookup(var, substitutions):
    """Look up a variable in substitutions, accepting bare or @-wrapped names."""
    if var in substitutions:
        return substitutions[var]
    wrapped = '@' + var + '@'
    if wrapped in substitutions:
        return substitutions[wrapped]
    return None


def generate_config(template_path, output_path, substitutions, extra_defines=None):
    """
    Generate a config header from a .cmake template.

    Args:
        template_path: Path to the .cmake template file
        output_path: Path for the generated header
        substitutions: Dict of @VAR@ -> value substitutions
        extra_defines: Dict of ADDITIONAL variables to treat as defined for cmakedefine
                       (beyond what's in substitutions)
    """
    if extra_defines is None:
        extra_defines = {}

    with open(template_path, 'r') as f:
        content = f.read()

    # ========================================================================
    # PASS 1: Substitute @VAR@ patterns
    # ========================================================================
    for key, val in substitutions.items():
        content = content.replace(key, val)

    # ========================================================================
    # PASS 2: Substitute ${VAR} patterns (same values, strip @ signs from keys)
    # ========================================================================
    dollar_subst = {}
    for key, val in substitutions.items():
        bare_key = key.strip('@')
        dollar_subst['${' + bare_key + '}'] = val
    for key, val in dollar_subst.items():
        content = content.replace(key, val)

    # Build a unified lookup: substitutions + extra_defines
    unified = {}
    for k, v in substitutions.items():
        bare = k.strip('@')
        unified[bare] = v
    unified.update(extra_defines)

    def var_is_true(var):
        if var in unified:
            return is_cmake_true(unified[var])
        return False

    # ========================================================================
    # PASS 3: Handle #cmakedefine01 VAR — MUST be before #cmakedefine
    # Avoid regex prefix collision: #cmakedefine01 must be caught before
    # the #cmakedefine pattern.
    # ========================================================================
    def handle_cmakedefine01(m):
        var = m.group(1).strip()
        if var_is_true(var):
            return f'#define {var} 1'
        return f'#define {var} 0'

    content = re.sub(r'#cmakedefine01\s+(\w+)', handle_cmakedefine01, content)

    # ========================================================================
    # PASS 4: Handle #cmakedefine VAR [body]
    # Captures variable name and any trailing text as the body (with substitutions)
    # ========================================================================
    def handle_cmakedefine(m):
        var = m.group(1).strip()
        body = m.group(2).rstrip()
        if var_is_true(var):
            if body:
                # Body may contain @var@ or ${var} references — substitute them
                for key2, val2 in substitutions.items():
                    body = body.replace(key2, val2)
                for key2, val2 in dollar_subst.items():
                    body = body.replace(key2, val2)
                return f'#define {var} {body}'
            else:
                return f'#define {var}'
        return f'/* #undef {var} */'

    content = re.sub(r'#cmakedefine\s+(\w+)(.*)', handle_cmakedefine, content)

    # ========================================================================
    # PASS 5: Nuke any remaining ${...} that weren't caught
    # ========================================================================
    content = re.sub(r'\$\{[^}]*\}', '', content)

    # ========================================================================
    # PASS 6: Nuke any remaining @...@ patterns
    # ========================================================================
    content = re.sub(r'@[A-Za-z_][A-Za-z_0-9]*@', '', content)

    # ========================================================================
    # PASS 7: Clean up triplicate blank lines
    # ========================================================================
    content = re.sub(r'\n\s*\n\s*\n', '\n\n', content)

    # ========================================================================
    # PASS 8: Clean up trailing whitespace on lines
    # ========================================================================
    content = re.sub(r' +\n', '\n', content)

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        f.write(content)

    print(f"Generated {output_path}")
    print(f"Size: {len(content)} bytes")


# ============================================================================
# Configuration for llvm-kain
# ============================================================================
KAIN_DIR = "X:/llvm-kain"
BUILD_INCLUDE = os.path.join(KAIN_DIR, "build/include")

# These are the variable substitutions (both @VAR@ and ${VAR} patterns)
SUBSTITUTIONS = {
    '@LLVM_CONFIGURATION_TYPE@': 'Release',
    '@LLVM_ENABLE_ASSERTIONS@': '1',
    '@LLVM_ENABLE_EXPENSIVE_CHECKS@': '0',
    '@LLVM_ENABLE_PLUGINS@': '0',
    '@LLVM_ENABLE_LIBXML2@': '0',
    '@LLVM_ENABLE_EH@': '1',
    '@LLVM_ENABLE_RTTI@': '1',
    '@LLVM_ENABLE_TERMINFO@': '0',
    '@LLVM_ENABLE_LIBPFM@': '0',
    '@LLVM_ENABLE_LIBCXX@': '0',
    '@LLVM_ENABLE_LLD@': '0',
    '@LLVM_ENABLE_ZLIB@': '0',
    '@LLVM_ENABLE_ZSTD@': '0',
    '@LLVM_ENABLE_DIA_SDK@': '0',
    '@LLVM_ENABLE_CURL@': '0',
    '@LLVM_ENABLE_HTTPLIB@': '0',
    '@LLVM_ENABLE_THREADS@': '1',
    '@LLVM_ENABLE_UNWIND_TABLES@': '1',
    '@LLVM_ENABLE_FFI@': '0',
    '@LLVM_NATIVE_ARCH@': 'X86',
    '@LLVM_DEFAULT_TARGET_TRIPLE@': 'x86_64-pc-win32-msvc',
    '@LLVM_HOST_TRIPLE@': 'x86_64-pc-win32-msvc',
    '@LLVM_PER_TARGET_PERFORMANCE_COUNTERS@': '0',
    '@LLVM_HAS_ATOMICS@': '1',
    '@LLVM_ENABLE_ABI_BREAKING_CHECKS@': '0',
    '@LLVM_ENABLE_MODULES@': '0',
    '@LLVM_ENABLE_LOCAL_SUBMODULE_VISIBILITY@': '0',
    '@LLVM_HAS_GLOBAL_ISEL@': '1',
    '@LLVM_USE_SANITIZER@': '',
    '@LLVM_USE_SANITIZE_COVERAGE@': '',
    '@LLVM_LIBDIR_SUFFIX@': '',
    '@LLVM_BUILD_EXTERNAL_COMPILER_RT@': '0',
    '@LLVM_BUILD_LLVM_DYLIB@': '0',
    '@LLVM_LINK_LLVM_DYLIB@': '0',
    '@LLVM_ENABLE_PIC@': '1',
    '@LLVM_ENABLE_BACKTRACES@': '1',
    '@LLVM_ENABLE_CRASH_OVERRIDES@': '1',
    '@LLVM_ENABLE_Z3_SOLVER@': '0',
    '@LLVM_ENABLE_WARNINGS@': '1',
    '@LLVM_ENABLE_MODULE_DEBUGGING@': '0',
    '@LLVM_VERSION_MAJOR@': '19',
    '@LLVM_VERSION_MINOR@': '0',
    '@LLVM_VERSION_PATCH@': '0',
    '@LLVM_VERSION_STRING@': '19.0.0',
    '@PACKAGE_VERSION@': '19.0.0',
    '@LLVM_PACKAGE_VERSION@': '19.0.0',
    '@LLVM_COMMON_DLL_NAME@': 'LLVM-KAIN',
    '@LLVM_BUILD_INSTRUMENTED_COVERAGE@': '',
    '@LLVM_BUILD_INSTRUMENTED@': '',
    '@LLVM_NATIVE_BUILD@': '0',
    '@LLVM_DYLIB_COMPONENTS@': 'all',
    '@LLVM_ENABLE_DUMP@': '',
    # Additional variables used by cmakedefine
    '@HAVE_SYSEXITS_H@': '0',
    '@LLVM_ON_UNIX@': '0',
    '@LLVM_WITH_Z3@': '0',
    '@LLVM_HAVE_TFLITE@': '0',
    '@LLVM_ENABLE_PROFCHECK@': '0',
    '@LLVM_BUILD_SHARED_LIBS@': '0',
    '@LLVM_ENABLE_LLVM_EXPORT_ANNOTATIONS@': '0',
    '@LLVM_ENABLE_LLVM_C_EXPORT_ANNOTATIONS@': '0',
    '@LLVM_FORCE_USE_OLD_TOOLCHAIN@': '0',
    '@LLVM_HAS_LOGF128@': '0',
    '@LLVM_ENABLE_TELEMETRY@': '0',
    '@LLVM_ENABLE_DEBUGLOC_TRACKING_COVERAGE@': '0',
    '@LLVM_ENABLE_DEBUGLOC_TRACKING_ORIGIN@': '0',
    '@LLVM_ENABLE_ONDISK_CAS@': '0',
    '@LLVM_UNREACHABLE_OPTIMIZE@': '',
    '@LLVM_ENABLE_IO_SANDBOX@': '',
    '@LLVM_FORCE_ENABLE_STATS@': '',
    '@LLVM_USE_INTEL_JITEVENTS@': '',
    '@LLVM_USE_OPROFILE@': '',
    '@LLVM_USE_PERF@': '',
    '@LLVM_ENABLE_REVERSE_ITERATION@': '',
    '@LLVM_INCLUDE_UTILS@': '',
    '@LLVM_INSTALL_UTILS@': '',
    '@LLVM_ENABLE_RUNTIME_BUILD@': '',
    '@LLVM_ENABLE_CRASH_DUMPS@': '',
}

# Extra variables that should be TRUE for cmakedefine but don't need @ substitutions
EXTRA_DEFINES = {
    # These are auto-set by CMake's config-ix.cmake but we hardcode for our build
    'LLVM_CONFIGURATION_TYPE': 'Release',
}


CONFIG_FILES = {
    'llvm-config.h.cmake': 'llvm-config.h',
    'abi-breaking.h.cmake': 'abi-breaking.h',
    'config.h.cmake': 'config.h',
    'Targets.h.cmake': 'Targets.h',
}


def main():
    config_dir = os.path.join(KAIN_DIR, 'include/core/config')
    out_core_config = os.path.join(BUILD_INCLUDE, 'core/config')
    os.makedirs(out_core_config, exist_ok=True)

    # Generate .cmake -> .h config headers
    for template_name, output_name in CONFIG_FILES.items():
        template_path = os.path.join(config_dir, template_name)
        output_path = os.path.join(out_core_config, output_name)
        if os.path.exists(template_path):
            generate_config(template_path, output_path, SUBSTITUTIONS, EXTRA_DEFINES)
        else:
            print(f"WARNING: template not found: {template_path}")

    # Generate .def.in -> .def registries
    for fname in os.listdir(config_dir):
        if fname.endswith('.def.in'):
            template_path = os.path.join(config_dir, fname)
            output_name = fname.replace('.in', '')
            output_path = os.path.join(out_core_config, output_name)
            print(f"Generating def registry: {fname} -> {output_name}")
            generate_config(template_path, output_path, SUBSTITUTIONS, EXTRA_DEFINES)

    print("\nAll config headers generated successfully.")


if __name__ == '__main__':
    main()
