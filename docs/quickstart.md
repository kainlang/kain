# Quickstart

This is the shortest path from a fresh checkout to a working Kain loop. It is a first-run path, not a full manual, so the goal is to verify the toolchain, run a file, inspect the output, and then hand off to the deeper guide tree.

## 1. Bootstrap The Toolchain

```bash
python3 install_kain.py
source generated/kain-env.sh
kain doctor
```

On Windows:

```powershell
py install_kain.py
. .\generated\kain-env.ps1
kain doctor
```

`kain doctor` is the first truth check. It tells you which binary you are running, which targets are available, and where the runtime and toolchain pieces resolved from. If that disagrees with an older README, trust the live binary and the current `guides/` tree.

## 2. Run A File

Create a file like `hello.kn`:

```kain
fn main() -> Int:
    println("hello from kain")
    return 0
```

Run it:

```bash
kain run hello.kn
```

Or, if you are using the `kn` launcher, the run-first default is even shorter:

```bash
kn hello.kn
```

If the file does not run, jump to `guides/reference/troubleshooting.md` first. That page is the current operator path for target mismatches, missing project roots, and importer surprises.

## 3. Build An Artifact

```bash
kain build hello.kn -t rust -o generated/hello
```

That path uses the language frontend, typechecker, and the selected backend pipeline. The exact output shape depends on the target. For target aliases and output families, use `guides/reference/target-matrix.md` after this quickstart.

## 4. Try The Native UI Lane

```bash
kain build native-ui path/to/app.kn --app-name DemoApp --window-title "Demo App"
```

This materializes a desktop app bundle instead of just emitting source text.

## 5. Import Existing Code

```bash
kain import-c path/to/c/source --output generated/from-c.kn
kain import-rust path/to/rust/source --output generated/from-rust.kn
kain import-crate my_crate --manifest-path Cargo.toml --output generated/crate
```

These importers are part of the language surface, not a side tool.

## 6. Use The Example Lanes

- [docs/examples/README.md](/home/ephemara/Dev/Kain/docs/examples/README.md) for the runnable example ladder and validator.
- [docs/examples/11_ultimate_kain_pipeline.kn](/home/ephemara/Dev/Kain/docs/examples/11_ultimate_kain_pipeline.kn) for the capstone local pipeline.
- [docs/examples/09_ue5_authoring_gallery.kn](/home/ephemara/Dev/Kain/docs/examples/09_ue5_authoring_gallery.kn) for the current UE5-authored surface that proves on this checkout.
- [docs/kn_library/README.md](/home/ephemara/Dev/Kain/docs/kn_library/README.md) for corpus-style language mining after you understand the validated examples.

## 7. What To Read Next

1. [guides/reference/legacy-crosswalk.md](/home/ephemara/Dev/Kain/guides/reference/legacy-crosswalk.md) if you are translating from older docs or repo lore.
2. [guides/language-overview.md](/home/ephemara/Dev/Kain/guides/language-overview.md) for the mental model.
3. [guides/syntax-and-semantics/expressions.md](/home/ephemara/Dev/Kain/guides/syntax-and-semantics/expressions.md) for actual language forms.
4. [guides/cli/cli-overview.md](/home/ephemara/Dev/Kain/guides/cli/cli-overview.md) for every command family.
5. [guides/runtime/runtime-model.md](/home/ephemara/Dev/Kain/guides/runtime/runtime-model.md) for how authored code executes.

## Practical Rule

If a feature matters enough to document, it should usually also have a smoke lane, a workflow example, or a current CLI path that proves it works.
