@echo off
REM ============================================================================
REM Kill All Test Processes - Clean up UE and Python processes
REM ============================================================================
REM Usage: scripts\test\kill_all_processes.bat
REM ============================================================================

echo ============================================================================
echo   Killing All Test Processes
echo ============================================================================
echo.

echo [1/5] Killing UnrealEditor processes...
taskkill /F /IM UnrealEditor.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo   ✓ UnrealEditor.exe killed
) else (
    echo   - No UnrealEditor.exe processes found
)

echo.
echo [2/5] Killing UnrealEditor-Cmd processes...
taskkill /F /IM UnrealEditor-Cmd.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo   ✓ UnrealEditor-Cmd.exe killed
) else (
    echo   - No UnrealEditor-Cmd.exe processes found
)

echo.
echo [3/5] Killing CrashReportClient processes...
taskkill /F /IM CrashReportClient.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo   ✓ CrashReportClient.exe killed
) else (
    echo   - No CrashReportClient.exe processes found
)

echo.
echo [4/5] Killing Python processes...
taskkill /F /IM python.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo   ✓ python.exe killed
) else (
    echo   - No python.exe processes found
)

echo.
echo [5/5] Waiting for cleanup...
timeout /t 2 /nobreak >nul
echo   ✓ Cleanup complete

echo.
echo ============================================================================
echo   All processes cleaned
echo ============================================================================
echo.
