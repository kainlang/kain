@echo off
REM `build` is the long-form alias for the release/install wrapper.
powershell -ExecutionPolicy Bypass -File "%~dp0cargo-build-install.ps1" %*
