# ============================================================================
#  Makefile — Kaintana Test Runner
#
#  Builds test_runner.exe by linking against all Kaintana C substrate files
#  plus the null backend. Uses gcc with strict C11 flags.
#
#  Targets:
#    make              Build test_runner.exe
#    make test         Build and run tests via pytest
#    make clean        Remove build artifacts
#    make list         List all test cases from core.tsv
# ============================================================================

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -pedantic -Werror -Wno-unused-function -g
INCLUDES = -I../../../include -I.. -I.

# Kaintana C substrate sources (all 8 .c files)
SRC_CORE = \
    ../tree.c \
    ../box_math.c \
    ../damage.c \
    ../draw_pixels.c \
    ../arena.c \
    ../hash_table.c \
    ../attr_table.c \
    ../color.c

# Backend sources
SRC_BACKEND = \
    ../backends/null/host_null.c

# Test runner
TEST_RUNNER_SRC = test_runner.c
TEST_RUNNER_BIN = test_runner.exe

# Native runtime core (provides arena, component_surface, handle, input_system)
SRC_RUNTIME = \
    ../../core/arena.c \
    ../../core/version.c \
    ../../core/component_surface.c \
    ../../core/handle.c \
    ../../core/input_system.c

# Runtime stubs (string_new, etc.)
SRC_STUBS = \
    ../kaintana_runtime_stubs.c

# All sources combined
ALL_SRCS = $(TEST_RUNNER_SRC) $(SRC_CORE) $(SRC_BACKEND) $(SRC_RUNTIME) $(SRC_STUBS)

.PHONY: all test clean list

all: $(TEST_RUNNER_BIN)

$(TEST_RUNNER_BIN): $(ALL_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(ALL_SRCS) -o $@

test: $(TEST_RUNNER_BIN)
	python -m pytest test_from_specs.py -v

list: $(TEST_RUNNER_BIN)
	./$(TEST_RUNNER_BIN) specs/core.tsv --list

clean:
	rm -f $(TEST_RUNNER_BIN)
	rm -rf __pycache__ .pytest_cache
	find . -name '*.pyc' -delete
