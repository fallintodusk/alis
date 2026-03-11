@echo off
REM ============================================================================
REM Kill UE Processes - Clean up Unreal Engine processes
REM ============================================================================
REM Usage: scripts\test\kill_ue_processes.bat
REM ============================================================================

echo ============================================================================
echo   Killing Unreal Engine Processes
echo ============================================================================
echo.

echo [1/3] Killing UnrealEditor processes...
taskkill /F /IM UnrealEditor.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo   ✓ UnrealEditor.exe killed
) else (
    echo   - No UnrealEditor.exe processes found
)

echo.
echo [2/3] Killing UnrealEditor-Cmd processes...
taskkill /F /IM UnrealEditor-Cmd.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo   ✓ UnrealEditor-Cmd.exe killed
) else (
    echo   - No UnrealEditor-Cmd.exe processes found
)

echo.
echo [3/3] Killing CrashReportClient processes...
taskkill /F /IM CrashReportClient.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo   ✓ CrashReportClient.exe killed
) else (
    echo   - No CrashReportClient.exe processes found
)

echo.
echo [Cleanup] Waiting for cleanup...
timeout /t 2 /nobreak >nul
echo   ✓ Cleanup complete

echo.
echo ============================================================================
echo   All UE processes cleaned
echo ============================================================================
echo.
