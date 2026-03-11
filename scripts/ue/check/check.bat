@echo off
REM Direct Windows wrapper for fast validation checks
REM Usage: check.bat [--uht|--syntax|--blueprints|--assets|--all]

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set CONFIG_FILE=%SCRIPT_DIR%..\..\config\ue_path.conf
set PROJECT_ROOT=%SCRIPT_DIR%..\..\..
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject
set REPORTS_DIR=%PROJECT_ROOT%\Saved\Validation\Reports

if exist "%CONFIG_FILE%" (
    for /f "usebackq tokens=1,2 delims==" %%a in ("%CONFIG_FILE%") do (
        if "%%a"=="UE_PATH" set UE_PATH=%%b
    )
)

if not defined UE_PATH (
    if exist "<ue-path>" set UE_PATH=<ue-path>
    if exist "<ue-path>" set UE_PATH=<ue-path>
)

if not defined UE_PATH (
    echo ERROR: UE_PATH not found. Check scripts\config\ue_path.conf
    exit /b 1
)

set UE_PATH=%UE_PATH:"=%

if not exist "%REPORTS_DIR%" mkdir "%REPORTS_DIR%"

set CHECK_TYPE=%1
if "%CHECK_TYPE%"=="" set CHECK_TYPE=--all

if "%CHECK_TYPE%"=="--uht" goto CHECK_UHT
if "%CHECK_TYPE%"=="--syntax" goto CHECK_SYNTAX
if "%CHECK_TYPE%"=="--blueprints" goto CHECK_BLUEPRINTS
if "%CHECK_TYPE%"=="--assets" goto CHECK_ASSETS
if "%CHECK_TYPE%"=="--all" goto CHECK_ALL

echo Usage: check.bat [--uht^|--syntax^|--blueprints^|--assets^|--all]
exit /b 1

:CHECK_ALL
call :RUN_UHT
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
call :RUN_SYNTAX
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
call :RUN_BLUEPRINTS
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
call :RUN_ASSETS
exit /b %ERRORLEVEL%

:CHECK_UHT
call :RUN_UHT
exit /b %ERRORLEVEL%

:CHECK_SYNTAX
call :RUN_SYNTAX
exit /b %ERRORLEVEL%

:CHECK_BLUEPRINTS
call :RUN_BLUEPRINTS
exit /b %ERRORLEVEL%

:CHECK_ASSETS
call :RUN_ASSETS
exit /b %ERRORLEVEL%

:RUN_UHT
echo [1/4] Running UnrealHeaderTool...
"%UE_PATH%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" ^
    -Mode=UnrealHeaderTool ^
    "-Target=AlisEditor Win64 Development -Project=\"%PROJECT_FILE%\"" ^
    -WarningsAsErrors ^
    -FailIfGeneratedCodeChanges ^
    > "%REPORTS_DIR%\uht.log" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [X] UHT failed
    exit /b 1
)
echo [OK] UHT passed
exit /b 0

:RUN_SYNTAX
echo [2/4] Running build validation...
"%UE_PATH%\Engine\Build\BatchFiles\Build.bat" ^
    AlisEditor Win64 Development ^
    "%PROJECT_FILE%" ^
    -skipcompile ^
    -NoHotReload ^
    > "%REPORTS_DIR%\skipcompile.log" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [X] Syntax validation failed
    exit /b 1
)
echo [OK] Syntax validation passed
exit /b 0

:RUN_BLUEPRINTS
echo [3/4] Compiling Blueprints...
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%PROJECT_FILE%" ^
    -run=CompileAllBlueprints ^
    -unattended ^
    -nop4 ^
    -CrashForUAT ^
    -log="%REPORTS_DIR%\bp_compile.log" ^
    > "%REPORTS_DIR%\bp_compile_stdout.log" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [X] Blueprint compilation failed
    exit /b 1
)
echo [OK] Blueprints compiled
exit /b 0

:RUN_ASSETS
echo [4/4] Validating assets...
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%PROJECT_FILE%" ^
    -run=DataValidation ^
    -unattended ^
    -nop4 ^
    -log="%REPORTS_DIR%\data_validation.log" ^
    > "%REPORTS_DIR%\data_validation_stdout.log" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [X] Asset validation failed
    exit /b 1
)

echo [OK] Asset validation complete
echo [4/4] Validating ProjectMind data mappings...
python "%SCRIPT_DIR%validate_projectmind_data.py" > "%REPORTS_DIR%\projectmind_data_validation.log" 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [X] ProjectMind data validation failed
    exit /b 1
)

echo [OK] ProjectMind data validation complete
exit /b 0
