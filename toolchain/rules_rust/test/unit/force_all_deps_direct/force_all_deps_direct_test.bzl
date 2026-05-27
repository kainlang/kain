"""Unittest to verify that we can treat all dependencies as direct dependencies"""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_library", "rust_test")
load(
    "//test/unit:common.bzl",
    "assert_action_mnemonic",
    "assert_argv_contains_prefix",
    "assert_argv_contains_prefix_not",
)
load("//test/unit/force_all_deps_direct:generator.bzl", "generator")

def _get_toolchain(ctx):
    return ctx.attr._toolchain[platform_common.ToolchainInfo]

def _force_all_deps_direct_rustc_flags_test(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = None
    for candidate in tut.actions:
        if candidate.mnemonic == "Rustc":
            action = candidate
            break
    toolchain = _get_toolchain(ctx)
    assert_action_mnemonic(env, action, "Rustc")
    assert_argv_contains_prefix(
        env,
        action,
        "--extern=direct=",
    )
    assert_argv_contains_prefix_not(
        env,
        action,
        "--extern=transitive=",
    )
    if toolchain.target_os == "windows":
        dependency_search_paths = [arg for arg in action.argv if arg.startswith("-Ldependency=")]
        compact_paths = [arg for arg in dependency_search_paths if "_compact_dependency_search" in arg]
        asserts.equals(env, 1, len(dependency_search_paths))
        asserts.equals(env, 1, len(compact_paths))
    return analysistest.end(env)

force_all_deps_direct_test = analysistest.make(
    _force_all_deps_direct_rustc_flags_test,
    attrs = {
        "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
    },
)

def _force_all_deps_direct_rust_test_compaction_test(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    action = None
    for candidate in tut.actions:
        if candidate.mnemonic == "Rustc":
            action = candidate
            break
    toolchain = _get_toolchain(ctx)
    assert_action_mnemonic(env, action, "Rustc")
    assert_argv_contains_prefix(env, action, "--extern=")
    if toolchain.target_os == "windows":
        dependency_search_paths = [arg for arg in action.argv if arg.startswith("-Ldependency=")]
        compact_paths = [arg for arg in dependency_search_paths if "_compact_dependency_search" in arg]
        asserts.equals(env, 1, len(dependency_search_paths))
        asserts.equals(env, 1, len(compact_paths))
    return analysistest.end(env)

force_all_deps_direct_rust_test_compaction_test = analysistest.make(
    _force_all_deps_direct_rust_test_compaction_test,
    attrs = {
        "_toolchain": attr.label(default = Label("//rust/toolchain:current_rust_toolchain")),
    },
)

def _force_all_deps_direct_test():
    rust_library(
        name = "direct",
        srcs = ["direct.rs"],
        edition = "2018",
        deps = [":transitive"],
    )

    rust_library(
        name = "transitive",
        srcs = ["transitive.rs"],
        edition = "2018",
    )

    generator(
        name = "generate",
        deps = [":direct"],
        tags = [
            "no-clippy",
            "no-unpretty",
        ],
    )

    force_all_deps_direct_test(
        name = "force_all_deps_direct_rustc_flags_test",
        target_under_test = ":generate",
    )

    rust_test(
        name = "direct_unit_test",
        crate = ":direct",
        edition = "2018",
        force_all_deps_direct = True,
    )

    force_all_deps_direct_rust_test_compaction_test(
        name = "force_all_deps_direct_rust_test_flags_test",
        target_under_test = ":direct_unit_test",
    )

def force_all_deps_direct_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _force_all_deps_direct_test()

    native.test_suite(
        name = name,
        tests = [
            ":force_all_deps_direct_rustc_flags_test",
            ":force_all_deps_direct_rust_test_flags_test",
        ],
    )
