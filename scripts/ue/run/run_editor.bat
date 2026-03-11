@echo off
setlocal
set PROJ=<project-root>\Alis.uproject
set UE=<ue-path>

"%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%" -NoLogTimes
exit /b %ERRORLEVEL%
