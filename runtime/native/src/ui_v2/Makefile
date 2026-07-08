# ============================================================================
#  Makefile — Kaintana C Substrate
#
#  Builds demos, test runner, and provides syntax-check targets for CI.
#  Mirrors the compile patterns from build.py and tests/Makefile.
#
#  Targets:
#    make              Syntax-check everything (default)
#    make check        Syntax-check all core + backend files
#    make hello        Build the terminal hello_kaintana demo
#    make demos        Build all four demos
#    make test_runner  Build the test runner (null backend)
#    make test         Build test runner and run pytest
#    make clean        Remove build artifacts
# ============================================================================

CC       = gcc
# Debug build: adds -g and -DDEBUG
DEBUG_CFLAGS = -g -DDEBUG
CFLAGS   = -std=c11 -Wall -Wextra -pedantic -Werror -Wno-unused-function
INCLUDES = -I X:/runtime/native/include -I .
RUNTIME_SRC = ../../src/core

# Core Kaintana substrate files (8 .c files, always compiled)
CORE = \
    tree.c \
    box_math.c \
    damage.c \
    draw_pixels.c \
    arena.c \
    hash_table.c \
    color.c \
    attr_table.c

# Runtime stubs (provides string_new etc. when full runtime is not linked)
RUNTIME = kaintana_runtime_stubs.c

# Native runtime core files (provides aria, component_surface, handle, input system)
RUNTIME_CORE = \
    $(RUNTIME_SRC)/arena.c \
    $(RUNTIME_SRC)/version.c \
    $(RUNTIME_SRC)/component_surface.c \
    $(RUNTIME_SRC)/handle.c \
    $(RUNTIME_SRC)/input_system.c
RUNTIME = kaintana_runtime_stubs.c

# Windows SDK link libraries
WIN32_LIBS = -lgdi32 -lws2_32 -lopengl32

.PHONY: all check hello demos demo_dashboard test_runner test clean

# ── Default target: syntax-check everything ──────────────────────────────────
all: check

# ── Syntax check (CI gate) ──────────────────────────────────────────────────
check:
	@echo "=== Checking core files ==="
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only tree.c && echo "  tree.c OK"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only box_math.c && echo "  box_math.c OK"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only damage.c && echo "  damage.c OK"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only draw_pixels.c && echo "  draw_pixels.c OK"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only arena.c && echo "  arena.c OK"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only hash_table.c && echo "  hash_table.c OK"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only color.c && echo "  color.c OK"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only attr_table.c && echo "  attr_table.c OK"
	@echo "  ---"
	@echo "All core files OK"
	@echo ""
	@echo "=== Checking backends ==="
	@$(CC) $(CFLAGS) $(INCLUDES) -D_WIN32 -fsyntax-only backends/win32/host_win32.c && echo "  win32 OK"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only backends/null/host_null.c && echo "  null OK"
	@$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only backends/terminal/host_terminal.c && echo "  terminal OK"
	@echo "  ---"
	@echo "All backends OK"

# ── Demos ───────────────────────────────────────────────────────────────────
hello: hello.exe

hello.exe: examples/hello_kaintana.c
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(RUNTIME) $(RUNTIME_CORE) examples/hello_kaintana.c -o hello.exe

demos: hello.exe demo_file_explorer.exe demo_ide_clone.exe demo_minecraft_ui.exe demo_dashboard.exe

demo_file_explorer.exe: examples/demo_file_explorer.c
	$(CC) $(CFLAGS) $(INCLUDES) -D_WIN32 $(CORE) $(RUNTIME) $(RUNTIME_CORE) backends/win32/host_win32.c backends/win32/render_gdi.c examples/demo_file_explorer.c -o demo_file_explorer.exe $(WIN32_LIBS)

demo_ide_clone.exe: examples/demo_ide_clone.c
	$(CC) $(CFLAGS) $(INCLUDES) -D_WIN32 $(CORE) $(RUNTIME) $(RUNTIME_CORE) examples/demo_ide_clone.c -o demo_ide_clone.exe $(WIN32_LIBS)

demo_minecraft_ui.exe: examples/demo_minecraft_ui.c
	$(CC) $(CFLAGS) $(INCLUDES) -D_WIN32 $(CORE) $(RUNTIME) $(RUNTIME_CORE) examples/demo_minecraft_ui.c -o demo_minecraft_ui.exe $(WIN32_LIBS)

demo_dashboard.exe: examples/demo_dashboard.c
	$(CC) $(CFLAGS) $(INCLUDES) -D_WIN32 $(CORE) $(RUNTIME) $(RUNTIME_CORE) examples/demo_dashboard.c -o demo_dashboard.exe $(WIN32_LIBS)

# ── Debug pattern rules ────────────────────────────────────────────────────
# Compile with -g -DDEBUG for detailed diagnostics.
# Usage: make debug-voxel_viewer  (compiles with DEBUG)
#        make debug-dashboard
#        make debug-api_coverage
#
debug_voxel_viewer_OBJS = $(CORE) $(RUNTIME) $(RUNTIME_CORE)

debug-voxel_viewer: examples/voxel_viewer_debug.exe
	@echo "debug-voxel_viewer: OK"

debug-dashboard: examples/demo_dashboard_debug.exe
	@echo "debug-dashboard: OK"

debug-api_coverage: examples/test_api_coverage_debug.exe
	@echo "debug-api_coverage: OK"

.PHONY: debug-voxel_viewer debug-dashboard debug-api_coverage debug-%


# Pattern fallback: first-configured debug target
# To add a new debug target, create an explicit rule above.
debug-%: examples/voxel_viewer_debug.exe
	@echo "Target 'debug-$*' resolved to voxel_viewer_debug.exe (default)"

examples/voxel_viewer_debug.exe: examples/voxel_viewer.c
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) $(INCLUDES) -D_WIN32 $(debug_voxel_viewer_OBJS) examples/voxel_viewer.c -o examples/voxel_viewer_debug.exe $(WIN32_LIBS)

examples/demo_dashboard_debug.exe: examples/demo_dashboard.c
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) $(INCLUDES) -D_WIN32 $(CORE) $(RUNTIME) $(RUNTIME_CORE) examples/demo_dashboard.c -o examples/demo_dashboard_debug.exe $(WIN32_LIBS)

examples/test_api_coverage_debug.exe: examples/demo_api_coverage.c
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) $(INCLUDES) -D_WIN32 $(CORE) $(RUNTIME) $(RUNTIME_CORE) examples/demo_api_coverage.c -o examples/test_api_coverage_debug.exe $(WIN32_LIBS)

# ── Test runner ──────────────────────────────────────────────────────────────
test_runner: tests/test_runner.exe

tests/test_runner.exe: tests/test_runner.c
	$(CC) $(CFLAGS) $(INCLUDES) $(CORE) $(RUNTIME) $(RUNTIME_CORE) tests/test_runner.c -o tests/test_runner.exe

test: test_runner
	cd tests && python -m pytest

# ── Clean ────────────────────────────────────────────────────────────────────
clean:
	rm -f hello.exe demo_file_explorer.exe demo_ide_clone.exe demo_minecraft_ui.exe
	rm -f tests/test_runner.exe
	rm -f examples/*.exe
