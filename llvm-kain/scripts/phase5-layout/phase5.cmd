@echo off
title Phase 5: LLVM Flat Layout Restructure

echo ======================================
echo Phase 5: LLVM Flat Layout Restructure
echo ======================================
echo.
echo SAFETY LAYER 1: Backup + Dry-run + Reverse (protect the real tree)
echo SAFETY LAYER 2: Sandbox (prove it works before touching real tree)
echo.
echo Available commands:
echo.
echo   python execute-phase5.py --help              Show full help
echo.
echo === SANDBOX MODE (safe testing ===
echo   python execute-phase5.py --sandbox-prep      Copy tree to sandbox
echo   python execute-phase5.py --sandbox-apply     Apply layout in sandbox only
echo   python execute-phase5.py --sandbox-build     Build sandbox, find errors
echo   python execute-phase5.py --sandbox-report    View sandbox results
echo   python execute-phase5.py --sandbox-cleanup   Delete sandbox
echo.
echo === REAL EXECUTION (requires sandbox pass) ===
echo   python execute-phase5.py --backup            Create backup first
echo   python execute-phase5.py --dry-run           Preview changes (default)
echo   python execute-phase5.py --execute           Execute layout restructure
echo   python execute-phase5.py --reverse           Undo everything
echo   python execute-phase5.py --status            Current state
echo.
echo RECOMMENDED WORKFLOW:
echo   1. python execute-phase5.py --sandbox-prep
echo   2. python execute-phase5.py --sandbox-apply
echo   3. python execute-phase5.py --sandbox-build
echo   4. python execute-phase5.py --sandbox-report
echo   5. python execute-phase5.py --backup
echo   6. python execute-phase5.py --execute
echo.
echo NOTE: Always run --backup first before --execute!
echo       Always prove with sandbox before touching real tree!
echo.
pause
