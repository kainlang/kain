/*
 * Test: stdlib ABI filesystem root-prefix handling
 *
 * Validates nested directory creation through both create_dir_all and the
 * write-text parent-dir helper, including Windows extended-length prefixes.
 */

#include "../include/stdlib_abi.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <stdlib.h>
#define PATH_SEP "\\"
#define SNPRINTF(buffer, size, format, ...) _snprintf_s((buffer), (size), _TRUNCATE, (format), __VA_ARGS__)
#else
#define PATH_SEP "/"
#define SNPRINTF(buffer, size, format, ...) snprintf((buffer), (size), (format), __VA_ARGS__)
#endif

static int expect_status(const char* label, int64_t actual, int64_t expected) {
    if (actual != expected) {
        printf(
            "FAIL: %s expected %lld, got %lld (kind=%s, detail=%s)\n",
            label,
            (long long)expected,
            (long long)actual,
            abi_fs_last_error_kind(),
            abi_fs_last_error_message()
        );
        return 0;
    }
    return 1;
}

static int expect_true(const char* label, int condition) {
    if (!condition) {
        printf("FAIL: %s\n", label);
        return 0;
    }
    return 1;
}

static int build_path(char* output, size_t output_size, const char* left, const char* right) {
    int written = SNPRINTF(output, output_size, "%s%s%s", left, PATH_SEP, right);
    return written >= 0 && (size_t)written < output_size;
}

#ifdef _WIN32
static int build_extended_path(char* output, size_t output_size, const char* path) {
    size_t index = 0u;
    int written = SNPRINTF(output, output_size, "\\\\?\\%s", path);
    if (written < 0 || (size_t)written >= output_size) {
        return 0;
    }
    for (index = 4u; output[index] != '\0'; ++index) {
        if (output[index] == '/') {
            output[index] = '\\';
        }
    }
    return 1;
}
#endif

static int test_create_dir_all_nested(void) {
    printf("\n=== Test 1: create_dir_all builds nested directories ===\n");

    const char* base = abi_fs_temp_dir("abi-fs-root");
    char nested[4096];

    if (!expect_true("abi_fs_temp_dir returned a path", base != NULL && base[0] != '\0')) {
        return 0;
    }
    if (!build_path(nested, sizeof(nested), base, "alpha" PATH_SEP "beta")) {
        printf("FAIL: could not build nested path\n");
        return 0;
    }
    if (!expect_status("abi_fs_create_dir_all", abi_fs_create_dir_all(nested), 0)) {
        (void)abi_fs_remove_dir_all(base);
        return 0;
    }
    if (!expect_true("nested directory exists", abi_fs_is_dir(nested) != 0)) {
        (void)abi_fs_remove_dir_all(base);
        return 0;
    }

    if (!expect_status("cleanup nested temp root", abi_fs_remove_dir_all(base), 0)) {
        return 0;
    }
    printf("PASS: nested directory creation works through stdlib ABI\n");
    return 1;
}

#ifdef _WIN32
static int test_extended_path_parent_creation(void) {
    printf("\n=== Test 2: extended Windows path parent creation ===\n");

    const char* base = abi_fs_temp_dir("abi-fs-extended");
    char extended_base[4096];
    char extended_dir[4096];
    char extended_file[4096];
    char plain_dir[4096];
    char plain_file[4096];

    if (!expect_true("abi_fs_temp_dir returned a Windows base path", base != NULL && base[0] != '\0')) {
        return 0;
    }
    if (!build_extended_path(extended_base, sizeof(extended_base), base)) {
        printf("FAIL: could not build extended base path\n");
        return 0;
    }
    if (!build_path(extended_dir, sizeof(extended_dir), extended_base, "unc-lane" PATH_SEP "delta")) {
        printf("FAIL: could not build extended nested directory\n");
        return 0;
    }
    if (!build_path(extended_file, sizeof(extended_file), extended_dir, "artifact.txt")) {
        printf("FAIL: could not build extended file path\n");
        return 0;
    }
    if (!build_path(plain_dir, sizeof(plain_dir), base, "unc-lane" PATH_SEP "delta")) {
        printf("FAIL: could not build plain nested directory\n");
        return 0;
    }
    if (!build_path(plain_file, sizeof(plain_file), plain_dir, "artifact.txt")) {
        printf("FAIL: could not build plain file path\n");
        return 0;
    }

    if (!expect_status("abi_fs_create_dir_all on extended path", abi_fs_create_dir_all(extended_dir), 0)) {
        (void)abi_fs_remove_dir_all(base);
        return 0;
    }
    if (!expect_true("plain nested directory exists after extended create_dir_all", abi_fs_is_dir(plain_dir) != 0)) {
        (void)abi_fs_remove_dir_all(base);
        return 0;
    }
    if (!expect_status("abi_fs_write_text on extended path", abi_fs_write_text(extended_file, "alien"), 0)) {
        (void)abi_fs_remove_dir_all(base);
        return 0;
    }
    if (!expect_true("plain file exists after extended write", abi_fs_exists(plain_file) != 0)) {
        (void)abi_fs_remove_dir_all(base);
        return 0;
    }

    if (!expect_status("cleanup extended temp root", abi_fs_remove_dir_all(base), 0)) {
        return 0;
    }
    printf("PASS: extended Windows paths create parents without touching the root prefix\n");
    return 1;
}
#endif

int main(void) {
    printf("Starting stdlib ABI filesystem tests\n");

    int passed = 0;
    int total = 1;

    passed += test_create_dir_all_nested();
#ifdef _WIN32
    total += 1;
    passed += test_extended_path_parent_creation();
#endif

    printf("\n=== Test Results ===\n");
    printf("Passed: %d/%d\n", passed, total);
    return passed == total ? 0 : 1;
}
