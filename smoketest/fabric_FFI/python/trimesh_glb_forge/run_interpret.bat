@echo off
setlocal
pushd "%~dp0..\..\.."
cargo run -q -p cli -- smoketest/python/trimesh_glb_forge/smoke.kn -t interpret
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
