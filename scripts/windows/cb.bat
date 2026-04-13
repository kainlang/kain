@echo off
REM Quick alias for cargo build --release with auto-install
powershell -ExecutionPolicy Bypass -File "%~dp0cargo-build-install.ps1" --release
