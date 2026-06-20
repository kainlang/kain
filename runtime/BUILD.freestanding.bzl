# Freestanding core runtime build definition.
#
# Compiles the bare-metal subset of the Kain native C runtime with
# -ffreestanding -nostdlib. Produces libkain_runtime_core.a (Linux/macOS)
# or kain_runtime_core.lib (Windows).
#
# Target: native_core_freestanding
#
# Load into runtime/BUILD.bazel:
#   load(":BUILD.freestanding.bzl", "declare_freestanding_core_runtime")
#   declare_freestanding_core_runtime()

load("@rules_cc//cc:defs.bzl", "cc_library")

# ── Core freestanding sources (18 files) ──────────────────────────────
# The minimal set of runtime files that compile without an OS or libc.
# See native_core_freestanding.toml for the rationale per file.
FREESTANDING_CORE_SRCS = [
    # Memory & layout
    "native/src/core/arena.c",
    "native/src/core/buddy.c",
    "native/src/core/bitfield.c",
    "native/src/core/union.c",
    "native/src/core/deferred_free.c",
    "native/src/core/handle.c",
    "native/src/core/fixup.c",

    # Compiler semantic runtime
    "native/src/core/entangle.c",
    "native/src/core/wire.c",
    "native/src/core/event.c",
    "native/src/core/batch_queue.c",

    # Machine stones (core subset)
    "native/src/core/ownership.c",
    "native/src/core/converge.c",
    "native/src/core/profile.c",

    # Infrastructure
    "native/src/core/version.c",
    "native/src/core/services.c",
    "native/src/core/crash_handler.c",

    # Freestanding stubs
    "native/src/core/freestanding_stubs.c",
]

# ── Freestanding compile options ──────────────────────────────────────
# -ffreestanding: no libc headers, no hosted environment assumptions
# -nostdlib:      no standard library linked
# -fno-stack-protector: no stack canaries (kernel provides own)
# -mno-red-zone:  required for kernel code on x86_64
FREESTANDING_COPTS = [
    "-ffreestanding",
    "-nostdlib",
    "-fno-stack-protector",
    "-mno-red-zone",
]

FREESTANDING_DEFINES = [
    "KAIN_FREESTANDING",
]

def declare_freestanding_core_runtime():
    """Declare the freestanding core runtime cc_library target.

    Produces `//runtime:native_core_freestanding` — a static library
    suitable for linking into bare-metal kernels and freestanding
    executables built with `-target x86_64-unknown-none`.

    No platform libraries, no OS headers, no libc.
    """

    cc_library(
        name = "native_core_freestanding",
        srcs = FREESTANDING_CORE_SRCS,
        hdrs = native.glob(
            [
                "native/include/**/*.h",
                "native/include/*.h",
                "native/src/core/*.h",
            ],
            allow_empty = True,
        ),
        copts = FREESTANDING_COPTS,
        defines = FREESTANDING_DEFINES,
        includes = ["native/include"],
        alwayslink = True,
        # No linkopts — bare metal doesn't link against platform libraries.
        visibility = ["//visibility:public"],
    )
