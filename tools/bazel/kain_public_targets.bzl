"""Shared public Rust target surface for the Kain Bazel workspace."""

_RUST_PUBLIC_ALIASES = {
    "kain": "//crates/cli:kain",
    "kn": "//crates/cli:kn",
    "kain_actor": "//crates/actor:kain-actor",
    "kain_blades": "//crates/blades:blade",
    "kain_build": "//crates/build:kain-build",
    "kain_check": "//crates/check:kain-check",
    "kain_codebase": "//crates/codebase:kain-codebase",
    "kain_commands": "//crates/commands:kain-commands",
    "kain_core": "//crates/core:kain-core",
    "kain_entangle": "//crates/entangle:kain-entangle",
    "kain_fs": "//crates/fs:kain-fs",
    "kain_input": "//crates/input:kain-input",
    "kain_net": "//crates/net:kain-net",
    "kain_process": "//crates/process:kain-process",
    "kain_run": "//crates/run:kain-run",
    "kain_stdlib_map": "//crates/stdlib-map:kain-stdlib-map",
    "kain_test": "//crates/test:kain-test",
}

_RUST_PUBLIC_TEST_SUITES = {
    "crate_tests": [
        "//crates/core:unit_test",
        "//crates/build:unit_test",
        "//crates/commands:unit_test",
        "//crates/cli:unit_test",
    ],
    "key_crate_tests": [
        "//crates/actor:unit_test",
        "//crates/blades:unit_test",
        "//crates/build:unit_test",
        "//crates/check:unit_test",
        "//crates/codebase:unit_test",
        "//crates/commands:unit_test",
        "//crates/entangle:unit_test",
        "//crates/fs:unit_test",
        "//crates/input:unit_test",
        "//crates/net:unit_test",
        "//crates/process:unit_test",
        "//crates/run:unit_test",
        "//crates/test:unit_test",
    ],
    "diagnostic_crate_tests": [
        "//crates/core:unit_test",
        "//crates/cli:unit_test",
    ],
}

def _qualify(label, target_prefix):
    if not target_prefix:
        return label
    return target_prefix + label

def declare_kain_public_targets(target_prefix = ""):
    """Emit the shared public Rust alias and test-suite surface.

    Args:
        target_prefix: Optional repository prefix, such as `@kain_workspace_rust`.
    """
    for name, actual in sorted(_RUST_PUBLIC_ALIASES.items()):
        native.alias(
            name = name,
            actual = _qualify(actual, target_prefix),
        )

    for name, tests in sorted(_RUST_PUBLIC_TEST_SUITES.items()):
        native.test_suite(
            name = name,
            tests = [_qualify(test, target_prefix) for test in tests],
        )
