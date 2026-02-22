@echo off
powershell -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" %*
