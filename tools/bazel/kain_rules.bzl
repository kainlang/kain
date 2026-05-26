"""Repo-local Bazel helpers for Kain-authored source artifacts."""

def _stdlib_map_args(native_manifests):
    return " ".join([
        "--native-manifest $(location %s)" % manifest
        for manifest in native_manifests
    ])

def kain_stdlib_map(
        name,
        srcs,
        native_manifests,
        tool = "//crates/stdlib-map:kain_stdlib_map_tool"):
    """Generate the stdlib atlas under bazel-bin."""
    json_out = name + ".json"
    llm_out = name + ".llm.md"
    manifest_args = _stdlib_map_args(native_manifests)
    command = (
        "$(location {tool}) --repo-root . --stdlib-root stdlib {manifest_args} " +
        "--write --json-out $(@D)/{json_out} --llm-out $(@D)/{llm_out}"
    ).format(
        tool = tool,
        manifest_args = manifest_args,
        json_out = json_out,
        llm_out = llm_out,
    )
    native.genrule(
        name = name,
        srcs = srcs + native_manifests,
        outs = [
            json_out,
            llm_out,
        ],
        tools = [tool],
        cmd = command,
        cmd_bat = command,
    )

def kain_stdlib_map_check(
        name,
        srcs,
        native_manifests,
        checked_json = "stdlib.map.json",
        checked_llm = "STDLIB_MAP.llm.md",
        tool = "//crates/stdlib-map:kain_stdlib_map_tool"):
    """Fail the build when checked-in stdlib atlas files drift."""
    manifest_args = _stdlib_map_args(native_manifests)
    command = (
        "$(location {tool}) --repo-root . --stdlib-root stdlib {manifest_args} " +
        "--check --json-out stdlib/{checked_json} --llm-out stdlib/{checked_llm}"
    ).format(
        tool = tool,
        manifest_args = manifest_args,
        checked_json = checked_json,
        checked_llm = checked_llm,
    )
    native.genrule(
        name = name,
        srcs = srcs + native_manifests + [
            checked_json,
            checked_llm,
        ],
        outs = [name + ".stamp"],
        tools = [tool],
        cmd = command + " && echo ok > $@",
        cmd_bat = command + " && echo ok > $@",
    )
