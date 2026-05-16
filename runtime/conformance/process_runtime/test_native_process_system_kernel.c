#include "process_system.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static int expect_int(const char* label, long long actual, long long expected) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %lld, got %lld\n", label, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_text_contains(const char* label, const char* actual, const char* expected_fragment) {
    if (actual == 0 || strstr(actual, expected_fragment) == 0) {
        fprintf(
            stderr,
            "%s: expected fragment '%s' in '%s'\n",
            label,
            expected_fragment,
            actual ? actual : ""
        );
        return 1;
    }
    return 0;
}

static int expect_pipe_stdio(const char* label, int64_t spec_id) {
    if (expect_int(label, abi_process_spec_set_stdin_mode(spec_id, "pipe"), 0)) return 1;
    if (expect_int(label, abi_process_spec_set_stdout_mode(spec_id, "pipe"), 0)) return 1;
    if (expect_int(label, abi_process_spec_set_stderr_mode(spec_id, "pipe"), 0)) return 1;
    return 0;
}

#ifdef _WIN32
static int prepare_temp_cwd(char* out_path, size_t out_path_size) {
    DWORD base_length;
    if (out_path == 0 || out_path_size == 0u) {
        return 0;
    }
    base_length = GetTempPathA((DWORD)out_path_size, out_path);
    if (base_length == 0u || base_length >= out_path_size) {
        return 0;
    }
    snprintf(
        out_path + strlen(out_path),
        out_path_size - strlen(out_path),
        "kain-process-proof-%lu",
        (unsigned long)GetCurrentProcessId()
    );
    _mkdir(out_path);
    return 1;
}
#endif

int main(void) {
    if (expect_int("reset", abi_process_reset(), 0)) return 1;

#ifndef _WIN32
    if (expect_int("platform unavailable", abi_process_platform_available(), 0)) return 2;
    {
        int64_t spec = abi_process_spec_create("sh");
        if (spec <= 0) return 3;
        if (expect_int(
                "unsupported spawn",
                abi_process_spawn(spec),
                ABI_PROCESS_UNSUPPORTED_PLATFORM
            )) return 4;
    }
    return 0;
#else
    {
        char cwd_override[ABI_PROCESS_MAX_PATH];
        int64_t spec;
        int64_t child;
        const char* captured;
        const char* env_text;
        const char* cwd_text;
        const char* mirrored_text;
        const char* pty_text;
        int64_t pty;

        if (expect_int("platform available", abi_process_platform_available(), 1)) return 2;

        spec = abi_process_spec_create("cmd.exe");
        if (spec <= 0) return 3;
        if (expect_pipe_stdio("echo stdio pipe", spec)) return 4;
        if (expect_int("echo arg /d", abi_process_spec_add_arg(spec, "/d"), 0)) return 5;
        if (expect_int("echo arg /c", abi_process_spec_add_arg(spec, "/c"), 0)) return 6;
        if (expect_int("echo arg payload", abi_process_spec_add_arg(spec, "echo process-proof"), 0)) return 7;
        child = abi_process_spawn(spec);
        if (child <= 0) return 8;
        if (expect_int("echo wait", abi_process_wait(child, 5000), 1)) return 9;
        if (expect_int("echo exit code", abi_process_exit_code(child), 0)) return 10;
        captured = abi_process_stdout_capture_text(child);
        if (expect_text_contains("echo capture", captured, "process-proof")) return 11;

        spec = abi_process_spec_create("cmd.exe");
        if (spec <= 0) return 12;
        if (expect_pipe_stdio("env stdio pipe", spec)) return 13;
        if (expect_int("env arg /d", abi_process_spec_add_arg(spec, "/d"), 0)) return 14;
        if (expect_int("env arg /c", abi_process_spec_add_arg(spec, "/c"), 0)) return 15;
        if (expect_int("env arg payload", abi_process_spec_add_arg(spec, "echo %KAIN_PROCESS_PROOF%"), 0)) return 16;
        if (expect_int("env override", abi_process_spec_set_env(spec, "KAIN_PROCESS_PROOF", "native-env"), 0)) return 17;
        child = abi_process_spawn(spec);
        if (child <= 0) return 18;
        if (expect_int("env wait", abi_process_wait(child, 5000), 1)) return 19;
        env_text = abi_process_stdout_capture_text(child);
        if (expect_text_contains("env capture", env_text, "native-env")) return 20;

        if (!prepare_temp_cwd(cwd_override, sizeof(cwd_override))) return 21;
        spec = abi_process_spec_create("cmd.exe");
        if (spec <= 0) return 22;
        if (expect_pipe_stdio("cwd stdio pipe", spec)) return 23;
        if (expect_int("cwd arg /d", abi_process_spec_add_arg(spec, "/d"), 0)) return 24;
        if (expect_int("cwd arg /c", abi_process_spec_add_arg(spec, "/c"), 0)) return 25;
        if (expect_int("cwd arg payload", abi_process_spec_add_arg(spec, "cd"), 0)) return 26;
        if (expect_int("cwd override", abi_process_spec_set_cwd(spec, cwd_override), 0)) return 27;
        child = abi_process_spawn(spec);
        if (child <= 0) return 28;
        if (expect_int("cwd wait", abi_process_wait(child, 5000), 1)) return 29;
        cwd_text = abi_process_stdout_capture_text(child);
        if (expect_text_contains("cwd capture", cwd_text, cwd_override)) return 30;

        spec = abi_process_spec_create("cmd.exe");
        if (spec <= 0) return 31;
        if (expect_pipe_stdio("mirror stdio pipe", spec)) return 32;
        if (expect_int("mirror arg /d", abi_process_spec_add_arg(spec, "/d"), 0)) return 33;
        if (expect_int("mirror arg /c", abi_process_spec_add_arg(spec, "/c"), 0)) return 34;
        if (expect_int("mirror arg payload", abi_process_spec_add_arg(spec, "more"), 0)) return 35;
        child = abi_process_spawn(spec);
        if (child <= 0) return 36;
        if (abi_process_stdin_write_text(child, "alpha\r\nbeta\r\n") <= 0) return 37;
        if (expect_int("mirror stdin close", abi_process_stdin_close(child), 0)) return 38;
        if (expect_int("mirror wait", abi_process_wait(child, 5000), 1)) return 39;
        mirrored_text = abi_process_stdout_capture_text(child);
        if (expect_text_contains("mirror alpha", mirrored_text, "alpha")) return 40;
        if (expect_text_contains("mirror beta", mirrored_text, "beta")) return 41;

        spec = abi_process_spec_create("cmd.exe");
        if (spec <= 0) return 42;
        if (expect_int("pty echo arg /d", abi_process_spec_add_arg(spec, "/d"), 0)) return 43;
        if (expect_int("pty echo arg /c", abi_process_spec_add_arg(spec, "/c"), 0)) return 44;
        if (expect_int("pty echo payload", abi_process_spec_add_arg(spec, "echo pty-proof"), 0)) return 45;
        pty = abi_process_spawn_pty(spec, 100, 30);
        if (pty <= 0) return 46;
        if (expect_int("pty wait", abi_process_wait(pty, 5000), 1)) return 47;
        pty_text = abi_process_pty_capture_text(pty);
        if (expect_text_contains("pty capture", pty_text, "pty-proof")) return 48;
        abi_process_close(pty);

        spec = abi_process_spec_create("cmd.exe");
        if (spec <= 0) return 49;
        if (expect_int("pty write arg /q", abi_process_spec_add_arg(spec, "/q"), 0)) return 50;
        pty = abi_process_spawn_pty(spec, 100, 30);
        if (pty <= 0) return 51;
        Sleep(100u);
        if (expect_int("pty resize", abi_process_pty_resize(pty, 120, 40), 0)) return 52;
        if (abi_process_pty_write_text(pty, "exit\r\n") <= 0) return 53;
        if (expect_int("pty kill", abi_process_kill(pty), 0)) return 54;
        abi_process_close(pty);

        if (cwd_override[0] != '\0') {
            _rmdir(cwd_override);
        }
    }
    return 0;
#endif
}
