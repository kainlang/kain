# KAIN-PRO Build System

## Quick Build & Install

### Option 1: PowerShell Script (Recommended)
```powershell
.\build.ps1
```
This will:
- ✅ Build in release mode
- ✅ Auto-install to `~/.cargo/bin`
- ✅ Ready to use `kain-pro` from anywhere!

### Option 2: Batch File (Windows)
```batch
build.bat
```
Same as above, just calls the PowerShell script.

### Option 3: Manual
```bash
# Build
cargo build --release

# Install manually
copy target\release\kain-pro.exe %USERPROFILE%\.cargo\bin\
```

## Build Options

### Debug Build
```powershell
.\build.ps1 -Debug
```
Builds in debug mode (faster compilation, no optimization).

### Build Without Installing
```powershell
.\build.ps1 -NoInstall
```
Builds but doesn't copy to cargo bin.

### Standard Cargo Commands
```bash
cargo build              # Debug build
cargo build --release    # Release build (manual install needed)
cargo test              # Run tests
cargo run -- [args]     # Run directly
```

## Quick Install Only
If you already built and just want to install:
```powershell
.\install-local.ps1
```

## Aliases

Added to `.cargo/config.toml`:
```bash
cargo br    # Shortcut for 'cargo build --release'
```

## Workflow

**Development:**
```powershell
.\build.ps1              # Build & install
kain-pro --version       # Test it works
```

**Testing Changes:**
```powershell
.\build.ps1              # Rebuild & reinstall
cd UE5/KainPluginFactory/Plugins/MyPlugin
kain-pro build --ue5     # Use new version immediately
```

**No more copy-paste!** 🎉
