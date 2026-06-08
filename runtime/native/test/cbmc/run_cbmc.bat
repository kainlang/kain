@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
cbmc --unwind 8 --no-unwinding-assertions --object-bits 10 --trace test/cbmc/combined_check_batch_queue.c
