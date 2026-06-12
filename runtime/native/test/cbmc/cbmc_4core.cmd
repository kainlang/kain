@echo off
REM cbmc_4core.cmd — CPU-limited CBMC wrapper (4 cores, 8 threads)
REM Sets Z3 thread limit for SMT solver + processor affinity for safety

set Z3_NUM_THREADS=4
echo [CPU-LIMITED 4c/8t] %* 1>&2
start "" /affinity FF /wait /b cbmc %*
exit /b %ERRORLEVEL%
