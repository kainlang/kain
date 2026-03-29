@echo off
echo Cleaning up generated folders...
if exist "Source" rd /s /q "Source"
if exist "Shaders" rd /s /q "Shaders"
echo Running KAIN build...
kain build --ue5
echo Done.
