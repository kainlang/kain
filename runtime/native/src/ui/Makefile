# ============================================================================
#  Kain Native UI — Master Makefile
#  ============================================================================
#  Single entry point for building ALL UI components, tests, and demos.
#  Supports incremental compilation (.o files cached), parallel builds
#  (make -j8), and per-target builds.
#
#  Usage:
#    make              — build everything (libs + tests + demos)
#    make libs         — build UI library objects only
#    make tests        — build test_ui/ executables
#    make demos        — build test_ui_v2/ demo executables
#    make widgets      — build widget library + test
#    make clean        — remove all build artifacts
#    make run_cosmic   — build + run cosmic dashboard
#    make run_retro    — build + run retro wave
#    make run_3d       — build + run 3D sandbox
#    make run_calc     — build + run calculator test
#    make -j8          — parallel build (8 jobs)
#    make CC=gcc       — use GCC instead of clang
#    make CFLAGS=-O2   — optimized build
# ============================================================================

# ── Toolchain ──────────────────────────────────────────────────────────
CC      := $(or $(CC),clang)
CFLAGS  := $(or $(CFLAGS),-std=c11 -g -O0)
LDFLAGS := -luser32 -lgdi32 -lopengl32
AR      := $(or $(AR),llvm-ar)
ARFLAGS := rcs

# Static library name — .a on Linux/MinGW, .lib on Windows
# llvm-ar creates .a files that lld-link can consume on Windows too.
# For MSVC-style .lib, llvm-lib would be used instead.
LIB_NAME := libkain_ui.a
LIB_DIR  := X:/runtime/native/src/ui

# ── Paths ──────────────────────────────────────────────────────────────
UI_DIR      := .
TEST_DIR    := test_ui
DEMO_DIR    := test_ui_v2
WIDGET_DIR  := widgets
CORE_DIR    := ../core
INCLUDE_DIR := ../../include
STB_DIR     := ../../extras/_stb-truetype

# ── Include paths ──────────────────────────────────────────────────────
INC := -I$(INCLUDE_DIR) -I$(UI_DIR) -I$(WIDGET_DIR) -I$(CORE_DIR) -I$(STB_DIR)

# ── MSVC Library path detection ────────────────────────────────────────
# Auto-detect VS + SDK library paths if LIB env var not set.
ifeq ($(origin LIB),undefined)
  VS_BASE    := $(shell cmd //c "echo %ProgramFiles(x86)%" 2>nul)\Microsoft Visual Studio\2022\BuildTools
  MSVC_VER   := $(shell dir "$(VS_BASE)\VC\Tools\MSVC" /b 2>nul | sort /r | head -1)
  SDK_VER    := $(shell dir "C:\Program Files (x86)\Windows Kits\10\Lib" /b 2>nul | sort /r | head -1)
  MSVC_LIB   := "$(VS_BASE)\VC\Tools\MSVC\$(MSVC_VER)\lib\x64"
  UCRT_LIB   := "C:\Program Files (x86)\Windows Kits\10\Lib\$(SDK_VER)\ucrt\x64"
  UM_LIB     := "C:\Program Files (x86)\Windows Kits\10\Lib\$(SDK_VER)\um\x64"
  export LIB := $(MSVC_LIB);$(UCRT_LIB);$(UM_LIB)
endif

# ── Source files ───────────────────────────────────────────────────────
# UI library sources
UI_SRCS := \
	$(UI_DIR)/ui_system.c \
	$(UI_DIR)/ui_host_adapter.c \
	$(UI_DIR)/ui_renderer.c \
	$(UI_DIR)/ui_layout.c \
	$(UI_DIR)/ui_color.c \
	$(UI_DIR)/ui_hot_reload.c \
	$(UI_DIR)/ui_runtime.c \
	$(UI_DIR)/ui_compiled_bundle.c \
	$(UI_DIR)/native_ui_surface.c

# Core sources needed by UI
CORE_SRCS := \
	$(CORE_DIR)/component_surface.c \
	$(CORE_DIR)/input_system.c

# Widget sources
WIDGET_SRCS := $(WIDGET_DIR)/ui_widget.c

# Stubs
STUBS := $(TEST_DIR)/stubs.c
WIDGET_STUBS := $(WIDGET_DIR)/stubs.c
DEMO_STUBS := $(DEMO_DIR)/stubs.c

# Test sources (test_ui/)
TEST_SRCS := \
	$(TEST_DIR)/calculator.c \
	$(TEST_DIR)/full_demo.c \
	$(TEST_DIR)/keypad.c \
	$(TEST_DIR)/anim_demo.c \
	$(TEST_DIR)/hot_reload_test.c \
	$(TEST_DIR)/renderer_smoke_test.c \
	$(TEST_DIR)/widget_hello.c \
	$(TEST_DIR)/widget_demo.c \
	$(TEST_DIR)/widget_calculator.c

# Demo sources (test_ui_v2/)
DEMO_SRCS := \
	$(DEMO_DIR)/cosmic_dashboard.c \
	$(DEMO_DIR)/retrowave.c \
	$(DEMO_DIR)/ui3d_sandbox.c \
	$(DEMO_DIR)/fire_life_plasma.c \
	$(DEMO_DIR)/voxel_viewer.c

# ── Object files (cached for incremental builds) ───────────────────────
UI_OBJS        := $(UI_SRCS:.c=.o)
CORE_OBJS      := $(CORE_SRCS:.c=.o)
WIDGET_OBJS    := $(WIDGET_SRCS:.c=.o)
ALL_LIB_OBJS   := $(UI_OBJS) $(CORE_OBJS) $(WIDGET_OBJS)

# ── Flags ──────────────────────────────────────────────────────────────
# Auto-generate dependency files for incremental builds
DEPFLAGS = -MMD -MF $(@:.o=.d)

.PHONY: all static libs tests demos widgets clean \
	run_cosmic run_retro run_3d run_voxel run_calc help

# ── Default: build static lib + everything ──────────────────────────
all: static tests demos
	@echo ""
	@echo "=== ALL BUILDS PASSED ==="

# ── Build static library ───────────────────────────────────────────────
static: $(ALL_LIB_OBJS)
	$(AR) $(ARFLAGS) $(LIB_DIR)/$(LIB_NAME) $(ALL_LIB_OBJS)
	@echo "[STATIC] $(LIB_NAME) ($(words $(ALL_LIB_OBJS)) objects)"

# ── Build UI library objects ───────────────────────────────────────────
libs: $(ALL_LIB_OBJS)
	@echo "[LIBS] $(words $^) objects cached"

# ── Pattern: compile any .c to .o with dependency tracking ────────────
%.o: %.c
	$(CC) $(CFLAGS) $(INC) $(DEPFLAGS) -c $< -o $@

# ── Build test executables (test_ui/) ──────────────────────────────────
tests: $(ALL_LIB_OBJS) $(TEST_SRCS)
	@for test in $(TEST_SRCS); do \
		name=$$(basename $$test .c); \
		exe=$(TEST_DIR)/$$name.exe; \
		echo "[TEST] $$name"; \
		$(CC) $(CFLAGS) $$test $(STUBS) $(UI_SRCS) $(CORE_SRCS) $(INC) $(LDFLAGS) -o $$exe; \
	done
	@echo "[TESTS] done"

# ── Build demo executables (test_ui_v2/) ───────────────────────────────
demos: $(ALL_LIB_OBJS) $(DEMO_SRCS)
	@for demo in $(DEMO_SRCS); do \
		name=$$(basename $$demo .c); \
		exe=$(DEMO_DIR)/$$name.exe; \
		echo "[DEMO] $$name"; \
		$(CC) $(CFLAGS) $$demo $(DEMO_STUBS) $(WIDGET_SRCS) $(UI_SRCS) $(CORE_SRCS) $(INC) $(LDFLAGS) -o $$exe; \
	done
	@echo "[DEMOS] done"

# ── Build widget library + test ────────────────────────────────────────
widgets: $(ALL_LIB_OBJS)
	$(CC) $(CFLAGS) $(WIDGET_DIR)/test_widgets.c $(WIDGET_STUBS) $(WIDGET_SRCS) \
		$(UI_SRCS) $(CORE_SRCS) $(INC) $(LDFLAGS) -o $(WIDGET_DIR)/test_widgets.exe
	@echo "[WIDGETS] done"

# ── Run targets ────────────────────────────────────────────────────────
run_cosmic: $(DEMO_DIR)/cosmic_dashboard.exe
	$(DEMO_DIR)/cosmic_dashboard.exe &

run_retro: $(DEMO_DIR)/retrowave.exe
	$(DEMO_DIR)/retrowave.exe &

run_3d: $(DEMO_DIR)/ui3d_sandbox.exe
	$(DEMO_DIR)/ui3d_sandbox.exe &

run_calc: $(TEST_DIR)/calculator.exe
	start $(TEST_DIR)/calculator.exe

run_flp: $(DEMO_DIR)/fire_life_plasma.exe
	$(DEMO_DIR)/fire_life_plasma.exe &

run_voxel: $(DEMO_DIR)/voxel_viewer.exe
	$(DEMO_DIR)/voxel_viewer.exe &

# ── Single-shot targets (build then run) ──────────────────────────────
$(DEMO_DIR)/cosmic_dashboard.exe: $(DEMO_DIR)/cosmic_dashboard.c $(ALL_LIB_OBJS)
	$(CC) $(CFLAGS) $< $(DEMO_STUBS) $(WIDGET_SRCS) $(UI_SRCS) $(CORE_SRCS) $(INC) $(LDFLAGS) -o $@

$(DEMO_DIR)/retrowave.exe: $(DEMO_DIR)/retrowave.c $(ALL_LIB_OBJS)
	$(CC) $(CFLAGS) $< $(DEMO_STUBS) $(WIDGET_SRCS) $(UI_SRCS) $(CORE_SRCS) $(INC) $(LDFLAGS) -o $@

$(DEMO_DIR)/ui3d_sandbox.exe: $(DEMO_DIR)/ui3d_sandbox.c $(ALL_LIB_OBJS)
	$(CC) $(CFLAGS) $< $(DEMO_STUBS) $(WIDGET_SRCS) $(UI_SRCS) $(CORE_SRCS) $(INC) $(LDFLAGS) -o $@

$(DEMO_DIR)/fire_life_plasma.exe: $(DEMO_DIR)/fire_life_plasma.c $(ALL_LIB_OBJS)
	$(CC) $(CFLAGS) $< $(DEMO_STUBS) $(WIDGET_SRCS) $(UI_SRCS) $(CORE_SRCS) $(INC) $(LDFLAGS) -o $@

$(DEMO_DIR)/voxel_viewer.exe: $(DEMO_DIR)/voxel_viewer.c $(ALL_LIB_OBJS)
	$(CC) $(CFLAGS) $< $(DEMO_STUBS) $(WIDGET_SRCS) $(UI_SRCS) $(CORE_SRCS) $(INC) $(LDFLAGS) -o $@

$(TEST_DIR)/calculator.exe: $(TEST_DIR)/calculator.c $(ALL_LIB_OBJS)
	$(CC) $(CFLAGS) $< $(STUBS) $(UI_SRCS) $(CORE_SRCS) $(INC) $(LDFLAGS) -o $@

# ── Clean ──────────────────────────────────────────────────────────────
clean:
	find $(UI_DIR) -name '*.o' -o -name '*.d' -o -name '*.exe' -o -name '*.pdb' 2>/dev/null | xargs rm -f 2>/dev/null; \
	find $(TEST_DIR) -name '*.o' -o -name '*.d' -o -name '*.exe' -o -name '*.pdb' 2>/dev/null | xargs rm -f 2>/dev/null; \
	find $(DEMO_DIR) -name '*.o' -o -name '*.d' -o -name '*.exe' -o -name '*.pdb' 2>/dev/null | xargs rm -f 2>/dev/null; \
	find $(WIDGET_DIR) -name '*.o' -o -name '*.d' -o -name '*.exe' -o -name '*.pdb' 2>/dev/null | xargs rm -f 2>/dev/null; \
	find $(CORE_DIR) -name '*.o' -o -name '*.d' 2>/dev/null | xargs rm -f 2>/dev/null; \
	echo "[CLEAN] done"

# ── Include auto-generated dependency files ────────────────────────────
-include $(ALL_LIB_OBJS:.o=.d)
-include $(UI_SRCS:.c=.d)
-include $(CORE_SRCS:.c=.d)
-include $(WIDGET_SRCS:.c=.d)

# ── Help ───────────────────────────────────────────────────────────────
help:
	@echo "Kain Native UI — Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make              — build static lib + tests + demos"
	@echo "  make static       — build libkain_ui.a (static library)"
	@echo "  make libs         — build UI .o files only (incremental)"
	@echo "  make tests        — build test_ui/ executables"
	@echo "  make demos        — build test_ui_v2/ executables"
	@echo "  make widgets      — build widget library test"
	@echo "  make clean        — remove all build artifacts"
	@echo "  make run_cosmic   — build + run cosmic dashboard"
	@echo "  make run_retro    — build + run retro wave"
	@echo "  make run_3d       — build + run 3D sandbox"
	@echo "  make run_calc     — build + run calculator test"
	@echo "  make run_flp      — build + run fire+life+plasma demo"
	@echo "  make run_voxel    — build + run isometric voxel viewer"
	@echo "  make quick_voxel  — run voxel viewer (skip build if up to date)"
	@echo ""
	@echo "Options:"
	@echo "  make -j8          — parallel build (8 jobs)"
	@echo "  make CC=gcc       — use GCC instead of clang"
	@echo "  make CFLAGS=-O2   — optimized build"
	@echo ""
	@echo "Incremental: Only changed .c files recompile. Dependencies tracked."
