@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..\..\..") do set PROJECT_ROOT=%%~fI
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject
set REPORTS_DIR=%PROJECT_ROOT%\Saved\Validation\Reports

call "%PROJECT_ROOT%\scripts\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1

if not exist "%REPORTS_DIR%" mkdir "%REPORTS_DIR%"

echo Compiling Blueprints...
REM Scope: validate content ALIS owns. Excluded with reasons:
REM   /Engine        - engine-owned content is not ours to fix (UE 5.8's own
REM                    MoverExamples fails its own validation)
REM   /MoverExamples - engine sample plugin content (same ownership)
REM   /MutableSample - vendored Epic DEMO content (showcase GUI/anim assets
REM                    read a property Mutable made private in 5.8; ALIS's
REM                    real Mutable use is C++ via ProjectSkeletalCapabilities)
"%UE_PATH%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
    "%PROJECT_FILE%" ^
    -run=CompileAllBlueprints ^
    -IgnoreFolder=/Engine,/MoverExamples,/MutableSample ^
    -unattended ^
    -nop4 ^
    -NoSound ^
    -NullRHI ^
    -CrashForUAT ^
    -log="%REPORTS_DIR%\bp_compile.log" ^
    > "%REPORTS_DIR%\bp_compile_stdout.log" 2>&1
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [OK] Blueprints compiled
exit /b 0
