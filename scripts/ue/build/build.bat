@echo off
REM Direct Windows wrapper for Unreal Build Tool
REM Usage: build.bat [target] [config] [extra_args...]
REM Examples:
REM   build.bat AlisEditor Development
REM   build.bat AlisEditor Development -Module=ProjectBoot

setlocal

REM Read UE path from config
set CONFIG_FILE=%~dp0..\..\config\ue_path.conf
if exist "%CONFIG_FILE%" (
    for /f "usebackq tokens=1,2 delims==" %%a in ("%CONFIG_FILE%") do (
        if "%%a"=="UE_PATH" set UE_PATH=%%b
    )
)

REM Fallback to common paths
if not defined UE_PATH (
    if exist "<ue-path>" set UE_PATH=<ue-path>
    if exist "<ue-path>" set UE_PATH=<ue-path>
)

if not defined UE_PATH (
    echo ERROR: UE_PATH not found! Create scripts\config\ue_path.conf with UE_PATH=<ue-path>
    exit /b 1
)

REM Remove quotes from UE_PATH
set UE_PATH=%UE_PATH:"=%

REM Project file
pushd "%~dp0..\..\.."
set PROJECT_ROOT=%CD%
popd
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject

REM Parse arguments
set TARGET=%1
set CONFIG=%2
if "%TARGET%"=="" set TARGET=AlisEditor
if "%CONFIG%"=="" set CONFIG=Development

REM Extra arguments (everything after first 2)
set EXTRA_ARGS=%3 %4 %5 %6 %7 %8 %9

echo Building %TARGET% (%CONFIG%)...
echo.

"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% "%PROJECT_FILE%" %EXTRA_ARGS%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build completed successfully!
    exit /b 0
) else (
    echo.
    echo Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
