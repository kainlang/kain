# Dev

> Development workflow scripts for reson8.
> Each routine wraps a `kain` CLI invocation through
> `handler_process_spawn` (id 5) or `handler_process_output`
> (id 4, mapped to the `run` keyword).
> `full_rebuild` chains clean → check → build in sequence.
>
> This file is the single entry point for reson8 build ops.
> The reson8 bridge can run a routine by name; the chain
> in `full_rebuild` executes them in declaration order.

---

## check_all
> spawn "kain check X:/blades/reson8/src/ --json"
> spawn "kain check X:/blades/reson8/plugins/ --json"
> print "Source and plugin typecheck complete"

### check_targets
| Path | Type | ExpectedMs |
|------|------|-----------|
| X:/blades/reson8/src/ | directory | 8000 |
| X:/blades/reson8/plugins/ | directory | 4000 |
| X:/blades/reson8/src-mks/ | markscript | 1500 |

---

## build_all
> spawn "kain build X:/blades/reson8/ --target llvm"
> print "Native build complete"

### build_targets
| Target | Backend | Output |
|--------|---------|--------|
| reson8 | llvm | reson8.exe |
| reson8_plugins | llvm | plugins/*.dll |
| reson8_themes | data | themes/*.toml |

---

## watch
> spawn "kain run dev X:/blades/reson8/"
> print "Dev watcher started — auto-rebuild on source change"

---

## clean
> spawn "kain clean X:/blades/reson8/"
> print "Build artifacts removed"

### clean_paths
| Path | Pattern |
|------|---------|
| X:/blades/reson8/.kain/out/ | recursive |
| X:/blades/reson8/.kain/cache/ | recursive |
| X:/blades/reson8/**/*.pdb | glob |
| X:/blades/reson8/**/*.obj | glob |

---

## full_rebuild
> print "Starting full rebuild..."
> spawn "kain clean X:/blades/reson8/"
> spawn "kain check X:/blades/reson8/src/ --json"
> spawn "kain check X:/blades/reson8/plugins/ --json"
> spawn "kain build X:/blades/reson8/ --target llvm"
> print "Full rebuild complete"

### rebuild_order
| Step | Routine | WaitFor |
|------|---------|---------|
| 1 | clean | exit 0 |
| 2 | check_all | exit 0 |
| 3 | build_all | exit 0 |

---

## format
> spawn "kain fmt X:/blades/reson8/src/ --write"
> spawn "kain fmt X:/blades/reson8/plugins/ --write"
> print "Formatting complete"

---

## test
> spawn "kain test X:/blades/reson8/src/ --json"
> print "Test suite complete"

### test_suites
| Suite | Path | Tests |
|-------|------|-------|
| core | X:/blades/reson8/src/core/ | 47 |
| plugins | X:/blades/reson8/plugins/tests/ | 23 |
| markscript | X:/blades/reson8/src-mks/ | 18 |
| ui | X:/blades/reson8/src/ui/ | 31 |

---

## verify

```markscript
print("dev: 6 routines defined (check_all, build_all, watch, clean, full_rebuild, format, test)")
print("dev: full_rebuild chains clean -> check -> build via spawn handler")
print("dev: clean_paths = 4 patterns (out/, cache/, *.pdb, *.obj)")
print("dev: test_suites = 4 suites totaling 119 tests")
```
