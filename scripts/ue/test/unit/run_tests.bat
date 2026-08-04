@echo off
setlocal
REM Resolve UE_PATH from the conf SOT (stale-env hard fail included)
call "%~dp0..\..\..\config\resolve_ue_path.bat"
if errorlevel 1 exit /b 1
set PROJ=<project-root>\Alis.uproject
set UE=%UE_PATH%
set OUT=<project-root>\Saved\Automation
set TLOG=<project-root>\Saved\Logs\tests_full.log

if not exist "<project-root>\Saved\Logs" mkdir "<project-root>\Saved\Logs"
if not exist "%OUT%" mkdir "%OUT%"

"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -unattended -nop4 -NoSound -NullRHI -stdout ^
  -ExecCmds="Automation RunTests ProjectMenuUI; Quit" -ReportExportPath="%OUT%" > "%TLOG%" 2>&1
exit /b %ERRORLEVEL%
