load("@rules_cc//cc:defs.bzl", "cc_library")

WINDOWS_RUNTIME_DEFINES = [
    "WIN32",
    "_WINDOWS",
]

WINDOWS_COPTS = [
    "/W3",
    "/std:c11",
    "/experimental:c11atomics",
    "/Gy",  # Enable function-level linking for linker GC
]

CLANG_CL_WINDOWS_COPTS = [
    "/W3",
    "/std:c11",
    "/Gy",  # Function-level linking (clang-cl compatible)
]

WINDOWS_CPP_COPTS = WINDOWS_COPTS + [
    "/std:c++20",
]

CLANG_CL_WINDOWS_CPP_COPTS = CLANG_CL_WINDOWS_COPTS + [
    "/std:c++20",
]

POSIX_C_COPTS = [
    "-Wall",
    "-Wextra",
    "-std=c11",
    "-ffunction-sections",
    "-fdata-sections",
]

POSIX_CPP_COPTS = [
    "-Wall",
    "-Wextra",
    "-std=c++20",
    "-ffunction-sections",
    "-fdata-sections",
]

RUNTIME_PRIVATE_HEADERS = [
    "native/src/**/*.h",
    # Runtime seam fragments are textually included from the owning C lane.
    "native/src/core/python_runtime_*.c",
    "third_party/**/*.h",
    "third_party/**/*.hpp",
]


def platform_select(windows = [], linux = [], macos = [], default = []):
    return select({
        ":windows": windows,
        ":linux": linux,
        ":macos": macos,
        "//conditions:default": default,
    })


def _runtime_headers(manifest):
    return native.glob(
        manifest["header_globs"] + RUNTIME_PRIVATE_HEADERS,
        exclude = ["3rdparty/skia-core/**"],
        allow_empty = True,
    )


def _runtime_defines(manifest):
    return manifest["common_defines"] + platform_select(
        windows = WINDOWS_RUNTIME_DEFINES + manifest["windows_defines"],
        linux = manifest["linux_defines"],
        macos = manifest["macos_defines"],
    )


def _runtime_linkopts(manifest):
    return platform_select(
        windows = manifest["windows_linkopts"],
        linux = manifest["linux_linkopts"],
        macos = manifest["macos_linkopts"],
    )


def _runtime_c_srcs(manifest):
    return manifest["common_c_srcs"] + platform_select(
        windows = manifest["windows_c_srcs"],
        linux = manifest["linux_c_srcs"],
        macos = manifest["macos_c_srcs"],
    )


def _runtime_cpp_srcs(manifest):
    return manifest["common_cpp_srcs"] + platform_select(
        windows = manifest["windows_cpp_srcs"],
        linux = manifest["linux_cpp_srcs"],
        macos = manifest["macos_cpp_srcs"],
    )


def _windows_copts(base_copts, clang_cl_copts):
    """Returns platform+compiler-conditional copts for Windows."""
    return select({
        ":windows": select({
            ":clang_cl_compiler": clang_cl_copts,
            "//conditions:default": base_copts,
        }),
        ":linux": base_copts,
        ":macos": base_copts,
        "//conditions:default": base_copts,
    })


def declare_runtime_bundle(name, manifest, target_compatible_with = []):
    runtime_headers = _runtime_headers(manifest)

    cc_library(
        name = name + "_c",
        srcs = _runtime_c_srcs(manifest),
        hdrs = runtime_headers,
        copts = _windows_copts(WINDOWS_COPTS, CLANG_CL_WINDOWS_COPTS),
        defines = _runtime_defines(manifest),
        includes = manifest["includes"],
        alwayslink = True,
        target_compatible_with = target_compatible_with,
        visibility = ["//visibility:private"],
    )

    runtime_bundle_deps = [":" + name + "_c"]

    if manifest["has_cpp_sources"]:
        cc_library(
            name = name + "_cpp",
            srcs = _runtime_cpp_srcs(manifest),
            hdrs = runtime_headers,
            copts = _windows_copts(WINDOWS_CPP_COPTS, CLANG_CL_WINDOWS_CPP_COPTS),
            defines = _runtime_defines(manifest),
            includes = manifest["includes"],
            alwayslink = True,
            target_compatible_with = target_compatible_with,
            visibility = ["//visibility:private"],
        )
        runtime_bundle_deps.append(":" + name + "_cpp")

    cc_library(
        name = name,
        deps = runtime_bundle_deps,
        linkopts = _runtime_linkopts(manifest),
        target_compatible_with = target_compatible_with,
    )
