# Quickstart

This is the shortest path from a fresh checkout to a working Kain loop.

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

`kain doctor` is the first truth check. It tells you which binary you are
running, which targets are available, and where the runtime/toolchain pieces
resolved from.

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

## 3. Build An Artifact

```bash
kain build hello.kn -t rust -o generated/hello
```

That path uses the language frontend, typechecker, and the selected backend
pipeline. The exact output shape depends on the target.

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

## 6. What To Read Next

- `language-overview.md` for the mental model.
- `syntax-and-semantics/expressions.md` for actual language forms.
- `cli/cli-overview.md` for every command family.
- `runtime/runtime-model.md` for how authored code executes.
