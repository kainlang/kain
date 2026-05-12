@echo off
setlocal
pushd "%~dp0"
cargo test -p kain-c-ffi --lib tests::c_ffi_can_mutate_shared_images_and_roundtrip_opaque_handles -- --exact
set "EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %EXIT_CODE%
