# runtime4kain

Curated vendor bundle for Kain runtime research.

This folder is built from the full upstream clones in the parent directory. It is not meant to mirror upstream repos exactly; it is meant to hold the smallest practical embed-focused slices that Kain would realistically vendor or prototype against.

## Vendors

| Vendor | Upstream Commit | Upstream Repo | Curated Shape | Status |
| --- | --- | --- | --- | --- |
| `quickjs` | `d7ae12a` | `https://github.com/bellard/quickjs.git` | core VM + support sources + libc helper | smoke-compiled |
| `lua` | `c037162a` | `https://github.com/lua/lua.git` | core/library sources, no CLI/test files | smoke-compiled |
| `wren` | `99d2f0b8` | `https://github.com/wren-lang/wren.git` | `src/include`, `src/vm`, `src/optional` | smoke-compiled |
| `mruby` | `48fc4220d` | `https://github.com/mruby/mruby.git` | minimal amalgam output | smoke-compiled |
| `wasm3` | `79d412e` | `https://github.com/wasm3/wasm3.git` | `source/` core interpreter tree | smoke-compiled |
| `wamr` | `389d206` | `https://github.com/bytecodealliance/wasm-micro-runtime.git` | runtime core subtrees + source-selection cmake | staged, not fully smoke-compiled |
| `libuv` | `aabb765` | `https://github.com/libuv/libuv.git` | `include/` + `src/` | staged, not fully smoke-compiled |
| `miniaudio` | `9634bed` | `https://github.com/mackron/miniaudio.git` | single-file embed pair + split variant | smoke-compiled |
| `mimalloc` | `75d69f4` | `https://github.com/microsoft/mimalloc.git` | `include/` + `src/` | smoke-compiled |
| `rpmalloc` | `262c698` | `https://github.com/mjansson/rpmalloc.git` | allocator core folder | smoke-compiled |

## Notes

- `quickjs` needs `CONFIG_VERSION` defined when compiling `quickjs.c` directly.
- `mruby` is curated as amalgam output because that is the most vendor-friendly embeddable shape.
- `wamr` and `libuv` are intentionally kept as larger core subtrees because their source graphs are wider than the single-file or tiny-library vendors.
- The old partial `cpython` fragment was intentionally not kept in this rebuilt bundle.

## Smoke Checks Used

```sh
clang -DCONFIG_VERSION=\"kain-vendor\" -I runtime4kain/quickjs -c runtime4kain/quickjs/quickjs.c
clang -I runtime4kain/wren/src/include -I runtime4kain/wren/src/vm -I runtime4kain/wren/src/optional -c runtime4kain/wren/src/vm/wren_vm.c
clang -I runtime4kain/mruby/amalgam -c runtime4kain/mruby/amalgam/mruby.c
clang -c runtime4kain/miniaudio/miniaudio.c
clang -I runtime4kain/lua -c runtime4kain/lua/lvm.c
clang -I runtime4kain/wasm3/source -c runtime4kain/wasm3/source/m3_env.c
clang -I runtime4kain/mimalloc/include -I runtime4kain/mimalloc/src -c runtime4kain/mimalloc/src/static.c
clang -I runtime4kain/rpmalloc/rpmalloc -c runtime4kain/rpmalloc/rpmalloc/rpmalloc.c
```
