@echo off
setlocal
REM Resolve UE_PATH from the conf SOT (stale-env hard fail included)
call "%~dp0..\..\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1
set PROJ=<project-root>\Alis.uproject
set UE=%UE_PATH%

"%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" -NoLogTimes
exit /b %ERRORLEVEL%
