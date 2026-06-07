# Kain Native Runtime — Fast Dev Makefile
# =======================================
# Bazel handles production builds. This Makefile is for fast local iteration:
# compile, test with sanitizers, fuzz, build shared/static libs in seconds.
#
#   make          — compile all .o files (no linking)
#   make lib      — static library (libkain_runtime.a / .lib)
#   make shared   — shared library (libkain_runtime.so / .dylib / .dll)
#   make test     — build + run all tests with ASan+UBSan
#   make fuzz     — build fuzz targets with libFuzzer
#   make stress   — build stress tests with TSan
#   make clean    — remove build artifacts

# ── Platform detection ──────────────────────────────────────────────
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME_S),Linux)
  PLATFORM := linux
  SHARED_EXT := .so
  SHARED_FLAG := -shared
  STATIC_EXT := .a
  PLATFORM_LIBS := -lpthread -lm -ldl
  EXE_EXT :=
endif
ifeq ($(UNAME_S),Darwin)
  PLATFORM := darwin
  SHARED_EXT := .dylib
  SHARED_FLAG := -dynamiclib
  STATIC_EXT := .a
  PLATFORM_LIBS := -lpthread -ldl
  EXE_EXT :=
endif
ifneq ($(findstring MINGW,$(UNAME_S)),)
  PLATFORM := windows
  SHARED_EXT := .dll
  SHARED_FLAG := -shared
  STATIC_EXT := .lib
  PLATFORM_LIBS := -lws2_32 -luser32 -lgdi32 -lole32 -lshell32
  EXE_EXT := .exe
endif
ifneq ($(findstring CYGWIN,$(UNAME_S)),)
  PLATFORM := windows
  SHARED_EXT := .dll
  SHARED_FLAG := -shared
  STATIC_EXT := .lib
  PLATFORM_LIBS := -lws2_32 -luser32 -lgdi32 -lole32 -lshell32
  EXE_EXT := .exe
endif
ifeq ($(OS),Windows_NT)
  PLATFORM := windows
  SHARED_EXT := .dll
  SHARED_FLAG := -shared
  STATIC_EXT := .lib
  PLATFORM_LIBS := -lws2_32 -luser32 -lgdi32 -lole32 -lshell32
  EXE_EXT := .exe
endif

# ── Compiler ────────────────────────────────────────────────────────
CC ?= clang
AR ?= ar
CFLAGS := -std=c11 -Wall -Wextra -Iinclude -Isrc/core -Isrc/platform -g
LDFLAGS := $(PLATFORM_LIBS)

# Disable noisy MSVC deprecation warnings on Windows
ifeq ($(PLATFORM),windows)
  CFLAGS += -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS
endif

# ── Source files ────────────────────────────────────────────────────
CORE_SRCS := $(sort $(wildcard src/core/*.c))
CORE_SRCS := $(filter-out %_benchmark.c,$(CORE_SRCS))
CORE_OBJS := $(patsubst src/core/%.c,_build/obj/%.o,$(CORE_SRCS))

PLATFORM_SRCS := src/platform/platform.c
ifeq ($(PLATFORM),windows)
  PLATFORM_SRCS += src/platform/win32/win32_shared.c
else
  PLATFORM_SRCS += src/platform/linux/linux_shared.c
endif
PLATFORM_OBJS := $(patsubst src/platform/%.c,_build/obj/%.o,$(PLATFORM_SRCS))

ALL_OBJS := $(CORE_OBJS) $(PLATFORM_OBJS)

# ── Test sources ────────────────────────────────────────────────────
TEST_SMOKE_SRCS := $(wildcard test/smoke/*.c)
TEST_PROP_SRCS := $(wildcard test/property/*.c)
TEST_FUZZ_SRCS := $(wildcard test/fuzz/*.c)
TEST_STRESS_SRCS := $(wildcard test/stress/*.c)

TEST_SMOKE_BINS := $(patsubst test/smoke/%.c,_build/test/smoke/%$(EXE_EXT),$(TEST_SMOKE_SRCS))
TEST_PROP_BINS := $(patsubst test/property/%.c,_build/test/property/%$(EXE_EXT),$(TEST_PROP_SRCS))
TEST_FUZZ_BINS := $(patsubst test/fuzz/%.c,_build/test/fuzz/%$(EXE_EXT),$(TEST_FUZZ_SRCS))
TEST_STRESS_BINS := $(patsubst test/stress/%.c,_build/test/stress/%$(EXE_EXT),$(TEST_STRESS_SRCS))

# ── Library targets ─────────────────────────────────────────────────
LIBSTATIC := _build/lib/libkain_runtime$(STATIC_EXT)
LIBSHARED := _build/lib/libkain_runtime$(SHARED_EXT)

# ── Default ─────────────────────────────────────────────────────────
.DEFAULT_GOAL := all
all: $(ALL_OBJS)
	@echo "  ✓ $(words $(ALL_OBJS)) object files compiled"
	@echo "  make lib     — static library"
	@echo "  make shared  — shared library"
	@echo "  make test    — run tests with ASan+UBSan"
	@echo "  make fuzz    — build fuzz harnesses"

# ── Object files ────────────────────────────────────────────────────
_build/obj/core/%.o: src/core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

_build/obj/platform/%.o: src/platform/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

_build/obj/platform/win32/%.o: src/platform/win32/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

_build/obj/platform/linux/%.o: src/platform/linux/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ── Static library ──────────────────────────────────────────────────
lib: $(LIBSTATIC)

$(LIBSTATIC): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $(ALL_OBJS)
	@echo "  ✓ static library: $@"

# ── Shared library ──────────────────────────────────────────────────
shared: $(LIBSHARED)

$(LIBSHARED): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(SHARED_FLAG) $(ALL_OBJS) -o $@ $(LDFLAGS)
	@echo "  ✓ shared library: $@"

# ── Tests with sanitizers ──────────────────────────────────────────
SAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer

test: test-smoke test-property

test-smoke: $(TEST_SMOKE_BINS)
	@echo "=== smoke tests ==="
	@passed=0; failed=0; \
	for bin in $(TEST_SMOKE_BINS); do \
		if [ -x "$$bin" ]; then \
			if ./$$bin > /dev/null 2>&1; then \
				echo "  PASS: $$(basename $$bin)"; \
				passed=$$((passed+1)); \
			else \
				echo "  FAIL: $$(basename $$bin)"; \
				failed=$$((failed+1)); \
			fi; \
		fi; \
	done; \
	echo "  $$((passed+failed)) tests, $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ] || exit 1

test-property: $(TEST_PROP_BINS)
	@echo "=== property tests ==="
	@passed=0; failed=0; \
	for bin in $(TEST_PROP_BINS); do \
		if [ -x "$$bin" ]; then \
			if ./$$bin > /dev/null 2>&1; then \
				echo "  PASS: $$(basename $$bin)"; \
				passed=$$((passed+1)); \
			else \
				echo "  FAIL: $$(basename $$bin)"; \
				failed=$$((failed+1)); \
			fi; \
		fi; \
	done; \
	echo "  $$((passed+failed)) tests, $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ] || exit 1

# Smoke test binaries
_build/test/smoke/%$(EXE_EXT): test/smoke/%.c $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $< $(ALL_OBJS) -o $@ $(LDFLAGS)

# Property test binaries
_build/test/property/%$(EXE_EXT): test/property/%.c $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $< $(ALL_OBJS) -o $@ $(LDFLAGS)

# ── Fuzz targets ────────────────────────────────────────────────────
FUZZ_FLAGS := -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer

fuzz: $(TEST_FUZZ_BINS)
	@echo "=== fuzz targets ==="
	@for bin in $(TEST_FUZZ_BINS); do \
		if [ -x "$$bin" ]; then \
			echo "  built: $$(basename $$bin)"; \
		fi; \
	done

_build/test/fuzz/%$(EXE_EXT): test/fuzz/%.c $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(FUZZ_FLAGS) $< $(ALL_OBJS) -o $@ $(LDFLAGS)

# ── Stress / thread-sanitizer tests ─────────────────────────────────
TSAN_FLAGS := -fsanitize=thread -g

stress: $(TEST_STRESS_BINS)
	@echo "=== stress tests (TSan) ==="
	@for bin in $(TEST_STRESS_BINS); do \
		if [ -x "$$bin" ]; then \
			echo "  running: $$(basename $$bin)..."; \
			./$$bin > /dev/null 2>&1 && echo "    PASS" || echo "    FAIL"; \
		fi; \
	done

_build/test/stress/%$(EXE_EXT): test/stress/%.c $(ALL_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(TSAN_FLAGS) $< $(ALL_OBJS) -o $@ $(LDFLAGS)

# ── Clean ───────────────────────────────────────────────────────────
clean:
	rm -rf _build
	@echo "  ✓ cleaned"

# ── Help ────────────────────────────────────────────────────────────
help:
	@echo "Kain Native Runtime Makefile"
	@echo ""
	@echo "  make          compile all .o files"
	@echo "  make lib      build static library"
	@echo "  make shared   build shared library (.so/.dylib/.dll)"
	@echo "  make test     run smoke + property tests (ASan+UBSan)"
	@echo "  make fuzz     build libFuzzer harnesses"
	@echo "  make stress   build + run TSan stress tests"
	@echo "  make clean    remove build artifacts"
	@echo ""
	@echo "  Platform: $(PLATFORM)"
	@echo "  Compiler: $(CC)"
	@echo "  Sources:  $(words $(CORE_SRCS)) core + $(words $(PLATFORM_SRCS)) platform"

.PHONY: all lib shared test test-smoke test-property fuzz stress clean help
