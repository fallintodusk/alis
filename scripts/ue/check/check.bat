@echo off
REM Legacy compatibility router. Prefer explicit validate_*.bat entry points.
REM Usage: check.bat [--uht|--syntax|--blueprints|--assets|--all]

setlocal

set SCRIPT_DIR=%~dp0
set CHECK_TYPE=%1
if "%CHECK_TYPE%"=="" set CHECK_TYPE=--all

if "%CHECK_TYPE%"=="--uht" (
    call "%SCRIPT_DIR%validate_uht.bat"
    exit /b %ERRORLEVEL%
)

if "%CHECK_TYPE%"=="--syntax" (
    call "%SCRIPT_DIR%validate_syntax.bat"
    exit /b %ERRORLEVEL%
)

if "%CHECK_TYPE%"=="--blueprints" (
    call "%SCRIPT_DIR%validate_blueprints.bat"
    exit /b %ERRORLEVEL%
)

if "%CHECK_TYPE%"=="--assets" (
    call "%SCRIPT_DIR%validate_assets.bat"
    exit /b %ERRORLEVEL%
)

if "%CHECK_TYPE%"=="--all" (
    call "%SCRIPT_DIR%validate_all.bat"
    exit /b %ERRORLEVEL%
)

echo Usage: check.bat [--uht^|--syntax^|--blueprints^|--assets^|--all]
exit /b 1
