# Fast Build & Install Guide

## TL;DR - Fastest Options

### Option 1: Use `cb` (Cargo Build shortcut)
```bash
cb              # Builds release + auto-installs
```

### Option 2: Use existing build script
```bash
build           # Builds release + auto-installs (same as build.bat)
```

### Option 3: Use cargo wrapper
```bash
.\cargo-build-install.ps1 --release
```

## All Build Commands

| Command | What it does |
|---------|-------------|
| `cb` or `cb.bat` | Build release + auto-install (FASTEST) |
| `build` or `build.bat` | Build release + auto-install |
| `build.ps1` | Build release + auto-install (PowerShell) |
| `cargo build --release` | Build only (no install) |
| `cargo install --path .` | Build + install (Rust standard way) |

## How Auto-Install Works

When you run `cb` or `build`:
1. Compiles `kain.exe` in release mode
2. Automatically copies to `C:\Users\Admin\.cargo\bin\`
3. You can use `kain` from anywhere immediately

## Why Not `cargo build` Directly?

Cargo's `build.rs` runs **during** compilation (before the binary exists), so it can't copy the final binary. The wrapper scripts run **after** compilation completes.

## Recommended Workflow

```bash
# Make changes to code
# Then just run:
cb

# That's it! kain-pro is now updated globally
```

## PowerShell Alias (Optional)

Add to your PowerShell profile (`$PROFILE`):
```powershell
function cb { & "D:\Kain\kain-private\cb.bat" }
```

Then you can just type `cb` from anywhere!
