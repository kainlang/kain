"""Bzlmod extension for the generated first-party Kain Rust workspace."""

def _find_root_module(module_ctx):
    for mod in module_ctx.modules:
        if mod.is_root:
            return mod
    fail("unable to locate the root module")

def _resolve_python_command(repository_ctx):
    for candidate in ("python3", "python"):
        resolved = repository_ctx.which(candidate)
        if resolved:
            return [str(resolved)]

    py_launcher = repository_ctx.which("py")
    if py_launcher:
        return [str(py_launcher), "-3"]

    fail("unable to locate a Python interpreter for kain_rust_workspace")

def _kain_rust_workspace_repo_impl(repository_ctx):
    repository_ctx.watch(repository_ctx.attr.workspace_manifest)
    repository_ctx.watch(repository_ctx.attr.cargo_lockfile)
    repository_ctx.watch(repository_ctx.attr.generator)
    repository_ctx.watch(repository_ctx.attr.legacy_generator)

    cargo = repository_ctx.which("cargo")
    if cargo == None:
        fail("unable to locate cargo for kain_rust_workspace generation")

    watch_file = repository_ctx.path("paths-to-track.json")
    source_root = repository_ctx.path(repository_ctx.attr.workspace_manifest).dirname
    command = _resolve_python_command(repository_ctx) + [
        str(repository_ctx.path(repository_ctx.attr.generator)),
        "--source-repo-root",
        str(source_root),
        "--output-root",
        str(repository_ctx.path(".")),
        "--repo-name",
        repository_ctx.name,
        "--cargo-bin",
        str(cargo),
        "--write-watch-file",
        str(watch_file),
    ]

    repository_ctx.report_progress("Generating first-party Kain Rust Bazel workspace")
    result = repository_ctx.execute(command, quiet = repository_ctx.attr.quiet)
    if result.return_code != 0:
        fail("kain_rust_workspace generation failed:\nSTDOUT:\n{}\nSTDERR:\n{}".format(
            result.stdout,
            result.stderr,
        ))

    for path in json.decode(repository_ctx.read(watch_file)):
        repository_ctx.watch(path)

_kain_workspace_tag = tag_class(
    attrs = {
        "name": attr.string(mandatory = True),
        "workspace_manifest": attr.label(mandatory = True),
        "cargo_lockfile": attr.label(mandatory = True),
        "generator": attr.label(mandatory = True),
        "legacy_generator": attr.label(mandatory = True),
        "quiet": attr.bool(default = True),
    },
)

_kain_rust_workspace_repo = repository_rule(
    implementation = _kain_rust_workspace_repo_impl,
    attrs = {
        "workspace_manifest": attr.label(mandatory = True),
        "cargo_lockfile": attr.label(mandatory = True),
        "generator": attr.label(mandatory = True),
        "legacy_generator": attr.label(mandatory = True),
        "quiet": attr.bool(default = True),
    },
)

def _kain_rust_impl(module_ctx):
    root = _find_root_module(module_ctx)
    repos = []
    for cfg in root.tags.workspace:
        _kain_rust_workspace_repo(
            name = cfg.name,
            workspace_manifest = cfg.workspace_manifest,
            cargo_lockfile = cfg.cargo_lockfile,
            generator = cfg.generator,
            legacy_generator = cfg.legacy_generator,
            quiet = cfg.quiet,
        )
        repos.append(cfg.name)

    return module_ctx.extension_metadata(
        root_module_direct_deps = repos,
        root_module_direct_dev_deps = [],
    )

kain_rust = module_extension(
    implementation = _kain_rust_impl,
    tag_classes = {
        "workspace": _kain_workspace_tag,
    },
)
