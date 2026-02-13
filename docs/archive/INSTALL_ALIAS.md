# How to Make `cb` Work From Anywhere

## Option 1: Add to PATH (Recommended)

1. Add this directory to your PATH:
   ```
   D:\Kain\kain-private
   ```

2. Then you can run `cb` from anywhere:
   ```bash
   cd anywhere
   cb  # Builds and installs kain-pro!
   ```

## Option 2: PowerShell Alias

Add to your PowerShell profile:

```powershell
# Open profile
notepad $PROFILE

# Add this line:
function cb { & "D:\Kain\kain-private\cb.bat" }

# Save and reload
. $PROFILE
```

## Option 3: Doskey Macro (CMD)

Add to your CMD startup script:

```batch
doskey cb=D:\Kain\kain-private\cb.bat
```

## Usage

Once set up, just run:
```bash
cb
```

This will:
- Build kain-pro in release mode
- Auto-install to cargo bin
- Make it available globally

## Current Working Commands

From the `kain-private` directory:

| Command | Description |
|---------|-------------|
| `cb` | Build release + auto-install |
| `cb.bat` | Same as above |
| `build` | Build release + auto-install |
| `build.bat` | Same as above |
| `build.ps1` | PowerShell version |
| `.\cargo-build-install.ps1 --release` | Direct wrapper |

All of these do the same thing: **build and auto-install**.
