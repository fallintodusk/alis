@echo off
REM Direct Windows wrapper for Unreal Build Tool
REM Usage: build.bat [target] [platform] [config] [extra_args...]
REM Examples:
REM   build.bat AlisEditor Win64 Development
REM   build.bat AlisEditor Win64 Development -Module=ProjectBoot

setlocal

REM Resolve UE_PATH (SOT: resolve_ue_path.bat)
call "%~dp0..\..\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1

REM Project file
pushd "%~dp0..\..\.."
set PROJECT_ROOT=%CD%
popd
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject

REM Parse arguments
set TARGET=%1
set PLATFORM=%2
set CONFIG=%3
if "%TARGET%"=="" set TARGET=AlisEditor
if "%PLATFORM%"=="" set PLATFORM=Win64
if "%CONFIG%"=="" set CONFIG=Development

REM Extra arguments (everything after first 3)
set EXTRA_ARGS=%4 %5 %6 %7 %8 %9

echo Building %TARGET% (%PLATFORM% %CONFIG%)...
echo.

REM Materialize project-local UBT config (Saved/ is gitignored and gets cleaned;
REM this restores BuildConfiguration.xml from its committed SOT before UBT reads
REM it). Required to avoid cold-build PCH OOM (C3859/C1076).
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    ". '%~dp0..\..\config\Sync-UBTConfig.ps1'; Sync-UBTConfig -ProjectRoot '%PROJECT_ROOT%'"
if errorlevel 1 (
    echo Sync-UBTConfig failed
    exit /b 1
)

"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" %TARGET% %PLATFORM% %CONFIG% "%PROJECT_FILE%" %EXTRA_ARGS% -NoHotReloadFromIDE

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build completed successfully!
    exit /b 0
) else (
    echo.
    echo Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
