@echo off
setlocal

set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..\..\..") do set PROJECT_ROOT=%%~fI
set PROJECT_FILE=%PROJECT_ROOT%\Alis.uproject
set REPORTS_DIR=%PROJECT_ROOT%\Saved\Validation\Reports

call "%PROJECT_ROOT%\scripts\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1

if not exist "%REPORTS_DIR%" mkdir "%REPORTS_DIR%"

REM UBT is invoked directly (not via Build.bat), so wire up the engine's
REM BUNDLED dotnet runtime first - UE 5.8+ UBT targets a .NET major that
REM machines usually don't have installed (verified: 0x80008096 launch
REM failure without this). GetDotnetPath.bat is engine-version-agnostic.
call "%UE_PATH%\Engine\Build\BatchFiles\GetDotnetPath.bat"
if errorlevel 1 (
    echo ERROR: engine bundled dotnet setup failed
    exit /b 1
)

echo Running UnrealHeaderTool validation...
"%UE_PATH%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" ^
    -Mode=UnrealHeaderTool ^
    "-Target=AlisEditor Win64 Development -Project=\"%PROJECT_FILE%\"" ^
    -WarningsAsErrors ^
    -FailIfGeneratedCodeChanges ^
    > "%REPORTS_DIR%\uht.log" 2>&1
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [OK] UHT validation passed
exit /b 0
