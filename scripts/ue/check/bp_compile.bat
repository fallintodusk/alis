@echo off
setlocal
set PROJ=<project-root>\Alis.uproject
set UE=<ue-path>
set LOG=<project-root>\Saved\Logs\bp_compile.log

if not exist "<project-root>\Saved\Logs" mkdir "<project-root>\Saved\Logs"

"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -run=CompileAllBlueprints -unattended -nop4 -NoSound -NullRHI -CrashForUAT > "%LOG%" 2>&1
exit /b %ERRORLEVEL%
