# Packaging — Kain Distribution Builder

One Python script to build distributable packages for all platforms.

## Files

| File | What | Ships inside distro? |
|------|------|---------------------|
| `build_package.py` | Builds .zip (Windows) and .tar.gz (Linux/macOS) from Bazel artifacts | ❌ |
| `setup.py` | User-facing setup script — adds Kain to PATH, sets KAIN_HOME | ✅ Yes |

## Usage

```bash
# Build a distribution zip
python packaging/build_package.py

# Build for a specific platform (cross-compile packaging)
python packaging/build_package.py --platform linux

# Custom version
python packaging/build_package.py --version 0.9.0

# Stage files without archiving (inspect what goes in)
python packaging/build_package.py --stage-only
```

## What the User Does

```bash
# Extract the zip anywhere
unzip kain-0.8.0-windows-x64.zip -d D:\tools\kain

# Run setup.py to add to PATH
cd D:\tools\kain
python setup.py           # adds to user PATH, sets KAIN_HOME
python setup.py --info   # show version, files
python setup.py --uninstall  # remove from environment
```
