@echo off
setlocal
REM Resolve UE_PATH from the conf SOT (stale-env hard fail included)
call "%~dp0..\..\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1
for %%I in ("%~dp0..\..\..") do set "PROJECT_ROOT=%%~fI"
set "PROJ=%PROJECT_ROOT%\Alis.uproject"
set "UE=%UE_PATH%"

if not exist "%PROJ%" (
    echo ERROR: Alis.uproject not found at: %PROJ%
    exit /b 1
)

"%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" %* -NoLogTimes
exit /b %ERRORLEVEL%
